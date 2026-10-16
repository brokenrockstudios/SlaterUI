// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterStyle.h"
#include "Widgets/Input/SButton.h"

class SSlaterSurface;

/** Which of its looks a `button` is drawing now. */
enum class ESlaterButtonLook : uint8
{
	Rest,
	Hover,
	Pressed,
	/** Keyboard or gamepad focus (not a click's focus). */
	Focused,
	Disabled,
};

/** The surface of a `button` in each look, resolved from its role (`<role>.Hover`, `.Pressed`, `.Focused`, `.Disabled`). */
struct FSlaterButtonLooks
{
	FSlaterSurface Rest;
	FSlaterSurface Hover;
	FSlaterSurface Pressed;
	FSlaterSurface Focused;
	FSlaterSurface Disabled;
};

/**
 * The `button` element (T-524): the engine's `SButton` for its input (pointer, Enter / Space / the confirm key through
 * the navigation config, keyboard focus and navigation) with no look of its own, around one `SSlaterSurface` face that
 * draws the role's surface for the current look. A click and a confirm key both run the same action as `surface on-click`
 * (`set-state`, then the command).
 */
class SLATERUI_API SSlaterButton : public SButton
{
public:
	SLATE_BEGIN_ARGS(SSlaterButton)
		: _Looks()
		, _Inset(0.f)
	{}
		SLATE_ARGUMENT(FSlaterButtonLooks, Looks)
		/** Space between the outline and the content. */
		SLATE_ARGUMENT(FMargin, Inset)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The command a click or confirm key runs (`on-click`, the word after its colon as Argument), or none. */
	void SetCommand(FName InCommand, const FString& InArgument = FString());
	/** The view state a click or confirm key sets (`set-state="tab=map"`), or none. */
	void SetStateChange(FName InKey, const FString& InValue);
	bool HasAction() const { return !Command.IsNone() || !StateKey.IsNone(); }

	/** Does what a click does. False when there is nothing to do, the button is disabled, or a part could not run. */
	bool Activate();

	/** The look being drawn. Disabled wins, then pressed, then focused, then hover. */
	ESlaterButtonLook GetLook() const { return Look; }
	/** The surface face the looks are drawn on. */
	TSharedRef<SSlaterSurface> GetFace() const { return Face.ToSharedRef(); }
	bool HasNavigationFocus() const { return bFocused; }
	/** Picks the look from the current state (enabled, pressed, focused, hovered) and draws it. Events and the tick call it; tests call it after changing `enabled`. */
	void UpdateLook();

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:
	virtual void Press() override;
	virtual void Release() override;

private:
	FReply HandleClicked();

	FSlaterButtonLooks Looks;
	TSharedPtr<SSlaterSurface> Face;
	ESlaterButtonLook Look = ESlaterButtonLook::Rest;
	FName Command;
	FString CommandArgument;
	FName StateKey;
	FString StateValue;
	bool bHovered = false;
	bool bFocused = false;
};
