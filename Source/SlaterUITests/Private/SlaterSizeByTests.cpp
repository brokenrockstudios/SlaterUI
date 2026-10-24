// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterBindTestKit.h"
#include "SlaterState.h"
#include "SlaterSurface.h"
#include "SlaterView.h"

namespace SlaterSizeByTestsPrivate
{
	FSlaterBuildResult SbBuild(const FString& Text, const FSlaterStyleStack& Stack)
	{
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}
}
using namespace SlaterSizeByTestsPrivate;

/** `width-by` / `height-by` and `clip` (T-405): sizes that follow a view state. */
TEST_CLASS(SlaterSizeByTests, "BRS.SlaterUI.SizeBy")
{
	FSlaterBindFixture Fix;
	FSlaterStyleStack Stack;

	BEFORE_EACH()
	{
		Fix.Init();
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TSharedRef<SSlaterView> MakeView(const TCHAR* Text)
	{
		Fix.Write(TEXT("SbTest.sltr"), Text);
		return SNew(SSlaterView).LayoutId(TEXT("SbTest"));
	}

	FVector2D Desired(const TSharedRef<SSlaterView>& View, const TCHAR* Id) const
	{
		View->SlatePrepass(1.f);
		const TSharedPtr<SWidget> Widget = View->FindById(Id);
		return Widget.IsValid() ? FVector2D(Widget->GetDesiredSize()) : FVector2D(-1.0, -1.0);
	}

	bool Click(const TSharedRef<SSlaterView>& View, const TCHAR* Id) const
	{
		const TSharedPtr<SSlaterSurface> Surface = StaticCastSharedPtr<SSlaterSurface>(View->FindById(Id));
		return Surface.IsValid() && Surface->Activate();
	}

	TEST_METHOD(TheSizeFollowsTheState_InBothDirections_Instantly)
	{
		const TSharedRef<SSlaterView> View = MakeView(TEXT(
			"<vbox><state name=\"expanded\" default=\"true\"/>"
			"<surface id=\"Toggle\" role=\"UI.Surface.Button\" on-click=\"ui.toggle:expanded\"><text>t</text></surface>"
			"<text id=\"Panel\" width-by=\"expanded: true=100, false=UI.Size.SidebarRail\" height-by=\"expanded: true=30, false=12\">Some label</text></vbox>"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		// The default state is applied at build, before any change.
		ASSERT_THAT(IsNear(Desired(View, TEXT("Panel")).X, 100.0, 0.01));
		ASSERT_THAT(IsNear(Desired(View, TEXT("Panel")).Y, 30.0, 0.01));
		ASSERT_THAT(IsTrue(Click(View, TEXT("Toggle"))));
		// A metric token resolves to its number.
		ASSERT_THAT(IsNear(Desired(View, TEXT("Panel")).X, 60.0, 0.01));
		ASSERT_THAT(IsNear(Desired(View, TEXT("Panel")).Y, 12.0, 0.01));
		ASSERT_THAT(IsTrue(Click(View, TEXT("Toggle"))));
		ASSERT_THAT(IsNear(Desired(View, TEXT("Panel")).X, 100.0, 0.01));
	}

	TEST_METHOD(AValueWithNoCase_GivesTheElementItsContentSize)
	{
		const TSharedRef<SSlaterView> View = MakeView(TEXT(
			"<vbox><state name=\"mode\" default=\"a\"/>"
			"<surface id=\"ToC\" role=\"UI.Surface.Button\" set-state=\"mode=c\"><text>t</text></surface>"
			"<text id=\"Panel\" width-by=\"mode: a=200, b=300\">x</text></vbox>"));
		ASSERT_THAT(IsNear(Desired(View, TEXT("Panel")).X, 200.0, 0.01));
		ASSERT_THAT(IsTrue(Click(View, TEXT("ToC"))));
		ASSERT_THAT(IsTrue(Desired(View, TEXT("Panel")).X < 100.0));
	}

	TEST_METHOD(ClipCutsTheElementToItsBox_AndIsOffByDefault)
	{
		const TSharedRef<SSlaterView> View = MakeView(TEXT(
			"<vbox><text id=\"Cut\" clip=\"true\">a</text><text id=\"Loose\">b</text><text id=\"Off\" clip=\"false\">c</text></vbox>"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->FindById(TEXT("Cut"))->GetClipping() == EWidgetClipping::ClipToBounds));
		// `clip` only turns clipping on; without it the widget keeps its own Slate default (OnDemand for STextBlock on 5.8, Inherit for most others).
		ASSERT_THAT(IsTrue(View->FindById(TEXT("Loose"))->GetClipping() != EWidgetClipping::ClipToBounds));
		ASSERT_THAT(IsTrue(View->FindById(TEXT("Off"))->GetClipping() != EWidgetClipping::ClipToBounds));
	}

	TEST_METHOD(ATemplateBody_CanUseWidthBy_WithAParameter)
	{
		const TSharedRef<SSlaterView> View = MakeView(TEXT(
			"<vbox><template name=\"chip\" params=\"open\"><text width-by=\"expanded: true=$open, false=20\">x</text></template>"
			"<state name=\"expanded\" default=\"true\"/>"
			"<surface id=\"Toggle\" role=\"UI.Surface.Button\" on-click=\"ui.toggle:expanded\"><text>t</text></surface>"
			"<chip id=\"One\" open=\"90\"/><chip id=\"Two\" open=\"140\"/></vbox>"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(AreEqual(View->GetResult().StateSizes.Num(), 2));
		ASSERT_THAT(IsNear(Desired(View, TEXT("One")).X, 90.0, 0.01));
		ASSERT_THAT(IsNear(Desired(View, TEXT("Two")).X, 140.0, 0.01));
		ASSERT_THAT(IsTrue(Click(View, TEXT("Toggle"))));
		ASSERT_THAT(IsNear(Desired(View, TEXT("One")).X, 20.0, 0.01));
		ASSERT_THAT(IsNear(Desired(View, TEXT("Two")).X, 20.0, 0.01));
	}

	TEST_METHOD(AnUndeclaredStateKey_IsRefused_ListingTheDeclaredNames)
	{
		const FSlaterBuildResult None = SbBuild(TEXT("<vbox>\n<text width-by=\"tab: a=1\">x</text>\n</vbox>"), Stack);
		ASSERT_THAT(IsFalse(None.Succeeded()));
		ASSERT_THAT(AreEqual(None.Errors.Num(), 1));
		ASSERT_THAT(AreEqual(None.Errors[0].Line, 2));
		ASSERT_THAT(IsTrue(None.Errors[0].Message.Contains(TEXT("'tab' is not a declared state"))));
		ASSERT_THAT(IsTrue(None.Errors[0].Message.Contains(TEXT("the layout declares none"))));
		const FSlaterBuildResult Other = SbBuild(TEXT("<vbox><state name=\"expanded\" default=\"true\"/><state name=\"mode\" default=\"a\"/><text height-by=\"tab: a=1\">x</text></vbox>"), Stack);
		ASSERT_THAT(IsFalse(Other.Succeeded()));
		ASSERT_THAT(IsTrue(Other.Errors[0].Message.Contains(TEXT("declared: expanded, mode"))));
	}

	TEST_METHOD(BadSizes_AreRefusedAtTheAttribute)
	{
		auto Build = [&](const TCHAR* Attrs) { return SbBuild(FString::Printf(TEXT("<vbox><state name=\"t\" default=\"a\"/><text %s>x</text></vbox>"), Attrs), Stack); };
		const FSlaterBuildResult NoColon = Build(TEXT("width-by=\"t a=1\""));
		ASSERT_THAT(IsFalse(NoColon.Succeeded()));
		ASSERT_THAT(IsTrue(NoColon.Errors[0].Message.Contains(TEXT("is not a state-driven size for width-by"))));
		ASSERT_THAT(IsFalse(Build(TEXT("width-by=\"t:\"")).Succeeded()));
		ASSERT_THAT(IsFalse(Build(TEXT("width-by=\"t: a\"")).Succeeded()));
		ASSERT_THAT(IsFalse(Build(TEXT("width-by=\"t: a=\"")).Succeeded()));
		ASSERT_THAT(IsFalse(Build(TEXT("width-by=\"t: =5\"")).Succeeded()));
		const FSlaterBuildResult Twice = Build(TEXT("width-by=\"t: a=1, a=2\""));
		ASSERT_THAT(IsTrue(Twice.Errors[0].Message.Contains(TEXT("the value 'a' appears twice"))));
		const FSlaterBuildResult Token = Build(TEXT("width-by=\"t: a=UI.Size.Nope\""));
		ASSERT_THAT(IsFalse(Token.Succeeded()));
		ASSERT_THAT(IsTrue(Token.Errors[0].Message.Contains(TEXT("unknown metric token 'UI.Size.Nope'"))));
		const FSlaterBuildResult Negative = Build(TEXT("height-by=\"t: a=-4\""));
		ASSERT_THAT(IsFalse(Negative.Succeeded()));
		ASSERT_THAT(IsFalse(Build(TEXT("width-by=\"{W}\"")).Succeeded()));
		ASSERT_THAT(IsTrue(Build(TEXT("width-by=\"t: a=1 , b = 2\"")).Succeeded()));
	}

	TEST_METHOD(WidthByWithWidth_IsRefused_ButWidthByWithHeightIsFine)
	{
		const FSlaterBuildResult Fight = SbBuild(TEXT("<vbox><state name=\"t\" default=\"a\"/><text width=\"10\" width-by=\"t: a=1\">x</text></vbox>"), Stack);
		ASSERT_THAT(IsFalse(Fight.Succeeded()));
		ASSERT_THAT(IsTrue(Fight.Errors[0].Message.Contains(TEXT("would fight over its size"))));
		const FSlaterBuildResult Fine = SbBuild(TEXT("<vbox><state name=\"t\" default=\"a\"/><text height=\"10\" width-by=\"t: a=1\">x</text></vbox>"), Stack);
		ASSERT_THAT(IsTrue(Fine.Succeeded()));
	}

	TEST_METHOD(WidthByInsideARepeatTemplate_IsRefused)
	{
		const FSlaterBuildResult Result = SbBuild(TEXT("<vbox><state name=\"t\" default=\"a\"/><repeat source=\"{L}\" key=\"Id\"><text width-by=\"t: a=1\">x</text></repeat></vbox>"), Stack);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("'width-by' is not supported inside a template"))));
	}

	TEST_METHOD(ParseSizeBy_SplitsTheKeyAndCases)
	{
		FString Key;
		TArray<Slater::FSizeByCase> Cases;
		ASSERT_THAT(IsTrue(Slater::ParseSizeBy(TEXT(" expanded : true = UI.Size.A , false=12 "), TEXT("width-by"), Key, Cases).IsEmpty()));
		ASSERT_THAT(IsTrue(Key == TEXT("expanded")));
		ASSERT_THAT(AreEqual(Cases.Num(), 2));
		ASSERT_THAT(IsTrue(Cases[0].Value == TEXT("true") && Cases[0].Metric == TEXT("UI.Size.A")));
		ASSERT_THAT(IsTrue(Cases[1].Value == TEXT("false") && Cases[1].Metric == TEXT("12")));
	}
};

#endif
