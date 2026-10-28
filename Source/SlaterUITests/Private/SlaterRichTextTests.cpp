// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "SlaterView.h"
#include "Misc/AutomationTest.h"
#include "Rendering/SlateRenderer.h"
#include "SlaterListTestKit.h"
#include "SlaterModel.h"
#include "SlaterRichText.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/SRichTextBlock.h"

namespace SlaterRichTextTestsPrivate
{
	FSlaterBuildResult RtBuild(const FString& Layout)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}

	TSharedPtr<SSlaterRichText> RtWidget(const FSlaterBuildResult& Result)
	{
		return Result.Root.IsValid() ? StaticCastSharedPtr<SSlaterRichText>(SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SSlaterRichText"))) : nullptr;
	}

	/** A widget with the text set and laid out (the engine builds the runs on the first size query). */
	TSharedRef<SSlaterRichText> RtLaidOut(const FString& Text)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		const TSharedRef<SSlaterRichText> Widget = SNew(SSlaterRichText).Font(FSlaterFont{TEXT("Regular"), 14});
		Widget->SetRichText(Text, Stack);
		Widget->SlatePrepass(1.f);
		return Widget;
	}

	/** The widget's size for a text, laid out by the engine, against the Base style. */
	FVector2D RtSizeOf(const FString& Text)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		const TSharedRef<SSlaterRichText> Widget = SNew(SSlaterRichText).Font(FSlaterFont{TEXT("Regular"), 14});
		Widget->SetRichText(Text, Stack);
		Widget->SlatePrepass(1.f);
		return Widget->GetDesiredSize();
	}

	/** A widget that reveals Text by Mode at Speed steps per second, laid out, with nothing shown yet (T-545). */
	TSharedRef<SSlaterRichText> RtRevealing(const FString& Text, Slater::ERichReveal Mode, float Speed)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		const TSharedRef<SSlaterRichText> Widget = SNew(SSlaterRichText).Font(FSlaterFont{TEXT("Regular"), 14}).Reveal(Mode).RevealSpeed(Speed);
		Widget->SetRichText(Text, Stack);
		return Widget;
	}

	/** Sets `Slater.MotionScale` and puts it back to 1 when it goes out of scope. */
	struct FMotionScaleGuard
	{
		explicit FMotionScaleGuard(float Value) { Set(Value); }
		~FMotionScaleGuard() { Set(1.f); }
		static void Set(float Value)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Slater.MotionScale")))
			{
				Variable->Set(Value, ECVF_SetByCode);
			}
		}
	};

	/** The line height and the descent below the baseline of the font RtLaidOut uses, from the engine's font measure. */
	void RtLineOf14(float& OutHeight, float& OutDescent)
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14);
		OutHeight = Measure->GetMaxCharacterHeight(Font, 1.f);
		OutDescent = -Measure->GetBaseline(Font, 1.f);
	}
}
using namespace SlaterRichTextTestsPrivate;

/** `<rich-text>` (T-527): plain text, runs, an inline image, bad markup with its line, and bound content. */
TEST_CLASS(SlaterRichTextTests, "BRS.SlaterUI.RichText")
{
	SlaterListKit::FRig Rig;

	AFTER_EACH()
	{
		Rig.Shutdown();
	}

	TEST_METHOD(PlainText_Builds_AndKeepsTheWords)
	{
		const FSlaterBuildResult Result = RtBuild(TEXT("<vbox><rich-text>Sharp and light</rich-text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SSlaterRichText> Widget = RtWidget(Result);
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		ASSERT_THAT(IsTrue(Widget->GetSource() == TEXT("Sharp and light")));
		Result.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(Widget->GetDesiredSize().X > 0.f && Widget->GetDesiredSize().Y > 0.f));
	}

	TEST_METHOD(BoldRun_IsOneStyledRun_WithTheBoldRole)
	{
		const TSharedRef<SSlaterRichText> Widget = RtLaidOut(TEXT("Hit <b>hard</> now"));
		ASSERT_THAT(IsTrue(Widget->GetRuns().Num() == 1));
		const FSlaterRichRun& Run = Widget->GetRuns()[0];
		ASSERT_THAT(IsTrue(Run.Tag == TEXT("b") && Run.Text == TEXT("hard")));
		ASSERT_THAT(IsTrue(Run.FontRole == TEXT("Bold") && Run.FontSize == 14));
	}

	TEST_METHOD(ItalicColorAndFontRuns_UseTheStyleTokens)
	{
		const FSlaterBuildResult Result = RtBuild(TEXT("<vbox><rich-text>A &lt;i&gt;light&lt;/&gt; and &lt;color value=\"UI.Color.Danger\"&gt;red&lt;/&gt; and &lt;font value=\"UI.Font.Heading\"&gt;big&lt;/&gt; word</rich-text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		Result.Root->SlatePrepass(1.f);
		const TArray<FSlaterRichRun>& Runs = RtWidget(Result)->GetRuns();
		ASSERT_THAT(AreEqual(3, Runs.Num()));
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FLinearColor Danger, Text;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("UI.Color.Danger"), Danger) && Stack.FindColor(TEXT("UI.Color.Text"), Text)));
		ASSERT_THAT(AreEqual(FString(TEXT("i")), Runs[0].Tag));
		ASSERT_THAT(AreEqual(FName(TEXT("Italic")), Runs[0].FontRole));
		ASSERT_THAT(AreEqual(Text.ToString(), Runs[0].Color.ToString()));
		ASSERT_THAT(AreEqual(Danger.ToString(), Runs[1].Color.ToString()));
		ASSERT_THAT(AreEqual(FName(TEXT("Regular")), Runs[1].FontRole));
		// UI.Font.Heading is Bold, 20 in the Base style.
		ASSERT_THAT(IsTrue(Runs[2].Tag == TEXT("font") && Runs[2].FontRole == TEXT("Bold") && Runs[2].FontSize == 20 && Runs[2].Text == TEXT("big")));
	}

	TEST_METHOD(InlineImage_TakesItsBrushFromTheImageSource)
	{
		Slater::SetImageSource(MakeShared<FSlaterTestImages>(4));
		const TSharedRef<SSlaterRichText> Found = RtLaidOut(TEXT("Cost <img src=\"icon0\"/>"));
		const TSharedRef<SSlaterRichText> Missing = RtLaidOut(TEXT("Cost <img src=\"nope\"/>"));
		const float With = RtSizeOf(TEXT("Cost <img src=\"icon0\"/>")).X;
		const float Without = RtSizeOf(TEXT("Cost ")).X;
		Slater::SetImageSource(nullptr);
		ASSERT_THAT(IsTrue(Found->GetRuns().Num() == 1 && Found->GetRuns()[0].Tag == TEXT("img")));
		ASSERT_THAT(IsTrue(Found->GetRuns()[0].Image == TEXT("icon0") && Found->GetRuns()[0].bImageFound));
		ASSERT_THAT(IsTrue(Missing->GetRuns().Num() == 1 && !Missing->GetRuns()[0].bImageFound));
		ASSERT_THAT(IsTrue(With >= Without + 7.9f));
	}

	TEST_METHOD(InlineImage_ByDefault_IsAsTallAsTheLine_AndCenteredOnIt)
	{
		float LineHeight = 0.f, Descent = 0.f;
		RtLineOf14(LineHeight, Descent);
		ASSERT_THAT(IsTrue(LineHeight > 0.f && Descent > 0.f));
		Slater::SetImageSource(MakeShared<FSlaterTestImages>(4));
		const TSharedRef<SSlaterRichText> Widget = RtLaidOut(TEXT("Cost <img src=\"icon0\"/>"));
		const float WithImage = RtSizeOf(TEXT("Cost <img src=\"icon0\"/>")).Y;
		const float TextOnly = RtSizeOf(TEXT("Cost ")).Y;
		Slater::SetImageSource(nullptr);
		ASSERT_THAT(IsTrue(Widget->GetRuns().Num() == 1));
		const FSlaterRichRun& Run = Widget->GetRuns()[0];
		// The 8x8 brush is drawn at the font's line height (square stays square), its bottom at the font's descent.
		ASSERT_THAT(IsNear(LineHeight, float(Run.ImageSize.Y), 0.01f));
		ASSERT_THAT(IsNear(LineHeight, float(Run.ImageSize.X), 0.01f));
		ASSERT_THAT(IsNear(Descent, Run.ImageBaseline, 0.01f));
		// So the image fills the line box exactly and the line is no taller than the words alone.
		ASSERT_THAT(IsNear(TextOnly, WithImage, 0.5f));
	}

	TEST_METHOD(InlineImage_SizeAndBaseline_FromTheAttributes_PlaceItOnTheLine)
	{
		float LineHeight = 0.f, Descent = 0.f;
		RtLineOf14(LineHeight, Descent);
		const TSharedRef<FSlaterTestImages> Images = MakeShared<FSlaterTestImages>(4);
		FSlateBrush Wide = Images->Brushes.FindChecked(TEXT("icon0"));
		Wide.ImageSize = FVector2D(16.0, 8.0);
		Images->Brushes.Add(TEXT("wide"), Wide);
		Slater::SetImageSource(Images);
		const TSharedRef<SSlaterRichText> Both = RtLaidOut(TEXT("Cost <img src=\"icon0\" size=\"24\" baseline=\"2\"/>"));
		const TSharedRef<SSlaterRichText> SizeOnly = RtLaidOut(TEXT("Cost <img src=\"wide\" size=\"10\"/>"));
		const TSharedRef<SSlaterRichText> Raised = RtLaidOut(TEXT("Cost <img src=\"icon0\" baseline=\"-3\"/>"));
		const float BothHeight = RtSizeOf(TEXT("Cost <img src=\"icon0\" size=\"24\" baseline=\"2\"/>")).Y;
		Slater::SetImageSource(nullptr);

		ASSERT_THAT(IsTrue(Both->GetRuns().Num() == 1 && SizeOnly->GetRuns().Num() == 1 && Raised->GetRuns().Num() == 1));
		ASSERT_THAT(IsTrue(Both->GetRuns()[0].ImageSize == FVector2D(24.0, 24.0)));
		ASSERT_THAT(IsNear(2.f, Both->GetRuns()[0].ImageBaseline, 0.01f));
		// The engine lays the line out from what is above and below the baseline: 22 above and 2 below for the image.
		ASSERT_THAT(IsNear(FMath::Max(LineHeight - Descent, 22.f) + FMath::Max(Descent, 2.f), BothHeight, 0.5f));

		// size alone: the width keeps the brush's aspect, and the image stays centered on the line.
		ASSERT_THAT(IsTrue(SizeOnly->GetRuns()[0].ImageSize == FVector2D(20.0, 10.0)));
		ASSERT_THAT(IsNear(Descent - (LineHeight - 10.f) * 0.5f, SizeOnly->GetRuns()[0].ImageBaseline, 0.01f));

		// baseline alone: the line-high default size, raised above the baseline.
		ASSERT_THAT(IsNear(LineHeight, float(Raised->GetRuns()[0].ImageSize.Y), 0.01f));
		ASSERT_THAT(IsNear(-3.f, Raised->GetRuns()[0].ImageBaseline, 0.01f));
	}

	TEST_METHOD(InlineImage_Tint_MultipliesTheBrush_AndWithoutIt_KeepsItsColors)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FLinearColor Danger;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("UI.Color.Danger"), Danger)));
		Slater::SetImageSource(MakeShared<FSlaterTestImages>(4));
		const TSharedRef<SSlaterRichText> Tinted = RtLaidOut(TEXT("Cost <img src=\"icon0\" tint=\"UI.Color.Danger\"/> and <img src=\"icon1\"/>"));
		const TSharedRef<SSlaterRichText> Bound = RtLaidOut(TEXT("Cost <img src=\"icon0\" tint=\"UI.Color.Nope\"/>"));
		Slater::SetImageSource(nullptr);
		ASSERT_THAT(AreEqual(2, Tinted->GetRuns().Num()));
		ASSERT_THAT(AreEqual(Danger.ToString(), Tinted->GetRuns()[0].Color.ToString()));
		ASSERT_THAT(AreEqual(FLinearColor::White.ToString(), Tinted->GetRuns()[1].Color.ToString()));
		// In bound text an unknown token is not checked: the image keeps its own colors.
		ASSERT_THAT(AreEqual(FLinearColor::White.ToString(), Bound->GetRuns()[0].Color.ToString()));
	}

	TEST_METHOD(InlineImage_SizeAndBaseline_InALayout_AreCheckedAtBuild)
	{
		const FSlaterBuildResult Good = RtBuild(TEXT("<vbox><rich-text>Cost &lt;img src=\"Gift\" size=\"16\" baseline=\"-1.5\" tint=\"UI.Color.Text\"/&gt; one</rich-text></vbox>"));
		ASSERT_THAT(IsTrue(Good.Succeeded()));
		const TCHAR* Cases[][2] = {
			{TEXT("<img src=\"a\" size=\"0\"/>"), TEXT("not a number above 0")},
			{TEXT("<img src=\"a\" size=\"-4\"/>"), TEXT("not a number above 0")},
			{TEXT("<img src=\"a\" size=\"big\"/>"), TEXT("not a number above 0")},
			{TEXT("<img src=\"a\" baseline=\"low\"/>"), TEXT("baseline=\"low\" is not a number")},
			{TEXT("<img src=\"a\" width=\"3\"/>"), TEXT("no attribute 'width'")},
			{TEXT("<img src=\"a\" tint=\"UI.Color.Txt\"/>"), TEXT("unknown color token 'UI.Color.Txt'")},
		};
		for (const TCHAR* const (&Case)[2] : Cases)
		{
			FSlaterStyleStack Stack;
			Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
			TArray<FSlaterTextProblem> Problems;
			Slater::CheckRichText(Case[0], Stack, Problems);
			ASSERT_THAT(IsTrue(Problems.Num() == 1));
			ASSERT_THAT(IsTrue(Problems[0].Message.Contains(Case[1])));
		}
		const FSlaterBuildResult Bad = RtBuild(TEXT("<vbox><rich-text>Cost &lt;img src=\"Gift\" size=\"tiny\"/&gt;</rich-text></vbox>"));
		ASSERT_THAT(IsFalse(Bad.Succeeded()));
		ASSERT_THAT(IsTrue(Bad.Errors.Num() == 1 && Bad.Errors[0].Message.Contains(TEXT("size=\"tiny\""))));
	}

	TEST_METHOD(UnknownTag_InBoundText_ShowsAsWritten_WithNoRun)
	{
		const TSharedRef<SSlaterRichText> Widget = RtLaidOut(TEXT("Hit <x>hard</> now"));
		ASSERT_THAT(IsTrue(Widget->GetRuns().IsEmpty()));
	}

	TEST_METHOD(EntityEscapedTags_ReachTheRichParser)
	{
		const FSlaterBuildResult Result = RtBuild(TEXT("<vbox><rich-text>Hit &lt;b&gt;hard&lt;/&gt; &amp;lt; end</rich-text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(RtWidget(Result)->GetSource() == TEXT("Hit <b>hard</> &lt; end")));
	}

	TEST_METHOD(LessThanSignThatStartsNoTag_IsPlainText)
	{
		const FSlaterBuildResult Result = RtBuild(TEXT("<vbox><rich-text>HP &lt; 5 and 3 &lt;= 4</rich-text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
	}

	TEST_METHOD(UnknownTag_IsRefused_AtItsLineAndColumn)
	{
		const FSlaterBuildResult Result = RtBuild(TEXT("<vbox>\n  <rich-text>First line\nSecond &lt;blink&gt;x&lt;/&gt; line</rich-text>\n</vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors.Num() == 1));
		ASSERT_THAT(IsTrue(Result.Errors[0].Line == 3 && Result.Errors[0].Column == 8));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("unknown rich-text tag <blink>"))));
	}

	TEST_METHOD(UnclosedRun_StrayClose_NestedRun_AndBadAttributes_AreRefused)
	{
		const TCHAR* Cases[][2] = {
			{TEXT("<b>never closed"), TEXT("never closed")},
			{TEXT("stray </> close"), TEXT("closes nothing")},
			{TEXT("<b>one <i>two</></>"), TEXT("do not nest")},
			{TEXT("<b>a</b>"), TEXT("never closed")},
			{TEXT("<b size=\"3\">a</>"), TEXT("no attribute 'size'")},
			{TEXT("<color>a</>"), TEXT("needs value")},
			{TEXT("<color value=\"UI.Color.Dangr\">a</>"), TEXT("unknown color token")},
			{TEXT("<font value=\"UI.Font.Nope\">a</>"), TEXT("unknown font token")},
			{TEXT("<img/>"), TEXT("needs src")},
			{TEXT("<img src=\"a\">x</>"), TEXT("holds nothing")},
			{TEXT("<b/>"), TEXT("holds nothing")},
		};
		for (const TCHAR* const (&Case)[2] : Cases)
		{
			FSlaterStyleStack Stack;
			Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
			TArray<FSlaterTextProblem> Problems;
			Slater::CheckRichText(Case[0], Stack, Problems);
			ASSERT_THAT(IsTrue(Problems.Num() >= 1));
			ASSERT_THAT(IsTrue(Problems[0].Message.Contains(Case[1])));
		}
	}

	TEST_METHOD(BoundContent_ShowsTheDataAndFollowsItsChanges)
	{
		ASSERT_THAT(IsTrue(Rig.Init(TEXT("<vbox><rich-text>{Description}</rich-text></vbox>"), TEXT("{\"Description\":\"Plain words\"}"), ESlaterBindMode::Pushed)));
		const TSharedPtr<SSlaterRichText> Widget = RtWidget(Rig.Result);
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		ASSERT_THAT(IsTrue(Widget->GetSource() == TEXT("Plain words")));
		ASSERT_THAT(IsTrue(Rig.Model->SetValue(TEXT("Description"), TEXT("A <b>strong</> sword"))));
		ASSERT_THAT(IsTrue(Widget->GetSource() == TEXT("A <b>strong</> sword")));
	}

	TEST_METHOD(TheRichTextSample_BuildsInBothStyles_AndShowsTheMockDescription)
	{
		for (const TCHAR* Style : {TEXT("Base"), TEXT("Dusk")})
		{
			const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("RichText")).StyleName(Style).UseMockContext(true);
			ASSERT_THAT(IsFalse(View->HasErrors()));
			const TSharedPtr<SWidget> Bound = SlaterListKit::Find(View, TEXT("SSlaterRichText"));
			ASSERT_THAT(IsTrue(Bound.IsValid()));
			View->SlatePrepass(1.f);
			TArray<TSharedRef<SWidget>> Stack;
			Stack.Add(View);
			int32 Texts = 0;
			bool bDescription = false;
			while (!Stack.IsEmpty())
			{
				const TSharedRef<SWidget> Widget = Stack.Pop();
				if (Widget->GetType() == FName(TEXT("SSlaterRichText")))
				{
					++Texts;
					bDescription |= StaticCastSharedRef<SSlaterRichText>(Widget)->GetSource().Contains(TEXT("<b>40 health</>"));
				}
				Widget->GetChildren()->ForEachWidget([&](SWidget& Child) { Stack.Add(Child.AsShared()); });
			}
			// The run sample, the image placement line (T-544), the bound description and the literal-bracket caption.
			ASSERT_THAT(AreEqual(4, Texts));
			ASSERT_THAT(IsTrue(bDescription));
		}
	}

	TEST_METHOD(Reveal_Prefix_CutsByCharacter_AndClosesARunItCutsInside)
	{
		const FString Text = TEXT("Hit <b>hard</> now");
		ASSERT_THAT(AreEqual(12, Slater::RichRevealTotal(Text, Slater::ERichReveal::Char)));
		ASSERT_THAT(AreEqual(FString(), Slater::RichRevealPrefix(Text, Slater::ERichReveal::Char, 0)));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit ")), Slater::RichRevealPrefix(Text, Slater::ERichReveal::Char, 4)));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>ha</>")), Slater::RichRevealPrefix(Text, Slater::ERichReveal::Char, 6)));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>hard</> ")), Slater::RichRevealPrefix(Text, Slater::ERichReveal::Char, 9)));
		ASSERT_THAT(AreEqual(Text, Slater::RichRevealPrefix(Text, Slater::ERichReveal::Char, 12)));
		ASSERT_THAT(AreEqual(Text, Slater::RichRevealPrefix(Text, Slater::ERichReveal::Char, 99)));
	}

	TEST_METHOD(Reveal_Prefix_CutsByWord_AnImageIsAWord_AnEntityIsOneCharacter)
	{
		const FString Words = TEXT("Hit <b>hard words</> now");
		ASSERT_THAT(AreEqual(4, Slater::RichRevealTotal(Words, Slater::ERichReveal::Word)));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit")), Slater::RichRevealPrefix(Words, Slater::ERichReveal::Word, 1)));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>hard</>")), Slater::RichRevealPrefix(Words, Slater::ERichReveal::Word, 2)));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>hard words</>")), Slater::RichRevealPrefix(Words, Slater::ERichReveal::Word, 3)));

		const FString Image = TEXT("A <img src=\"Gift\" tint=\"UI.Color.Text\"/> B");
		ASSERT_THAT(AreEqual(3, Slater::RichRevealTotal(Image, Slater::ERichReveal::Word)));
		ASSERT_THAT(AreEqual(FString(TEXT("A <img src=\"Gift\" tint=\"UI.Color.Text\"/>")), Slater::RichRevealPrefix(Image, Slater::ERichReveal::Word, 2)));
		ASSERT_THAT(AreEqual(FString(TEXT("A ")), Slater::RichRevealPrefix(Image, Slater::ERichReveal::Char, 2)));

		const FString Entity = TEXT("a &lt; b");
		ASSERT_THAT(AreEqual(5, Slater::RichRevealTotal(Entity, Slater::ERichReveal::Char)));
		ASSERT_THAT(AreEqual(FString(TEXT("a &lt;")), Slater::RichRevealPrefix(Entity, Slater::ERichReveal::Char, 3)));
	}

	TEST_METHOD(Reveal_NoPrefix_ShowsATagAsText)
	{
		// Every cut of a text with every kind of tag is a valid text: the checker finds no problem in it.
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		const FString Text = TEXT("A <b>bold</> and <color value=\"UI.Color.Danger\">red words</> and <img src=\"Gift\"/> end");
		for (const Slater::ERichReveal Mode : {Slater::ERichReveal::Char, Slater::ERichReveal::Word})
		{
			const int32 Total = Slater::RichRevealTotal(Text, Mode);
			for (int32 Count = 0; Count <= Total; ++Count)
			{
				TArray<FSlaterTextProblem> Problems;
				Slater::CheckRichText(Slater::RichRevealPrefix(Text, Mode, Count), Stack, Problems);
				ASSERT_THAT(IsTrue(Problems.IsEmpty()));
			}
		}
	}

	TEST_METHOD(Reveal_Char_FollowsAManualClock_AndTicksOnlyWhileTextIsLeft)
	{
		const TSharedRef<SSlaterRichText> Widget = RtRevealing(TEXT("Hit <b>hard</> now"), Slater::ERichReveal::Char, 10.f);
		ASSERT_THAT(AreEqual(12, Widget->GetRevealTotal()));
		ASSERT_THAT(AreEqual(0, Widget->GetRevealedCount()));
		ASSERT_THAT(IsTrue(Widget->GetShownMarkup().IsEmpty()));
		ASSERT_THAT(IsTrue(Widget->IsRevealing()));
		ASSERT_THAT(IsTrue(Widget->AdvanceReveal(0.5f)));
		ASSERT_THAT(AreEqual(5, Widget->GetRevealedCount()));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>h</>")), Widget->GetShownMarkup()));
		ASSERT_THAT(IsTrue(Widget->AdvanceReveal(0.04f)));
		ASSERT_THAT(AreEqual(5, Widget->GetRevealedCount())); // part of a step: nothing new shown
		ASSERT_THAT(IsTrue(Widget->AdvanceReveal(0.07f)));
		ASSERT_THAT(AreEqual(6, Widget->GetRevealedCount()));
		ASSERT_THAT(IsFalse(Widget->AdvanceReveal(10.f)));
		ASSERT_THAT(AreEqual(12, Widget->GetRevealedCount()));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>hard</> now")), Widget->GetShownMarkup()));
	}

	TEST_METHOD(Reveal_Word_FollowsAManualClock)
	{
		const TSharedRef<SSlaterRichText> Widget = RtRevealing(TEXT("Hit <b>hard words</> now"), Slater::ERichReveal::Word, 2.f);
		ASSERT_THAT(AreEqual(4, Widget->GetRevealTotal()));
		ASSERT_THAT(IsTrue(Widget->AdvanceReveal(1.f)));
		ASSERT_THAT(AreEqual(2, Widget->GetRevealedCount()));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>hard</>")), Widget->GetShownMarkup()));
		ASSERT_THAT(IsFalse(Widget->AdvanceReveal(1.f)));
		ASSERT_THAT(AreEqual(4, Widget->GetRevealedCount()));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>hard words</> now")), Widget->GetShownMarkup()));
	}

	TEST_METHOD(Reveal_AStyledRunRevealedInTheMiddle_KeepsItsStyle)
	{
		const TSharedRef<SSlaterRichText> Widget = RtRevealing(TEXT("Go <color value=\"UI.Color.Danger\">red alert</> now"), Slater::ERichReveal::Char, 10.f);
		Widget->AdvanceReveal(0.5f); // 5 characters: "Go " and "re"
		Widget->SlatePrepass(1.f);
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FLinearColor Danger;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("UI.Color.Danger"), Danger)));
		ASSERT_THAT(AreEqual(1, Widget->GetRuns().Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("re")), Widget->GetRuns()[0].Text));
		ASSERT_THAT(AreEqual(Danger.ToString(), Widget->GetRuns()[0].Color.ToString()));
	}

	TEST_METHOD(Reveal_ChangedTextRestarts_TheSameTextKeepsItsProgress)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		const TSharedRef<SSlaterRichText> Widget = RtRevealing(TEXT("First line"), Slater::ERichReveal::Char, 8.f);
		Widget->AdvanceReveal(0.5f);
		ASSERT_THAT(AreEqual(4, Widget->GetRevealedCount()));
		Widget->SetRichText(TEXT("First line"), Stack);
		ASSERT_THAT(AreEqual(4, Widget->GetRevealedCount()));
		ASSERT_THAT(IsFalse(Widget->AdvanceReveal(10.f)));
		ASSERT_THAT(AreEqual(10, Widget->GetRevealedCount()));

		Widget->SetRichText(TEXT("A <b>second</> text"), Stack);
		ASSERT_THAT(AreEqual(0, Widget->GetRevealedCount()));
		ASSERT_THAT(AreEqual(13, Widget->GetRevealTotal()));
		ASSERT_THAT(IsTrue(Widget->GetShownMarkup().IsEmpty()));
		ASSERT_THAT(IsTrue(Widget->IsRevealing()));
		ASSERT_THAT(IsTrue(Widget->AdvanceReveal(0.25f)));
		ASSERT_THAT(AreEqual(FString(TEXT("A ")), Widget->GetShownMarkup()));
	}

	TEST_METHOD(Reveal_AtMotionScaleZero_ShowsEverythingAtOnce_AndNeverTicks)
	{
		FMotionScaleGuard Scale(0.f);
		const TSharedRef<SSlaterRichText> Widget = RtRevealing(TEXT("Hit <b>hard</> now"), Slater::ERichReveal::Char, 10.f);
		ASSERT_THAT(AreEqual(12, Widget->GetRevealedCount()));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>hard</> now")), Widget->GetShownMarkup()));
		ASSERT_THAT(IsFalse(Widget->IsRevealing()));

		// A scale set mid-reveal finishes it on the next advance.
		FMotionScaleGuard Back(1.f);
		const TSharedRef<SSlaterRichText> Midway = RtRevealing(TEXT("Hit <b>hard</> now"), Slater::ERichReveal::Char, 8.f);
		Midway->AdvanceReveal(0.25f);
		ASSERT_THAT(AreEqual(2, Midway->GetRevealedCount()));
		FMotionScaleGuard Instant(0.f);
		ASSERT_THAT(IsFalse(Midway->AdvanceReveal(0.f)));
		ASSERT_THAT(AreEqual(12, Midway->GetRevealedCount()));
	}

	TEST_METHOD(Reveal_NotSet_ShowsTheWholeText_AndNeverTicks)
	{
		const TSharedRef<SSlaterRichText> Widget = RtLaidOut(TEXT("Hit <b>hard</> now"));
		ASSERT_THAT(AreEqual(FString(TEXT("Hit <b>hard</> now")), Widget->GetShownMarkup()));
		ASSERT_THAT(IsFalse(Widget->IsRevealing()));
		ASSERT_THAT(IsFalse(Widget->AdvanceReveal(1.f)));
	}

	TEST_METHOD(Reveal_Attributes_AreTyped_AndBoundTextRestartsIt)
	{
		const FSlaterBuildResult Good = RtBuild(TEXT("<vbox><rich-text reveal=\"word\" reveal-speed=\"4\">One two three</rich-text></vbox>"));
		ASSERT_THAT(IsTrue(Good.Succeeded()));
		const TSharedPtr<SSlaterRichText> Widget = RtWidget(Good);
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		ASSERT_THAT(AreEqual(3, Widget->GetRevealTotal()));
		Widget->AdvanceReveal(0.5f);
		ASSERT_THAT(AreEqual(2, Widget->GetRevealedCount()));

		ASSERT_THAT(IsFalse(RtBuild(TEXT("<vbox><rich-text reveal=\"line\">x</rich-text></vbox>")).Succeeded()));
		ASSERT_THAT(IsFalse(RtBuild(TEXT("<vbox><rich-text reveal-speed=\"4\">x</rich-text></vbox>")).Succeeded()));
		ASSERT_THAT(IsFalse(RtBuild(TEXT("<vbox><rich-text reveal=\"char\" reveal-speed=\"0\">x</rich-text></vbox>")).Succeeded()));

		ASSERT_THAT(IsTrue(Rig.Init(TEXT("<vbox><rich-text reveal=\"char\" reveal-speed=\"10\">{Description}</rich-text></vbox>"), TEXT("{\"Description\":\"Plain words\"}"), ESlaterBindMode::Pushed)));
		const TSharedPtr<SSlaterRichText> Bound = RtWidget(Rig.Result);
		ASSERT_THAT(IsTrue(Bound.IsValid()));
		Bound->AdvanceReveal(10.f);
		ASSERT_THAT(AreEqual(11, Bound->GetRevealedCount()));
		ASSERT_THAT(IsTrue(Rig.Model->SetValue(TEXT("Description"), TEXT("A <b>strong</> sword"))));
		ASSERT_THAT(AreEqual(0, Bound->GetRevealedCount()));
		ASSERT_THAT(AreEqual(14, Bound->GetRevealTotal()));
		ASSERT_THAT(IsTrue(Bound->IsRevealing()));
	}

	TEST_METHOD(Reveal_LongText_CostPerTick_IsLoggedForDevNotes)
	{
		// Not an assertion on time (machines differ): the numbers go to the log and to DevNotes (T-545).
		FString Text;
		for (int32 I = 0; I < 40; ++I)
		{
			Text += TEXT("A <b>sharp</> blade, <i>light</> in the hand, deals <color value=\"UI.Color.Danger\">12 damage</> and costs one gift. ");
		}
		const TSharedRef<SSlaterRichText> Widget = RtRevealing(Text, Slater::ERichReveal::Char, 40.f);
		Widget->SlatePrepass(1.f);
		const int32 Ticks = 200;
		const double Start = FPlatformTime::Seconds();
		for (int32 I = 0; I < Ticks; ++I)
		{
			Widget->AdvanceReveal(1.f / 40.f);
			Widget->SlatePrepass(1.f);
		}
		const double PerTickMs = (FPlatformTime::Seconds() - Start) * 1000.0 / Ticks;
		UE_LOG(LogTemp, Log, TEXT("T-545 reveal cost: %d chars of markup, prefix + SetText + prepass %.3f ms per tick"), Text.Len(), PerTickMs);
		ASSERT_THAT(AreEqual(Ticks, Widget->GetRevealedCount()));
	}

	TEST_METHOD(TheTypewriterSample_BuildsInBothStyles_AndRevealsItsText)
	{
		for (const TCHAR* Style : {TEXT("Base"), TEXT("Dusk")})
		{
			const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Typewriter")).StyleName(Style).UseMockContext(true);
			ASSERT_THAT(IsFalse(View->HasErrors()));
			const TSharedPtr<SWidget> Found = SlaterListKit::Find(View, TEXT("SSlaterRichText"));
			ASSERT_THAT(IsTrue(Found.IsValid()));
			const TSharedRef<SSlaterRichText> Widget = StaticCastSharedRef<SSlaterRichText>(Found.ToSharedRef());
			ASSERT_THAT(IsTrue(Widget->GetRevealTotal() > 0));
			ASSERT_THAT(AreEqual(0, Widget->GetRevealedCount()));
		}
	}

	TEST_METHOD(Attributes_Wrap_Justify_AreTyped)
	{
		const FSlaterBuildResult Good = RtBuild(TEXT("<vbox><rich-text wrap=\"true\" justify=\"center\" color=\"UI.Color.Accent\" font=\"UI.Font.Heading\">Words</rich-text></vbox>"));
		ASSERT_THAT(IsTrue(Good.Succeeded()));
		const FSlaterBuildResult Bad = RtBuild(TEXT("<vbox><rich-text justify=\"middle\">Words</rich-text></vbox>"));
		ASSERT_THAT(IsFalse(Bad.Succeeded()));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
