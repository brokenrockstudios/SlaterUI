// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterImages.h"
#include "SlaterView.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** The text of every visible STextBlock under Widget, in tree order; a collapsed widget hides its subtree. */
	void WgCollectVisibleTexts(SWidget& Widget, TArray<FString>& Out)
	{
		if (Widget.GetVisibility() == EVisibility::Collapsed)
		{
			return;
		}
		if (Widget.GetType() == FName(TEXT("STextBlock")))
		{
			Out.Add(static_cast<STextBlock&>(Widget).GetText().ToString());
		}
		Widget.GetChildren()->ForEachWidget([&](SWidget& Child) { WgCollectVisibleTexts(Child, Out); });
	}
}

/** The widget gallery sample (a page of the stock controls, laid out like the engine's Starship Gallery): `Layouts/WidgetGallery.sltr`. */
TEST_CLASS(SlaterWidgetGalleryTests, "BRS.SlaterUI.WidgetGallery")
{
	BEFORE_EACH()
	{
		Slater::SetImageSource(Slater::MakePluginIconSource());
	}

	AFTER_EACH()
	{
		Slater::SetImageSource(nullptr);
	}

	TArray<FString> VisibleTexts(const TSharedRef<SSlaterView>& View) const
	{
		TArray<FString> Texts;
		WgCollectVisibleTexts(*View, Texts);
		return Texts;
	}

	TEST_METHOD(TheShippedLayout_BuildsWithoutErrorsOrWarnings_InEveryStyle)
	{
		for (const TCHAR* Style : {TEXT("Base"), TEXT("Dusk"), TEXT("Skyline")})
		{
			const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("WidgetGallery")).StyleName(Style);
			for (const FSlaterError& Error : View->GetResult().Errors)
			{
				AddError(Error.ToString());
			}
			for (const FSlaterError& Warning : View->GetResult().Warnings)
			{
				AddError(Warning.ToString());
			}
			ASSERT_THAT(IsFalse(View->HasErrors()));
			ASSERT_THAT(IsTrue(View->GetResult().WidgetCount > 100));
		}
	}

	TEST_METHOD(EveryRowOfTheGalleryIsThere_AndTheMissingControlsAreMarkedTodo)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("WidgetGallery"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TArray<FString> Texts = VisibleTexts(View);
		for (const TCHAR* Row : {TEXT("Primary Button"), TEXT("Default/Secondary Button"), TEXT("Simple Button"), TEXT("Toggle Button"), TEXT("SegmentedControl"), TEXT("SegmentedControl Alt"), TEXT("Check Box"), TEXT("Radio Button"), TEXT("SimpleComboBox"), TEXT("ComboBox"), TEXT("SSlider"), TEXT("SSpinBox"),
			TEXT("SEditableTextBox"), TEXT("SMultiLineEditableText"), TEXT("SMultiLineEditableTextBox"), TEXT("SInlineEditableText"), TEXT("SSearchBox"), TEXT("SProgressBar"), TEXT("SProgressBar marquee"), TEXT("Window Flash Button"), TEXT("Menu Bar"), TEXT("Toolbar"), TEXT("Toolbar 2")})
		{
			if (!Texts.Contains(Row))
			{
				AddError(FString::Printf(TEXT("the gallery has no row labelled '%s'"), Row));
			}
		}
		int32 Todos = 0;
		for (const FString& Text : Texts)
		{
			Todos += Text.StartsWith(TEXT("TODO: ")) ? 1 : 0;
		}
		ASSERT_THAT(AreEqual(Todos, 8));
	}

	TEST_METHOD(AToggle_ShowsOneOfItsTwoLooks_ByTheViewState)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("WidgetGallery"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		FString State;
		ASSERT_THAT(IsTrue(View->GetState(TEXT("ToggleA"), State) && State == TEXT("true")));
		// "Label" appears three times for the buttons of the first three rows and once for the plain toggle, so count it before and after.
		const int32 Before = VisibleTexts(View).FilterByPredicate([](const FString& Text) { return Text == TEXT("Label"); }).Num();
		ASSERT_THAT(IsTrue(View->SetState(TEXT("ToggleA"), TEXT("false"))));
		const int32 After = VisibleTexts(View).FilterByPredicate([](const FString& Text) { return Text == TEXT("Label"); }).Num();
		// One look collapses and the other shows, so the count stays and only the toggle's look changed: neither look is ever missing.
		ASSERT_THAT(AreEqual(Before, After));
		ASSERT_THAT(IsTrue(View->GetState(TEXT("ToggleA"), State) && State == TEXT("false")));
	}
};

#endif
