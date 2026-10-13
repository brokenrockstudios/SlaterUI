// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** A place in a markup file. Both are 1-based; the column counts characters from the start of the line. */
struct FSlaterPos
{
	int32 Line = 0;
	int32 Column = 0;
	/** The file label when the place is not in the file being built: an element of an imported template (T-346). None means the file being built. */
	FName File;
};

struct FSlaterAttribute
{
	FString Name;
	/** Entities already decoded. */
	FString Value;
	/** Where the name starts. */
	FSlaterPos Pos;
};

/** One element of a `.sltr` file. Plain data: the parser knows nothing about Slate, themes or which tags exist. */
struct SLATERUI_API FSlaterNode
{
	FString Tag;
	/** In file order. Names are unique within the element. */
	TArray<FSlaterAttribute> Attributes;
	TArray<FSlaterNode> Children;
	/**
	 * Character data written directly inside this element (not in its children), entities decoded. Each run between
	 * tags is trimmed, empty runs are dropped, and the rest are joined with one space.
	 */
	FString Text;
	/** Where the `<` of the opening tag is. */
	FSlaterPos Pos;
	/** Where the first character of Text is; zero when there is no text. */
	FSlaterPos TextPos;

	/** The attribute with exactly this name (case-sensitive), or null. */
	const FSlaterAttribute* FindAttribute(const FString& Name) const;
};

/** What went wrong and where. The format is `Path(Line,Column): Message`, one format for every layout error. */
struct SLATERUI_API FSlaterError
{
	/** The file name or other label given to the parser. */
	FString Path;
	int32 Line = 0;
	int32 Column = 0;
	FString Message;

	FString ToString() const;
};

/** Caps that keep a hostile or broken file from costing much. A file over any of them fails like a syntax error. */
struct SLATERUI_API FSlaterLimits
{
	/** Characters in the whole file. */
	int32 MaxChars = 256 * 1024;
	/** Elements open at once; the root is depth 1. */
	int32 MaxDepth = 32;
	/** Elements in the file. */
	int32 MaxNodes = 4096;
};

namespace Slater
{
	/**
	 * Parses a `.sltr` file into a node tree. Strict XML subset: one root element; elements, quoted attributes, text,
	 * comments, an optional `<?xml ?>` declaration at the start; the entities `&amp; &lt; &gt; &quot; &apos;` and
	 * numeric references. DOCTYPE, CDATA and other processing instructions are refused.
	 *
	 * Fails (and leaves OutRoot empty) on a duplicate attribute, an unquoted or valueless attribute, a mismatched or
	 * unclosed tag, an unknown entity, a second root, text outside the root, a control character, or a cap in
	 * Limits. OutError then says where. Path is only a label for the error text (for example `Layouts/Inventory.sltr`).
	 */
	SLATERUI_API bool ParseMarkup(const FString& Text, const FString& Path, FSlaterNode& OutRoot, FSlaterError& OutError, const FSlaterLimits& Limits = FSlaterLimits());
}
