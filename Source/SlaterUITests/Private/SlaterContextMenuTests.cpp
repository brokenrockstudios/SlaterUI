// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Input/HittestGrid.h"
#include "SlaterCommands.h"
#include "SlaterImages.h"
#include "SlaterSurface.h"
#include "SlaterView.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"
#include "Widgets/SWindow.h"

namespace
{
	FPointerEvent CmLeftMouse()
	{
		return FPointerEvent(0, FVector2f::ZeroVector, FVector2f::ZeroVector, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	}

	bool CmSame(const FString& A, const TCHAR* B)
	{
		return A.Equals(B, ESearchCase::CaseSensitive);
	}
}

/**
 * The popup menu sample (T-415): `Layouts/ContextMenu.sltr`, a footer chevron, a scrim, rows that fire commands and a submenu opened
 * by hover, all from markup. Pointer events are synthesized by calling the widgets' handlers (as the State tests do); the one real
 * hit-test, for the layer that must not block the page, paints the view in a window and asks its hit-test grid.
 */
TEST_CLASS(SlaterContextMenuTests, "BRS.SlaterUI.ContextMenu")
{
	TMap<FName, int32> Fired;

	BEFORE_EACH()
	{
		Slater::SetImageSource(Slater::MakePluginIconSource());
		Fired.Reset();
	}

	AFTER_EACH()
	{
		Slater::SetImageSource(nullptr);
	}

	/** A view of the shipped layout with a counting handler registered for each command it names. */
	TSharedRef<SSlaterView> MakeView()
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ContextMenu"));
		for (const TCHAR* Name : {TEXT("Page.Action"), TEXT("Account.Settings"), TEXT("Account.Language"), TEXT("Account.Help"), TEXT("Account.Upgrade"), TEXT("Account.Gift"), TEXT("Account.Logout"), TEXT("Account.Terms"), TEXT("Account.Privacy"), TEXT("Account.Usage")})
		{
			View->GetCommands().Register(Name, [this](const FSlaterCommandCall& Call) { Fired.FindOrAdd(Call.Name)++; });
		}
		return View;
	}

	int32 Count(const TCHAR* Command) const
	{
		const int32* Found = Fired.Find(FName(Command));
		return Found ? *Found : 0;
	}

	TSharedPtr<SSlaterSurface> Surf(const TSharedRef<SSlaterView>& View, const TCHAR* Id) const
	{
		return StaticCastSharedPtr<SSlaterSurface>(View->FindById(Id));
	}

	bool Click(const TSharedRef<SSlaterView>& View, const TCHAR* Id) const
	{
		const TSharedPtr<SSlaterSurface> Surface = Surf(View, Id);
		return Surface.IsValid() && Surface->Activate();
	}

	bool Shown(const TSharedRef<SSlaterView>& View, const TCHAR* Id) const
	{
		const TSharedPtr<SWidget> Widget = View->FindById(Id);
		return Widget.IsValid() && Widget->GetVisibility() != EVisibility::Collapsed;
	}

	FString StateOf(const TSharedRef<SSlaterView>& View, const TCHAR* Key) const
	{
		FString Value;
		View->GetState(Key, Value);
		return Value;
	}

	bool IsOpen(const TSharedRef<SSlaterView>& View) const
	{
		return CmSame(StateOf(View, TEXT("menu")), TEXT("true"));
	}

	/** Every widget the hit-test grid finds at the center of Target, after painting View in a window the grid belongs to. */
	TArray<TSharedRef<SWidget>> WidgetsAtCenterOf(const TSharedRef<SSlaterView>& View, const TCHAR* Target) const
	{
		TArray<TSharedRef<SWidget>> Found;
		const TSharedPtr<SWidget> TargetWidget = View->FindById(Target);
		if (!TargetWidget.IsValid())
		{
			return Found;
		}
		View->SlatePrepass(1.f);
		const TSharedRef<SWindow> Window = SNew(SWindow)
			.ClientSize(FVector2D(View->GetDesiredSize()))
			.ScreenPosition(FVector2D(100.f, 100.f))
			.SizingRule(ESizingRule::FixedSize)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			[View];
		Window->SlatePrepass(1.f);
		FSlateWindowElementList Elements(Window);
		Window->PaintWindow(0.0, 0.f, Elements, FWidgetStyle(), true);

		const FVector2f Center = TargetWidget->GetPaintSpaceGeometry().GetAbsolutePositionAtCoordinates(FVector2f(0.5f, 0.5f));
		const FVector2D Desktop = FVector2D(Window->GetPositionInScreen()) + FVector2D(Center);
		for (const FWidgetAndPointer& Entry : Window->GetHittestGrid().GetBubblePath(Desktop, 0.f, false))
		{
			Found.Add(Entry.Widget);
		}
		return Found;
	}

	bool PathHas(const TArray<TSharedRef<SWidget>>& Path, const TSharedPtr<SWidget>& Widget) const
	{
		return Widget.IsValid() && Path.ContainsByPredicate([&](const TSharedRef<SWidget>& Entry) { return &Entry.Get() == Widget.Get(); });
	}

	TEST_METHOD(TheShippedLayout_BuildsClosed_WithEveryIconInTheSource)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		for (const FSlaterError& Error : View->GetResult().Errors)
		{
			AddError(Error.ToString());
		}
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsFalse(IsOpen(View)));
		ASSERT_THAT(IsTrue(CmSame(StateOf(View, TEXT("sub")), TEXT("none"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("Popup"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("Scrim"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("Page"))));
		// Every icon the layout names resolves to an SVG brush.
		for (const TCHAR* Name : {TEXT("User"), TEXT("ChevronUp"), TEXT("ChevronRight"), TEXT("External"), TEXT("Settings"), TEXT("Globe"), TEXT("Help"), TEXT("Upgrade"), TEXT("Gift"), TEXT("Book"), TEXT("Logout")})
		{
			const FSlateBrush* Brush = Slater::FindImage(FName(Name, FNAME_Find));
			ASSERT_THAT(IsNotNull(Brush));
			ASSERT_THAT(IsTrue(Brush->GetImageType() == ESlateBrushImageType::Vector));
		}
	}

	TEST_METHOD(TheChevron_OpensAndClosesTheMenu)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		ASSERT_THAT(IsTrue(Click(View, TEXT("Chevron"))));
		ASSERT_THAT(IsTrue(IsOpen(View)));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("Popup"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("Scrim"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("ItemSettings"))));
		// The submenu is not open until the pointer is on Learn more.
		ASSERT_THAT(IsFalse(Shown(View, TEXT("Submenu"))));
		ASSERT_THAT(IsTrue(Click(View, TEXT("Chevron"))));
		ASSERT_THAT(IsFalse(IsOpen(View)));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("Popup"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("Scrim"))));
	}

	TEST_METHOD(AClickOnTheScrim_ClosesTheMenu_AndRunsNoCommand)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		Click(View, TEXT("Chevron"));
		ASSERT_THAT(IsTrue(IsOpen(View)));
		const TSharedPtr<SSlaterSurface> Scrim = Surf(View, TEXT("Scrim"));
		ASSERT_THAT(IsTrue(Scrim.IsValid()));
		// A real press and release on the scrim, not Activate: it takes the click and does not look pressed (non-interactive).
		const FGeometry Geometry;
		ASSERT_THAT(IsTrue(Scrim->GetVisibility() == EVisibility::Visible));
		ASSERT_THAT(IsFalse(Scrim->IsInteractive()));
		ASSERT_THAT(IsTrue(Scrim->OnMouseButtonDown(Geometry, CmLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsTrue(IsOpen(View)));
		ASSERT_THAT(IsTrue(Scrim->OnMouseButtonUp(Geometry, CmLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsFalse(IsOpen(View)));
		ASSERT_THAT(AreEqual(Fired.Num(), 0));
	}

	TEST_METHOD(AnItem_ClosesTheMenu_AndFiresItsCommandOnce)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		Click(View, TEXT("Chevron"));
		ASSERT_THAT(IsTrue(Click(View, TEXT("ItemUpgrade"))));
		ASSERT_THAT(IsFalse(IsOpen(View)));
		ASSERT_THAT(AreEqual(Count(TEXT("Account.Upgrade")), 1));
		ASSERT_THAT(AreEqual(Fired.Num(), 1));

		// A row with a shortcut works the same, through a real press and release.
		Click(View, TEXT("Chevron"));
		const TSharedPtr<SSlaterSurface> Settings = Surf(View, TEXT("ItemSettings"));
		ASSERT_THAT(IsTrue(Settings.IsValid() && Settings->IsInteractive()));
		const FGeometry Geometry;
		ASSERT_THAT(IsTrue(Settings->OnMouseButtonDown(Geometry, CmLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsTrue(Settings->OnMouseButtonUp(Geometry, CmLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsFalse(IsOpen(View)));
		ASSERT_THAT(AreEqual(Count(TEXT("Account.Settings")), 1));
	}

	TEST_METHOD(AClickInsideTheMenuThatIsNoItem_KeepsItOpen)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		Click(View, TEXT("Chevron"));
		// The menu surface takes clicks on its header, dividers and padding so they do not fall through to the frame and the scrim.
		const TSharedPtr<SSlaterSurface> Menu = Surf(View, TEXT("Menu"));
		ASSERT_THAT(IsTrue(Menu.IsValid()));
		const FGeometry Geometry;
		ASSERT_THAT(IsTrue(Menu->OnMouseButtonDown(Geometry, CmLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsTrue(Menu->OnMouseButtonUp(Geometry, CmLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsTrue(IsOpen(View)));
		ASSERT_THAT(AreEqual(Fired.Num(), 0));
	}

	TEST_METHOD(HoveringLearnMore_OpensTheSubmenu_AndAnotherRowClosesIt)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		Click(View, TEXT("Chevron"));
		const FGeometry Geometry;
		const TSharedPtr<SSlaterSurface> Learn = Surf(View, TEXT("ItemLearn"));
		const TSharedPtr<SSlaterSurface> Gift = Surf(View, TEXT("ItemGift"));
		ASSERT_THAT(IsTrue(Learn.IsValid() && Gift.IsValid()));
		// Learn more has no command and no click state, but it still follows the pointer (interactive="true").
		ASSERT_THAT(IsTrue(Learn->IsInteractive()));
		ASSERT_THAT(IsFalse(Learn->HasAction()));

		Learn->OnMouseEnter(Geometry, CmLeftMouse());
		ASSERT_THAT(IsTrue(CmSame(StateOf(View, TEXT("sub")), TEXT("learn"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("Submenu"))));
		ASSERT_THAT(IsTrue(Shown(View, TEXT("LinkTerms"))));

		// The pointer moves to another row: leave of the old one, then enter of the new one.
		Learn->OnMouseLeave(CmLeftMouse());
		Gift->OnMouseEnter(Geometry, CmLeftMouse());
		ASSERT_THAT(IsTrue(CmSame(StateOf(View, TEXT("sub")), TEXT("none"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("Submenu"))));
		ASSERT_THAT(IsTrue(IsOpen(View)));
	}

	TEST_METHOD(TheSubmenu_StaysOpenAcrossTheGap_AndClosesWhenThePointerLeavesThePopup)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		Click(View, TEXT("Chevron"));
		const FGeometry Geometry;
		const TSharedPtr<SSlaterSurface> Popup = Surf(View, TEXT("Popup"));
		const TSharedPtr<SSlaterSurface> Learn = Surf(View, TEXT("ItemLearn"));
		const TSharedPtr<SSlaterSurface> Terms = Surf(View, TEXT("LinkTerms"));
		ASSERT_THAT(IsTrue(Popup.IsValid() && Learn.IsValid() && Terms.IsValid()));

		Popup->OnMouseEnter(Geometry, CmLeftMouse());
		Learn->OnMouseEnter(Geometry, CmLeftMouse());
		// Over the gap between menu and submenu only the row's leave arrives; the frame (which owns the gap) is not left.
		Learn->OnMouseLeave(CmLeftMouse());
		ASSERT_THAT(IsTrue(CmSame(StateOf(View, TEXT("sub")), TEXT("learn"))));
		// A submenu row keeps it open.
		Terms->OnMouseEnter(Geometry, CmLeftMouse());
		ASSERT_THAT(IsTrue(CmSame(StateOf(View, TEXT("sub")), TEXT("learn"))));
		Terms->OnMouseLeave(CmLeftMouse());
		ASSERT_THAT(IsTrue(Shown(View, TEXT("Submenu"))));
		// Leaving the whole popup closes the submenu; the menu itself stays open until a click.
		Popup->OnMouseLeave(CmLeftMouse());
		ASSERT_THAT(IsTrue(CmSame(StateOf(View, TEXT("sub")), TEXT("none"))));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("Submenu"))));
		ASSERT_THAT(IsTrue(IsOpen(View)));
	}

	TEST_METHOD(ASubmenuRow_FiresItsCommand_AndClosesTheMenu)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		Click(View, TEXT("Chevron"));
		View->SetState(TEXT("sub"), TEXT("learn"));
		ASSERT_THAT(IsTrue(Click(View, TEXT("LinkPrivacy"))));
		ASSERT_THAT(AreEqual(Count(TEXT("Account.Privacy")), 1));
		ASSERT_THAT(IsFalse(IsOpen(View)));
		ASSERT_THAT(IsFalse(Shown(View, TEXT("Popup"))));
	}

	TEST_METHOD(TheClosedPopupLayer_DoesNotBlockThePageUnderIt)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SWidget> PageAction = View->FindById(TEXT("PageAction"));
		ASSERT_THAT(IsTrue(PageAction.IsValid()));

		// Real hit-test of the point at the middle of the page button, with the popup and the scrim closed.
		const TArray<TSharedRef<SWidget>> Closed = WidgetsAtCenterOf(View, TEXT("PageAction"));
		ASSERT_THAT(IsFalse(Closed.IsEmpty()));
		ASSERT_THAT(IsTrue(PathHas(Closed, PageAction)));
		ASSERT_THAT(IsFalse(PathHas(Closed, View->FindById(TEXT("Scrim")))));
		ASSERT_THAT(IsFalse(PathHas(Closed, View->FindById(TEXT("Popup")))));

		// And the click arrives: the button takes it and runs its command.
		const TSharedPtr<SSlaterSurface> Button = Surf(View, TEXT("PageAction"));
		const FGeometry Geometry;
		ASSERT_THAT(IsTrue(Button->OnMouseButtonDown(Geometry, CmLeftMouse()).IsEventHandled()));
		ASSERT_THAT(IsTrue(Button->OnMouseButtonUp(Geometry, CmLeftMouse()).IsEventHandled()));
		ASSERT_THAT(AreEqual(Count(TEXT("Page.Action")), 1));
	}

	TEST_METHOD(TheOpenMenu_CoversThePage_SoAnOutsideClickHitsTheScrim)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		Click(View, TEXT("Chevron"));
		// The same point as above is now under the popup frame; the page button is not what the pointer reaches.
		const TArray<TSharedRef<SWidget>> Open = WidgetsAtCenterOf(View, TEXT("PageAction"));
		ASSERT_THAT(IsFalse(Open.IsEmpty()));
		ASSERT_THAT(IsFalse(PathHas(Open, View->FindById(TEXT("PageAction")))));
		ASSERT_THAT(IsTrue(PathHas(Open, View->FindById(TEXT("Popup")))));
		// A point on the page outside the popup (the middle of the footer) reaches the scrim.
		const TArray<TSharedRef<SWidget>> Outside = WidgetsAtCenterOf(View, TEXT("Footer"));
		ASSERT_THAT(IsTrue(PathHas(Outside, View->FindById(TEXT("Scrim")))));
		ASSERT_THAT(IsFalse(PathHas(Outside, View->FindById(TEXT("Popup")))));
	}
};

#endif
