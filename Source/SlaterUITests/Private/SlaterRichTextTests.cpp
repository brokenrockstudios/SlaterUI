// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Misc/AutomationTest.h"
#include "SlaterListTestKit.h"
#include "SlaterModel.h"
#include "SlaterRichText.h"
#include "Widgets/Text/SRichTextBlock.h"

namespace
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
}

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

	TEST_METHOD(BoldRun_ShowsOnlyTheWords_AndIsWiderThanThePlainRun)
	{
		const float Plain = RtSizeOf(TEXT("Hit hard")).X;
		const float Bold = RtSizeOf(TEXT("Hit <b>hard</>")).X;
		const float Literal = RtSizeOf(TEXT("Hit <x>hard</>")).X; // an unknown tag is shown as written
		ASSERT_THAT(IsTrue(Bold > Plain));
		ASSERT_THAT(IsTrue(Bold < Literal));
	}

	TEST_METHOD(ItalicColorAndFontRuns_UseTheStyleTokens)
	{
		const FSlaterBuildResult Result = RtBuild(TEXT("<vbox><rich-text>A <i>light</> and <color value=\"UI.Color.Danger\">red</> and <font value=\"UI.Font.Heading\">big</> word</rich-text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		// The heading font is 20 high against the body's 14, so the line is taller than the same words without the font run.
		const float WithFont = RtSizeOf(TEXT("a <font value=\"UI.Font.Heading\">big</> word")).Y;
		const float Without = RtSizeOf(TEXT("a big word")).Y;
		ASSERT_THAT(IsTrue(WithFont > Without));
	}

	TEST_METHOD(InlineImage_TakesItsSizeFromTheImageSource)
	{
		Slater::SetImageSource(MakeShared<FSlaterTestImages>(4));
		const float With = RtSizeOf(TEXT("Cost <img src=\"icon0\"/>")).X;
		const float Without = RtSizeOf(TEXT("Cost ")).X;
		Slater::SetImageSource(nullptr);
		ASSERT_THAT(IsTrue(With >= Without + 7.9f));
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

	TEST_METHOD(Attributes_Wrap_Justify_AreTyped)
	{
		const FSlaterBuildResult Good = RtBuild(TEXT("<vbox><rich-text wrap=\"true\" justify=\"center\" color=\"UI.Color.Accent\" font=\"UI.Font.Heading\">Words</rich-text></vbox>"));
		ASSERT_THAT(IsTrue(Good.Succeeded()));
		const FSlaterBuildResult Bad = RtBuild(TEXT("<vbox><rich-text justify=\"middle\">Words</rich-text></vbox>"));
		ASSERT_THAT(IsFalse(Bad.Succeeded()));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
