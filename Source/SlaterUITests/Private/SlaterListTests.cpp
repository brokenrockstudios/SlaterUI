// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Dom/JsonObject.h"
#include "SlaterListTestKit.h"
#include "SlaterModel.h"
#include "SlaterSurface.h"
#include "Widgets/Text/STextBlock.h"

namespace SlaterListTestsPrivate
{
	const TCHAR* const LsRepeatLayout = TEXT("<vbox><repeat source='{L}' key='Id' columns='1'><text>{N}</text></repeat></vbox>");
	const TCHAR* const LsListJson = TEXT("{\"L\":[{\"Id\":\"a\",\"N\":1},{\"Id\":\"b\",\"N\":2},{\"Id\":\"c\",\"N\":3}]}");

	FString LsErrors(const FSlaterBuildResult& Result)
	{
		FString Text;
		for (const FSlaterError& Error : Result.Errors)
		{
			Text += Error.ToString() + TEXT("\n");
		}
		return Text;
	}

	FSlaterBuildResult LsBuild(const TCHAR* Layout)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}

	FString LsTextOf(const TSharedPtr<SWidget>& Item)
	{
		const TSharedPtr<SWidget> Text = Item.IsValid() ? SlaterListKit::Find(Item.ToSharedRef(), TEXT("STextBlock")) : nullptr;
		return Text.IsValid() ? StaticCastSharedPtr<STextBlock>(Text)->GetText().ToString() : FString(TEXT("<none>"));
	}

	TSharedRef<FJsonObject> LsItem(const TCHAR* Id, int32 N)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("Id"), Id);
		Item->SetNumberField(TEXT("N"), N);
		return Item;
	}
}
using namespace SlaterListTestsPrivate;

/** The mock data model: JSON in, scalars and keyed items out, a broadcast per change. */
TEST_CLASS(SlaterModelTests, "BRS.SlaterUI.List.Model")
{
	TSharedPtr<FSlaterModel> Model;

	BEFORE_EACH()
	{
		Model = MakeShared<FSlaterModel>();
		FSlaterError Error;
		ASSERT_THAT(IsTrue(Model->LoadMock(TEXT("{\"Title\":\"Pack\",\"Gold\":120,\"Half\":1.5,\"Open\":true,\"P\":{\"Name\":\"Ana\"},\"L\":[{\"Id\":\"a\",\"N\":1},{\"Id\":7,\"N\":2}]}"), TEXT("m"), Error)));
	}

	TEST_METHOD(Scalars_AreReadAsText_WholeNumbersWithoutPoint)
	{
		FString Text;
		ASSERT_THAT(IsTrue(Model->GetString(TEXT("Title"), Text)));
		ASSERT_THAT(IsTrue(Text == TEXT("Pack")));
		ASSERT_THAT(IsTrue(Model->GetString(TEXT("Gold"), Text)));
		ASSERT_THAT(IsTrue(Text == TEXT("120")));
		ASSERT_THAT(IsTrue(Model->GetString(TEXT("Half"), Text)));
		ASSERT_THAT(IsTrue(Text == TEXT("1.5")));
		ASSERT_THAT(IsTrue(Model->GetString(TEXT("Open"), Text)));
		ASSERT_THAT(IsTrue(Text == TEXT("true")));
		ASSERT_THAT(IsTrue(Model->GetString(TEXT("P.Name"), Text)));
		ASSERT_THAT(IsTrue(Text == TEXT("Ana")));
		ASSERT_THAT(IsFalse(Model->GetString(TEXT("Missing"), Text)));
		ASSERT_THAT(IsFalse(Model->GetString(TEXT("Q.Name"), Text)));
		ASSERT_THAT(IsFalse(Model->GetString(TEXT("L"), Text)));
	}

	TEST_METHOD(BadJson_IsAnErrorWithTheLabel)
	{
		FSlaterModel Other;
		FSlaterError Error;
		ASSERT_THAT(IsFalse(Other.LoadMock(TEXT("{\"A\":"), TEXT("Layouts/X.mock.json"), Error)));
		ASSERT_THAT(IsTrue(Error.ToString().StartsWith(TEXT("Layouts/X.mock.json(1,1): not a JSON object"))));
	}

	TEST_METHOD(Items_AreFoundByKey_NumbersAsText)
	{
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("a")).IsValid()));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("7")).IsValid()));
		ASSERT_THAT(IsFalse(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("z")).IsValid()));
		ASSERT_THAT(IsFalse(Model->FindItem(TEXT("Nope"), TEXT("Id"), TEXT("a")).IsValid()));
	}

	TEST_METHOD(Changes_BroadcastOnce_AndOnlyWhenTheValueDiffers)
	{
		int32 Values = 0;
		int32 Fields = 0;
		int32 Adds = 0;
		int32 Removes = 0;
		Model->OnValueChanged.AddLambda([&](const FString&) { ++Values; });
		Model->OnItemFieldChanged.AddLambda([&](const FString&, const FString&, const FString&) { ++Fields; });
		Model->OnItemsChanged.AddLambda([&](const FString&, const FString&, bool bAdded) { bAdded ? ++Adds : ++Removes; });

		ASSERT_THAT(IsTrue(Model->SetValue(TEXT("Title"), TEXT("Stash"))));
		ASSERT_THAT(IsTrue(Model->SetValue(TEXT("Title"), TEXT("Stash"))));
		ASSERT_THAT(AreEqual(Values, 1));
		ASSERT_THAT(IsTrue(Model->SetItemNumber(TEXT("L"), TEXT("Id"), TEXT("a"), TEXT("N"), 9)));
		ASSERT_THAT(IsTrue(Model->SetItemNumber(TEXT("L"), TEXT("Id"), TEXT("a"), TEXT("N"), 9)));
		ASSERT_THAT(AreEqual(Fields, 1));
		ASSERT_THAT(IsFalse(Model->SetItemNumber(TEXT("L"), TEXT("Id"), TEXT("zz"), TEXT("N"), 9)));
		ASSERT_THAT(IsTrue(Model->AddItem(TEXT("L"), TEXT("Id"), LsItem(TEXT("c"), 3))));
		ASSERT_THAT(IsFalse(Model->AddItem(TEXT("L"), TEXT("Id"), LsItem(TEXT("c"), 3))));
		ASSERT_THAT(AreEqual(Adds, 1));
		ASSERT_THAT(IsTrue(Model->RemoveItem(TEXT("L"), TEXT("Id"), TEXT("c"))));
		ASSERT_THAT(IsFalse(Model->RemoveItem(TEXT("L"), TEXT("Id"), TEXT("c"))));
		ASSERT_THAT(AreEqual(Removes, 1));
		FString Text;
		ASSERT_THAT(IsTrue(Model->GetString(TEXT("Title"), Text)));
		ASSERT_THAT(IsTrue(Text == TEXT("Stash")));
	}

	TEST_METHOD(KeyIndex_FollowsAddRemoveAndAChangedKey)
	{
		// The first lookup builds the index; every later change has to keep it right.
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("a")).IsValid()));
		ASSERT_THAT(IsTrue(Model->AddItem(TEXT("L"), TEXT("Id"), LsItem(TEXT("c"), 3))));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("c")).IsValid()));
		ASSERT_THAT(IsTrue(Model->RemoveItem(TEXT("L"), TEXT("Id"), TEXT("a"))));
		ASSERT_THAT(IsFalse(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("a")).IsValid()));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("c")).IsValid()));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("7")).IsValid()));

		ASSERT_THAT(IsTrue(Model->SetItemString(TEXT("L"), TEXT("Id"), TEXT("c"), TEXT("Id"), TEXT("seven"))));
		ASSERT_THAT(IsFalse(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("c")).IsValid()));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("seven")).IsValid()));
		// A removal by the new key takes the right element out of the array.
		ASSERT_THAT(IsTrue(Model->RemoveItem(TEXT("L"), TEXT("Id"), TEXT("seven"))));
		ASSERT_THAT(AreEqual(Model->FindArray(TEXT("L"))->Num(), 1));
	}

	TEST_METHOD(KeyIndex_IsPerKeyField_AndSurvivesChangesMadeThroughAnotherField)
	{
		// Items found by a second key field, then one added: both fields still find their items.
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("a")).IsValid()));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("N"), TEXT("2")).IsValid()));
		ASSERT_THAT(IsTrue(Model->AddItem(TEXT("L"), TEXT("Id"), LsItem(TEXT("c"), 3))));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("N"), TEXT("3")).IsValid()));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("c")).IsValid()));
		ASSERT_THAT(IsTrue(Model->RemoveItem(TEXT("L"), TEXT("Id"), TEXT("a"))));
		ASSERT_THAT(IsFalse(Model->FindItem(TEXT("L"), TEXT("N"), TEXT("1")).IsValid()));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("N"), TEXT("2")).IsValid()));
	}

	TEST_METHOD(KeyIndex_WithADuplicateKeyInTheFile_FindsTheFirst_AndThenTheSecondAfterItIsRemoved)
	{
		FSlaterModel Other;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(Other.LoadMock(TEXT("{\"L\":[{\"Id\":\"x\",\"N\":1},{\"Id\":\"x\",\"N\":2}]}"), TEXT("m"), Error)));
		FString Text;
		const TSharedPtr<FJsonObject> First = Other.FindItem(TEXT("L"), TEXT("Id"), TEXT("x"));
		ASSERT_THAT(IsTrue(First.IsValid() && FSlaterModel::ReadField(*First, TEXT("N"), Text) && Text == TEXT("1")));
		ASSERT_THAT(IsTrue(Other.RemoveItem(TEXT("L"), TEXT("Id"), TEXT("x"))));
		const TSharedPtr<FJsonObject> Second = Other.FindItem(TEXT("L"), TEXT("Id"), TEXT("x"));
		ASSERT_THAT(IsTrue(Second.IsValid() && FSlaterModel::ReadField(*Second, TEXT("N"), Text) && Text == TEXT("2")));
	}

	TEST_METHOD(ReloadingTheMock_DropsTheIndex)
	{
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("a")).IsValid()));
		FSlaterError Error;
		ASSERT_THAT(IsTrue(Model->LoadMock(TEXT("{\"L\":[{\"Id\":\"b\",\"N\":1}]}"), TEXT("m2"), Error)));
		ASSERT_THAT(IsFalse(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("a")).IsValid()));
		ASSERT_THAT(IsTrue(Model->FindItem(TEXT("L"), TEXT("Id"), TEXT("b")).IsValid()));
	}
};

/** Surfaces with the same values draw with one shared brush, and a surface set to what it already is changes nothing. */
TEST_CLASS(SlaterSurfaceBrushTests, "BRS.SlaterUI.Content.SurfaceBrush")
{
	static FSlaterSurface Surface(float Red)
	{
		FSlaterSurface Result;
		Result.Fill = FLinearColor(Red, 0.2f, 0.3f, 1.f);
		Result.Border = FLinearColor::White;
		Result.BorderWidth = 1.f;
		Result.CornerRadius = 4.f;
		return Result;
	}

	TEST_METHOD(EqualSurfaces_ShareOneBrush_DifferentOnesDoNot)
	{
		const TSharedRef<const FSlateBrush> A = SSlaterSurface::BrushFor(Surface(0.5f));
		const TSharedRef<const FSlateBrush> B = SSlaterSurface::BrushFor(Surface(0.5f));
		const TSharedRef<const FSlateBrush> C = SSlaterSurface::BrushFor(Surface(0.6f));
		ASSERT_THAT(IsTrue(&A.Get() == &B.Get()));
		ASSERT_THAT(IsFalse(&A.Get() == &C.Get()));
		ASSERT_THAT(IsTrue(A->TintColor.GetSpecifiedColor() == Surface(0.5f).Fill));
	}

	TEST_METHOD(SetSurface_ToTheSameValues_KeepsTheBrush_ToOthers_SwitchesIt)
	{
		TSharedRef<SSlaterSurface> Widget = SNew(SSlaterSurface).Surface(Surface(0.5f));
		const FSlateBrush* Before = Widget->GetBorderImage();
		ASSERT_THAT(IsTrue(Before == &SSlaterSurface::BrushFor(Surface(0.5f)).Get()));
		Widget->SetSurface(Surface(0.5f));
		ASSERT_THAT(IsTrue(Widget->GetBorderImage() == Before));
		Widget->SetSurface(Surface(0.9f));
		ASSERT_THAT(IsFalse(Widget->GetBorderImage() == Before));
		ASSERT_THAT(IsTrue(Widget->GetBorderImage()->TintColor.GetSpecifiedColor() == Surface(0.9f).Fill));
		ASSERT_THAT(IsTrue(Widget->GetSurface() == Surface(0.9f)));
	}

	TEST_METHOD(AWidgetKeepsItsBrush_WhenTheCacheForgetsIt)
	{
		TSharedRef<SSlaterSurface> Widget = SNew(SSlaterSurface).Surface(Surface(0.11f));
		const FLinearColor Fill = Surface(0.11f).Fill;
		// More distinct surfaces than the cache keeps.
		for (int32 I = 0; I < 100; ++I)
		{
			SSlaterSurface::BrushFor(Surface(0.2f + I * 0.005f));
		}
		ASSERT_THAT(IsTrue(Widget->GetBorderImage()->TintColor.GetSpecifiedColor() == Fill));
	}
};

/** `<repeat>` and `{Path}` are checked when the layout is built, with the usual line and column. */
TEST_CLASS(SlaterRepeatValidationTests, "BRS.SlaterUI.List.Validation")
{
	TEST_METHOD(RepeatWithoutKey_NamesTheMissingAttribute)
	{
		const FSlaterBuildResult Result = LsBuild(TEXT("<repeat source='{L}'><text>x</text></repeat>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("needs the attribute 'key'"))));
	}

	TEST_METHOD(RepeatSourceWithoutBraces_SaysHowToWriteIt)
	{
		const FSlaterBuildResult Result = LsBuild(TEXT("<repeat source='L' key='Id'><text>x</text></repeat>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("source=\"{Items}\""))));
	}

	TEST_METHOD(RepeatNeedsExactlyOneChild)
	{
		const FSlaterBuildResult None = LsBuild(TEXT("<repeat source='{L}' key='Id'/>"));
		ASSERT_THAT(IsFalse(None.Succeeded()));
		ASSERT_THAT(IsTrue(None.Errors[0].Message.Contains(TEXT("one child"))));
		const FSlaterBuildResult Two = LsBuild(TEXT("<repeat source='{L}' key='Id'><text>a</text><text>b</text></repeat>"));
		ASSERT_THAT(IsFalse(Two.Succeeded()));
		ASSERT_THAT(IsTrue(Two.Errors[0].Message.Contains(TEXT("takes one child"))));
	}

	TEST_METHOD(AnErrorInsideTheTemplate_IsFoundAtBuildTime_WithItsPosition)
	{
		const FSlaterBuildResult Result = LsBuild(TEXT("<repeat source='{L}' key='Id'>\n\t<txet>{N}</txet>\n</repeat>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].ToString().StartsWith(TEXT("Layouts/Test.sltr(2,2): unknown element <txet>; did you mean 'text'?"))));
	}

	TEST_METHOD(RepeatInsideARepeatTemplate_IsRefused)
	{
		const FSlaterBuildResult Result = LsBuild(TEXT("<repeat source='{L}' key='Id'><repeat source='{M}' key='Id'><text>x</text></repeat></repeat>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("inside the template"))));
	}

	TEST_METHOD(IdInsideATemplate_IsRefused)
	{
		const FSlaterBuildResult Result = LsBuild(TEXT("<repeat source='{L}' key='Id'><text id='t'>{N}</text></repeat>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("not allowed inside a template"))));
	}

	TEST_METHOD(BindingOnAnAttributeThatCannotBind_IsRefused)
	{
		const FSlaterBuildResult Result = LsBuild(TEXT("<text font='{F}'>x</text>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].ToString().StartsWith(TEXT("Layouts/Test.sltr(1,7): 'font' on <text> cannot be bound"))));
	}

	TEST_METHOD(BoundAttributes_AreNotCheckedForTheirValue_UntilTheyArrive)
	{
		const FSlaterBuildResult Result = LsBuild(TEXT("<vbox><text>{Title}</text><image src='{Icon}' visible='{Open}'/><surface role='UI.Surface.Panel' state='{S}'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Bindings.Num(), 4));
		ASSERT_THAT(AreEqual(Result.Repeats.Num(), 0));
	}

	TEST_METHOD(TemplateBindings_DoNotBelongToTheLayout)
	{
		const FSlaterBuildResult Result = LsBuild(LsRepeatLayout);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Bindings.Num(), 0));
		ASSERT_THAT(AreEqual(Result.Repeats.Num(), 1));
	}
};

/** Pushed bindings: a change reaches exactly the widget it feeds. */
TEST_CLASS(SlaterRepeatPushedTests, "BRS.SlaterUI.List.Pushed")
{
	SlaterListKit::FRig Rig;

	AFTER_EACH()
	{
		Rig.Shutdown();
	}

	TEST_METHOD(ShippedProbeLayout_BuildsAndBindsItsMockFile)
	{
		FSlaterError Error;
		FSlaterModel Mock;
		ASSERT_THAT(IsTrue(Slater::LoadMockFor(Slater::FindLayoutFile(TEXT("ListProbe")), Mock, Error)));
		FString Title;
		ASSERT_THAT(IsTrue(Mock.GetString(TEXT("Title"), Title)));
		ASSERT_THAT(IsTrue(Title == TEXT("Backpack")));

		FString Json;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Json, *FPaths::ChangeExtension(Slater::FindLayoutFile(TEXT("ListProbe")), TEXT("mock.json")))));
		ASSERT_THAT(IsTrue(Rig.Init(SlaterListKit::LayoutText(10), Json, ESlaterBindMode::Pushed)));
		ASSERT_THAT(IsTrue(Rig.Result.Warnings.IsEmpty()));
		ASSERT_THAT(AreEqual(Rig.Result.Bindings.Num(), 1));
		ASSERT_THAT(AreEqual(Rig.Result.Repeats.Num(), 2));
		ASSERT_THAT(AreEqual(Rig.Repeat(0)->NumItems(), 20));
		ASSERT_THAT(AreEqual(Rig.Repeat(1)->NumItems(), 3));
		const TSharedPtr<SWidget> Title0 = SlaterListKit::Find(Rig.Result.Root.ToSharedRef(), TEXT("STextBlock"));
		ASSERT_THAT(IsTrue(StaticCastSharedPtr<STextBlock>(Title0)->GetText().ToString() == TEXT("Backpack")));
		ASSERT_THAT(IsTrue(LsTextOf(Rig.Repeat(1)->FindItemWidget(TEXT("arrows"))) == TEXT("64")));
	}

	TEST_METHOD(AChangedField_UpdatesOnlyItsOwnWidget)
	{
		ASSERT_THAT(IsTrue(Rig.Init(LsRepeatLayout, LsListJson, ESlaterBindMode::Pushed)));
		const TSharedPtr<SSlaterRepeat> Repeat = Rig.Repeat(0);
		const TSharedPtr<SWidget> A = Repeat->FindItemWidget(TEXT("a"));
		const TSharedPtr<SWidget> B = Repeat->FindItemWidget(TEXT("b"));
		ASSERT_THAT(IsTrue(LsTextOf(B) == TEXT("2")));

		Slater::ListStats().Reset();
		ASSERT_THAT(IsTrue(Rig.Model->SetItemNumber(TEXT("L"), TEXT("Id"), TEXT("b"), TEXT("N"), 20)));
		ASSERT_THAT(IsTrue(LsTextOf(B) == TEXT("20")));
		ASSERT_THAT(IsTrue(LsTextOf(A) == TEXT("1")));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 1));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 0));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsDestroyed, 0));
		ASSERT_THAT(IsTrue(Repeat->FindItemWidget(TEXT("b")) == B));
	}

	TEST_METHOD(AChangeToAFieldNothingBinds_AppliesNothing)
	{
		ASSERT_THAT(IsTrue(Rig.Init(LsRepeatLayout, LsListJson, ESlaterBindMode::Pushed)));
		Slater::ListStats().Reset();
		ASSERT_THAT(IsTrue(Rig.Model->SetItemNumber(TEXT("L"), TEXT("Id"), TEXT("b"), TEXT("Other"), 5)));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 0));
	}

	TEST_METHOD(ARemovedKey_DestroysOnlyItsWidget)
	{
		ASSERT_THAT(IsTrue(Rig.Init(LsRepeatLayout, LsListJson, ESlaterBindMode::Pushed)));
		const TSharedPtr<SSlaterRepeat> Repeat = Rig.Repeat(0);
		TWeakPtr<SWidget> A = Repeat->FindItemWidget(TEXT("a"));
		TWeakPtr<SWidget> B = Repeat->FindItemWidget(TEXT("b"));
		TWeakPtr<SWidget> C = Repeat->FindItemWidget(TEXT("c"));

		Slater::ListStats().Reset();
		ASSERT_THAT(IsTrue(Rig.Model->RemoveItem(TEXT("L"), TEXT("Id"), TEXT("b"))));
		ASSERT_THAT(AreEqual(Repeat->NumItems(), 2));
		ASSERT_THAT(IsFalse(B.IsValid()));
		ASSERT_THAT(IsTrue(A.IsValid()));
		ASSERT_THAT(IsTrue(C.IsValid()));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsDestroyed, 1));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 0));
		ASSERT_THAT(IsTrue(Repeat->FindItemWidget(TEXT("b")) == nullptr));
		ASSERT_THAT(IsTrue(Repeat->FindItemWidget(TEXT("c")) == C.Pin()));
	}

	TEST_METHOD(ARemovedKey_ClosesTheGap_WithoutRebuildingTheOthers)
	{
		ASSERT_THAT(IsTrue(Rig.Init(LsRepeatLayout, LsListJson, ESlaterBindMode::Pushed)));
		const TSharedRef<SWidget> Root = Rig.Result.Root.ToSharedRef();
		Root->SlatePrepass(1.f);
		const float Before = Root->GetDesiredSize().Y;
		ASSERT_THAT(IsTrue(Before > 0.f));
		ASSERT_THAT(IsTrue(Rig.Model->RemoveItem(TEXT("L"), TEXT("Id"), TEXT("a"))));
		Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(Root->GetDesiredSize().Y < Before * 0.8f));
		ASSERT_THAT(IsTrue(LsTextOf(Rig.Repeat(0)->FindItemWidget(TEXT("c"))) == TEXT("3")));
	}

	TEST_METHOD(AnAddedItem_BuildsOneInstance)
	{
		ASSERT_THAT(IsTrue(Rig.Init(LsRepeatLayout, LsListJson, ESlaterBindMode::Pushed)));
		Slater::ListStats().Reset();
		ASSERT_THAT(IsTrue(Rig.Model->AddItem(TEXT("L"), TEXT("Id"), LsItem(TEXT("d"), 4))));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 1));
		ASSERT_THAT(AreEqual(Rig.Repeat(0)->NumItems(), 4));
		ASSERT_THAT(IsTrue(LsTextOf(Rig.Repeat(0)->FindItemWidget(TEXT("d"))) == TEXT("4")));
	}

	TEST_METHOD(ADuplicateKey_IsIgnoredWithAWarning)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("has two items with the key 'a'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		ASSERT_THAT(IsTrue(Rig.Init(LsRepeatLayout, TEXT("{\"L\":[{\"Id\":\"a\",\"N\":1},{\"Id\":\"a\",\"N\":2}]}"), ESlaterBindMode::Pushed)));
		ASSERT_THAT(AreEqual(Rig.Repeat(0)->NumItems(), 1));
		ASSERT_THAT(IsTrue(LsTextOf(Rig.Repeat(0)->FindItemWidget(TEXT("a"))) == TEXT("1")));
	}

	TEST_METHOD(AMovedItem_KeepsItsWidget)
	{
		ASSERT_THAT(IsTrue(Rig.Init(SlaterListKit::LayoutText(10), SlaterListKit::MakeMockJson(20, 3), ESlaterBindMode::Pushed)));
		const TSharedPtr<SSlaterRepeat> Items = Rig.Repeat(1);
		const TSharedPtr<SWidget> Before = Items->FindItemWidget(TEXT("i1"));
		Slater::ListStats().Reset();
		ASSERT_THAT(IsTrue(Rig.Model->SetItemNumber(TEXT("Items"), TEXT("Id"), TEXT("i1"), TEXT("Slot"), 15)));
		ASSERT_THAT(IsTrue(Items->FindItemWidget(TEXT("i1")) == Before));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 0));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsDestroyed, 0));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 0));
	}

	TEST_METHOD(ACellState_ChangesItsSurface_AndNoOtherCells)
	{
		ASSERT_THAT(IsTrue(Rig.Init(SlaterListKit::LayoutText(10), SlaterListKit::MakeMockJson(20, 3), ESlaterBindMode::Pushed)));
		const TSharedPtr<SSlaterRepeat> Cells = Rig.Repeat(0);
		const auto SurfaceOf = [&](const TCHAR* Index) { return StaticCastSharedPtr<SSlaterSurface>(Cells->FindItemWidget(Index)); };
		FSlaterSurface Normal;
		FSlaterSurface Hover;
		ASSERT_THAT(IsTrue(Rig.Stack.FindSurface(TEXT("UI.Surface.Button"), Normal)));
		ASSERT_THAT(IsTrue(Rig.Stack.FindSurface(TEXT("UI.Surface.Button.Hover"), Hover)));
		ASSERT_THAT(IsTrue(SurfaceOf(TEXT("4"))->GetSurface().Fill == Normal.Fill));

		Slater::ListStats().Reset();
		ASSERT_THAT(IsTrue(Rig.Model->SetItemString(TEXT("Cells"), TEXT("Index"), TEXT("4"), TEXT("State"), TEXT("hover"))));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 1));
		ASSERT_THAT(IsTrue(SurfaceOf(TEXT("4"))->GetSurface().Fill == Hover.Fill));
		ASSERT_THAT(IsTrue(SurfaceOf(TEXT("5"))->GetSurface().Fill == Normal.Fill));
		ASSERT_THAT(IsTrue(SurfaceOf(TEXT("3"))->GetSurface().Fill == Normal.Fill));
		ASSERT_THAT(IsTrue(Rig.Model->SetItemString(TEXT("Cells"), TEXT("Index"), TEXT("4"), TEXT("State"), TEXT("normal"))));
		ASSERT_THAT(IsTrue(SurfaceOf(TEXT("4"))->GetSurface().Fill == Normal.Fill));
	}

	TEST_METHOD(ALockFlag_TogglesTheLockVisibility)
	{
		ASSERT_THAT(IsTrue(Rig.Init(SlaterListKit::LayoutText(10), SlaterListKit::MakeMockJson(20, 3), ESlaterBindMode::Pushed)));
		const TSharedPtr<SWidget> Item = Rig.Repeat(1)->FindItemWidget(TEXT("i0"));
		// i0 is locked in the mock data (every fifth item): its lock image is the second SImage.
		TArray<TSharedPtr<SWidget>> Images;
		TFunction<void(SWidget&)> Collect = [&](SWidget& Widget)
		{
			if (Widget.GetType() == FName(TEXT("SImage")))
			{
				Images.Add(Widget.AsShared());
			}
			Widget.GetChildren()->ForEachWidget([&](SWidget& Child) { Collect(Child); });
		};
		Collect(*Item);
		ASSERT_THAT(AreEqual(Images.Num(), 2));
		ASSERT_THAT(IsTrue(Images[1]->GetVisibility() == EVisibility::HitTestInvisible));
		ASSERT_THAT(IsTrue(Rig.Model->SetItemBool(TEXT("Items"), TEXT("Id"), TEXT("i0"), TEXT("Locked"), false)));
		ASSERT_THAT(IsTrue(Images[1]->GetVisibility() == EVisibility::Collapsed));
	}

	TEST_METHOD(ATopLevelValue_UpdatesItsBinding)
	{
		ASSERT_THAT(IsTrue(Rig.Init(SlaterListKit::LayoutText(10), SlaterListKit::MakeMockJson(20, 3), ESlaterBindMode::Pushed)));
		const TSharedPtr<SWidget> Title = SlaterListKit::Find(Rig.Result.Root.ToSharedRef(), TEXT("STextBlock"));
		Slater::ListStats().Reset();
		ASSERT_THAT(IsTrue(Rig.Model->SetValue(TEXT("Title"), TEXT("Stash"))));
		ASSERT_THAT(IsTrue(StaticCastSharedPtr<STextBlock>(Title)->GetText().ToString() == TEXT("Stash")));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 1));
	}

	TEST_METHOD(DroppingTheViewAndBinder_RemovesEverySubscription)
	{
		for (int32 Round = 0; Round < 25; ++Round)
		{
			ASSERT_THAT(IsTrue(Rig.Init(SlaterListKit::LayoutText(10), SlaterListKit::MakeMockJson(20, 3), ESlaterBindMode::Pushed)));
			const TSharedPtr<FSlaterModel> Model = Rig.Model;
			ASSERT_THAT(IsTrue(Model->OnItemFieldChanged.IsBound()));
			Rig.Binder.Reset();
			Rig.Result = FSlaterBuildResult();
			ASSERT_THAT(IsFalse(Model->OnItemFieldChanged.IsBound()));
			ASSERT_THAT(IsFalse(Model->OnItemsChanged.IsBound()));
			ASSERT_THAT(IsFalse(Model->OnValueChanged.IsBound()));
			Rig.Shutdown();
		}
	}
};

/** Polled bindings read the same data through Slate attributes each frame. */
TEST_CLASS(SlaterRepeatPolledTests, "BRS.SlaterUI.List.Polled")
{
	SlaterListKit::FRig Rig;

	AFTER_EACH()
	{
		Rig.Shutdown();
	}

	TEST_METHOD(AChange_ShowsAfterTheNextPrepass_WithoutAnyPush)
	{
		ASSERT_THAT(IsTrue(Rig.Init(LsRepeatLayout, LsListJson, ESlaterBindMode::Polled)));
		const TSharedRef<SWidget> Root = Rig.Result.Root.ToSharedRef();
		Root->SlatePrepass(1.f);
		const TSharedPtr<SWidget> B = Rig.Repeat(0)->FindItemWidget(TEXT("b"));
		ASSERT_THAT(IsTrue(LsTextOf(B) == TEXT("2")));

		Slater::ListStats().Reset();
		ASSERT_THAT(IsTrue(Rig.Model->SetItemNumber(TEXT("L"), TEXT("Id"), TEXT("b"), TEXT("N"), 20)));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 0));
		Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(LsTextOf(B) == TEXT("20")));
		ASSERT_THAT(IsTrue(Slater::ListStats().PolledReads >= 3));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 0));
	}

	TEST_METHOD(EveryFrame_ReadsEveryBinding_EvenWhenNothingChanged)
	{
		ASSERT_THAT(IsTrue(Rig.Init(LsRepeatLayout, LsListJson, ESlaterBindMode::Polled)));
		const TSharedRef<SWidget> Root = Rig.Result.Root.ToSharedRef();
		Root->SlatePrepass(1.f);
		Slater::ListStats().Reset();
		Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(Slater::ListStats().PolledReads >= 3));
	}

	TEST_METHOD(AddAndRemove_StillChangeTheWidgets)
	{
		ASSERT_THAT(IsTrue(Rig.Init(LsRepeatLayout, LsListJson, ESlaterBindMode::Polled)));
		ASSERT_THAT(IsTrue(Rig.Model->AddItem(TEXT("L"), TEXT("Id"), LsItem(TEXT("d"), 4))));
		ASSERT_THAT(AreEqual(Rig.Repeat(0)->NumItems(), 4));
		ASSERT_THAT(IsTrue(Rig.Model->RemoveItem(TEXT("L"), TEXT("Id"), TEXT("a"))));
		ASSERT_THAT(AreEqual(Rig.Repeat(0)->NumItems(), 3));
		Rig.Result.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(LsTextOf(Rig.Repeat(0)->FindItemWidget(TEXT("d"))) == TEXT("4")));
	}

	TEST_METHOD(ACellStateAndALockFlag_FollowTheModel)
	{
		ASSERT_THAT(IsTrue(Rig.Init(SlaterListKit::LayoutText(10), SlaterListKit::MakeMockJson(20, 3), ESlaterBindMode::Polled)));
		const TSharedRef<SWidget> Root = Rig.Result.Root.ToSharedRef();
		Root->SlatePrepass(1.f);
		FSlaterSurface Hover;
		ASSERT_THAT(IsTrue(Rig.Stack.FindSurface(TEXT("UI.Surface.Button.Hover"), Hover)));
		ASSERT_THAT(IsTrue(Rig.Model->SetItemString(TEXT("Cells"), TEXT("Index"), TEXT("2"), TEXT("State"), TEXT("hover"))));
		Root->SlatePrepass(1.f);
		const TSharedPtr<SSlaterSurface> Cell = StaticCastSharedPtr<SSlaterSurface>(Rig.Repeat(0)->FindItemWidget(TEXT("2")));
		ASSERT_THAT(IsTrue(Cell->GetBorderImage()->TintColor.GetSpecifiedColor() == Hover.Fill));
	}
};

#endif
