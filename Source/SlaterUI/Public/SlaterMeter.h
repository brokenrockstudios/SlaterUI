// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

/**
 * A bar filled from the left to a fraction (`<meter>`): a rounded track with a rounded fill on top. The value is clamped
 * to 0..1 when set. Both parts are procedural rounded boxes, so it needs no texture or material.
 */
class SLATERUI_API SSlaterMeter : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSlaterMeter)
		: _Value(0.f)
		, _FillColor(FLinearColor::White)
		, _TrackColor(FLinearColor::Black)
		, _Height(8.f)
	{}
		SLATE_ARGUMENT(float, Value)
		SLATE_ARGUMENT(FLinearColor, FillColor)
		SLATE_ARGUMENT(FLinearColor, TrackColor)
		/** Desired height; a `height` on the element wraps the meter in a box that overrides it. */
		SLATE_ARGUMENT(float, Height)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Sets the fraction, clamped to 0..1 (NaN counts as 0). */
	void SetValue(float InValue);
	float GetValue() const { return Value; }
	FLinearColor GetFillColor() const { return FillBrush.TintColor.GetSpecifiedColor(); }
	FLinearColor GetTrackColor() const { return TrackBrush.TintColor.GetSpecifiedColor(); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	float Value = 0.f;
	float Height = 8.f;
	FSlateBrush TrackBrush;
	FSlateBrush FillBrush;
};
