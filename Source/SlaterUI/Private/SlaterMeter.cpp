// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterMeter.h"

#include "Rendering/DrawElements.h"

namespace
{
	FSlateBrush MakeBar(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.ImageType = ESlateBrushImageType::NoImage;
		Brush.TintColor = Color;
		// A fully rounded bar: the radius is clamped to half the height when drawn.
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(1.f, 1.f, 1.f, 1.f), FLinearColor::Transparent, 0.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Brush;
	}
}

void SSlaterMeter::Construct(const FArguments& InArgs)
{
	Height = InArgs._Height;
	TrackBrush = MakeBar(InArgs._TrackColor);
	FillBrush = MakeBar(InArgs._FillColor);
	SetValue(InArgs._Value);
	SetCanTick(false);
}

void SSlaterMeter::SetValue(float InValue)
{
	// NaN fails both comparisons of Clamp, so it is handled first.
	const float Clamped = FMath::IsNaN(InValue) ? 0.f : FMath::Clamp(InValue, 0.f, 1.f);
	if (Clamped != Value)
	{
		Value = Clamped;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FVector2D SSlaterMeter::ComputeDesiredSize(float) const
{
	return FVector2D(0.f, Height);
}

int32 SSlaterMeter::OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const ESlateDrawEffect Effects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FVector2f Size = AllottedGeometry.GetLocalSize();
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &TrackBrush, Effects, TrackBrush.TintColor.GetSpecifiedColor() * InWidgetStyle.GetColorAndOpacityTint());
	if (Value > 0.f)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Size.X * Value, Size.Y), FSlateLayoutTransform()), &FillBrush, Effects, FillBrush.TintColor.GetSpecifiedColor() * InWidgetStyle.GetColorAndOpacityTint());
	}
	return LayerId + 1;
}
