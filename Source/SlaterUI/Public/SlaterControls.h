// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Types/ISlateMetaData.h"

class SWidget;

/**
 * The look of a stock control (T-514): the engine's own style struct, filled from the layout's style tokens. A Slate
 * widget only points at its style, so the control keeps the struct alive as widget metadata; only the one that
 * belongs to the control is set. For tests and for code that wants to read what a control was given.
 */
class SLATERUI_API FSlaterControlLook : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FSlaterControlLook, ISlateMetaData)

	TSharedPtr<FSliderStyle> Slider;
	TSharedPtr<FCheckBoxStyle> CheckBox;
	TSharedPtr<FEditableTextBoxStyle> TextBox;
};

namespace Slater
{
	/** The look of a `slider`, `checkbox` or `textbox` widget, or null for any other widget. */
	SLATERUI_API TSharedPtr<FSlaterControlLook> FindControlLook(const SWidget& Widget);
}
