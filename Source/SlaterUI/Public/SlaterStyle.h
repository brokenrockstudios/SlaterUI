// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterMarkup.h"

/** The kinds of style token. Each has its own namespace: a color and a metric may share a name. */
enum class ESlaterTokenKind : uint8
{
	Color,
	Metric,
	Font,
	Surface,
};

SLATERUI_API const TCHAR* LexToString(ESlaterTokenKind Kind);

/**
 * A font as a role plus a size. Role is a name on the engine's default font style (`Regular`, `Bold`, `Italic`);
 * the Slate `FSlateFontInfo` is made from it when the first element needs one. Plain data, no Slate dependency.
 */
struct FSlaterFont
{
	FName Role = TEXT("Regular");
	int32 Size = 12;
};

/** A procedural rounded box, fully resolved. */
struct FSlaterSurface
{
	FLinearColor Fill = FLinearColor::Transparent;
	FLinearColor Border = FLinearColor::Transparent;
	float BorderWidth = 0.f;
	float CornerRadius = 0.f;

	bool operator==(const FSlaterSurface& Other) const
	{
		return Fill == Other.Fill && Border == Other.Border && BorderWidth == Other.BorderWidth && CornerRadius == Other.CornerRadius;
	}
};

/**
 * Where tokens are looked up. Names are dotted (`UI.Color.Accent.Hover`). A lookup tries the name, then each parent
 * (`UI.Color.Accent`, `UI.Color`, `UI`) and never a child, so a style can define `UI.Color.Accent` once and
 * override only the states that differ.
 */
class SLATERUI_API ISlaterStyleSource
{
public:
	virtual ~ISlaterStyleSource() = default;

	virtual bool FindColor(FName Token, FLinearColor& OutColor) const = 0;
	virtual bool FindMetric(FName Token, float& OutMetric) const = 0;
	virtual bool FindFont(FName Token, FSlaterFont& OutFont) const = 0;
	/** Resolves the surface's color and metric tokens through this same source, so a layer above can retheme it. */
	virtual bool FindSurface(FName Token, FSlaterSurface& OutSurface) const = 0;

	/** Every token this source defines itself (not the fall-back parents), for "did you mean" errors. */
	virtual void GetKnownTokens(ESlaterTokenKind Kind, TArray<FName>& OutTokens) const = 0;
};

namespace Slater
{
	/** The token name with its last segment removed (`A.B.C` gives `A.B`); empty when there is no parent. */
	SLATERUI_API FName ParentToken(FName Token);

	/** True for dotted names whose segments start with a letter or underscore and hold letters, digits and underscores. */
	SLATERUI_API bool IsValidTokenName(const FString& Name);

	/**
	 * The candidate nearest to Name (case-insensitive edit distance), or empty when none is near enough to be a
	 * typo. The registry (T-342) uses it for tags and attributes too.
	 */
	SLATERUI_API FString SuggestNearest(const FString& Name, const TArray<FString>& Candidates);

	/** `SuggestNearest` over a source's tokens of one kind, in the form `; did you mean 'X'?` or empty. */
	SLATERUI_API FString DidYouMean(const ISlaterStyleSource& Source, ESlaterTokenKind Kind, const FString& Name);
}

/** What a `<surface>` says. Colors and metrics are token names, resolved at lookup so layers can override them. */
struct FSlaterSurfaceDef
{
	FName Fill;
	FName Border;
	/** Literal width and radius, used when the matching token is empty. */
	float BorderWidth = 0.f;
	float CornerRadius = 0.f;
	FName BorderWidthToken;
	FName CornerRadiusToken;

	FSlaterPos Pos;
	FSlaterPos FillPos;
	FSlaterPos BorderPos;
	FSlaterPos BorderWidthPos;
	FSlaterPos CornerRadiusPos;
};

/** One named style: the tokens one file defines, plus the style it extends. Lookups walk parent tokens within this style only. */
class SLATERUI_API FSlaterStyle : public ISlaterStyleSource
{
public:
	FName Name;
	/** None when the style stands alone. */
	FName Extends;
	/** Where the `extends` value is, for errors about it. */
	FSlaterPos ExtendsPos;
	/** The file it was read from, a label only. */
	FString Path;

	TMap<FName, FLinearColor> Colors;
	TMap<FName, float> Metrics;
	TMap<FName, FSlaterFont> Fonts;
	TMap<FName, FSlaterSurfaceDef> Surfaces;

	virtual bool FindColor(FName Token, FLinearColor& OutColor) const override;
	virtual bool FindMetric(FName Token, float& OutMetric) const override;
	virtual bool FindFont(FName Token, FSlaterFont& OutFont) const override;
	virtual bool FindSurface(FName Token, FSlaterSurface& OutSurface) const override;
	virtual void GetKnownTokens(ESlaterTokenKind Kind, TArray<FName>& OutTokens) const override;
};

/**
 * Styles layered lowest first. A lookup tries the name in every layer, highest first, then the parent name in every
 * layer, and so on: an exact definition in any layer beats a fall-back to a parent in a higher one. A mod pushes its
 * overrides on top; `extends` is the same thing done by the library.
 */
class SLATERUI_API FSlaterStyleStack : public ISlaterStyleSource
{
public:
	void PushLayer(TSharedRef<const FSlaterStyle> Layer) { Layers.Add(MoveTemp(Layer)); }
	int32 NumLayers() const { return Layers.Num(); }

	virtual bool FindColor(FName Token, FLinearColor& OutColor) const override;
	virtual bool FindMetric(FName Token, float& OutMetric) const override;
	virtual bool FindFont(FName Token, FSlaterFont& OutFont) const override;
	virtual bool FindSurface(FName Token, FSlaterSurface& OutSurface) const override;
	virtual void GetKnownTokens(ESlaterTokenKind Kind, TArray<FName>& OutTokens) const override;

private:
	TArray<TSharedRef<const FSlaterStyle>> Layers;
};

namespace Slater
{
	/** File extension of style files, without the dot. */
	inline const TCHAR* StyleFileExtension() { return TEXT("sltrstyle"); }

	/**
	 * Reads one style file's text into a style. Same parser, error format and caps as layouts. Reports every problem
	 * it finds in the file (not only the first syntax error), each with its line and column; fails if there is any.
	 * References between tokens (a surface naming a color) are checked later, by the library, once `extends` is known.
	 */
	SLATERUI_API bool ParseStyle(const FString& Text, const FString& Path, FSlaterStyle& OutStyle, TArray<FSlaterError>& OutErrors, const FSlaterLimits& Limits = FSlaterLimits());
}

/** All loaded styles by name, with `extends` resolved into stacks. */
class SLATERUI_API FSlaterStyleLibrary
{
public:
	/** Parses and adds one style. A name already taken is an error that names the first file. */
	bool AddFromText(const FString& Text, const FString& Path, TArray<FSlaterError>& OutErrors, const FSlaterLimits& Limits = FSlaterLimits());

	/**
	 * Adds every `*.sltrstyle` file in Directory (not recursive, name order), then validates. A file over the size cap
	 * is refused before it is read. A missing directory is an error.
	 */
	bool LoadDirectory(const FString& Directory, TArray<FSlaterError>& OutErrors, const FSlaterLimits& Limits = FSlaterLimits());

	/** `Content/UI/Styles` of this plugin: the shipped `Base` and `Dusk`. */
	bool LoadPluginStyles(TArray<FSlaterError>& OutErrors);

	/**
	 * Checks what needs every style loaded: `extends` names a loaded style and has no cycle, and each surface names
	 * colors and metrics its stack defines (suggesting the nearest name when not). Run after the last add.
	 */
	bool Validate(TArray<FSlaterError>& OutErrors) const;

	TSharedPtr<const FSlaterStyle> Find(FName Name) const;
	void GetStyleNames(TArray<FName>& OutNames) const;

	/** The style and everything it extends, base first. False (OutStack untouched) for an unknown name or a cycle. */
	bool BuildStack(FName Name, FSlaterStyleStack& OutStack) const;

private:
	TMap<FName, TSharedRef<FSlaterStyle>> Styles;
};
