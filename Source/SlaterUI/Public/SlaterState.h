// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterMarkup.h"
#include "SlaterMotion.h"
#include "SlaterStyle.h"

class SWidget;
class SBox;

/** A `<state name="tab" default="gear"/>` declared as a direct child of a layout's root: a view-local text value (T-353). */
struct FSlaterStateDecl
{
	FName Name;
	FString Default;
	FSlaterPos Pos;
};

/** An element with `visible-when="tab=gear"`: shown while the state `Key` equals `Value`, collapsed otherwise. */
struct FSlaterStateCondition
{
	TWeakPtr<SWidget> Widget;
	FName Key;
	FString Value;
	/** True for a `popup` with `open-when`: the widget is the popup, which opens while the state matches instead of being shown. */
	bool bPopup = false;
};

/** An element with `width-by="expanded: true=240, false=UI.Size.Rail"` (or `height-by`): the size of its sizing box follows the state `Key` (T-405). */
struct FSlaterStateSize
{
	TWeakPtr<SBox> Box;
	FName Key;
	bool bHeight = false;
	/** One entry per written case: the state value and the resolved size. Same length. */
	TArray<FString> Values;
	TArray<float> Sizes;
	/** `transition="UI.Motion.Panel"` (T-406): the size moves instead of jumping. Needs `Anim`. */
	bool bAnimate = false;
	FSlaterMotion Motion;
	/** Made per built instance; shared by the copies of this struct. Null unless `bAnimate`. */
	TSharedPtr<FSlaterAnimator> Anim;
};

namespace Slater
{
	/** One case of a `width-by` / `height-by`: a state value and the metric text (a number or a metric token) written for it. */
	struct FSizeByCase
	{
		FString Value;
		FString Metric;
	};

	/** Splits `key: value=metric, value=metric`. Returns what is wrong with the text, or empty (and fills the outputs) when it is fine. */
	SLATERUI_API FString ParseSizeBy(const FString& Text, const TCHAR* AttributeName, FString& OutKey, TArray<FSizeByCase>& OutCases);

	/**
	 * Sets the box's width or height for the state's current value (null: the state is unknown); a value with no case gives the box back its content size.
	 * With bAnimate and a `transition` the size moves there from where it is (and `Slater.MotionScale` 0 makes it jump); the first apply, a value with no case
	 * on either side, and bAnimate false all snap. The view then feeds time through `AdvanceStateSize`.
	 */
	SLATERUI_API void ApplyStateSize(const FSlaterStateSize& Size, const FString* StateValue, bool bAnimate = false);

	/** Moves the size's animator on by DeltaTime and writes the box. True while it is still moving. */
	SLATERUI_API bool AdvanceStateSize(const FSlaterStateSize& Size, float DeltaTime);

	/** A state name: letters, digits and `_`, not starting with a digit. */
	SLATERUI_API bool IsValidStateName(const FString& Name);

	/** A state value: letters, digits and `_ . -`, at least one character. Strings only, compared for equality. */
	SLATERUI_API bool IsValidStateValue(const FString& Value);

	/** Splits `key=value`; false when it is not one name, one `=` and one value. */
	SLATERUI_API bool ParseStatePair(const FString& Text, FString& OutKey, FString& OutValue);

	/** What is wrong with a `key=value` attribute text (for `set-state` and `visible-when`), or empty when it is fine. */
	SLATERUI_API FString CheckStatePair(const FString& Text, const TCHAR* AttributeName);

	/**
	 * Takes the `<state>` children out of Root and returns them in OutStates. Reports a missing or bad `name` or `default`,
	 * a duplicate name, an attribute it does not know, and content inside a `<state>`. Errors have the usual file, line and column.
	 */
	SLATERUI_API void ExtractStates(FSlaterNode& Root, const FString& Path, TArray<FSlaterStateDecl>& OutStates, TArray<FSlaterError>& OutErrors);

	/**
	 * Walks the layout and reports a `<state>` that is not a direct child of the root, and every `set-state`, `visible-when`
	 * and `on-click="ui.toggle:key"` that names a state States does not declare (with the declared names, so a typo is a copy away).
	 */
	SLATERUI_API void CheckStateReferences(const FSlaterNode& Root, const FString& Path, const TArray<FSlaterStateDecl>& States, TArray<FSlaterError>& OutErrors);
}
