// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SMenuAnchor.h"

class SSlaterPopup;

/**
 * The menu content of a `popup`. The engine's menu stack re-parents the content into its own window, so a row inside
 * it can no longer find the view by walking up; this wrapper remembers the popup, and `Slater::FindOwningView` hops
 * from it to the popup. Holds the layout's menu child and nothing else.
 */
class SLATERUI_API SSlaterPopupMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSlaterPopupMenu) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SSlaterPopup>& InPopup);

	TSharedPtr<SSlaterPopup> GetPopup() const { return Popup.Pin(); }

private:
	TWeakPtr<SSlaterPopup> Popup;
};

/**
 * The `popup` element (T-439): the engine's `SMenuAnchor` with the rules T-416 found. The first child is the anchor
 * (always shown), the second the menu (drawn by the engine's menu stack in the game viewport, outside clicks close it
 * and still reach what is under them, Escape closes it). Placement is `above`, `below` or `right`, with
 * `FitInWindow(true)`. It does not rely on Slate focus on rows.
 *
 * It opens by a click on the anchor (the rule `SComboButton` uses: a press that closed the menu must not reopen it on
 * the release) or by a view state (`open-when="menu=true"`). A close from the engine (outside click, Escape) writes
 * `close-state` back, and a click that opens writes the `open-when` pair, so the state and the menu agree.
 */
class SLATERUI_API SSlaterPopup : public SMenuAnchor
{
public:
	SLATE_BEGIN_ARGS(SSlaterPopup)
		: _Placement(MenuPlacement_BelowAnchor)
	{}
		SLATE_ARGUMENT(EMenuPlacement, Placement)
		/** The menu child. */
		SLATE_ARGUMENT(TSharedPtr<SWidget>, Menu)
		/** The anchor child. */
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EMenuPlacement GetPlacement() const { return Placement; }

	/** The state a click that opens the menu sets (`open-when="menu=true"`), or none. */
	void SetOpenState(FName InKey, const FString& InValue);
	/** The state a close from the engine sets (`close-state="menu=false"`), or none. */
	void SetCloseState(FName InKey, const FString& InValue);

	/** True from the moment something asked for the menu open until something closed it. In a window the engine really shows it (`IsOpen`); without one (a test) only this is set. */
	bool IsOpenRequested() const { return bOpenRequested; }

	/** The view state matches `open-when`: open or close the menu. Opening is skipped right after a press dismissed it. Writes no state. */
	void SetOpenRequested(bool bOpen);

	/** A click on the anchor: closes it if open, else opens it unless this press just closed it. Writes `open-when`. Returns true if it opened. */
	bool ToggleFromClick();

	/** The menu content wrapper, as the engine shows it. */
	TSharedRef<SSlaterPopupMenu> GetMenuRoot() const { return MenuRoot.ToSharedRef(); }

	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** The engine's open-changed callback: a close it reports (outside click, Escape) writes `close-state`. Public so a test can report one without a window. */
	void HandleMenuOpenChanged(bool bOpen);

private:
	/** Closes the popups inside this one's menu (a submenu), each writing its `close-state`, so a state such as `sub=learn` does not outlive the menu that showed it. */
	void CloseNestedPopups();
	void CloseFromParent();
	void WriteState(FName Key, const FString& Value);

	EMenuPlacement Placement = MenuPlacement_BelowAnchor;
	TSharedPtr<SSlaterPopupMenu> MenuRoot;
	FName OpenKey;
	FString OpenValue;
	FName CloseKey;
	FString CloseValue;
	bool bOpenRequested = false;
	/** Set at the press on the anchor when the menu was open or had just been closed, so the release does not reopen it. */
	bool bBlockOpenOnRelease = false;
};
