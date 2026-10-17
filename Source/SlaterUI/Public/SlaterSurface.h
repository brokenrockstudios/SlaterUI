// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBorder.h"

class SSlaterView;

/** The size of the diagonal cut at each corner of a chamfered surface (`shape="chamfer"`), in slate units. 0 leaves the corner square. */
struct FSlaterChamfer
{
	float TopLeft = 0.f;
	float TopRight = 0.f;
	float BottomRight = 0.f;
	float BottomLeft = 0.f;

	bool IsSet() const { return TopLeft > 0.f || TopRight > 0.f || BottomRight > 0.f || BottomLeft > 0.f; }
	bool operator==(const FSlaterChamfer& Other) const
	{
		return TopLeft == Other.TopLeft && TopRight == Other.TopRight && BottomRight == Other.BottomRight && BottomLeft == Other.BottomLeft;
	}
};

namespace Slater
{
	/**
	 * The outline of a chamfered box of this size, clockwise from the top left, as 4 to 8 points (a corner with no cut is
	 * one point, a cut corner is two). `Inset` moves the outline inwards by that distance on every side, which is how the
	 * line of a border of that width is centered inside the box. Cuts are scaled down together when two of them would not
	 * fit along an edge.
	 */
	SLATERUI_API void MakeChamferPath(const FVector2f& Size, const FSlaterChamfer& Cuts, float Inset, TArray<FVector2f>& OutPoints);

	/**
	 * What a click does, shared by `surface` and `button`: sets the view state (when `StateKey` is set), then runs the
	 * command in the table of the `SSlaterView` the source is in, then the process-wide one. False when a part could not
	 * run (no handler, a state the layout does not declare, no view for a state; those are logged). The command may
	 * rebuild the tree the source is in, so nothing of the source is touched after it runs.
	 */
	SLATERUI_API bool ActivateAction(const TSharedRef<SWidget>& Source, FName StateKey, const FString& StateValue, FName Command, const FString& CommandArgument);
}

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

	/**
	 * Cuts the corners (`shape="chamfer"`): the fill is a polygon and the outline a line, both drawn in `OnPaint`, and the
	 * rounded-box brush is not used (the corner radius of the surface is ignored). Hit testing stays the whole rectangle.
	 * No cut at all (`!IsSet()`) goes back to the rounded box.
	 */
	void SetChamfer(const FSlaterChamfer& InCuts);
	const FSlaterChamfer& GetChamfer() const { return Chamfer; }
	bool IsChamfered() const { return Chamfer.IsSet(); }

	/**
	 * A fake drop shadow (`shadow="N"`, `shadow-color`): a copy of the surface's shape in `InColor`, moved (+N, +N) slate
	 * units and painted under the fill, on the surface's own layer. Paint only: desired size, geometry, hit testing and snapping
	 * do not change, and a clipping parent cuts it. An offset of 0 or less turns it off.
	 */
	void SetShadow(float InOffset, const FLinearColor& InColor = FLinearColor(0.f, 0.f, 0.f, 0.5f));
	bool HasShadow() const { return ShadowOffset > 0.f; }
	float GetShadowOffset() const { return ShadowOffset; }
	const FLinearColor& GetShadowColor() const { return ShadowColor; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

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
	/** Rebuilds `ShadowBrush` for the current surface and shadow color. */
	void UpdateShadowBrush();
	/** Sets a pointer-driven state in the view; logs and does nothing outside one. */
	void ApplyPointerState(FName Key, const FString& Value, const TCHAR* Attribute) const;

	FSlaterSurface Surface;
	FSlaterSurface RestingSurface;
	FSlaterSurface HoverSurface;
	FSlaterSurface PressedSurface;
	FSlaterChamfer Chamfer;
	float ShadowOffset = 0.f;
	FLinearColor ShadowColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);
	/** The shadow of a rounded box: the current surface's radius, filled with the shadow color, no outline. */
	TSharedPtr<const FSlateBrush> ShadowBrush;
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
