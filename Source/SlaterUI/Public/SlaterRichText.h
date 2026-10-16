// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterElements.h"
#include "SlaterStyle.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"

class SRichTextBlock;
struct FSlaterRichSnapshot;

namespace Slater
{
	/**
	 * Checks rich text markup (T-527) against the style: `<b>..</>`, `<i>..</>`, `<font value="UI.Font.Heading">..</>`,
	 * `<color value="UI.Color.Accent">..</>` and `<img src="Name"/>`. A run closes with `</>` on the line it opened,
	 * runs do not nest, and every other tag is a problem. A `<` that does not start a tag is plain text.
	 */
	SLATERUI_API void CheckRichText(const FString& Text, const ISlaterStyleSource& Style, TArray<FSlaterTextProblem>& OutProblems);

	/** Adds `rich-text` (SlaterRichText.h, T-527). */
	SLATERUI_API void RegisterRichTextElements(FSlaterElementRegistry& Registry);
}

/**
 * The `rich-text` element (T-527): the engine's `SRichTextBlock` with decorators that read the layout's style tokens.
 * `b` and `i` use the Bold and Italic roles at the body size, `font` and `color` name tokens, `img` names an image of
 * the installed `ISlaterImageSource`. The text may be bound (`{Description}`); bound text is not checked, a tag the
 * decorators do not know shows as written.
 */
class SLATERUI_API SSlaterRichText : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSlaterRichText)
		: _Font()
		, _Color(FLinearColor::White)
		, _Wrap(false)
		, _Justify(ETextJustify::Left)
	{}
		SLATE_ARGUMENT(FSlaterFont, Font)
		SLATE_ARGUMENT(FLinearColor, Color)
		SLATE_ARGUMENT(bool, Wrap)
		SLATE_ARGUMENT(ETextJustify::Type, Justify)
	SLATE_END_ARGS()

	SSlaterRichText();
	virtual ~SSlaterRichText() override;

	void Construct(const FArguments& InArgs);

	/** Shows the markup text, with token names resolved against Style now. */
	void SetRichText(const FString& Text, const ISlaterStyleSource& Style);

	const FString& GetSource() const { return Source; }
	TSharedPtr<SRichTextBlock> GetBlock() const { return Block; }

private:
	FString Source;
	TSharedPtr<SRichTextBlock> Block;
	TSharedPtr<FSlaterRichSnapshot> Snapshot;
};
