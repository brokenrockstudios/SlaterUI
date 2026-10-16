// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Text/STextBlock.h"

/**
 * The `<text>` widget: an `STextBlock` that can also stop growing after a number of lines (`max-lines`), which the
 * engine's text block has no setting for. It reports at most that many lines of height and clips what does not fit.
 * The other text attributes (overflow, line height, shadow, transform, minimum width) are the engine's own and are set
 * on the base class; this class only remembers the three the base cannot be asked for.
 */
class SLATERUI_API SSlaterText : public STextBlock
{
public:
	/** Lines the text may take before it is cut off; 0 means no limit. */
	void SetMaxLines(int32 InMaxLines);
	int32 GetMaxLines() const { return MaxLines; }

	/** The line height multiplier the text was built with (1 is the font's own). Also used to size `max-lines`. */
	void SetLineHeight(float InLineHeight);
	float GetLineHeight() const { return LineHeight; }

	/** How text too long for the room is cut off. */
	void SetOverflow(ETextOverflowPolicy InOverflow);
	ETextOverflowPolicy GetOverflow() const { return Overflow; }

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	int32 MaxLines = 0;
	float LineHeight = 1.f;
	ETextOverflowPolicy Overflow = ETextOverflowPolicy::Clip;
};
