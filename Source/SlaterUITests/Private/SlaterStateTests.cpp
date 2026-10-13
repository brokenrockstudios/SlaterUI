// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterBindTestKit.h"
#include "SlaterCommands.h"
#include "SlaterState.h"
#include "SlaterSurface.h"
#include "SlaterView.h"

namespace
{
	FPointerEvent StLeftMouse()
	{
		return FPointerEvent(0, FVector2f::ZeroVector, FVector2f::ZeroVector, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	}

	FSlaterBuildResult StBuild(const FString& Text, const FSlaterStyleStack& Stack)
	{
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}

	bool StSame(const FString& A, const TCHAR* B)
	{
		return A.Equals(B, ESearchCase::CaseSensitive);
	}

	const TCHAR* const StTabs = TEXT(
		"<vbox>\n"
		"\t<state name=\"tab\" default=\"gear\"/>\n"
		"\t<hbox>\n"
		"\t\t<surface id=\"TabGear\" role=\"UI.Surface.Button\" set-state=\"tab=gear\"><text>Gear</text></surface>\n"
		"\t\t<surface id=\"TabMap\" role=\"UI.Surface.Button\" set-state=\"tab=map\"><text>Map</text></surface>\n"
		"\t</hbox>\n"
		"\t<text id=\"PageGear\" visible-when=\"tab=gear\">Gear page</text>\n"
		"\t<text id=\"PageMap\" visible-when=\"tab=map\">Map page</text>\n"
		"</vbox>");

	/** Records what the stock screen commands asked for. */
	class FStFakeHost : public ISlaterScreenHost
	{
	public:
		TArray<FString> Log;
		virtual void PushScreen(const FString& LayoutId, SSlaterView&) override { Log.Add(TEXT("push ") + LayoutId); }
		virtual void PopScreen(SSlaterView&) override { Log.Add(TEXT("pop")); }
		virtual void CloseScreen(SSlaterView&) override { Log.Add(TEXT("close")); }
	};
}

/** `<state>`, `set-state` and `visible-when` (T-353): tabs and toggles from markup alone. */
TEST_CLASS(SlaterStateTests, "BRS.SlaterUI.State")
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

	bool Shown(const TSharedRef<SSlaterView>& View, const TCHAR* Id) const
	{
		const TSharedPtr<SWidget> Widget = View->FindById(Id);
		return Widget.IsValid() && Widget->GetVisibility() != EVisibility::Collapsed;
	}

	bool Click(const TSharedRef<SSlaterView>& View, const TCHAR* Id) const
	{
		const TSharedPtr<SSlaterSurface> Surface = StaticCastSharedPtr<SSlaterSurface>(View->FindById(Id));
		return Surface.IsValid() && Surface->Activate();
	}

	TEST_METHOD(DeclaredStates_StartAtTheirDefaults_AndVisibleWhenFollowsThem)
	{
		Fix.Write(TEXT("StTabs.sltr"), StTabs);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("StTabs"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		FString Tab;
		ASSERT_THAT(IsTrue(View->GetState(TEXT("tab"), Tab)));
		ASSERT_THAT(IsTrue(StSame(Tab, TEXT("gear"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageGear"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("PageMap"))));
		// The `<state>` is not a widget: the tree is the vbox and what is in it.
		ASSERT_THAT(AreEqual(View->GetResult().States.Num(), 1));
		ASSERT_THAT(AreEqual(View->GetResult().StateConditions.Num(), 2));
	}

	TEST_METHOD(AClickOnASetStateSurface_SwitchesThePage)
	{
		Fix.Write(TEXT("StTabs.sltr"), StTabs);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("StTabs"));
		ASSERT_THAT(IsTrue(Click(View, TEXT("TabMap"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("PageGear"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageMap"))));
		FString Tab;
		View->GetState(TEXT("tab"), Tab);
		ASSERT_THAT(IsTrue(StSame(Tab, TEXT("map"))));
		ASSERT_THAT(IsTrue(Click(View, TEXT("TabGear"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageGear"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("PageMap"))));
	}

	TEST_METHOD(ARealMouseClick_OnATabSurface_SwitchesThePage)
	{
		Fix.Write(TEXT("StTabs.sltr"), StTabs);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("StTabs"));
		const TSharedPtr<SSlaterSurface> Tab = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("TabMap")));
		ASSERT_THAT(IsTrue(Tab.IsValid()));
		// A surface with only a state change takes clicks and follows the pointer, like one with a command.
		ASSERT_THAT(IsTrue(Tab->HasAction()));
		ASSERT_THAT(IsTrue(Tab->IsInteractive()));
		ASSERT_THAT(IsTrue(Tab->GetVisibility() == EVisibility::Visible));
		const FGeometry Geometry;
		ASSERT_THAT(IsTrue(Tab->OnMouseButtonDown(Geometry, StLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("PageMap"))));
		ASSERT_THAT(IsTrue(Tab->OnMouseButtonUp(Geometry, StLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageMap"))));
	}

	TEST_METHOD(ADisabledTab_DoesNotSwitch)
	{
		Fix.Write(TEXT("StOff.sltr"), TEXT("<vbox><state name=\"t\" default=\"a\"/><surface id=\"B\" role=\"UI.Surface.Button\" set-state=\"t=b\" enabled=\"false\"/><text id=\"P\" visible-when=\"t=b\">x</text></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("StOff"));
		ASSERT_THAT(IsFalse(Click(View, TEXT("B"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("P"))));
	}

	TEST_METHOD(AValueDiffersByCase_StringEqualityOnly)
	{
		Fix.Write(TEXT("StCase.sltr"), TEXT("<vbox><state name=\"t\" default=\"Gear\"/><text id=\"Lower\" visible-when=\"t=gear\">a</text><text id=\"Exact\" visible-when=\"t=Gear\">b</text></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("StCase"));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("Lower"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("Exact"))));
	}

	TEST_METHOD(SetState_OnTheView_ShowsAndCollapses_AndRefusesAnUndeclaredName)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("does not declare a state 'nope'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Fix.Write(TEXT("StTabs.sltr"), StTabs);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("StTabs"));
		ASSERT_THAT(IsTrue(View->SetState(TEXT("tab"), TEXT("map"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageMap"))));
		// A value no `visible-when` names hides every page; the state still holds it.
		ASSERT_THAT(IsTrue(View->SetState(TEXT("tab"), TEXT("other"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("PageMap"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("PageGear"))));
		ASSERT_THAT(IsFalse(View->SetState(TEXT("nope"), TEXT("x"))));
	}

	TEST_METHOD(AStateValue_SurvivesAReload_WhileTheNameIsStillDeclared)
	{
		Fix.Write(TEXT("StTabs.sltr"), StTabs);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("StTabs"));
		View->SetState(TEXT("tab"), TEXT("map"));

		// The same file saved again: the tab the player chose stays.
		ASSERT_THAT(IsTrue(Slater::NotifyFilesChanged({Fix.Dir / TEXT("Layouts/StTabs.sltr")})));
		FString Tab;
		ASSERT_THAT(IsTrue(View->GetState(TEXT("tab"), Tab)));
		ASSERT_THAT(IsTrue(StSame(Tab, TEXT("map"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageMap"))));

		// A restyle keeps it too.
		ASSERT_THAT(IsTrue(View->SetStyle(TEXT("Dusk"))));
		View->GetState(TEXT("tab"), Tab);
		ASSERT_THAT(IsTrue(StSame(Tab, TEXT("map"))));

		// The layout drops `tab` and adds `other`: the old value is gone and the new name starts at its default.
		Fix.Write(TEXT("StTabs.sltr"), TEXT("<vbox><state name=\"other\" default=\"o1\"/><text id=\"P\" visible-when=\"other=o1\">p</text></vbox>"));
		Slater::NotifyFilesChanged({Fix.Dir / TEXT("Layouts/StTabs.sltr")});
		ASSERT_THAT(IsFalse(View->GetState(TEXT("tab"), Tab)));
		ASSERT_THAT(IsTrue(View->GetState(TEXT("other"), Tab)));
		ASSERT_THAT(IsTrue(StSame(Tab, TEXT("o1"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("P"))));
	}

	TEST_METHOD(ABrokenSave_KeepsTheStateAndThePageOnScreen)
	{
		Fix.Write(TEXT("StTabs.sltr"), StTabs);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("StTabs"));
		View->SetState(TEXT("tab"), TEXT("map"));
		Fix.Write(TEXT("StTabs.sltr"), TEXT("<vbox><text visible-when=\"nope=1\">x</text></vbox>"));
		Slater::NotifyFilesChanged({Fix.Dir / TEXT("Layouts/StTabs.sltr")});
		ASSERT_THAT(IsTrue(View->HasReloadErrors()));
		ASSERT_THAT(IsTrue(View->GetReloadErrors()[0].Message.Contains(TEXT("'nope' is not a declared state"))));
		FString Tab;
		ASSERT_THAT(IsTrue(View->GetState(TEXT("tab"), Tab)));
		ASSERT_THAT(IsTrue(StSame(Tab, TEXT("map"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageMap"))));
	}

	TEST_METHOD(ALayoutBuiltWithoutAView_ShowsTheDefaultPage)
	{
		const FSlaterBuildResult Result = StBuild(StTabs, Stack);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.ById[TEXT("PageGear")].Pin()->GetVisibility() != EVisibility::Collapsed));
		ASSERT_THAT(IsTrue(Result.ById[TEXT("PageMap")].Pin()->GetVisibility() == EVisibility::Collapsed));
	}

	TEST_METHOD(TheShippedTabsLayout_IsATabbedPanelFromMarkupAlone)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Tabs"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageGear"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("PageMap"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("DetailsLine"))));

		ASSERT_THAT(IsTrue(Click(View, TEXT("TabMap"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("PageGear"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageMap"))));

		// The toggle button flips its own state with a stock command, no registration.
		ASSERT_THAT(IsTrue(Click(View, TEXT("Details"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("DetailsLine"))));
		ASSERT_THAT(IsTrue(Click(View, TEXT("Details"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("DetailsLine"))));
		// The pages are independent of the toggle.
		ASSERT_THAT(IsTrue(Shown(View, TEXT("PageMap"))));
	}

	TEST_METHOD(ASetStateOutsideAView_WarnsAndChangesNothing)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("needs the surface to be inside an SSlaterView"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		const FSlaterBuildResult Result = StBuild(TEXT("<surface id=\"S\" role=\"UI.Surface.Button\" set-state=\"t=b\"/>"), Stack);
		// No `<state>` declares `t` here, so the layout itself is refused; a surface made by hand shows the runtime path.
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		const TSharedRef<SSlaterSurface> Loose = SNew(SSlaterSurface);
		Loose->SetStateChange(TEXT("t"), TEXT("b"));
		ASSERT_THAT(IsFalse(Loose->Activate()));
	}

	TEST_METHOD(BuildErrors_NameTheStateAndTheFix)
	{
		// An undeclared name in each of the three places that can use one.
		FSlaterBuildResult Visible = StBuild(TEXT("<vbox>\n<text visible-when=\"tab=a\">x</text>\n</vbox>"), Stack);
		ASSERT_THAT(IsFalse(Visible.Succeeded()));
		ASSERT_THAT(AreEqual(Visible.Errors.Num(), 1));
		ASSERT_THAT(AreEqual(Visible.Errors[0].Line, 2));
		ASSERT_THAT(IsTrue(Visible.Errors[0].Message.Contains(TEXT("'tab' is not a declared state"))));
		ASSERT_THAT(IsTrue(Visible.Errors[0].Message.Contains(TEXT("the layout declares none: add <state"))));

		FSlaterBuildResult Set = StBuild(TEXT("<vbox><state name=\"tab\" default=\"a\"/><surface role=\"UI.Surface.Button\" set-state=\"tap=b\"/></vbox>"), Stack);
		ASSERT_THAT(IsFalse(Set.Succeeded()));
		ASSERT_THAT(IsTrue(Set.Errors[0].Message.Contains(TEXT("'tap' is not a declared state; declared: tab"))));

		FSlaterBuildResult Toggle = StBuild(TEXT("<vbox><state name=\"tab\" default=\"a\"/><surface role=\"UI.Surface.Button\" on-click=\"ui.toggle:menu\"/></vbox>"), Stack);
		ASSERT_THAT(IsFalse(Toggle.Succeeded()));
		ASSERT_THAT(IsTrue(Toggle.Errors[0].Message.Contains(TEXT("'menu' is not a declared state"))));
	}

	TEST_METHOD(BadDeclarations_AreRefused)
	{
		FSlaterBuildResult Duplicate = StBuild(TEXT("<vbox>\n<state name=\"tab\" default=\"a\"/>\n<state name=\"tab\" default=\"b\"/>\n</vbox>"), Stack);
		ASSERT_THAT(IsFalse(Duplicate.Succeeded()));
		ASSERT_THAT(IsTrue(Duplicate.Errors[0].Message.Contains(TEXT("duplicate state 'tab' (first declared at 2,"))));
		ASSERT_THAT(AreEqual(Duplicate.Errors[0].Line, 3));

		FSlaterBuildResult NoDefault = StBuild(TEXT("<vbox><state name=\"tab\"/></vbox>"), Stack);
		ASSERT_THAT(IsTrue(NoDefault.Errors[0].Message.Contains(TEXT("<state> needs the attribute 'default'"))));
		FSlaterBuildResult NoName = StBuild(TEXT("<vbox><state default=\"a\"/></vbox>"), Stack);
		ASSERT_THAT(IsTrue(NoName.Errors[0].Message.Contains(TEXT("<state> needs the attribute 'name'"))));
		FSlaterBuildResult BadName = StBuild(TEXT("<vbox><state name=\"1tab\" default=\"a\"/></vbox>"), Stack);
		ASSERT_THAT(IsTrue(BadName.Errors[0].Message.Contains(TEXT("'1tab' is not a state name"))));
		FSlaterBuildResult BadDefault = StBuild(TEXT("<vbox><state name=\"tab\" default=\"a b\"/></vbox>"), Stack);
		ASSERT_THAT(IsTrue(BadDefault.Errors[0].Message.Contains(TEXT("'a b' is not a state value"))));
		FSlaterBuildResult Extra = StBuild(TEXT("<vbox><state name=\"tab\" default=\"a\" color=\"red\"/></vbox>"), Stack);
		ASSERT_THAT(IsTrue(Extra.Errors[0].Message.Contains(TEXT("unknown attribute 'color' on <state>; allowed: name, default"))));
		FSlaterBuildResult Content = StBuild(TEXT("<vbox><state name=\"tab\" default=\"a\"><text>x</text></state></vbox>"), Stack);
		ASSERT_THAT(IsTrue(Content.Errors[0].Message.Contains(TEXT("<state> cannot have child elements"))));
	}

	TEST_METHOD(AStateBelowTheRoot_IsRefused)
	{
		const FSlaterBuildResult Result = StBuild(TEXT("<vbox>\n<hbox>\n<state name=\"tab\" default=\"a\"/>\n</hbox>\n</vbox>"), Stack);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("<state> must be a direct child of the root element"))));
		ASSERT_THAT(AreEqual(Result.Errors[0].Line, 3));
	}

	TEST_METHOD(BadStateTests_AreRefusedAtTheAttribute)
	{
		const FSlaterBuildResult NoEquals = StBuild(TEXT("<vbox><state name=\"tab\" default=\"a\"/><text visible-when=\"tab\">x</text></vbox>"), Stack);
		ASSERT_THAT(IsFalse(NoEquals.Succeeded()));
		ASSERT_THAT(IsTrue(NoEquals.Errors[0].Message.Contains(TEXT("is not a state test for visible-when: write one name, '=' and one value"))));
		const FSlaterBuildResult TwoEquals = StBuild(TEXT("<vbox><state name=\"tab\" default=\"a\"/><surface role=\"UI.Surface.Button\" set-state=\"tab=a=b\"/></vbox>"), Stack);
		ASSERT_THAT(IsTrue(TwoEquals.Errors[0].Message.Contains(TEXT("is not a state test for set-state"))));
		// A binding is no state test: the test is written out, not read from data.
		const FSlaterBuildResult Bound = StBuild(TEXT("<vbox><state name=\"tab\" default=\"a\"/><text visible-when=\"{Tab}\">x</text></vbox>"), Stack);
		ASSERT_THAT(IsFalse(Bound.Succeeded()));
	}

	TEST_METHOD(VisibleWhenWithVisible_IsRefused)
	{
		const FSlaterBuildResult Literal = StBuild(TEXT("<vbox><state name=\"t\" default=\"a\"/><text visible=\"false\" visible-when=\"t=a\">x</text></vbox>"), Stack);
		ASSERT_THAT(IsFalse(Literal.Succeeded()));
		ASSERT_THAT(IsTrue(Literal.Errors[0].Message.Contains(TEXT("would fight over its visibility"))));
		const FSlaterBuildResult Bound = StBuild(TEXT("<vbox><state name=\"t\" default=\"a\"/><text visible=\"{Show}\" visible-when=\"t=a\">x</text></vbox>"), Stack);
		ASSERT_THAT(IsFalse(Bound.Succeeded()));
	}

	TEST_METHOD(VisibleWhenInsideARepeatTemplate_IsRefused_ButSetStateIsFine)
	{
		const FSlaterBuildResult When = StBuild(TEXT("<vbox><state name=\"t\" default=\"a\"/><repeat source=\"{L}\" key=\"Id\"><text visible-when=\"t=a\">x</text></repeat></vbox>"), Stack);
		ASSERT_THAT(IsFalse(When.Succeeded()));
		ASSERT_THAT(IsTrue(When.Errors[0].Message.Contains(TEXT("'visible-when' is not supported inside a template"))));
		const FSlaterBuildResult Set = StBuild(TEXT("<vbox><state name=\"t\" default=\"a\"/><repeat source=\"{L}\" key=\"Id\"><surface role=\"UI.Surface.Button\" set-state=\"t=b\"/></repeat></vbox>"), Stack);
		ASSERT_THAT(IsTrue(Set.Succeeded()));
	}

	TEST_METHOD(ATemplateBody_CanUseSetState_CheckedAgainstTheUsingLayoutsStates)
	{
		// A template body can use `set-state` and `visible-when`; the names are checked against the using layout's states.
		const FSlaterBuildResult Result = StBuild(
			TEXT("<vbox>\n<template name=\"tab-button\" params=\"label to\"><surface role=\"UI.Surface.Button\" set-state=\"tab=$to\"><text>$label</text></surface></template>\n<state name=\"tab\" default=\"a\"/>\n<tab-button label=\"One\" to=\"a\"/>\n<tab-button label=\"Two\" to=\"b\"/>\n</vbox>"), Stack);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.States.Num(), 1));
		const FSlaterBuildResult Undeclared = StBuild(
			TEXT("<vbox>\n<template name=\"tab-button\" params=\"label to\"><surface role=\"UI.Surface.Button\" set-state=\"menu=$to\"><text>$label</text></surface></template>\n<state name=\"tab\" default=\"a\"/>\n<tab-button label=\"One\" to=\"a\"/>\n</vbox>"), Stack);
		ASSERT_THAT(IsFalse(Undeclared.Succeeded()));
		ASSERT_THAT(IsTrue(Undeclared.Errors[0].Message.Contains(TEXT("'menu' is not a declared state"))));
	}

	TEST_METHOD(StateNamesAndValues_FollowTheirRules)
	{
		ASSERT_THAT(IsTrue(Slater::IsValidStateName(TEXT("tab"))));
		ASSERT_THAT(IsTrue(Slater::IsValidStateName(TEXT("_t2"))));
		ASSERT_THAT(IsFalse(Slater::IsValidStateName(TEXT(""))));
		ASSERT_THAT(IsFalse(Slater::IsValidStateName(TEXT("2t"))));
		ASSERT_THAT(IsFalse(Slater::IsValidStateName(TEXT("a.b"))));
		ASSERT_THAT(IsTrue(Slater::IsValidStateValue(TEXT("a-b.c_1"))));
		ASSERT_THAT(IsFalse(Slater::IsValidStateValue(TEXT(""))));
		ASSERT_THAT(IsFalse(Slater::IsValidStateValue(TEXT("a b"))));
		FString Key, Value;
		ASSERT_THAT(IsTrue(Slater::ParseStatePair(TEXT("tab=map"), Key, Value)));
		ASSERT_THAT(IsTrue(StSame(Key, TEXT("tab")) && StSame(Value, TEXT("map"))));
		ASSERT_THAT(IsFalse(Slater::ParseStatePair(TEXT("tab="), Key, Value)));
		ASSERT_THAT(IsFalse(Slater::ParseStatePair(TEXT("=map"), Key, Value)));
	}
};

/** The stock commands `ui.close ui.push ui.pop ui.toggle ui.sound ui.style` and command arguments (T-353). */
TEST_CLASS(SlaterStockCommandTests, "BRS.SlaterUI.StockCommand")
{
	FSlaterBindFixture Fix;
	FSlaterStyleStack Stack;
	TSharedRef<FStFakeHost> Host = MakeShared<FStFakeHost>();
	int32 Fired = 0;
	FString LastArgument;
	FName LastName;

	BEFORE_EACH()
	{
		Fix.Init();
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		Slater::SetScreenHost(Host);
		Fired = 0;
	}

	AFTER_EACH()
	{
		Slater::SetScreenHost(nullptr);
		Slater::SetSoundPlayer(nullptr);
		Slater::Commands().Unregister(TEXT("Test.Use"));
		Fix.Shutdown();
	}

	TSharedRef<SSlaterView> ViewWith(const FString& OnClick)
	{
		Fix.Write(TEXT("StBtn.sltr"), FString::Printf(TEXT("<vbox><state name=\"flag\" default=\"false\"/><surface id=\"B\" role=\"UI.Surface.Button\" on-click=\"%s\"/><text id=\"P\" visible-when=\"flag=true\">x</text></vbox>"), *OnClick));
		return SNew(SSlaterView).LayoutId(TEXT("StBtn"));
	}

	bool ClickB(const TSharedRef<SSlaterView>& View) const
	{
		const TSharedPtr<SSlaterSurface> Surface = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("B")));
		return Surface.IsValid() && Surface->Activate();
	}

	TEST_METHOD(UiPush_AsksTheHostForTheLayout)
	{
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("ui.push:Hud/Inventory"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(ClickB(View)));
		ASSERT_THAT(IsTrue(Host->Log.Num() == 1 && Host->Log[0] == TEXT("push Hud/Inventory")));
	}

	TEST_METHOD(UiPop_AndUiClose_AskTheHost)
	{
		const TSharedRef<SSlaterView> Pop = ViewWith(TEXT("ui.pop"));
		ASSERT_THAT(IsTrue(ClickB(Pop)));
		const TSharedRef<SSlaterView> Close = ViewWith(TEXT("ui.close"));
		ASSERT_THAT(IsTrue(ClickB(Close)));
		ASSERT_THAT(IsTrue(Host->Log.Num() == 2 && Host->Log[0] == TEXT("pop") && Host->Log[1] == TEXT("close")));
	}

	TEST_METHOD(AScreenCommandWithNoHost_WarnsAndDoesNothing)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("ui.close needs a screen host and none is set"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Slater::SetScreenHost(nullptr);
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("ui.close"));
		ASSERT_THAT(IsFalse(ClickB(View)));
		ASSERT_THAT(AreEqual(Host->Log.Num(), 0));
	}

	TEST_METHOD(UiToggle_FlipsAStateBetweenTrueAndFalse)
	{
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("ui.toggle:flag"));
		const TSharedPtr<SWidget> Page = View->FindById(TEXT("P"));
		ASSERT_THAT(IsTrue(Page->GetVisibility() == EVisibility::Collapsed));
		ASSERT_THAT(IsTrue(ClickB(View)));
		FString Flag;
		View->GetState(TEXT("flag"), Flag);
		ASSERT_THAT(IsTrue(Flag.Equals(TEXT("true"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Page->GetVisibility() != EVisibility::Collapsed));
		ASSERT_THAT(IsTrue(ClickB(View)));
		View->GetState(TEXT("flag"), Flag);
		ASSERT_THAT(IsTrue(Flag.Equals(TEXT("false"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Page->GetVisibility() == EVisibility::Collapsed));
	}

	TEST_METHOD(UiToggle_OnAStateThatIsNotTrueOrFalse_GoesToTrue)
	{
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("ui.toggle:flag"));
		View->SetState(TEXT("flag"), TEXT("maybe"));
		ASSERT_THAT(IsTrue(ClickB(View)));
		FString Flag;
		View->GetState(TEXT("flag"), Flag);
		ASSERT_THAT(IsTrue(Flag.Equals(TEXT("true"), ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(UiStyle_RestylesTheViewWithoutLosingState)
	{
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("ui.style:Dusk"));
		View->SetState(TEXT("flag"), TEXT("true"));
		ASSERT_THAT(IsTrue(View->GetStyleName() == FName(TEXT("Base"))));
		ASSERT_THAT(IsTrue(ClickB(View)));
		ASSERT_THAT(IsTrue(View->GetStyleName() == FName(TEXT("Dusk"))));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		FString Flag;
		View->GetState(TEXT("flag"), Flag);
		ASSERT_THAT(IsTrue(Flag.Equals(TEXT("true"), ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(UiStyle_WithAnUnknownStyle_WarnsAndKeepsTheTree)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("ui.style names the style 'Nope'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("ui.style:Nope"));
		const TSharedPtr<SWidget> Before = View->GetResult().Root;
		ASSERT_THAT(IsFalse(ClickB(View)));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->GetResult().Root == Before));
		ASSERT_THAT(IsTrue(View->GetStyleName() == FName(TEXT("Base"))));
	}

	TEST_METHOD(UiSound_GoesToTheSoundPlayer)
	{
		TArray<FName> Played;
		Slater::SetSoundPlayer([&Played](FName Token) { Played.Add(Token); });
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("ui.sound:UI.Click"));
		ASSERT_THAT(IsTrue(ClickB(View)));
		ASSERT_THAT(IsTrue(Played.Num() == 1 && Played[0] == FName(TEXT("UI.Click"))));
	}

	TEST_METHOD(UiSound_WithNoPlayer_Warns)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("ui.sound:UI.Click needs a sound player"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("ui.sound:UI.Click"));
		ASSERT_THAT(IsFalse(ClickB(View)));
	}

	TEST_METHOD(AStockCommand_RunsEvenWhenAViewTableHasOtherNames)
	{
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("ui.pop"));
		ASSERT_THAT(IsTrue(View->GetCommands().Register(TEXT("Test.Use"), [this](const FSlaterCommandCall&) { ++Fired; })));
		ASSERT_THAT(IsTrue(ClickB(View)));
		ASSERT_THAT(AreEqual(Fired, 0));
		ASSERT_THAT(AreEqual(Host->Log.Num(), 1));
	}

	TEST_METHOD(ACommandArgument_ReachesTheHandler_AndNamesStayWhole)
	{
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("Test.Use:Sword"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->GetCommands().Register(TEXT("Test.Use"), [this](const FSlaterCommandCall& Call)
		{
			++Fired;
			LastName = Call.Name;
			LastArgument = Call.Argument;
		})));
		ASSERT_THAT(IsTrue(ClickB(View)));
		ASSERT_THAT(AreEqual(Fired, 1));
		ASSERT_THAT(IsTrue(LastName == FName(TEXT("Test.Use"))));
		ASSERT_THAT(IsTrue(LastArgument.Equals(TEXT("Sword"), ESearchCase::CaseSensitive)));
		const TSharedPtr<SSlaterSurface> Surface = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("B")));
		ASSERT_THAT(IsTrue(Surface->GetCommand() == FName(TEXT("Test.Use"))));
	}

	TEST_METHOD(ACommandWithNoArgument_GetsAnEmptyOne)
	{
		const TSharedRef<SSlaterView> View = ViewWith(TEXT("Test.Use"));
		View->GetCommands().Register(TEXT("Test.Use"), [this](const FSlaterCommandCall& Call) { LastArgument = Call.Argument; ++Fired; });
		ASSERT_THAT(IsTrue(ClickB(View)));
		ASSERT_THAT(AreEqual(Fired, 1));
		ASSERT_THAT(IsTrue(LastArgument.IsEmpty()));
	}

	TEST_METHOD(StockCommandNames_AreReserved_AndListed)
	{
		FString Why;
		ASSERT_THAT(IsFalse(Slater::Commands().Register(TEXT("ui.close"), [](const FSlaterCommandCall&) {}, &Why)));
		ASSERT_THAT(IsTrue(Why.Contains(TEXT("reserved for the stock commands"))));
		FSlaterCommandTable Table;
		ASSERT_THAT(IsFalse(Table.Register(TEXT("ui.mine"), [](const FSlaterCommandCall&) {}, &Why)));
		TArray<FString> Names;
		Slater::GetStockCommands(Names);
		ASSERT_THAT(AreEqual(Names.Num(), 6));
		for (const TCHAR* Name : {TEXT("ui.close"), TEXT("ui.push"), TEXT("ui.pop"), TEXT("ui.toggle"), TEXT("ui.sound"), TEXT("ui.style")})
		{
			ASSERT_THAT(IsTrue(Names.Contains(Name)));
			ASSERT_THAT(IsTrue(Slater::IsStockCommand(Name)));
		}
		ASSERT_THAT(IsFalse(Slater::IsStockCommand(TEXT("ui.nope"))));
		ASSERT_THAT(IsFalse(Slater::IsStockCommand(TEXT("Ui.Close"))));
	}

	TEST_METHOD(BadCommandText_IsRefusedAtTheAttribute)
	{
		auto Problem = [&](const TCHAR* Text) { return StBuild(FString::Printf(TEXT("<vbox><state name=\"flag\" default=\"false\"/><surface role=\"UI.Surface.Button\" on-click=\"%s\"/></vbox>"), Text), Stack); };
		const FSlaterBuildResult CloseArg = Problem(TEXT("ui.close:now"));
		ASSERT_THAT(IsFalse(CloseArg.Succeeded()));
		ASSERT_THAT(IsTrue(CloseArg.Errors[0].Message.Contains(TEXT("ui.close takes no argument"))));
		const FSlaterBuildResult PushNone = Problem(TEXT("ui.push"));
		ASSERT_THAT(IsTrue(PushNone.Errors[0].Message.Contains(TEXT("ui.push needs an argument after a ':', for example on-click=\"ui.push:Inventory\""))));
		const FSlaterBuildResult PushBadId = Problem(TEXT("ui.push:../x"));
		ASSERT_THAT(IsTrue(PushBadId.Errors[0].Message.Contains(TEXT("is not a layout ID"))));
		const FSlaterBuildResult ToggleBad = Problem(TEXT("ui.toggle:1x"));
		ASSERT_THAT(IsTrue(ToggleBad.Errors[0].Message.Contains(TEXT("is not a state name"))));
		const FSlaterBuildResult Unknown = Problem(TEXT("ui.nope"));
		ASSERT_THAT(IsTrue(Unknown.Errors[0].Message.Contains(TEXT("not a stock command; names starting with 'ui.' are reserved for these: ui.close, ui.push"))));
		const FSlaterBuildResult EmptyArg = Problem(TEXT("Test.Use:"));
		ASSERT_THAT(IsTrue(EmptyArg.Errors[0].Message.Contains(TEXT("needs one word after the ':'"))));
		const FSlaterBuildResult TwoWords = Problem(TEXT("Test.Use:a b"));
		ASSERT_THAT(IsFalse(TwoWords.Succeeded()));
		const FSlaterBuildResult Fine = Problem(TEXT("ui.toggle:flag"));
		ASSERT_THAT(IsTrue(Fine.Succeeded()));
	}

	FSlaterBuildResult StBuild(const FString& Text, const FSlaterStyleStack& InStack) const
	{
		return ::StBuild(Text, InStack);
	}
};

#endif
