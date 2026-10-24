// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterBindTestKit.h"
#include "SlaterElements.h"
#include "SlaterImages.h"
#include "SlaterObjectScope.h"
#include "SlaterView.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace SlaterTooltipTestsPrivate
{
	const FSlaterStyle& TipStyle()
	{
		static FSlaterStyle* Style = []()
		{
			FSlaterStyle* Result = new FSlaterStyle();
			TArray<FSlaterError> Errors;
			Slater::ParseStyle(TEXT(
				"<style name='T'>"
				"<color name='UI.Color.Menu' value='#262A32'/><color name='UI.Color.MenuText' value='#E6E8EC'/>"
				"<metric name='UI.Space.M' value='8'/>"
				"<font name='UI.Font.Body' role='Regular' size='14'/>"
				"<surface name='UI.Surface.Menu' fill='UI.Color.Menu'/>"
				"</style>"), TEXT("Styles/T.sltrstyle"), *Result, Errors);
			return Result;
		}();
		return *Style;
	}

	FSlaterBuildResult TipBuild(const TCHAR* Text)
	{
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, TEXT("Layouts/Test.sltr"), TipStyle(), Result, FSlaterBudget(), Slater::Elements());
		return Result;
	}

	/** The text of the first text block inside the widget's tooltip; empty when it has none. */
	FString TipText(const TSharedPtr<SWidget>& Widget)
	{
		if (!Widget.IsValid() || !Widget->GetToolTip().IsValid())
		{
			return FString();
		}
		TArray<FString> Texts;
		SlaterBindKit::CollectTexts(*Widget->GetToolTip()->AsWidget(), Texts);
		return Texts.Num() > 0 ? Texts[0] : FString();
	}
}
using namespace SlaterTooltipTestsPrivate;

/** The `tooltip` attribute (T-423): engine tooltips on any element, styled by the tokens. */
TEST_CLASS(SlaterTooltipTests, "BRS.SlaterUI.Tooltip")
{
	TEST_METHOD(ALiteralTooltip_BuildsAToolTipOnTheWidget)
	{
		const FSlaterBuildResult Result = TipBuild(TEXT("<vbox><text id=\"A\" tooltip=\"Hello there\">Label</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SWidget> Text = Result.ById.FindRef(TEXT("A")).Pin();
		ASSERT_THAT(IsTrue(Text.IsValid() && Text->GetToolTip().IsValid()));
		ASSERT_THAT(IsTrue(TipText(Text) == TEXT("Hello there")));
	}

	TEST_METHOD(AnEmptyTooltip_AddsNone)
	{
		const FSlaterBuildResult Result = TipBuild(TEXT("<vbox><text id=\"A\" tooltip=\"\">Label</text><text id=\"B\">Label</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsFalse(Result.ById.FindRef(TEXT("A")).Pin()->GetToolTip().IsValid()));
		ASSERT_THAT(IsFalse(Result.ById.FindRef(TEXT("B")).Pin()->GetToolTip().IsValid()));
	}

	TEST_METHOD(ATooltip_OnAnElementWithASizingBox_LandsOnTheOutermostWidget)
	{
		const FSlaterBuildResult Result = TipBuild(TEXT("<vbox><text id=\"A\" width=\"50\" tooltip=\"Sized\">Label</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(TipText(Result.ById.FindRef(TEXT("A")).Pin()) == TEXT("Sized")));
	}

	TEST_METHOD(ATooltip_InATemplateBody_TakesTheParam)
	{
		const FSlaterBuildResult Result = TipBuild(TEXT(
			"<vbox><template name=\"item\" params=\"label\"><text tooltip=\"$label\">x</text></template>"
			"<item id=\"One\" label=\"First\"/><item id=\"Two\" label=\"Second\"/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(TipText(Result.ById.FindRef(TEXT("One")).Pin()) == TEXT("First")));
		ASSERT_THAT(IsTrue(TipText(Result.ById.FindRef(TEXT("Two")).Pin()) == TEXT("Second")));
	}
};

/** A bound tooltip follows the view model, and an empty value removes it. */
TEST_CLASS(SlaterTooltipBindingTests, "BRS.SlaterUI.Tooltip.Bind")
{
	FSlaterBindFixture Fix;

	BEFORE_EACH()
	{
		Fix.Init();
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TEST_METHOD(ABoundTooltip_FollowsTheValue)
	{
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Title = TEXT("First");
		Fix.Write(TEXT("TipBound.sltr"), TEXT("<vbox><text id=\"A\" tooltip=\"{Title}\">Label</text></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TipBound"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Root))));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(TipText(View->FindById(TEXT("A"))) == TEXT("First")));

		Root->SetTitle(TEXT("Second"));
		ASSERT_THAT(IsTrue(TipText(View->FindById(TEXT("A"))) == TEXT("Second")));

		Root->SetTitle(FString());
		ASSERT_THAT(IsFalse(View->FindById(TEXT("A"))->GetToolTip().IsValid()));
	}
};

/** `Sidebar.sltr` labels its collapsed icons (T-423). */
TEST_CLASS(SlaterSidebarTooltipTests, "BRS.SlaterUI.Sidebar.Tooltip")
{
	BEFORE_EACH()
	{
		Slater::SetImageSource(Slater::MakePluginIconSource());
	}

	AFTER_EACH()
	{
		Slater::SetImageSource(nullptr);
	}

	TEST_METHOD(EachNavItem_ShowsItsLabelAsATooltip)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Sidebar"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TPair<const TCHAR*, const TCHAR*> Items[] = {
			{TEXT("NavHome"), TEXT("Home")}, {TEXT("NavProfile"), TEXT("Profile")}, {TEXT("NavMessages"), TEXT("Messages")},
			{TEXT("NavFiles"), TEXT("Files")}, {TEXT("NavSettings"), TEXT("Settings")}, {TEXT("NavLogout"), TEXT("Log out")}};
		for (const TPair<const TCHAR*, const TCHAR*>& Item : Items)
		{
			ASSERT_THAT(IsTrue(TipText(View->FindById(Item.Key)) == Item.Value));
		}
		// The hamburger is the one control that is not a nav item, and it needs a label on the rail too.
		ASSERT_THAT(IsTrue(TipText(View->FindById(TEXT("Hamburger"))) == TEXT("Toggle sidebar")));
	}
};

#endif
