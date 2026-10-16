// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ISlateMetaData.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class FSlaterEventSink;
class STextBlock;
class FSlaterElementRegistry;

/** One choice of a `combo`: the text the layout and the data use, and the text the user sees. */
struct SLATERUI_API FSlaterComboOption
{
	FString Value;
	FString Label;
};

/** Metadata on the widget an `<option>` builds: how its parent `combo` reads it. */
class SLATERUI_API FSlaterComboOptionMeta : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FSlaterComboOptionMeta, ISlateMetaData)

	FSlaterComboOption Option;
};

/** The look of a `combo` (T-525): the engine's `FComboBoxStyle` filled from style tokens, kept alive with the widget. */
class SLATERUI_API FSlaterComboLook : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FSlaterComboLook, ISlateMetaData)

	TSharedPtr<FComboBoxStyle> Style;
};

/**
 * The `combo` element (T-525): a pick-one list over the engine's `SComboBox`. The options are `<option>` children or the
 * `options` attribute; `selected` holds the value of the chosen option; the user's pick reports the new value through
 * `on-change`. A selected value no option has shows the hint and selects nothing. The menu is the engine's combo menu
 * (`SMenuAnchor`, `FitInWindow(true)`, T-439's finding), so it draws in the game viewport.
 */
class SLATERUI_API SSlaterCombo : public SCompoundWidget
{
public:
	using FOptionPtr = TSharedPtr<FSlaterComboOption>;

	SLATE_BEGIN_ARGS(SSlaterCombo) {}
		SLATE_ARGUMENT(const FComboBoxStyle*, Style)
		SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_ARGUMENT(FLinearColor, TextColor)
		SLATE_ARGUMENT(FLinearColor, MenuTextColor)
		SLATE_ARGUMENT(FString, Hint)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Replaces the options. The selected value stays selected when an option still has it. */
	void SetOptions(const TArray<FSlaterComboOption>& InOptions);
	/** `a|Label A, b|Label B` or `a, b`; entries are trimmed and an entry without `|` uses itself as its label. */
	static bool ParseOptions(const FString& Text, TArray<FSlaterComboOption>& OutOptions, FString& OutError);

	/** Selects the option with this value from code. Fires nothing. */
	void SetSelectedValue(const FString& InValue);
	/** For a polled binding: the value is read every frame. */
	void SetSelectedGetter(TFunction<FString()> InGetter);
	/** For a polled `options` binding: read every frame, the options replaced when the text changes. */
	void SetOptionsGetter(TFunction<FString()> InGetter);

	/** The value of the selected option; empty when none is. */
	FString GetSelectedValue() const;
	const TArray<FOptionPtr>& GetOptions() const { return Options; }
	bool IsMenuOpen() const;
	/** True when the menu was told to flip at the window edge (T-439; the engine keeps the flag to itself). */
	bool FitsInWindow() const { return bFitsInWindow; }
	/** What the closed box shows: the selected option's label, else the hint. */
	FText GetShownText() const;

	/** Picks an option as the user would (the list's selection event): selects it and reports its value. */
	void PickOption(const FString& InValue);

	void SetEventSink(const TSharedRef<FSlaterEventSink>& InSink) { Sink = InSink; }
	const TSharedPtr<SComboBox<FOptionPtr>>& GetComboBox() const { return ComboBox; }

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TArray<FOptionPtr> Options;
	FOptionPtr Selected;
	/** The selected value when it matches no option (yet): kept so a later option list can pick it up. */
	FString PendingValue;
	FString Hint;
	FSlateFontInfo Font;
	FLinearColor TextColor = FLinearColor::Black;
	FLinearColor MenuTextColor = FLinearColor::White;
	TSharedPtr<SComboBox<FOptionPtr>> ComboBox;
	TWeakPtr<FSlaterEventSink> Sink;
	TFunction<FString()> SelectedGetter;
	TFunction<FString()> OptionsGetter;
	FString LastOptionsText;
	bool bSettingFromCode = false;
	bool bFitsInWindow = false;

	TSharedRef<SWidget> MakeRow(FOptionPtr Option) const;
	void HandleSelectionChanged(FOptionPtr Picked, ESelectInfo::Type How);
};

namespace Slater
{
	/** The look of a `combo` widget, or null for any other widget. */
	SLATERUI_API TSharedPtr<FSlaterComboLook> FindComboLook(const SWidget& Widget);

	/** Adds `combo` and `option` (T-525). */
	SLATERUI_API void RegisterComboElements(FSlaterElementRegistry& Registry);
}
