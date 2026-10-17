// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterBindTestKit.h"
#include "SlaterCombo.h"
#include "SlaterCommands.h"
#include "SlaterEvents.h"
#include "SlaterImages.h"
#include "SlaterList.h"
#include "SlaterListTestKit.h"
#include "SlaterObjectScope.h"
#include "SlaterScope.h"
#include "SlaterStyle.h"
#include "SlaterTestViewModels.h"
#include "SlaterView.h"

namespace
{
	/** Builds a layout against Base; the combo, when it built. */
	TSharedPtr<SSlaterCombo> CbBuild(const TCHAR* Layout, FSlaterBuildResult& Result)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		if (!Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result) || !Result.Root.IsValid())
		{
			return nullptr;
		}
		return StaticCastSharedPtr<SSlaterCombo>(SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SSlaterCombo")));
	}

	FString CbError(const TCHAR* Layout)
	{
		FSlaterBuildResult Result;
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result.Errors.Num() > 0 ? Result.Errors[0].Message : FString();
	}

	const TCHAR* const CbQuality = TEXT("<vbox><combo id=\"Q\" selected=\"{Nickname}\" on-change=\"{Nickname}\" hint=\"Pick one\"><option value=\"low\">Low</option><option value=\"high\">High</option><option value=\"ultra\">Ultra</option></combo><text>{Nickname}</text></vbox>");
}

/** The `combo` element (T-525): build, options, bound selection, write-back, empty list, refused misuse. */
TEST_CLASS(SlaterComboTests, "BRS.SlaterUI.Combo")
{
	FSlaterBindFixture Fix;

	BEFORE_EACH()
	{
		Fix.Init();
		Slater::ListStats().Reset();
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TEST_METHOD(OptionChildren_Build_AsTheEnginesComboBox_WithTheirLabels)
	{
		FSlaterBuildResult Result;
		const TSharedPtr<SSlaterCombo> Combo = CbBuild(TEXT("<vbox><combo selected=\"high\"><option value=\"low\">Low</option><option value=\"high\">High quality</option></combo></vbox>"), Result);
		ASSERT_THAT(IsTrue(Combo.IsValid()));
		ASSERT_THAT(IsTrue(Combo->GetComboBox().IsValid()));
		ASSERT_THAT(AreEqual(Combo->GetOptions().Num(), 2));
		ASSERT_THAT(AreEqual(Combo->GetOptions()[1]->Value, FString(TEXT("high"))));
		ASSERT_THAT(AreEqual(Combo->GetOptions()[1]->Label, FString(TEXT("High quality"))));
		ASSERT_THAT(AreEqual(Combo->GetSelectedValue(), FString(TEXT("high"))));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("High quality"))));
		// The option elements are data for the combo: only the combo's own widgets are in the tree.
		ASSERT_THAT(IsFalse(Combo->IsMenuOpen()));
	}

	TEST_METHOD(TheOptionsAttribute_ListsChoices_WithOptionalLabels)
	{
		FSlaterBuildResult Result;
		const TSharedPtr<SSlaterCombo> Combo = CbBuild(TEXT("<vbox><combo options=\"a|Alpha, b , c|Gamma\" selected=\"b\"/></vbox>"), Result);
		ASSERT_THAT(IsTrue(Combo.IsValid()));
		ASSERT_THAT(AreEqual(Combo->GetOptions().Num(), 3));
		ASSERT_THAT(AreEqual(Combo->GetOptions()[0]->Label, FString(TEXT("Alpha"))));
		ASSERT_THAT(AreEqual(Combo->GetOptions()[1]->Value, FString(TEXT("b"))));
		ASSERT_THAT(AreEqual(Combo->GetOptions()[1]->Label, FString(TEXT("b"))));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("b"))));
	}

	TEST_METHOD(TheLook_ComesFromTheStyleTokens)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		const TSharedPtr<SSlaterCombo> Combo = CbBuild(TEXT("<vbox><combo/></vbox>"), Result);
		ASSERT_THAT(IsTrue(Combo.IsValid()));
		const TSharedPtr<FSlaterComboLook> Look = Slater::FindComboLook(*Combo);
		ASSERT_THAT(IsTrue(Look.IsValid() && Look->Style.IsValid()));
		FLinearColor Control, Menu;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("UI.Color.Control"), Control) && Stack.FindColor(TEXT("UI.Color.Menu"), Menu)));
		ASSERT_THAT(IsTrue(Look->Style->ComboButtonStyle.ButtonStyle.Normal.TintColor.GetSpecifiedColor() == Control));
		ASSERT_THAT(IsTrue(Look->Style->ComboButtonStyle.MenuBorderBrush.TintColor.GetSpecifiedColor() == Menu));
	}

	TEST_METHOD(TheMenu_IsToldToFlipAtTheWindowEdge)
	{
		// T-439: the engine flips a menu at the edge of its window only when asked. Whether that keeps it inside a view smaller than its window is the by-hand check in DevNotes.
		FSlaterBuildResult Result;
		const TSharedPtr<SSlaterCombo> Combo = CbBuild(TEXT("<vbox><combo/></vbox>"), Result);
		ASSERT_THAT(IsTrue(Combo.IsValid()));
		ASSERT_THAT(IsTrue(Combo->FitsInWindow()));
	}

	TEST_METHOD(ABoundSelection_ShowsTheDataValue_AndFollowsIt)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Settings->Nickname = TEXT("high");
		Fix.Write(TEXT("CbBound.sltr"), CbQuality);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CbBound"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TSharedPtr<SSlaterCombo> Combo = StaticCastSharedPtr<SSlaterCombo>(View->FindById(TEXT("Q")));
		ASSERT_THAT(IsTrue(Combo.IsValid()));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("High"))));

		Settings->SetNickname(TEXT("ultra"));
		ASSERT_THAT(AreEqual(Combo->GetSelectedValue(), FString(TEXT("ultra"))));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("Ultra"))));
		// Moving the data is not a pick: nothing was written back.
		ASSERT_THAT(AreEqual(Settings->NicknameSets, 1));
		ASSERT_THAT(AreEqual(Slater::ListStats().Writes, 0));
	}

	TEST_METHOD(APick_WritesTheValueThroughTheSetter_AndEveryBindingFollows)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Settings->Nickname = TEXT("high");
		Fix.Write(TEXT("CbWrite.sltr"), CbQuality);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CbWrite"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TSharedPtr<SSlaterCombo> Combo = StaticCastSharedPtr<SSlaterCombo>(View->FindById(TEXT("Q")));

		Combo->PickOption(TEXT("low"));
		ASSERT_THAT(AreEqual(Settings->NicknameSets, 1));
		ASSERT_THAT(AreEqual(Settings->Nickname, FString(TEXT("low"))));
		ASSERT_THAT(AreEqual(Slater::ListStats().Writes, 1));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("Low"))));
		ASSERT_THAT(AreEqual(SlaterBindKit::Texts(View)[1], FString(TEXT("low"))));
	}

	TEST_METHOD(APick_AlsoRunsACommand_WithTheValue)
	{
		Fix.Write(TEXT("CbCommand.sltr"), TEXT("<vbox><combo id=\"Q\" on-change=\"Gfx.SetQuality:Main\"><option value=\"low\">Low</option><option value=\"high\">High</option></combo></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CbCommand"));
		FSlaterCommandCall Got;
		int32 Calls = 0;
		ASSERT_THAT(IsTrue(View->GetCommands().Register(TEXT("Gfx.SetQuality"), [&](const FSlaterCommandCall& Call) { Got = Call; ++Calls; })));
		const TSharedPtr<SSlaterCombo> Combo = StaticCastSharedPtr<SSlaterCombo>(View->FindById(TEXT("Q")));
		Combo->PickOption(TEXT("high"));
		ASSERT_THAT(AreEqual(Calls, 1));
		ASSERT_THAT(AreEqual(Got.Value, FString(TEXT("high"))));
		ASSERT_THAT(AreEqual(Got.Argument, FString(TEXT("Main"))));
	}

	TEST_METHOD(ASelectionFromCode_FiresNoEvent)
	{
		Fix.Write(TEXT("CbEcho.sltr"), TEXT("<vbox><combo id=\"Q\" on-change=\"Gfx.SetQuality\"><option value=\"low\">Low</option><option value=\"high\">High</option></combo></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CbEcho"));
		int32 Calls = 0;
		ASSERT_THAT(IsTrue(View->GetCommands().Register(TEXT("Gfx.SetQuality"), [&](const FSlaterCommandCall&) { ++Calls; })));
		const TSharedPtr<SSlaterCombo> Combo = StaticCastSharedPtr<SSlaterCombo>(View->FindById(TEXT("Q")));
		Combo->SetSelectedValue(TEXT("high"));
		ASSERT_THAT(AreEqual(Combo->GetSelectedValue(), FString(TEXT("high"))));
		ASSERT_THAT(AreEqual(Calls, 0));
		// Picking the value it already shows is the same pick again; only a different one is a change the layout hears twice.
		Combo->PickOption(TEXT("low"));
		ASSERT_THAT(AreEqual(Calls, 1));
	}

	TEST_METHOD(AnEmptyList_ShowsTheHint_AndAPickFindsNothing)
	{
		FSlaterBuildResult Result;
		const TSharedPtr<SSlaterCombo> Combo = CbBuild(TEXT("<vbox><combo hint=\"Pick one\" on-change=\"Gfx.Never\"/></vbox>"), Result);
		ASSERT_THAT(IsTrue(Combo.IsValid()));
		ASSERT_THAT(AreEqual(Combo->GetOptions().Num(), 0));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("Pick one"))));
		ASSERT_THAT(AreEqual(Combo->GetSelectedValue(), FString()));
		Combo->PickOption(TEXT("anything"));
		ASSERT_THAT(AreEqual(Combo->GetSelectedValue(), FString()));
		ASSERT_THAT(IsFalse(Combo->IsMenuOpen()));
	}

	TEST_METHOD(ABoundValueNoOptionHas_ShowsTheHint_UntilTheOptionsGainIt)
	{
		FSlaterBuildResult Result;
		const TSharedPtr<SSlaterCombo> Combo = CbBuild(TEXT("<vbox><combo hint=\"Pick one\" options=\"a, b\"/></vbox>"), Result);
		ASSERT_THAT(IsTrue(Combo.IsValid()));
		Combo->SetSelectedValue(TEXT("c"));
		ASSERT_THAT(AreEqual(Combo->GetSelectedValue(), FString()));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("Pick one"))));
		Combo->SetOptions({{TEXT("a"), TEXT("A")}, {TEXT("c"), TEXT("Cee")}});
		ASSERT_THAT(AreEqual(Combo->GetSelectedValue(), FString(TEXT("c"))));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("Cee"))));
	}

	TEST_METHOD(BoundOptions_ReplaceTheChoices_AndKeepTheSelection)
	{
		Fix.Write(TEXT("CbOptions.sltr"), TEXT("<vbox><combo id=\"Q\" options=\"{Choices}\" selected=\"{Pick}\" on-change=\"{Pick}\"/></vbox>"));
		FString Choices = TEXT("a|Alpha, b|Beta");
		FString Pick = TEXT("b");
		const TSharedRef<FSlaterGetterScope> Scope = MakeShared<FSlaterGetterScope>();
		Scope->AddValue(TEXT("Choices"), [&Choices]() { return Choices; });
		Scope->AddValue(TEXT("Pick"), [&Pick]() { return Pick; });
		Scope->AddSetter(TEXT("Pick"), [&Pick](const FString& Text) { Pick = Text; return true; });
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("CbOptions"));
		ASSERT_THAT(IsTrue(View->SetContext(Scope)));
		const TSharedPtr<SSlaterCombo> Combo = StaticCastSharedPtr<SSlaterCombo>(View->FindById(TEXT("Q")));
		ASSERT_THAT(IsTrue(Combo.IsValid()));
		ASSERT_THAT(AreEqual(Combo->GetOptions().Num(), 2));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("Beta"))));

		// The list changes under the selection: the value keeps its choice, with the new label.
		Choices = TEXT("b|Bee, c|Cee, d|Dee");
		Scope->Notify(TEXT("Choices"));
		ASSERT_THAT(AreEqual(Combo->GetOptions().Num(), 3));
		ASSERT_THAT(AreEqual(Combo->GetShownText().ToString(), FString(TEXT("Bee"))));

		// The list loses the chosen value: the hint shows, and the data still holds what it held.
		Choices = TEXT("c, d");
		Scope->Notify(TEXT("Choices"));
		ASSERT_THAT(AreEqual(Combo->GetSelectedValue(), FString()));
		ASSERT_THAT(AreEqual(Pick, FString(TEXT("b"))));

		Combo->PickOption(TEXT("d"));
		ASSERT_THAT(AreEqual(Pick, FString(TEXT("d"))));
	}

	TEST_METHOD(ABadChild_BothSources_ADuplicateValue_AndAnUnknownSelected_AreRefused)
	{
		ASSERT_THAT(IsTrue(CbError(TEXT("<vbox><combo><text>x</text></combo></vbox>")).Contains(TEXT("children must be <option> elements (found <text>)"))));
		ASSERT_THAT(IsTrue(CbError(TEXT("<vbox><combo options=\"a\"><option value=\"a\">A</option></combo></vbox>")).Contains(TEXT("both an options attribute and <option> children"))));
		ASSERT_THAT(IsTrue(CbError(TEXT("<vbox><combo><option value=\"a\">A</option><option value=\"a\">B</option></combo></vbox>")).Contains(TEXT("two <option>s with the value 'a'"))));
		ASSERT_THAT(IsTrue(CbError(TEXT("<vbox><combo options=\"a, a|B\"/></vbox>")).Contains(TEXT("two entries have the value 'a'"))));
		ASSERT_THAT(IsTrue(CbError(TEXT("<vbox><combo selected=\"z\"><option value=\"a\">A</option></combo></vbox>")).Contains(TEXT("selected 'z' is not one of its options (a)"))));
		ASSERT_THAT(IsTrue(CbError(TEXT("<vbox><combo><option>A</option></combo></vbox>")).Contains(TEXT("value"))));
	}

	TEST_METHOD(AnIconOnlyCombo_ShowsTheSelectedIcon_AndPicksByValue)
	{
		Slater::SetImageSource(Slater::MakePluginIconSource());
		FSlaterBuildResult Result;
		const TSharedPtr<SSlaterCombo> Combo = CbBuild(TEXT("<vbox><combo icon-only=\"true\" selected=\"b\"><option value=\"a\" icon=\"Home\"/><option value=\"b\" icon=\"Mail\"/></combo></vbox>"), Result);
		ASSERT_THAT(IsTrue(Combo.IsValid()));
		ASSERT_THAT(IsTrue(Combo->IsIconOnly()));
		ASSERT_THAT(AreEqual(Combo->GetOptions()[1]->Icon, FString(TEXT("Mail"))));
		ASSERT_THAT(IsTrue(Combo->GetShownBrush() == Slater::FindImage(TEXT("Mail"))));
		ASSERT_THAT(IsTrue(Combo->GetShownBrush() != nullptr));
		Combo->PickOption(TEXT("a"));
		ASSERT_THAT(AreEqual(Combo->GetSelectedValue(), FString(TEXT("a"))));
		ASSERT_THAT(IsTrue(Combo->GetShownBrush() == Slater::FindImage(TEXT("Home"))));
		Slater::SetImageSource(nullptr);
	}

	TEST_METHOD(AnIconOnlyCombo_WithAMissingOrUnknownIcon_OrAnIconOnATextCombo_IsRefused)
	{
		Slater::SetImageSource(Slater::MakePluginIconSource());
		ASSERT_THAT(IsTrue(CbError(TEXT("<vbox><combo icon-only=\"true\"><option value=\"a\" icon=\"Home\"/><option value=\"b\">B</option></combo></vbox>")).Contains(TEXT("option 'b' has no icon"))));
		ASSERT_THAT(IsTrue(CbError(TEXT("<vbox><combo icon-only=\"true\"><option value=\"a\" icon=\"NoSuchIcon\"/></combo></vbox>")).Contains(TEXT("icon 'NoSuchIcon' is not a known image"))));
		ASSERT_THAT(IsTrue(CbError(TEXT("<vbox><combo><option value=\"a\" icon=\"Home\"/></combo></vbox>")).Contains(TEXT("not icon-only"))));
		Slater::SetImageSource(nullptr);
	}
};

#endif
