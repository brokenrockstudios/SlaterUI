// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBorder.h"

class SSlaterView;

/**
 * A procedural rounded box (fill, outline, corner radius) around one child. The brush is owned here, so the
 * surface can change at runtime without a texture or a material.
 */
class SLATERUI_API SSlaterSurface : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SSlaterSurface)
		: _Surface()
		, _Inset(0.f)
	{}
		SLATE_ARGUMENT(FSlaterSurface, Surface)
		/** Space between the outline and the content. */
		SLATE_ARGUMENT(FMargin, Inset)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The command a click runs (`on-click`, with the word after its colon as Argument), or none. A surface with a command takes clicks. */
	void SetCommand(FName InCommand, const FString& InArgument = FString());
	FName GetCommand() const { return Command; }
	const FString& GetCommandArgument() const { return CommandArgument; }

	/** The view state a click sets (`set-state="tab=map"`), or none. A surface with one takes clicks too. */
	void SetStateChange(FName InKey, const FString& InValue);
	FName GetStateKey() const { return StateKey; }
	const FString& GetStateValue() const { return StateValue; }
	/** True when a click does anything: it has a command or a state change. */
	bool HasAction() const { return !Command.IsNone() || !StateKey.IsNone(); }

	/**
	 * The view state the pointer sets on entering (`hover-set-state="sub=learn"`) and on leaving (`leave-set-state="sub=none"`)
	 * the surface, or none. Instant, and independent of `interactive`; either one makes the surface take pointer events.
	 * Slate sends a parent its leave only when the pointer has left the parent and all of its children.
	 */
	void SetHoverStateChange(FName InKey, const FString& InValue);
	void SetLeaveStateChange(FName InKey, const FString& InValue);
	FName GetHoverStateKey() const { return HoverStateKey; }
	const FString& GetHoverStateValue() const { return HoverStateValue; }
	FName GetLeaveStateKey() const { return LeaveStateKey; }
	const FString& GetLeaveStateValue() const { return LeaveStateValue; }

	/**
	 * Does what a click does: sets the state, then runs the command in the command table of the `SSlaterView` this surface
	 * is in, then the process-wide one. False when there is nothing to do, the surface is disabled, or a part could not
	 * run (no handler, a state the layout does not declare, no view for a state; those are logged).
	 */
	bool Activate();

	/**
	 * Makes the surface follow the pointer (`interactive`): the hover surface while the pointer is over it, the pressed
	 * one while the left button is down over it, the surface set by `SetSurface` otherwise. A disabled widget does not
	 * react. The surface takes pointer events (hit-testable) even when no command is set.
	 */
	void SetInteractive(const FSlaterSurface& InHover, const FSlaterSurface& InPressed);
	bool IsInteractive() const { return bInteractive; }

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Redraws with another surface (for example a hover variant). For an interactive surface this is the resting surface; the pointer state still wins while it applies. */
	void SetSurface(const FSlaterSurface& InSurface);
	/** The surface drawn now (the resting one, or the hover or pressed one for an interactive surface). */
	const FSlaterSurface& GetSurface() const { return Surface; }

	/** The style role this surface was made from (`UI.Surface.Button`), so a state can look up `<role>.Hover` later. */
	void SetRole(FName InRole) { Role = InRole; }
	FName GetRole() const { return Role; }

	/** The brush a surface is drawn with. Exposed for tests. */
	static FSlateBrush MakeBrush(const FSlaterSurface& Surface);

	/** One brush per distinct set of surface values, shared by the surfaces that use it (a grid of identical cells makes one). Game thread only; keeps the most recent 64. */
	static TSharedRef<const FSlateBrush> BrushFor(const FSlaterSurface& Surface);

private:
	/** Draws the surface the pointer state selects. */
	void Refresh();
	/** The `SSlaterView` this surface is in, or null. */
	TSharedPtr<SSlaterView> FindView() const;
	/** Sets a pointer-driven state in the view; logs and does nothing outside one. */
	void ApplyPointerState(FName Key, const FString& Value, const TCHAR* Attribute) const;

	FSlaterSurface Surface;
	FSlaterSurface RestingSurface;
	FSlaterSurface HoverSurface;
	FSlaterSurface PressedSurface;
	bool bInteractive = false;
	bool bHovered = false;
	bool bPressed = false;
	FName Role;
	FName Command;
	FString CommandArgument;
	FName StateKey;
	FString StateValue;
	FName HoverStateKey;
	FString HoverStateValue;
	FName LeaveStateKey;
	FString LeaveStateValue;
	/** Shared with every surface of the same values (see `BrushFor`); the border draws this one. */
	TSharedPtr<const FSlateBrush> Brush;
};
