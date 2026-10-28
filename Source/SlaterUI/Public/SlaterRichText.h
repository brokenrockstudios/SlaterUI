// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterElements.h"
#include "SlaterStyle.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"

class SRichTextBlock;
struct FSlaterRichSnapshot;

/** A styled run the decorators made for the current text, for tests and probes. Plain stretches are not listed. */
struct FSlaterRichRun
{
	/** `b`, `i`, `font`, `color` or `img`. */
	FString Tag;
	/** The words of the run; empty for an image. */
	FString Text;
	FName FontRole;
	int32 FontSize = 0;
	/** The run's text color; for `img`, its `tint` (white when it has none and keeps its own colors). */
	FLinearColor Color = FLinearColor::White;
	/** For `img`: the name written in `src` and whether the image source had it. */
	FString Image;
	bool bImageFound = false;
	/** For `img` (T-544): the size it is laid out at, and how far its bottom sits below the text baseline (negative = above), in Slate units. */
	FVector2D ImageSize = FVector2D::ZeroVector;
	float ImageBaseline = 0.f;
};

namespace Slater
{
	/**
	 * Checks rich text markup (T-527) against the style: `<b>..</>`, `<i>..</>`, `<font value="UI.Font.Heading">..</>`,
	 * `<color value="UI.Color.Accent">..</>` and `<img src="Name" size="16" baseline="3" tint="UI.Color.Text"/>` (`size`,
	 * `baseline` and `tint` optional, T-544). A run closes with `</>` on the line it opened, runs do not nest, and every other tag is a problem. A `<` that
	 * does not start a tag is plain text.
	 */
	SLATERUI_API void CheckRichText(const FString& Text, const ISlaterStyleSource& Style, TArray<FSlaterTextProblem>& OutProblems);

	/** Adds `rich-text` (SlaterRichText.h, T-527). */
	SLATERUI_API void RegisterRichTextElements(FSlaterElementRegistry& Registry);

	/** How `rich-text` reveals its text (T-545): not at all, by character or by word. */
	enum class ERichReveal : uint8
	{
		None,
		Char,
		Word,
	};

	/**
	 * How many reveal steps the text has (T-545): visible characters (an `&lt;` entity and an `img` count as one) or words (an
	 * `img` is a word). Tags are not steps.
	 */
	SLATERUI_API int32 RichRevealTotal(const FString& Text, ERichReveal Mode);

	/**
	 * The markup that shows the first Count steps (T-545): the text cut after the Count-th character or word. A cut inside a
	 * run closes it with `</>`, so a tag never shows as text. Count at or above the total returns the whole text.
	 */
	SLATERUI_API FString RichRevealPrefix(const FString& Text, ERichReveal Mode, int32 Count);
}

/**
 * The `rich-text` element (T-527): the engine's `SRichTextBlock` with decorators that read the layout's style tokens.
 * `b` and `i` use the Bold and Italic roles at the body size, `font` and `color` name tokens, `img` names an image of
 * the installed `ISlaterImageSource`. An image is as tall as the font's line and centered on it unless `size` (its height,
 * the width keeps the brush's aspect) or `baseline` (how far its bottom sits below the text baseline) say otherwise.
 * The text may be bound (`{Description}`); bound text is not checked, a tag the decorators do not know shows as written.
 */
class SLATERUI_API SSlaterRichText : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSlaterRichText)
		: _Font()
		, _Color(FLinearColor::White)
		, _Wrap(false)
		, _Justify(ETextJustify::Left)
		, _Reveal(Slater::ERichReveal::None)
		, _RevealSpeed(0.f)
	{}
		SLATE_ARGUMENT(FSlaterFont, Font)
		SLATE_ARGUMENT(FLinearColor, Color)
		SLATE_ARGUMENT(bool, Wrap)
		SLATE_ARGUMENT(ETextJustify::Type, Justify)
		/** Typewriter reveal (T-545): characters or words appear at RevealSpeed steps per second. */
		SLATE_ARGUMENT(Slater::ERichReveal, Reveal)
		SLATE_ARGUMENT(float, RevealSpeed)
	SLATE_END_ARGS()

	SSlaterRichText();
	virtual ~SSlaterRichText() override;

	void Construct(const FArguments& InArgs);

	/** Shows the markup text, with token names resolved against Style now. */
	void SetRichText(const FString& Text, const ISlaterStyleSource& Style);

	const FString& GetSource() const { return Source; }
	TSharedPtr<SRichTextBlock> GetBlock() const { return Block; }

	/** The styled runs of the text as laid out. The layout happens on the first size query or paint, so call `SlatePrepass` first. */
	const TArray<FSlaterRichRun>& GetRuns() const;

	/**
	 * Moves the reveal on by DeltaTime seconds (T-545). The active timer calls it; a test calls it with a manual clock.
	 * Shows everything at once when `Slater.MotionScale` is 0. True while text is still to appear.
	 */
	bool AdvanceReveal(float DeltaTime);

	/** The steps shown so far and the steps in all (characters or words by the mode); equal when nothing is left to reveal. */
	int32 GetRevealedCount() const;
	int32 GetRevealTotal() const { return RevealTotal; }
	/** The markup the block shows right now: the source, or its prefix while revealing. */
	const FString& GetShownMarkup() const { return Shown; }
	/** True while the active timer runs, i.e. while text is still to appear. */
	bool IsRevealing() const { return RevealTimer.IsValid(); }

private:
	EActiveTimerReturnType OnRevealTimer(double InCurrentTime, float InDeltaTime);
	void ShowRevealed();
	void StopRevealTimer();

	FString Source;
	FString Shown;
	bool bHasText = false;
	Slater::ERichReveal RevealMode = Slater::ERichReveal::None;
	float RevealSpeed = 0.f;
	/** Steps revealed, fractional between ticks. */
	double Revealed = 0.0;
	int32 RevealTotal = 0;
	int32 ShownCount = -1;
	TSharedPtr<FActiveTimerHandle> RevealTimer;
	TSharedPtr<SRichTextBlock> Block;
	TSharedPtr<FSlaterRichSnapshot> Snapshot;
};
