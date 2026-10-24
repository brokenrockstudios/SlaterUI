// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterBindTestKit.h"
#include "SlaterBindings.h"
#include "SlaterCommands.h"
#include "SlaterList.h"
#include "SlaterModel.h"
#include "SlaterObjectScope.h"
#include "SlaterOutlet.h"
#include "SlaterSurface.h"
#include "SlaterView.h"
#include "Widgets/Layout/SBox.h"

namespace SlaterOutletCommandTestsPrivate
{
	TSharedPtr<SWidget> OcFindType(const TSharedRef<SWidget>& Root, const TCHAR* Type)
	{
		if (Root->GetType() == FName(Type))
		{
			return Root;
		}
		TSharedPtr<SWidget> Found;
		Root->GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			if (!Found.IsValid())
			{
				Found = OcFindType(Child.AsShared(), Type);
			}
		});
		return Found;
	}

	TSharedPtr<SSlaterSurface> OcFirstSurface(const TSharedRef<SWidget>& Root)
	{
		return StaticCastSharedPtr<SSlaterSurface>(OcFindType(Root, TEXT("SSlaterSurface")));
	}

	FPointerEvent OcLeftMouse()
	{
		return FPointerEvent(0, FVector2f::ZeroVector, FVector2f::ZeroVector, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	}

	FSlaterBuildResult OcBuild(const FString& Text, const FSlaterStyleStack& Stack)
	{
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}

	const TCHAR* const OcHostLayout = TEXT("<vbox>\n\t<text>Before</text>\n\t<outlet id=\"Content\" fill=\"1\"><text>Default</text></outlet>\n\t<text>After</text>\n</vbox>");
}
using namespace SlaterOutletCommandTestsPrivate;

/** `<outlet>`: a named place C++ fills, kept through reloads. */
TEST_CLASS(SlaterOutletTests, "BRS.SlaterUI.Outlet")
{
	FSlaterBindFixture Fix;
	FSlaterStyleStack Stack;

	BEFORE_EACH()
	{
		Fix.Init();
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TSharedRef<STextBlock> MakeMine() const
	{
		return SNew(STextBlock).Text(FText::FromString(TEXT("Mine")));
	}

	TEST_METHOD(TheDefaultContent_ShowsUntilSomethingIsSet)
	{
		Fix.Write(TEXT("OcHost.sltr"), OcHostLayout);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("OcHost"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TArray<FString> Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(AreEqual(Texts.Num(), 3));
		ASSERT_THAT(IsTrue(Texts[0] == TEXT("Before") && Texts[1] == TEXT("Default") && Texts[2] == TEXT("After")));
		ASSERT_THAT(IsTrue(View->GetResult().Outlets.Contains(TEXT("Content"))));
	}

	TEST_METHOD(SetOutlet_ReplacesTheDefault_AndNullBringsItBack)
	{
		Fix.Write(TEXT("OcHost.sltr"), OcHostLayout);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("OcHost"));
		const TSharedRef<STextBlock> Mine = MakeMine();
		ASSERT_THAT(IsTrue(View->SetOutlet(TEXT("Content"), Mine)));
		TArray<FString> Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(AreEqual(Texts.Num(), 3));
		ASSERT_THAT(IsTrue(Texts[1] == TEXT("Mine")));
		ASSERT_THAT(IsTrue(View->GetOutletContent(TEXT("Content")) == Mine));

		ASSERT_THAT(IsTrue(View->SetOutlet(TEXT("Content"), nullptr)));
		Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(IsTrue(Texts[1] == TEXT("Default")));
		ASSERT_THAT(IsTrue(View->GetOutletContent(TEXT("Content")) == nullptr));
	}

	TEST_METHOD(TheContent_SurvivesAReload_ADifferentStyle_AndABrokenSave)
	{
		Fix.Write(TEXT("OcHost.sltr"), OcHostLayout);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("OcHost"));
		const TSharedRef<STextBlock> Mine = MakeMine();
		View->SetOutlet(TEXT("Content"), Mine);
		const TSharedPtr<SWidget> OldRoot = View->GetResult().Root;
		const int32 Builds = View->GetBuildCount();

		// The layout changes on disk: a new tree, with the same content in it.
		Fix.Write(TEXT("OcHost.sltr"), TEXT("<vbox>\n\t<text>First</text>\n\t<text>Before</text>\n\t<outlet id=\"Content\"><text>Default2</text></outlet>\n</vbox>"));
		ASSERT_THAT(IsTrue(Slater::NotifyFilesChanged({Fix.Dir / TEXT("Layouts/OcHost.sltr")})));
		ASSERT_THAT(AreEqual(View->GetBuildCount(), Builds + 1));
		ASSERT_THAT(IsTrue(View->GetResult().Root != OldRoot));
		TArray<FString> Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(AreEqual(Texts.Num(), 3));
		ASSERT_THAT(IsTrue(Texts[2] == TEXT("Mine")));
		ASSERT_THAT(IsTrue(View->GetOutletContent(TEXT("Content")) == Mine));

		ASSERT_THAT(IsTrue(View->SetStyle(TEXT("Dusk"))));
		Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(IsTrue(Texts.Num() == 3 && Texts[2] == TEXT("Mine")));

		// A broken save keeps the last good tree, and the content with it.
		Fix.Write(TEXT("OcHost.sltr"), TEXT("<vbox><outlet id=\"Content\">"));
		Slater::NotifyFilesChanged({Fix.Dir / TEXT("Layouts/OcHost.sltr")});
		ASSERT_THAT(IsTrue(View->HasReloadErrors()));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(IsTrue(Texts.Contains(TEXT("Mine"))));

		// Fixed again: still there.
		Fix.Write(TEXT("OcHost.sltr"), OcHostLayout);
		Slater::NotifyFilesChanged({Fix.Dir / TEXT("Layouts/OcHost.sltr")});
		ASSERT_THAT(IsFalse(View->HasReloadErrors()));
		Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(IsTrue(Texts.Num() == 3 && Texts[1] == TEXT("Mine")));
	}

	TEST_METHOD(AnOutletTheLayoutDoesNotHave_KeepsItsContentForALayoutThatDoes)
	{
		Fix.Write(TEXT("OcLater.sltr"), TEXT("<vbox><text>One</text></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("OcLater"));
		const TSharedRef<STextBlock> Mine = MakeMine();
		ASSERT_THAT(IsFalse(View->SetOutlet(TEXT("Slot"), Mine)));
		ASSERT_THAT(IsFalse(SlaterBindKit::Texts(View).Contains(TEXT("Mine"))));

		Fix.Write(TEXT("OcLater.sltr"), TEXT("<vbox><text>One</text><outlet id=\"Slot\"/></vbox>"));
		Slater::NotifyFilesChanged({Fix.Dir / TEXT("Layouts/OcLater.sltr")});
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View).Contains(TEXT("Mine"))));
	}

	TEST_METHOD(AnOutletWithASizeBox_IsStillFound)
	{
		const FSlaterBuildResult Result = OcBuild(TEXT("<vbox><outlet id=\"Sized\" width=\"100\" height=\"40\"/></vbox>"), Stack);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Outlets.Contains(TEXT("Sized"))));
		ASSERT_THAT(IsTrue(Result.ById.Contains(TEXT("Sized"))));
		// `id` names the outermost widget (the size box), the outlet map the outlet itself.
		ASSERT_THAT(IsTrue(Result.ById[TEXT("Sized")].Pin()->GetType() == FName(TEXT("SBox"))));
		Result.Outlets[TEXT("Sized")].Pin()->SetContent(MakeMine());
		ASSERT_THAT(IsTrue(Result.Outlets[TEXT("Sized")].Pin()->HasContent()));
	}

	TEST_METHOD(AnOutletWithoutAnId_NamesTheFix)
	{
		const FSlaterBuildResult Result = OcBuild(TEXT("<vbox>\n<outlet/>\n</vbox>"), Stack);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(AreEqual(Result.Errors[0].Line, 2));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("<outlet> needs an id"))));
	}

	TEST_METHOD(AnOutletInsideARepeatTemplate_IsRefused)
	{
		const FSlaterBuildResult Result = OcBuild(TEXT("<repeat source=\"{L}\" key=\"Id\"><outlet id=\"X\"/></repeat>"), Stack);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("'id' is not allowed inside a template"))));
	}

	TEST_METHOD(TwoOutletsWithOneId_AreRefused)
	{
		const FSlaterBuildResult Result = OcBuild(TEXT("<vbox><outlet id=\"A\"/><outlet id=\"A\"/></vbox>"), Stack);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("duplicate id 'A'"))));
	}
};

/** `on-click` and the command tables. */
TEST_CLASS(SlaterCommandTests, "BRS.SlaterUI.Command")
{
	FSlaterBindFixture Fix;
	FSlaterStyleStack Stack;
	int32 Fired = 0;
	FName LastName;
	TWeakPtr<SWidget> LastSource;

	BEFORE_EACH()
	{
		Fix.Init();
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		Slater::ListStats().Reset();
		Fired = 0;
	}

	AFTER_EACH()
	{
		Slater::Commands().Unregister(TEXT("Test.Go"));
		Fix.Shutdown();
	}

	FSlaterCommandHandler Counter()
	{
		return [this](const FSlaterCommandCall& Call)
		{
			++Fired;
			LastName = Call.Name;
			LastSource = Call.Source;
		};
	}

	TEST_METHOD(AClick_FiresTheNamedCommand_WithTheSurfaceThatWasClicked)
	{
		Fix.Write(TEXT("CmButton.sltr"), TEXT("<surface role=\"UI.Surface.Button\" on-click=\"Test.Go\"><text>Go</text></surface>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CmButton"));
		ASSERT_THAT(IsTrue(View->GetCommands().Register(TEXT("Test.Go"), Counter())));
		const TSharedPtr<SSlaterSurface> Surface = OcFirstSurface(View);
		ASSERT_THAT(IsTrue(Surface.IsValid()));
		ASSERT_THAT(IsTrue(Surface->GetCommand() == FName(TEXT("Test.Go"))));
		ASSERT_THAT(IsTrue(Surface->GetVisibility() == EVisibility::Visible));

		const FGeometry Geometry;
		ASSERT_THAT(IsTrue(Surface->OnMouseButtonDown(Geometry, OcLeftMouse()).IsEventHandled()));
		ASSERT_THAT(AreEqual(Fired, 0));
		ASSERT_THAT(IsTrue(Surface->OnMouseButtonUp(Geometry, OcLeftMouse()).IsEventHandled()));
		ASSERT_THAT(AreEqual(Fired, 1));
		ASSERT_THAT(IsTrue(LastName == FName(TEXT("Test.Go"))));
		ASSERT_THAT(IsTrue(LastSource.Pin() == Surface));
	}

	TEST_METHOD(OtherMouseButtons_DoNotFire)
	{
		Fix.Write(TEXT("CmButton.sltr"), TEXT("<surface role=\"UI.Surface.Button\" on-click=\"Test.Go\"/>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CmButton"));
		View->GetCommands().Register(TEXT("Test.Go"), Counter());
		const TSharedPtr<SSlaterSurface> Surface = OcFirstSurface(View);
		const FPointerEvent Right(0, FVector2f::ZeroVector, FVector2f::ZeroVector, TSet<FKey>(), EKeys::RightMouseButton, 0.f, FModifierKeysState());
		Surface->OnMouseButtonUp(FGeometry(), Right);
		ASSERT_THAT(AreEqual(Fired, 0));
	}

	TEST_METHOD(TheViewsTable_WinsOverTheProcessWideOne)
	{
		Fix.Write(TEXT("CmButton.sltr"), TEXT("<surface role=\"UI.Surface.Button\" on-click=\"Test.Go\"/>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CmButton"));
		int32 Global = 0;
		ASSERT_THAT(IsTrue(Slater::Commands().Register(TEXT("Test.Go"), [&Global](const FSlaterCommandCall&) { ++Global; })));
		const TSharedPtr<SSlaterSurface> Surface = OcFirstSurface(View);

		// Only the process-wide table has it: it runs.
		ASSERT_THAT(IsTrue(Surface->Activate()));
		ASSERT_THAT(AreEqual(Global, 1));
		View->GetCommands().Register(TEXT("Test.Go"), Counter());
		ASSERT_THAT(IsTrue(Surface->Activate()));
		ASSERT_THAT(AreEqual(Global, 1));
		ASSERT_THAT(AreEqual(Fired, 1));
	}

	TEST_METHOD(ACommandNobodyRegistered_WarnsAndFiresNothing)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("no handler is registered for the command 'Test.Nope'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Fix.Write(TEXT("CmButton.sltr"), TEXT("<surface role=\"UI.Surface.Button\" on-click=\"Test.Nope\"/>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CmButton"));
		ASSERT_THAT(IsFalse(OcFirstSurface(View)->Activate()));
	}

	TEST_METHOD(ACommandNobodyRegistered_WarnsOncePerView)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("no handler is registered for the command 'Test.Nope'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Fix.Write(TEXT("CmButton.sltr"), TEXT("<surface role=\"UI.Surface.Button\" on-click=\"Test.Nope\"/>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CmButton"));
		const TSharedPtr<SSlaterSurface> Surface = OcFirstSurface(View);
		ASSERT_THAT(IsFalse(Surface->Activate()));
		ASSERT_THAT(IsFalse(Surface->Activate()));
		ASSERT_THAT(IsFalse(Surface->Activate()));
	}

	TEST_METHOD(ADisabledSurface_DoesNotFire)
	{
		Fix.Write(TEXT("CmButton.sltr"), TEXT("<surface role=\"UI.Surface.Button\" on-click=\"Test.Go\" enabled=\"false\"/>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CmButton"));
		View->GetCommands().Register(TEXT("Test.Go"), Counter());
		const TSharedPtr<SSlaterSurface> Surface = OcFirstSurface(View);
		ASSERT_THAT(IsFalse(Surface->IsEnabled()));
		ASSERT_THAT(IsFalse(Surface->Activate()));
		Surface->OnMouseButtonUp(FGeometry(), OcLeftMouse());
		ASSERT_THAT(AreEqual(Fired, 0));
	}

	TEST_METHOD(InteractiveSurface_FollowsTheHoverPressAndLeave)
	{
		Fix.Write(TEXT("CmLive.sltr"), TEXT("<surface role=\"UI.Surface.Button\" interactive=\"true\"><text>Go</text></surface>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CmLive"));
		const TSharedPtr<SSlaterSurface> Surface = OcFirstSurface(View);
		ASSERT_THAT(IsTrue(Surface.IsValid()));
		ASSERT_THAT(IsTrue(Surface->IsInteractive()));
		ASSERT_THAT(IsTrue(Surface->GetVisibility() == EVisibility::Visible));
		FSlaterSurface Normal, Hover, Pressed;
		Stack.FindSurface(TEXT("UI.Surface.Button"), Normal);
		Stack.FindSurface(TEXT("UI.Surface.Button.Hover"), Hover);
		Stack.FindSurface(TEXT("UI.Surface.Button.Pressed"), Pressed);
		ASSERT_THAT(IsTrue(Normal.Fill != Hover.Fill && Hover.Fill != Pressed.Fill));
		ASSERT_THAT(IsTrue(Surface->GetSurface().Fill == Normal.Fill));

		const FGeometry Geometry;
		Surface->OnMouseEnter(Geometry, OcLeftMouse());
		ASSERT_THAT(IsTrue(Surface->GetSurface().Fill == Hover.Fill));
		ASSERT_THAT(IsTrue(Surface->OnMouseButtonDown(Geometry, OcLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsTrue(Surface->GetSurface().Fill == Pressed.Fill));
		ASSERT_THAT(IsTrue(Surface->OnMouseButtonUp(Geometry, OcLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsTrue(Surface->GetSurface().Fill == Hover.Fill));
		Surface->OnMouseLeave(OcLeftMouse());
		ASSERT_THAT(IsTrue(Surface->GetSurface().Fill == Normal.Fill));

		// Leaving while pressed drops the press.
		Surface->OnMouseEnter(Geometry, OcLeftMouse());
		Surface->OnMouseButtonDown(Geometry, OcLeftMouse());
		Surface->OnMouseLeave(OcLeftMouse());
		ASSERT_THAT(IsTrue(Surface->GetSurface().Fill == Normal.Fill));
	}

	TEST_METHOD(InteractiveSurface_WithACommand_StillFiresOnClick)
	{
		Fix.Write(TEXT("CmLiveGo.sltr"), TEXT("<surface role=\"UI.Surface.Button\" on-click=\"Test.Go\"><text>Go</text></surface>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CmLiveGo"));
		ASSERT_THAT(IsTrue(View->GetCommands().Register(TEXT("Test.Go"), Counter())));
		const TSharedPtr<SSlaterSurface> Surface = OcFirstSurface(View);
		ASSERT_THAT(IsTrue(Surface->IsInteractive()));
		const FGeometry Geometry;
		Surface->OnMouseEnter(Geometry, OcLeftMouse());
		Surface->OnMouseButtonDown(Geometry, OcLeftMouse());
		ASSERT_THAT(AreEqual(Fired, 0));
		Surface->OnMouseButtonUp(Geometry, OcLeftMouse());
		ASSERT_THAT(AreEqual(Fired, 1));
	}

	TEST_METHOD(ASurface_IsNotInteractiveUnlessAskedOrGivenACommand_AndDisabledNeverIs)
	{
		Fix.Write(TEXT("CmStill.sltr"), TEXT("<surface role=\"UI.Surface.Button\"/>"));
		const TSharedRef<SSlaterView> ViewCmStill = SNew(SSlaterView).LayoutId(TEXT("CmStill"));
		ASSERT_THAT(IsTrue(OcFirstSurface(ViewCmStill).IsValid()));
		ASSERT_THAT(IsFalse(OcFirstSurface(ViewCmStill)->IsInteractive()));
		Fix.Write(TEXT("CmOff.sltr"), TEXT("<surface role=\"UI.Surface.Button\" on-click=\"Test.Go\" interactive=\"false\"/>"));
		const TSharedRef<SSlaterView> ViewCmOff = SNew(SSlaterView).LayoutId(TEXT("CmOff"));
		ASSERT_THAT(IsTrue(OcFirstSurface(ViewCmOff).IsValid()));
		ASSERT_THAT(IsFalse(OcFirstSurface(ViewCmOff)->IsInteractive()));
		Fix.Write(TEXT("CmDis.sltr"), TEXT("<surface role=\"UI.Surface.Button\" state=\"disabled\" interactive=\"true\"/>"));
		const TSharedRef<SSlaterView> ViewCmDis = SNew(SSlaterView).LayoutId(TEXT("CmDis"));
		const TSharedPtr<SSlaterSurface> Disabled = OcFirstSurface(ViewCmDis);
		ASSERT_THAT(IsTrue(Disabled.IsValid()));
		ASSERT_THAT(IsFalse(Disabled->IsInteractive()));
	}

	TEST_METHOD(ASurfaceWithoutACommand_TakesNoClicks)
	{
		Fix.Write(TEXT("CmPlain.sltr"), TEXT("<surface role=\"UI.Surface.Button\"/>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CmPlain"));
		const TSharedPtr<SSlaterSurface> Surface = OcFirstSurface(View);
		ASSERT_THAT(IsTrue(Surface->GetCommand().IsNone()));
		ASSERT_THAT(IsFalse(Surface->Activate()));
		ASSERT_THAT(IsFalse(Surface->OnMouseButtonUp(FGeometry(), OcLeftMouse()).IsEventHandled()));
	}

	TEST_METHOD(ABadCommandName_FailsAtTheAttribute)
	{
		const FSlaterBuildResult Result = OcBuild(TEXT("<vbox>\n<surface role=\"UI.Surface.Button\" on-click=\"Bad Name\"/>\n</vbox>"), Stack);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(AreEqual(Result.Errors[0].Line, 2));
		ASSERT_THAT(AreEqual(Result.Errors[0].Column, 35));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("'Bad Name' is not a command name"))));
		// A binding is not a command name either: a command is named, not read from data.
		const FSlaterBuildResult Bound = OcBuild(TEXT("<surface role=\"UI.Surface.Button\" on-click=\"{Cmd}\"/>"), Stack);
		ASSERT_THAT(IsFalse(Bound.Succeeded()));
	}

	TEST_METHOD(AButtonInARepeatedItem_Fires)
	{
		FSlaterBuildResult Result = OcBuild(TEXT("<repeat source=\"{L}\" key=\"Id\" columns=\"1\"><surface role=\"UI.Surface.Button\" on-click=\"Test.Go\"><text>{N}</text></surface></repeat>"), Stack);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedRef<FSlaterModel> Model = MakeShared<FSlaterModel>();
		FSlaterError LoadError;
		ASSERT_THAT(IsTrue(Model->LoadMock(TEXT("{\"L\":[{\"Id\":\"a\",\"N\":1}]}"), TEXT("mock"), LoadError)));
		const TSharedRef<FSlaterBinder> Binder = Slater::Bind(Result, Model, ESlaterBindMode::Pushed, Stack);
		ASSERT_THAT(IsTrue(Slater::Commands().Register(TEXT("Test.Go"), Counter())));

		// The repeat takes no clicks itself; the button inside the item does.
		ASSERT_THAT(IsTrue(Result.Root->GetVisibility() == EVisibility::SelfHitTestInvisible));
		const TSharedPtr<SSlaterSurface> Surface = OcFirstSurface(Result.Root.ToSharedRef());
		ASSERT_THAT(IsTrue(Surface.IsValid()));
		ASSERT_THAT(IsTrue(Surface->Activate()));
		ASSERT_THAT(AreEqual(Fired, 1));
	}

	TEST_METHOD(TheTable_RefusesBadNamesAndDuplicates_AndListsWhatIsRegistered)
	{
		FSlaterCommandTable Table;
		FString Why;
		ASSERT_THAT(IsFalse(Table.Register(TEXT("Bad Name"), Counter(), &Why)));
		ASSERT_THAT(IsTrue(Why.Contains(TEXT("not a command name"))));
		ASSERT_THAT(IsFalse(Table.Register(TEXT("Ok"), nullptr, &Why)));
		ASSERT_THAT(IsTrue(Why.Contains(TEXT("no handler"))));
		ASSERT_THAT(IsTrue(Table.Register(TEXT("Ui.Close"), Counter())));
		ASSERT_THAT(IsTrue(Table.Register(TEXT("Ui.Back"), Counter())));
		ASSERT_THAT(IsFalse(Table.Register(TEXT("Ui.Close"), Counter(), &Why)));
		ASSERT_THAT(IsTrue(Why.Contains(TEXT("already registered"))));

		TArray<FString> Names;
		Table.GetNames(Names);
		ASSERT_THAT(IsTrue(Names.Num() == 2 && Names[0] == TEXT("Ui.Back") && Names[1] == TEXT("Ui.Close")));
		ASSERT_THAT(IsTrue(Table.Run({TEXT("Ui.Close"), nullptr})));
		ASSERT_THAT(AreEqual(Fired, 1));
		ASSERT_THAT(IsTrue(Table.Unregister(TEXT("Ui.Close"))));
		ASSERT_THAT(IsFalse(Table.Run({TEXT("Ui.Close"), nullptr})));
	}

	TEST_METHOD(CommandNames_AreWordsJoinedByDots)
	{
		ASSERT_THAT(IsTrue(Slater::IsValidCommandName(TEXT("Close"))));
		ASSERT_THAT(IsTrue(Slater::IsValidCommandName(TEXT("Inventory.Close"))));
		ASSERT_THAT(IsTrue(Slater::IsValidCommandName(TEXT("_a.b2.c_3"))));
		ASSERT_THAT(IsFalse(Slater::IsValidCommandName(TEXT(""))));
		ASSERT_THAT(IsFalse(Slater::IsValidCommandName(TEXT(".A"))));
		ASSERT_THAT(IsFalse(Slater::IsValidCommandName(TEXT("A."))));
		ASSERT_THAT(IsFalse(Slater::IsValidCommandName(TEXT("A..B"))));
		ASSERT_THAT(IsFalse(Slater::IsValidCommandName(TEXT("1A"))));
		ASSERT_THAT(IsFalse(Slater::IsValidCommandName(TEXT("A.1"))));
		ASSERT_THAT(IsFalse(Slater::IsValidCommandName(TEXT("A B"))));
		ASSERT_THAT(IsFalse(Slater::IsValidCommandName(TEXT("A-B"))));
	}
};

/** `visible` and `enabled` on any element, literal or bound. */
TEST_CLASS(SlaterVisibleEnabledTests, "BRS.SlaterUI.VisibleEnabled")
{
	FSlaterStyleStack Stack;
	TSharedPtr<FSlaterModel> Model;
	FSlaterBuildResult Result;
	TSharedPtr<FSlaterBinder> Binder;

	BEFORE_EACH()
	{
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
	}

	AFTER_EACH()
	{
		Binder.Reset();
		Result = FSlaterBuildResult();
		Model.Reset();
	}

	bool Bound(const FString& Layout, const TCHAR* Json, ESlaterBindMode Mode = ESlaterBindMode::Pushed)
	{
		Model = MakeShared<FSlaterModel>();
		FSlaterError Error;
		if (!Model->LoadMock(Json, TEXT("mock"), Error))
		{
			return false;
		}
		Result = OcBuild(Layout, Stack);
		if (!Result.Succeeded())
		{
			return false;
		}
		Binder = Slater::Bind(Result, Model.ToSharedRef(), Mode, Stack);
		return true;
	}

	TEST_METHOD(ALiteralFalse_CollapsesTheOutermostWidget_SizeBoxIncluded)
	{
		const FSlaterBuildResult Built = OcBuild(TEXT("<vbox><text id=\"T\" width=\"100\" visible=\"false\">x</text><text id=\"U\" enabled=\"false\">y</text></vbox>"), Stack);
		ASSERT_THAT(IsTrue(Built.Succeeded()));
		const TSharedPtr<SWidget> T = Built.ById[TEXT("T")].Pin();
		ASSERT_THAT(IsTrue(T->GetType() == FName(TEXT("SBox"))));
		ASSERT_THAT(IsTrue(T->GetVisibility() == EVisibility::Collapsed));
		const TSharedPtr<SWidget> U = Built.ById[TEXT("U")].Pin();
		ASSERT_THAT(IsFalse(U->IsEnabled()));
		ASSERT_THAT(IsTrue(U->GetVisibility() != EVisibility::Collapsed));
	}

	TEST_METHOD(ABoundVisible_TogglesBetweenCollapsedAndTheWidgetsOwnVisibility)
	{
		ASSERT_THAT(IsTrue(Bound(TEXT("<vbox><text id=\"T\" width=\"50\" visible=\"{Show}\">x</text></vbox>"), TEXT("{\"Show\":\"true\"}"))));
		const TSharedPtr<SWidget> T = Result.ById[TEXT("T")].Pin();
		const EVisibility Shown = T->GetVisibility();
		ASSERT_THAT(IsTrue(Shown != EVisibility::Collapsed));
		ASSERT_THAT(IsTrue(Model->SetValue(TEXT("Show"), TEXT("false"))));
		ASSERT_THAT(IsTrue(T->GetVisibility() == EVisibility::Collapsed));
		ASSERT_THAT(IsTrue(Model->SetValue(TEXT("Show"), TEXT("true"))));
		ASSERT_THAT(IsTrue(T->GetVisibility() == Shown));
	}

	TEST_METHOD(ABoundEnabled_FollowsTheData)
	{
		ASSERT_THAT(IsTrue(Bound(TEXT("<surface id=\"S\" role=\"UI.Surface.Button\" enabled=\"{On}\"/>"), TEXT("{\"On\":\"false\"}"))));
		const TSharedPtr<SWidget> S = Result.ById[TEXT("S")].Pin();
		ASSERT_THAT(IsFalse(S->IsEnabled()));
		ASSERT_THAT(IsTrue(Model->SetValue(TEXT("On"), TEXT("true"))));
		ASSERT_THAT(IsTrue(S->IsEnabled()));
		// A value that is not a bool leaves it as it was.
		ASSERT_THAT(IsTrue(Model->SetValue(TEXT("On"), TEXT("maybe"))));
		ASSERT_THAT(IsTrue(S->IsEnabled()));
	}

	TEST_METHOD(PolledVisibleAndEnabled_AreReadEachFrame)
	{
		ASSERT_THAT(IsTrue(Bound(TEXT("<vbox><surface id=\"S\" role=\"UI.Surface.Button\" visible=\"{Show}\" enabled=\"{On}\"/></vbox>"), TEXT("{\"Show\":\"true\",\"On\":\"true\"}"), ESlaterBindMode::Polled)));
		const TSharedPtr<SWidget> S = Result.ById[TEXT("S")].Pin();
		Result.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(S->IsEnabled()));
		ASSERT_THAT(IsTrue(Model->SetValue(TEXT("On"), TEXT("false"))));
		Result.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsFalse(S->IsEnabled()));
		// A collapsed widget is not updated by the prepass any more, so visibility is the last thing to switch.
		ASSERT_THAT(IsTrue(Model->SetValue(TEXT("Show"), TEXT("false"))));
		Result.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(S->GetVisibility() == EVisibility::Collapsed));
	}

	TEST_METHOD(ANonBoolLiteral_IsRefused)
	{
		const FSlaterBuildResult Built = OcBuild(TEXT("<text visible=\"yes\">x</text>"), Stack);
		ASSERT_THAT(IsFalse(Built.Succeeded()));
		ASSERT_THAT(IsTrue(Built.Errors[0].Message.Contains(TEXT("is not a bool"))));
	}
};

/** The mock file next to a layout is its context in the preview. */
TEST_CLASS(SlaterMockContextTests, "BRS.SlaterUI.MockContext")
{
	TEST_METHOD(TheShippedListProbe_ShowsItsMockData)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ListProbe")).UseMockContext(true);
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->GetActiveContext().IsValid()));
		ASSERT_THAT(IsTrue(View->GetBinder() != nullptr));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View).Contains(TEXT("Backpack"))));
		ASSERT_THAT(IsTrue(SlaterBindKit::Repeat(View, 0)->NumItems() > 0));
		ASSERT_THAT(IsTrue(SlaterBindKit::Repeat(View, 1)->NumItems() > 0));
	}

	TEST_METHOD(WithoutTheMockFlag_TheLayoutShowsAsWritten)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ListProbe"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsFalse(View->GetActiveContext().IsValid()));
		ASSERT_THAT(AreEqual(SlaterBindKit::Repeat(View, 0)->NumItems(), 0));
	}

	TEST_METHOD(ALayoutWithNoMockFile_HasNoContext_AndIsNotAnError)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Panel")).UseMockContext(true);
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsFalse(View->GetActiveContext().IsValid()));
	}

	TEST_METHOD(AnExplicitContext_WinsOverTheMock)
	{
		FSlaterBindFixture Fix;
		Fix.Init();
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Title = TEXT("From the view model");
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ListProbe")).UseMockContext(true);
		// ListProbe binds {Cells} too, which the view model lacks: the context is refused with the reason.
		ASSERT_THAT(IsFalse(View->SetContext(Slater::MakeObjectScope(Root))));
		ASSERT_THAT(IsTrue(View->HasReloadErrors()));
		ASSERT_THAT(IsTrue(View->GetReloadErrors()[0].Message.Contains(TEXT("is not a field of SlaterTestRootVM"))));
		Fix.Shutdown();
	}
};

#endif
