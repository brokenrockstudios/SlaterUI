// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterControls.h"
#include "SlaterListTestKit.h"
#include "SlaterStyle.h"
#include "SlaterView.h"

namespace
{
	FLinearColor CsTint(const FSlateBrush& Brush)
	{
		return Brush.TintColor.GetSpecifiedColor();
	}

	FLinearColor CsLine(const FSlateBrush& Brush)
	{
		return Brush.OutlineSettings.Color.GetSpecifiedColor();
	}

	FLinearColor CsToken(const FSlaterStyleStack& Stack, const TCHAR* Token)
	{
		FLinearColor Color = FLinearColor(1.f, 0.f, 1.f);
		Stack.FindColor(Token, Color);
		return Color;
	}

	/** The look of the first widget of a Slate type in a layout built against a stack. */
	TSharedPtr<FSlaterControlLook> CsLook(const FSlaterStyleStack& Stack, const TCHAR* Layout, const TCHAR* Type)
	{
		FSlaterBuildResult Result;
		if (!Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result) || !Result.Root.IsValid())
		{
			return nullptr;
		}
		const TSharedPtr<SWidget> Widget = SlaterListKit::Find(Result.Root.ToSharedRef(), Type);
		return Widget.IsValid() ? Slater::FindControlLook(*Widget) : nullptr;
	}
}

/** The stock controls (T-514) draw from the style: a token reaches the slider, the checkbox and the text box, in both shipped styles. */
TEST_CLASS(SlaterControlStyleTests, "BRS.SlaterUI.ControlStyle")
{
	void CheckStyle(const TCHAR* StyleName)
	{
		FSlaterStyleStack Stack;
		ASSERT_THAT(IsTrue(Slater::StyleLibrary().BuildStack(StyleName, Stack)));
		const FLinearColor Accent = CsToken(Stack, TEXT("UI.Color.Accent"));
		const FLinearColor Track = CsToken(Stack, TEXT("UI.Color.Track"));
		const FLinearColor Control = CsToken(Stack, TEXT("UI.Color.Control"));
		const FLinearColor ControlLine = CsToken(Stack, TEXT("UI.Color.ControlLine"));
		const FLinearColor Text = CsToken(Stack, TEXT("UI.Color.Text"));

		const TSharedPtr<FSlaterControlLook> Slider = CsLook(Stack, TEXT("<vbox><slider/></vbox>"), TEXT("SSlider"));
		ASSERT_THAT(IsTrue(Slider.IsValid() && Slider->Slider.IsValid()));
		ASSERT_THAT(IsTrue(CsTint(Slider->Slider->NormalThumbImage) == Accent));
		ASSERT_THAT(IsTrue(CsTint(Slider->Slider->NormalBarImage) == Track));

		const TSharedPtr<FSlaterControlLook> Check = CsLook(Stack, TEXT("<vbox><checkbox/></vbox>"), TEXT("SCheckBox"));
		ASSERT_THAT(IsTrue(Check.IsValid() && Check->CheckBox.IsValid()));
		ASSERT_THAT(IsTrue(CsTint(Check->CheckBox->UncheckedImage) == Control));
		ASSERT_THAT(IsTrue(CsLine(Check->CheckBox->UncheckedImage) == ControlLine));
		ASSERT_THAT(IsTrue(CsTint(Check->CheckBox->CheckedImage) == Accent));
		// The engine multiplies the box brush by the foreground color, so a dark text color there would blacken the box.
		ASSERT_THAT(IsTrue(Check->CheckBox->ForegroundColor.GetSpecifiedColor() == FLinearColor::White));

		const TSharedPtr<FSlaterControlLook> TextBox = CsLook(Stack, TEXT("<vbox><textbox/></vbox>"), TEXT("SEditableTextBox"));
		ASSERT_THAT(IsTrue(TextBox.IsValid() && TextBox->TextBox.IsValid()));
		ASSERT_THAT(IsTrue(CsTint(TextBox->TextBox->BackgroundImageNormal) == Control));
		ASSERT_THAT(IsTrue(CsLine(TextBox->TextBox->BackgroundImageNormal) == ControlLine));
		ASSERT_THAT(IsTrue(CsLine(TextBox->TextBox->BackgroundImageFocused) == Accent));
		ASSERT_THAT(IsTrue(TextBox->TextBox->ForegroundColor.GetSpecifiedColor() == Text));
	}
	TEST_METHOD(TokensOfBase_ReachEachControl)
	{
		CheckStyle(TEXT("Base"));
	}

	TEST_METHOD(TokensOfDusk_ReachEachControl_AndDifferFromBase)
	{
		CheckStyle(TEXT("Dusk"));
		FSlaterStyleStack Base, Dusk;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Base);
		Slater::StyleLibrary().BuildStack(TEXT("Dusk"), Dusk);
		const TSharedPtr<FSlaterControlLook> InBase = CsLook(Base, TEXT("<vbox><textbox/></vbox>"), TEXT("SEditableTextBox"));
		const TSharedPtr<FSlaterControlLook> InDusk = CsLook(Dusk, TEXT("<vbox><textbox/></vbox>"), TEXT("SEditableTextBox"));
		ASSERT_THAT(IsTrue(InBase.IsValid() && InDusk.IsValid()));
		ASSERT_THAT(IsFalse(CsTint(InBase->TextBox->BackgroundImageNormal) == CsTint(InDusk->TextBox->BackgroundImageNormal)));
	}

	TEST_METHOD(SliderColorAndTrack_WhenWritten_ReplaceTheTokens)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		const TSharedPtr<FSlaterControlLook> Slider = CsLook(Stack, TEXT("<vbox><slider color='#102030' track='UI.Color.Danger'/></vbox>"), TEXT("SSlider"));
		ASSERT_THAT(IsTrue(Slider.IsValid() && Slider->Slider.IsValid()));
		ASSERT_THAT(IsTrue(CsTint(Slider->Slider->NormalThumbImage) == FLinearColor::FromSRGBColor(FColor(0x10, 0x20, 0x30))));
		ASSERT_THAT(IsTrue(CsTint(Slider->Slider->NormalBarImage) == CsToken(Stack, TEXT("UI.Color.Danger"))));
	}

	TEST_METHOD(AStyleWithoutTheTokens_StillBuildsTheControls)
	{
		const FSlaterStyleStack Empty;
		const TSharedPtr<FSlaterControlLook> Slider = CsLook(Empty, TEXT("<vbox><slider/></vbox>"), TEXT("SSlider"));
		const TSharedPtr<FSlaterControlLook> Check = CsLook(Empty, TEXT("<vbox><checkbox/></vbox>"), TEXT("SCheckBox"));
		ASSERT_THAT(IsTrue(Slider.IsValid() && Slider->Slider.IsValid()));
		ASSERT_THAT(IsTrue(Check.IsValid() && Check->CheckBox.IsValid()));
		ASSERT_THAT(IsTrue(CsTint(Check->CheckBox->UncheckedImage) == FLinearColor::White));
	}
};

#endif
