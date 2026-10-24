// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterBindTestKit.h"
#include "SlaterCommands.h"
#include "SlaterEvents.h"
#include "SlaterImages.h"
#include "SlaterPopup.h"
#include "SlaterSurface.h"
#include "SlaterView.h"

namespace SlaterPopupTestsPrivate
{
	FPointerEvent PopLeftMouse()
	{
		return FPointerEvent(0, FVector2f::ZeroVector, FVector2f::ZeroVector, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	}

	bool PopSame(const FString& A, const TCHAR* B)
	{
		return A.Equals(B, ESearchCase::CaseSensitive);
	}

	/** The common layout: a page button that opens a menu of two rows, the menu following the state `menu`. */
	const TCHAR* const PopLayout = TEXT(
		"<overlay>\n"
		"\t<state name=\"menu\" default=\"false\"/>\n"
		"\t<popup id=\"P\" placement=\"above\" open-when=\"menu=true\">\n"
		"\t\t<surface id=\"Anchor\" role=\"UI.Surface.Panel\" interactive=\"false\"><text>Open</text></surface>\n"
		"\t\t<surface id=\"MenuBox\" role=\"UI.Surface.Menu\"><vbox>\n"
		"\t\t\t<surface id=\"RowA\" role=\"UI.Surface.MenuItem\" set-state=\"menu=false\" on-click=\"Pop.A\"><text>A</text></surface>\n"
		"\t\t\t<surface id=\"RowB\" role=\"UI.Surface.MenuItem\" on-click=\"Pop.B\"><text>B</text></surface>\n"
		"\t\t</vbox></surface>\n"
		"\t</popup>\n"
		"</overlay>");
}
using namespace SlaterPopupTestsPrivate;

/**
 * The `popup` element (T-439): an anchor child, a menu child, open and close by a view state or by a click on the anchor.
 * No window is shown, so the engine's menu stack does not run; `IsOpenRequested` is what the element asked for, and the
 * engine's close is reported by calling its callback (`HandleMenuOpenChanged`). That the menu really draws in the game
 * viewport is the T-416 hand check, repeated for the element in DevNotes "Popup element (T-439)".
 */
TEST_CLASS(SlaterPopupTests, "BRS.SlaterUI.Popup")
{
	FSlaterBindFixture Fix;
	FSlaterStyleStack Stack;
	TMap<FName, int32> Fired;

	BEFORE_EACH()
	{
		Fix.Init();
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		Fired.Reset();
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TSharedRef<SSlaterView> MakeView(const TCHAR* Layout = PopLayout)
	{
		Fix.Write(TEXT("PopTest.sltr"), Layout);
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("PopTest"));
		for (const TCHAR* Name : {TEXT("Pop.A"), TEXT("Pop.B")})
		{
			View->GetCommands().Register(Name, [this](const FSlaterCommandCall& Call) { Fired.FindOrAdd(Call.Name)++; });
		}
		return View;
	}

	TSharedPtr<SSlaterPopup> PopupOf(const TSharedRef<SSlaterView>& View, const TCHAR* Id = TEXT("P")) const
	{
		const TSharedPtr<SWidget> Widget = View->FindById(Id);
		return Widget.IsValid() && Widget->GetType() == FName(TEXT("SSlaterPopup")) ? StaticCastSharedPtr<SSlaterPopup>(Widget) : nullptr;
	}

	FString StateOf(const TSharedRef<SSlaterView>& View, const TCHAR* Key) const
	{
		FString Value;
		View->GetState(Key, Value);
		return Value;
	}

	bool Click(const TSharedRef<SSlaterView>& View, const TCHAR* Id) const
	{
		const TSharedPtr<SSlaterSurface> Surface = StaticCastSharedPtr<SSlaterSurface>(View->FindById(Id));
		return Surface.IsValid() && Surface->Activate();
	}

	/** A real press and release on the popup itself (the anchor is not interactive, so the release reaches the popup). */
	void PressAndRelease(const TSharedRef<SSlaterPopup>& Popup) const
	{
		const FGeometry Geometry;
		Popup->OnPreviewMouseButtonDown(Geometry, PopLeftMouse());
		Popup->OnMouseButtonUp(Geometry, PopLeftMouse());
	}

	FSlaterBuildResult Build(const FString& Text) const
	{
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}

	int32 Count(const TCHAR* Command) const
	{
		const int32* Found = Fired.Find(FName(Command));
		return Found ? *Found : 0;
	}

	TEST_METHOD(APopup_BuildsWithAnAnchorAndAMenuChild)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		for (const FSlaterError& Error : View->GetResult().Errors)
		{
			AddError(Error.ToString());
		}
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TSharedPtr<SSlaterPopup> Popup = PopupOf(View);
		ASSERT_THAT(IsTrue(Popup.IsValid()));
		ASSERT_THAT(IsTrue(Popup->GetPlacement() == MenuPlacement_AboveAnchor));
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		// The anchor is the popup's own content; the menu is held for the engine, wrapped, not in the popup's tree until it opens.
		const TSharedPtr<SWidget> Anchor = View->FindById(TEXT("Anchor"));
		ASSERT_THAT(IsTrue(Anchor.IsValid()));
		ASSERT_THAT(IsTrue(Anchor->GetParentWidget() == Popup));
		const TSharedPtr<SWidget> MenuBox = View->FindById(TEXT("MenuBox"));
		ASSERT_THAT(IsTrue(MenuBox.IsValid()));
		ASSERT_THAT(IsTrue(MenuBox->GetParentWidget() == Popup->GetMenuRoot()));
		ASSERT_THAT(IsTrue(Popup->GetMenuRoot()->GetPopup() == Popup));
	}

	TEST_METHOD(EveryPlacementWord_Builds_AndAnUnknownOneIsRefused)
	{
		const TPair<const TCHAR*, EMenuPlacement> Words[] = {{TEXT("above"), MenuPlacement_AboveAnchor}, {TEXT("below"), MenuPlacement_BelowAnchor}, {TEXT("right"), MenuPlacement_MenuRight}};
		for (const TPair<const TCHAR*, EMenuPlacement>& Word : Words)
		{
			const FSlaterBuildResult Result = Build(FString::Printf(TEXT("<popup id=\"P\" placement=\"%s\"><text>a</text><text>m</text></popup>"), Word.Key));
			ASSERT_THAT(IsTrue(Result.Succeeded()));
			ASSERT_THAT(IsTrue(StaticCastSharedPtr<SSlaterPopup>(Result.ById.FindRef(TEXT("P")).Pin())->GetPlacement() == Word.Value));
		}
		// No placement written: below.
		const FSlaterBuildResult Default = Build(TEXT("<popup id=\"P\"><text>a</text><text>m</text></popup>"));
		ASSERT_THAT(IsTrue(Default.Succeeded()));
		ASSERT_THAT(IsTrue(StaticCastSharedPtr<SSlaterPopup>(Default.ById.FindRef(TEXT("P")).Pin())->GetPlacement() == MenuPlacement_BelowAnchor));
		const FSlaterBuildResult Bad = Build(TEXT("<popup placement=\"left\"><text>a</text><text>m</text></popup>"));
		ASSERT_THAT(IsFalse(Bad.Succeeded()));
		ASSERT_THAT(IsTrue(Bad.Errors[0].Message.Contains(TEXT("placement"))));
	}

	TEST_METHOD(APopupWithNoAnchor_OrNoMenu_OrTooManyChildren_IsRefusedWithItsLine)
	{
		const FSlaterBuildResult None = Build(TEXT("<vbox>\n<popup/>\n</vbox>"));
		ASSERT_THAT(IsFalse(None.Succeeded()));
		ASSERT_THAT(AreEqual(None.Errors.Num(), 1));
		ASSERT_THAT(IsTrue(None.Errors[0].Message.Contains(TEXT("needs an anchor"))));
		ASSERT_THAT(AreEqual(None.Errors[0].Line, 2));

		const FSlaterBuildResult OnlyAnchor = Build(TEXT("<popup><text>Open</text></popup>"));
		ASSERT_THAT(IsFalse(OnlyAnchor.Succeeded()));
		ASSERT_THAT(IsTrue(OnlyAnchor.Errors[0].Message.Contains(TEXT("needs a menu"))));

		const FSlaterBuildResult Three = Build(TEXT("<popup><text>a</text><text>b</text><text>c</text></popup>"));
		ASSERT_THAT(IsFalse(Three.Succeeded()));
		ASSERT_THAT(IsTrue(Three.Errors[0].Message.Contains(TEXT("exactly two children"))));
	}

	TEST_METHOD(OpenWhen_OpensAndClosesThePopup_FollowingTheState)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterPopup> Popup = PopupOf(View);
		ASSERT_THAT(IsTrue(Popup.IsValid()));
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		View->SetState(TEXT("menu"), TEXT("true"));
		ASSERT_THAT(IsTrue(Popup->IsOpenRequested()));
		View->SetState(TEXT("menu"), TEXT("false"));
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		// The anchor never hides: the popup is not shown or collapsed by its state.
		ASSERT_THAT(IsTrue(View->FindById(TEXT("Anchor"))->GetVisibility() != EVisibility::Collapsed));
	}

	TEST_METHOD(ARowInTheMenu_FindsItsView_SetsTheState_AndRunsItsCommand)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterPopup> Popup = PopupOf(View);
		View->SetState(TEXT("menu"), TEXT("true"));
		ASSERT_THAT(IsTrue(Popup->IsOpenRequested()));
		// The row sits in the engine's menu content, not under the view: it still reaches the view's state and commands.
		ASSERT_THAT(IsTrue(Slater::FindOwningView(*View->FindById(TEXT("RowA"))) == View));
		ASSERT_THAT(IsTrue(Click(View, TEXT("RowA"))));
		ASSERT_THAT(AreEqual(Count(TEXT("Pop.A")), 1));
		ASSERT_THAT(IsTrue(PopSame(StateOf(View, TEXT("menu")), TEXT("false"))));
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
	}

	TEST_METHOD(AnEngineClose_OutsideClickOrEscape_WritesTheCloseStateBack)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterPopup> Popup = PopupOf(View);
		View->SetState(TEXT("menu"), TEXT("true"));
		// The engine dismissed the menu (a press outside, Escape). open-when="menu=true" closes by writing menu=false.
		Popup->HandleMenuOpenChanged(false);
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		ASSERT_THAT(IsTrue(PopSame(StateOf(View, TEXT("menu")), TEXT("false"))));
		// A close the popup asked for itself does not write a second time: a state set to something else stays.
		View->SetState(TEXT("menu"), TEXT("true"));
		View->SetState(TEXT("menu"), TEXT("elsewhere"));
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		ASSERT_THAT(IsTrue(PopSame(StateOf(View, TEXT("menu")), TEXT("elsewhere"))));
	}

	TEST_METHOD(AClickOnTheAnchor_OpensTheMenu_WritesTheState_AndASecondClickCloses)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterPopup> Popup = PopupOf(View);
		PressAndRelease(Popup.ToSharedRef());
		ASSERT_THAT(IsTrue(Popup->IsOpenRequested()));
		ASSERT_THAT(IsTrue(PopSame(StateOf(View, TEXT("menu")), TEXT("true"))));
		PressAndRelease(Popup.ToSharedRef());
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		ASSERT_THAT(IsTrue(PopSame(StateOf(View, TEXT("menu")), TEXT("false"))));
	}

	TEST_METHOD(APressThatCloses_TheMenu_DoesNotReopenItOnTheRelease)
	{
		// The T-416 bug: the engine closes the menu on the press outside, and the release on the anchor then toggled it open again.
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterPopup> Popup = PopupOf(View);
		PressAndRelease(Popup.ToSharedRef());
		ASSERT_THAT(IsTrue(Popup->IsOpenRequested()));

		const FGeometry Geometry;
		Popup->OnPreviewMouseButtonDown(Geometry, PopLeftMouse());
		Popup->HandleMenuOpenChanged(false);
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		Popup->OnMouseButtonUp(Geometry, PopLeftMouse());
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		ASSERT_THAT(IsTrue(PopSame(StateOf(View, TEXT("menu")), TEXT("false"))));
		// The next whole click opens it again.
		PressAndRelease(Popup.ToSharedRef());
		ASSERT_THAT(IsTrue(Popup->IsOpenRequested()));
	}

	TEST_METHOD(APopupWithoutOpenWhen_OpensByClick_AndNoStateIsNeeded)
	{
		const FSlaterBuildResult Result = Build(TEXT("<popup id=\"P\"><surface role=\"UI.Surface.Panel\" interactive=\"false\"><text>Open</text></surface><text>Menu</text></popup>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SSlaterPopup> Popup = StaticCastSharedPtr<SSlaterPopup>(Result.ById.FindRef(TEXT("P")).Pin());
		ASSERT_THAT(IsTrue(Popup.IsValid()));
		ASSERT_THAT(IsTrue(Popup->ToggleFromClick()));
		ASSERT_THAT(IsTrue(Popup->IsOpenRequested()));
		ASSERT_THAT(IsFalse(Popup->ToggleFromClick()));
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
	}

	TEST_METHOD(OpenWhenAndCloseState_AreCheckedAgainstTheDeclaredStates)
	{
		const FSlaterBuildResult Undeclared = Build(TEXT("<overlay><state name=\"sub\" default=\"none\"/><popup open-when=\"menu=true\"><text>a</text><text>m</text></popup></overlay>"));
		ASSERT_THAT(IsFalse(Undeclared.Succeeded()));
		ASSERT_THAT(IsTrue(Undeclared.Errors[0].Message.Contains(TEXT("'menu' is not a declared state; declared: sub"))));

		const FSlaterBuildResult Shape = Build(TEXT("<overlay><state name=\"menu\" default=\"false\"/><popup open-when=\"menu\"><text>a</text><text>m</text></popup></overlay>"));
		ASSERT_THAT(IsFalse(Shape.Succeeded()));
		ASSERT_THAT(IsTrue(Shape.Errors[0].Message.Contains(TEXT("open-when"))));

		const FSlaterBuildResult CloseAlone = Build(TEXT("<overlay><state name=\"menu\" default=\"false\"/><popup close-state=\"menu=false\"><text>a</text><text>m</text></popup></overlay>"));
		ASSERT_THAT(IsFalse(CloseAlone.Succeeded()));
		ASSERT_THAT(IsTrue(CloseAlone.Errors[0].Message.Contains(TEXT("close-state but no open-when"))));

		const FSlaterBuildResult Other = Build(TEXT("<overlay><state name=\"menu\" default=\"false\"/><state name=\"sub\" default=\"none\"/><popup open-when=\"menu=true\" close-state=\"sub=none\"><text>a</text><text>m</text></popup></overlay>"));
		ASSERT_THAT(IsFalse(Other.Succeeded()));
		ASSERT_THAT(IsTrue(Other.Errors[0].Message.Contains(TEXT("different states"))));
	}

	TEST_METHOD(ACloseState_ForANonBooleanState_IsWrittenOnAnEngineClose)
	{
		const TSharedRef<SSlaterView> View = MakeView(TEXT(
			"<overlay><state name=\"panel\" default=\"none\"/>"
			"<popup id=\"P\" open-when=\"panel=menu\" close-state=\"panel=none\"><text>Open</text><text>Menu</text></popup></overlay>"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TSharedPtr<SSlaterPopup> Popup = PopupOf(View);
		View->SetState(TEXT("panel"), TEXT("menu"));
		ASSERT_THAT(IsTrue(Popup->IsOpenRequested()));
		Popup->HandleMenuOpenChanged(false);
		ASSERT_THAT(IsTrue(PopSame(StateOf(View, TEXT("panel")), TEXT("none"))));
	}

	TEST_METHOD(InsideATemplate_EachUseOpensItsOwnPopup_ByClick)
	{
		const TSharedRef<SSlaterView> View = MakeView(TEXT(
			"<vbox>"
			"<template name=\"pick\" params=\"label\"><popup><surface role=\"UI.Surface.Panel\" interactive=\"false\"><text>$label</text></surface><text>Menu</text></popup></template>"
			"<pick id=\"First\" label=\"One\"/><pick id=\"Second\" label=\"Two\"/>"
			"</vbox>"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TSharedPtr<SSlaterPopup> First = PopupOf(View, TEXT("First"));
		const TSharedPtr<SSlaterPopup> Second = PopupOf(View, TEXT("Second"));
		ASSERT_THAT(IsTrue(First.IsValid() && Second.IsValid() && First != Second));
		PressAndRelease(First.ToSharedRef());
		ASSERT_THAT(IsTrue(First->IsOpenRequested()));
		ASSERT_THAT(IsFalse(Second->IsOpenRequested()));
	}

	TEST_METHOD(OpenWhenInsideARepeat_IsRefused_WithTheWayOut)
	{
		// A `repeat` instantiates its child per item, and a state is the view's, not the item's.
		const FSlaterBuildResult Result = Build(TEXT(
			"<vbox><state name=\"menu\" default=\"false\"/>"
			"<repeat source=\"{Items}\" key=\"Id\" columns=\"1\"><popup open-when=\"menu=true\"><text>a</text><text>m</text></popup></repeat></vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("open-when"))));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("not supported inside a template"))));
	}

	TEST_METHOD(TheContextMenuPopupVariant_BuildsClosed_WithTheSameRows)
	{
		Slater::SetImageSource(Slater::MakePluginIconSource());
		const TSharedRef<SSlaterView> Menu = SNew(SSlaterView).LayoutId(TEXT("ContextMenuPopup"));
		for (const TCHAR* Name : {TEXT("Page.Action"), TEXT("Account.Settings"), TEXT("Account.Language"), TEXT("Account.Help"), TEXT("Account.Upgrade"), TEXT("Account.Gift"), TEXT("Account.Logout"), TEXT("Account.Terms"), TEXT("Account.Privacy"), TEXT("Account.Usage")})
		{
			Menu->GetCommands().Register(Name, [this](const FSlaterCommandCall& Call) { Fired.FindOrAdd(Call.Name)++; });
		}
		for (const FSlaterError& Error : Menu->GetResult().Errors)
		{
			AddError(Error.ToString());
		}
		ASSERT_THAT(IsFalse(Menu->HasErrors()));
		const TSharedPtr<SSlaterPopup> Popup = PopupOf(Menu, TEXT("AccountMenu"));
		ASSERT_THAT(IsTrue(Popup.IsValid()));
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		// The same rows as ContextMenu.sltr, by the same ids.
		for (const TCHAR* Id : {TEXT("ItemSettings"), TEXT("ItemLanguage"), TEXT("ItemHelp"), TEXT("ItemUpgrade"), TEXT("ItemGift"), TEXT("ItemLearn"), TEXT("ItemLogout"), TEXT("LinkTerms"), TEXT("LinkPrivacy"), TEXT("LinkUsage")})
		{
			ASSERT_THAT(IsTrue(Menu->FindById(Id).IsValid()));
		}
		// The footer chevron opens it (a click on the anchor, via the state), and an item closes it again.
		ASSERT_THAT(IsTrue(Click(Menu, TEXT("Chevron"))));
		ASSERT_THAT(IsTrue(Popup->IsOpenRequested()));
		ASSERT_THAT(IsTrue(Click(Menu, TEXT("ItemUpgrade"))));
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		ASSERT_THAT(AreEqual(Count(TEXT("Account.Upgrade")), 1));

		// The submenu is a second popup on the Learn more row: hovering it opens it, and closing the menu closes it and writes sub=none.
		ASSERT_THAT(IsTrue(Click(Menu, TEXT("Chevron"))));
		const TSharedPtr<SSlaterPopup> Sub = PopupOf(Menu, TEXT("LearnMenu"));
		ASSERT_THAT(IsTrue(Sub.IsValid()));
		ASSERT_THAT(IsFalse(Sub->IsOpenRequested()));
		StaticCastSharedPtr<SSlaterSurface>(Menu->FindById(TEXT("ItemLearn")))->OnMouseEnter(FGeometry(), PopLeftMouse());
		ASSERT_THAT(IsTrue(PopSame(StateOf(Menu, TEXT("sub")), TEXT("learn"))));
		ASSERT_THAT(IsTrue(Sub->IsOpenRequested()));
		ASSERT_THAT(IsTrue(Click(Menu, TEXT("LinkPrivacy"))));
		ASSERT_THAT(IsFalse(Popup->IsOpenRequested()));
		ASSERT_THAT(IsFalse(Sub->IsOpenRequested()));
		ASSERT_THAT(IsTrue(PopSame(StateOf(Menu, TEXT("sub")), TEXT("none"))));
		Slater::SetImageSource(nullptr);
	}
};

#endif
