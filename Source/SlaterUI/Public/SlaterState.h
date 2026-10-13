// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterMarkup.h"

class SWidget;

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
};

namespace Slater
{
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
