// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterMarkup.h"

class FSlaterElementRegistry;

/**
 * Finds the text of an imported file. Id is what `<import src>` says (a layout ID, `Common` or `Shared/Rows`). Fills
 * OutText and OutLabel (the name errors in that file are reported under) and returns true, or returns false with
 * OutProblem saying why. Tests pass their own; the default reads the layout folders.
 */
using FSlaterImportResolver = TFunction<bool(const FString& Id, FString& OutText, FString& OutLabel, FString& OutProblem)>;

namespace Slater
{
	/** Reads `<Id>.sltr` from the layout folders (the first root that has it), under the file size cap. For `<import>` in a layout. */
	SLATERUI_API FSlaterImportResolver LayoutImportResolver();

	/** True when the root element has a `template` or `import` child, so `ExpandTemplates` has work to do. */
	SLATERUI_API bool HasTemplateDeclarations(const FSlaterNode& Root);

	/**
	 * Replaces every use of a template by a copy of its body, in place, so the layout builder sees plain elements.
	 * Run before `BuildLayout`; `BuildLayout` runs it itself when the root has declarations.
	 *
	 * Declarations are direct children of the root element and are removed from it:
	 *   `<template name="stat-row" params="label value"> ...one element using $label and $value... </template>`
	 *   `<import src="Common"/>`  (the file's root is `<templates>`, holding `<template>`s and `<import>`s)
	 * A use is an element whose tag is the template name; its attributes are the arguments (all required), plus the
	 * slot attributes `fill margin halign valign` and `id`, which land on the body's root element. A template may use
	 * another; a template that uses itself, an import cycle, an unknown or missing parameter, a name that is already a
	 * built-in element or another template, and `$name` in a body that is not a parameter are errors at their line
	 * (an element of an imported template is reported under that file's label). A `$` is written `$$`.
	 *
	 * Returns false with every problem in OutErrors; Root is then not usable. Path labels the root file.
	 */
	SLATERUI_API bool ExpandTemplates(FSlaterNode& Root, const FString& Path, const FSlaterElementRegistry& Registry, const FSlaterImportResolver& Resolver, TArray<FSlaterError>& OutErrors, const FSlaterLimits& Limits = FSlaterLimits());
}
