// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "SlaterBindTestKit.h"
#include "SlaterListTestKit.h"
#include "Widgets/Input/SEditableTextBox.h"

namespace SlaterLocTestsPrivate
{
	FSlaterBuildResult LocBuild(const TCHAR* Layout, TArray<FSlaterError>* OutErrors = nullptr)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result);
		if (OutErrors)
		{
			*OutErrors = Result.Errors;
		}
		return Result;
	}

	/** Stands in for a loaded culture: the manager answers these keys with French strings, as it would from a `.locres`. Keys are unique to this file. */
	void LoadFrench()
	{
		FTextLocalizationResource Resource;
		Resource.AddEntry(FTextKey(TEXT("SlaterLocTest")), FTextKey(TEXT("Menu.Play")), TEXT("Play"), TEXT("Jouer"), 0);
		Resource.AddEntry(FTextKey(TEXT("SlaterLocTest")), FTextKey(TEXT("Menu.Name.hint")), TEXT("Your name"), TEXT("Votre nom"), 0);
		Resource.AddEntry(FTextKey(TEXT("SlaterLocTest")), FTextKey(TEXT("Menu.Play.tooltip")), TEXT("Start a game"), TEXT("Lancer une partie"), 0);
		Resource.AddEntry(FTextKey(TEXT("Slater")), FTextKey(TEXT("Menu.Quit")), TEXT("Quit"), TEXT("Quitter"), 0);
		// A translation made for an older source string: it must not be applied to the edited literal.
		Resource.AddEntry(FTextKey(TEXT("SlaterLocTest")), FTextKey(TEXT("Menu.Stale")), TEXT("Old wording"), TEXT("Ancien texte"), 0);
		FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
	}

	FString FirstText(const FSlaterBuildResult& Result)
	{
		TArray<FString> Texts;
		SlaterBindKit::CollectTexts(*Result.Root, Texts);
		return Texts.Num() > 0 ? Texts[0] : FString();
	}
}
using namespace SlaterLocTestsPrivate;

/** Localization of literal text (T-535): the `loc` attribute gives a layout string an `FText` namespace and key. */
TEST_CLASS(SlaterLocTests, "BRS.SlaterUI.Loc")
{
	TEST_METHOD(ALiteralWithLoc_ResolvesThroughTheLoadedCulture)
	{
		LoadFrench();
		const FSlaterBuildResult Result = LocBuild(TEXT("<vbox><text loc=\"SlaterLocTest/Menu.Play\">Play</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(FirstText(Result) == TEXT("Jouer")));
	}

	TEST_METHOD(ALiteralWithoutLoc_StaysTheLiteral)
	{
		LoadFrench();
		const FSlaterBuildResult Result = LocBuild(TEXT("<vbox><text>Play</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(FirstText(Result) == TEXT("Play")));
	}

	TEST_METHOD(AKeyWithoutANamespace_UsesSlater)
	{
		LoadFrench();
		const FSlaterBuildResult Result = LocBuild(TEXT("<vbox><text loc=\"Menu.Quit\">Quit</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(FirstText(Result) == TEXT("Quitter")));
	}

	TEST_METHOD(ATranslationOfAnOlderLiteral_IsNotApplied)
	{
		LoadFrench();
		const FSlaterBuildResult Result = LocBuild(TEXT("<vbox><text loc=\"SlaterLocTest/Menu.Stale\">New wording</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(FirstText(Result) == TEXT("New wording")));
	}

	TEST_METHOD(AKeyNobodyTranslated_ShowsTheLiteral)
	{
		LoadFrench();
		const FSlaterBuildResult Result = LocBuild(TEXT("<vbox><text loc=\"SlaterLocTest/Menu.Missing\">Not translated</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(FirstText(Result) == TEXT("Not translated")));
	}

	TEST_METHOD(TheTextboxHint_UsesTheKeyWithAHintSuffix)
	{
		LoadFrench();
		const FSlaterBuildResult Result = LocBuild(TEXT("<vbox><textbox loc=\"SlaterLocTest/Menu.Name\" hint=\"Your name\"/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SEditableTextBox> Box = StaticCastSharedPtr<SEditableTextBox>(SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SEditableTextBox")));
		ASSERT_THAT(IsTrue(Box.IsValid()));
		ASSERT_THAT(IsTrue(Box->GetHintText().ToString() == TEXT("Votre nom")));
	}

	TEST_METHOD(TheTooltip_UsesTheKeyWithATooltipSuffix)
	{
		LoadFrench();
		const FSlaterBuildResult Result = LocBuild(TEXT("<vbox><text id=\"A\" loc=\"SlaterLocTest/Menu.Play\" tooltip=\"Start a game\">Play</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SWidget> Text = Result.ById.FindRef(TEXT("A")).Pin();
		ASSERT_THAT(IsTrue(Text.IsValid() && Text->GetToolTip().IsValid()));
		TArray<FString> Texts;
		SlaterBindKit::CollectTexts(*Text->GetToolTip()->AsWidget(), Texts);
		ASSERT_THAT(IsTrue(Texts.Num() > 0 && Texts[0] == TEXT("Lancer une partie")));
	}

	TEST_METHOD(ABadKey_IsRefusedAtItsAttribute)
	{
		for (const TCHAR* Bad : {TEXT(""), TEXT("a b"), TEXT("/Key"), TEXT("Ns/"), TEXT("a/b/c")})
		{
			TArray<FSlaterError> Errors;
			const FString Layout = FString::Printf(TEXT("<vbox><text loc=\"%s\">Hi</text></vbox>"), Bad);
			const FSlaterBuildResult Result = LocBuild(*Layout, &Errors);
			ASSERT_THAT(IsFalse(Result.Succeeded()));
			ASSERT_THAT(IsTrue(Errors.Num() > 0 && Errors[0].Message.Contains(TEXT("not a localization key"))));
		}
	}
};

#endif
