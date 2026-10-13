// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterPreviewTab.h"

#include "SlaterView.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SlaterPreview"

void SSlaterPreview::Construct(const FArguments& InArgs)
{
	OnRescan = InArgs._OnRescan;
	RescanOptions();
	UpdateStyleErrors();
	ContentChangedHandle = Slater::OnContentChanged().AddSP(this, &SSlaterPreview::UpdateStyleErrors);

	auto MakeOptionWidget = [](FOption Option) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock).Text(FText::FromString(*Option));
	};

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("Layout", "Layout"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 12.f, 0.f)
			[
				SAssignNew(LayoutCombo, SComboBox<FOption>)
				.OptionsSource(&LayoutOptions)
				.OnGenerateWidget_Lambda(MakeOptionWidget)
				.OnSelectionChanged_Lambda([this](FOption Selected, ESelectInfo::Type)
				{
					if (Selected.IsValid() && View.IsValid() && View->GetLayoutId() != *Selected)
					{
						View->SetLayout(*Selected);
					}
				})
				[
					SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(View.IsValid() ? View->GetLayoutId() : FString()); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("Style", "Style"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 12.f, 0.f)
			[
				SAssignNew(StyleCombo, SComboBox<FOption>)
				.OptionsSource(&StyleOptions)
				.OnGenerateWidget_Lambda(MakeOptionWidget)
				.OnSelectionChanged_Lambda([this](FOption Selected, ESelectInfo::Type)
				{
					if (Selected.IsValid() && View.IsValid() && View->GetStyleName() != FName(**Selected))
					{
						View->SetStyle(FName(**Selected));
					}
				})
				[
					SNew(STextBlock).Text_Lambda([this]() { return FText::FromName(View.IsValid() ? View->GetStyleName() : NAME_None); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Rescan", "Rescan"))
				.ToolTipText(LOCTEXT("RescanTip", "Look for new layout and style files and refresh the view."))
				.OnClicked_Lambda([this]()
				{
					OnRescan.ExecuteIfBound();
					RescanOptions();
					LayoutCombo->RefreshOptions();
					StyleCombo->RefreshOptions();
					if (View.IsValid())
					{
						View->Refresh();
					}
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.45f, 0.05f, 0.05f))
			.Padding(6.f)
			.Visibility_Lambda([this]() { return StyleErrors.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FLinearColor::White)
				.Text_Lambda([this]() { return StyleErrors; })
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(6.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(View, SSlaterView)
				.UseMockContext(true)
			]
		]
	];
}

SSlaterPreview::~SSlaterPreview()
{
	Slater::OnContentChanged().Remove(ContentChangedHandle);
}

void SSlaterPreview::UpdateStyleErrors()
{
	const TArray<FSlaterError>& Errors = Slater::GetStyleReloadErrors();
	if (Errors.IsEmpty())
	{
		StyleErrors = FText::GetEmpty();
		return;
	}
	TArray<FString> Lines;
	for (const FSlaterError& Error : Errors)
	{
		Lines.Add(Error.ToString());
	}
	StyleErrors = FText::Format(LOCTEXT("StyleRefused", "Style file refused, the colors in use stay:\n{0}"), FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void SSlaterPreview::SetLayout(const FString& LayoutId)
{
	if (!View.IsValid())
	{
		return;
	}
	RescanOptions();
	LayoutCombo->RefreshOptions();
	View->SetLayout(LayoutId);
	if (const FOption Option = FindOption(LayoutOptions, LayoutId))
	{
		LayoutCombo->SetSelectedItem(Option);
	}
}

void SSlaterPreview::RescanOptions()
{
	LayoutOptions.Reset();
	TArray<FString> Ids;
	Slater::GetLayoutIds(Ids);
	for (const FString& Id : Ids)
	{
		LayoutOptions.Add(MakeShared<FString>(Id));
	}
	StyleOptions.Reset();
	TArray<FName> Names;
	Slater::StyleLibrary().GetStyleNames(Names);
	Names.Sort(FNameLexicalLess());
	for (const FName Name : Names)
	{
		StyleOptions.Add(MakeShared<FString>(Name.ToString()));
	}
}

SSlaterPreview::FOption SSlaterPreview::FindOption(const TArray<FOption>& Options, const FString& Value) const
{
	for (const FOption& Option : Options)
	{
		if (Option.IsValid() && *Option == Value)
		{
			return Option;
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
