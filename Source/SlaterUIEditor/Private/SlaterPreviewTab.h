// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SSlaterView;
template <typename OptionType> class SComboBox;

/**
 * Content of the preview tab: a layout picker, a style picker and the view. The view refreshes itself when a file on
 * disk changes (`Slater::OnContentChanged`), so editing a `.sltr` with this tab open changes it with no restart.
 */
class SSlaterPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSlaterPreview) {}
		/** Called by the Rescan button before the view refreshes: watch folders that appeared since startup. */
		SLATE_EVENT(FSimpleDelegate, OnRescan)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SSlaterPreview() override;

	/** Shows this layout (an unknown ID shows the view's error text). */
	void SetLayout(const FString& LayoutId);

private:
	using FOption = TSharedPtr<FString>;

	FSimpleDelegate OnRescan;
	FDelegateHandle ContentChangedHandle;
	/** Why the last style reload was refused (file, line, column); empty when it worked. Shown as a banner. */
	FText StyleErrors;
	TSharedPtr<SSlaterView> View;
	TSharedPtr<SComboBox<FOption>> LayoutCombo;
	TSharedPtr<SComboBox<FOption>> StyleCombo;
	TArray<FOption> LayoutOptions;
	TArray<FOption> StyleOptions;

	void RescanOptions();
	void UpdateStyleErrors();
	FOption FindOption(const TArray<FOption>& Options, const FString& Value) const;
};
