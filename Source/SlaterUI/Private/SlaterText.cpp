// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterText.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

void SSlaterText::SetMaxLines(int32 InMaxLines)
{
	MaxLines = FMath::Max(InMaxLines, 0);
	Invalidate(EInvalidateWidgetReason::Layout);
	SetClipping(MaxLines > 0 ? EWidgetClipping::ClipToBounds : EWidgetClipping::Inherit);
}

void SSlaterText::SetLineHeight(float InLineHeight)
{
	LineHeight = FMath::Max(InLineHeight, 0.f);
	SetLineHeightPercentage(LineHeight);
}

void SSlaterText::SetOverflow(ETextOverflowPolicy InOverflow)
{
	Overflow = InOverflow;
	SetOverflowPolicy(InOverflow);
}

FVector2D SSlaterText::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	FVector2D Size = STextBlock::ComputeDesiredSize(LayoutScaleMultiplier);
	if (MaxLines > 0 && FSlateApplication::IsInitialized())
	{
		FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
		if (Renderer)
		{
			const float FontHeight = Renderer->GetFontMeasureService()->GetMaxCharacterHeight(GetFont(), LayoutScaleMultiplier);
			Size.Y = FMath::Min(Size.Y, FontHeight * LineHeight * MaxLines);
		}
	}
	return Size;
}
