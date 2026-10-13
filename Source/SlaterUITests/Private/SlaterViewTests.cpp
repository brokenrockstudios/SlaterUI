// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SlaterSurface.h"
#include "SlaterView.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const TCHAR* const VwPath = TEXT("Layouts/Test.sltr");

	FSlaterStyleStack VwStack(const TCHAR* Name = TEXT("Base"))
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(Name, Stack);
		return Stack;
	}

	FSlaterBuildResult VwBuild(const TCHAR* Text, const TCHAR* StyleName = TEXT("Base"))
	{
		const FSlaterStyleStack Stack = VwStack(StyleName);
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, VwPath, Stack, Result);
		return Result;
	}

	/** First widget in the tree (Root included) of this Slate type, or null. */
	TSharedPtr<SWidget> VwFind(const TSharedRef<SWidget>& Root, const TCHAR* Type)
	{
		if (Root->GetType() == FName(Type))
		{
			return Root;
		}
		TSharedPtr<SWidget> Found;
		Root->GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			if (!Found.IsValid())
			{
				Found = VwFind(Child.AsShared(), Type);
			}
		});
		return Found;
	}

	FSlaterSurface VwSurfaceToken(const TCHAR* Token, const TCHAR* StyleName = TEXT("Base"))
	{
		FSlaterSurface Surface;
		VwStack(StyleName).FindSurface(Token, Surface);
		return Surface;
	}

	FVector2D VwDesired(const TSharedRef<SWidget>& Widget)
	{
		Widget->SlatePrepass(1.f);
		return FVector2D(Widget->GetDesiredSize());
	}

	bool VwSame(const FLinearColor& A, const FLinearColor& B)
	{
		return A == B;
	}
}

/** The two content elements, built through the stock registry against the shipped style. */
TEST_CLASS(SlaterContentElementTests, "BRS.SlaterUI.Content.Elements")
{
	TEST_METHOD(Surface_UsesTheRolesFillOutlineAndRadius)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<surface role='UI.Surface.Panel'/>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SWidget> Found = VwFind(Result.Root.ToSharedRef(), TEXT("SSlaterSurface"));
		ASSERT_THAT(IsTrue(Found.IsValid()));
		const FSlaterSurface Expected = VwSurfaceToken(TEXT("UI.Surface.Panel"));
		const FSlaterSurface& Actual = StaticCastSharedPtr<SSlaterSurface>(Found)->GetSurface();
		ASSERT_THAT(IsTrue(VwSame(Actual.Fill, Expected.Fill)));
		ASSERT_THAT(IsTrue(VwSame(Actual.Border, Expected.Border)));
		ASSERT_THAT(IsTrue(Actual.BorderWidth == 1.f));
		ASSERT_THAT(IsTrue(Actual.CornerRadius == 4.f));
	}

	TEST_METHOD(Surface_StateWithItsOwnDefinition_UsesIt)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<surface role='UI.Surface.Button' state='hover'/>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const FSlaterSurface& Actual = StaticCastSharedPtr<SSlaterSurface>(VwFind(Result.Root.ToSharedRef(), TEXT("SSlaterSurface")))->GetSurface();
		ASSERT_THAT(IsTrue(VwSame(Actual.Fill, VwSurfaceToken(TEXT("UI.Surface.Button.Hover")).Fill)));
		ASSERT_THAT(IsFalse(VwSame(Actual.Fill, VwSurfaceToken(TEXT("UI.Surface.Button")).Fill)));
	}

	TEST_METHOD(Surface_StateWithoutADefinition_FallsBackToTheRole)
	{
		// Base defines no UI.Surface.Panel.Hover.
		const FSlaterBuildResult Result = VwBuild(TEXT("<surface role='UI.Surface.Panel' state='hover'/>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const FSlaterSurface& Actual = StaticCastSharedPtr<SSlaterSurface>(VwFind(Result.Root.ToSharedRef(), TEXT("SSlaterSurface")))->GetSurface();
		const FSlaterSurface Panel = VwSurfaceToken(TEXT("UI.Surface.Panel"));
		ASSERT_THAT(IsTrue(VwSame(Actual.Fill, Panel.Fill)));
		ASSERT_THAT(IsTrue(Actual.BorderWidth == Panel.BorderWidth));
	}

	TEST_METHOD(Surface_Dusk_RecolorsTheSameLayout)
	{
		const FSlaterBuildResult Base = VwBuild(TEXT("<surface role='UI.Surface.Panel'/>"), TEXT("Base"));
		const FSlaterBuildResult Dusk = VwBuild(TEXT("<surface role='UI.Surface.Panel'/>"), TEXT("Dusk"));
		ASSERT_THAT(IsTrue(Base.Succeeded() && Dusk.Succeeded()));
		const FSlaterSurface& A = StaticCastSharedPtr<SSlaterSurface>(VwFind(Base.Root.ToSharedRef(), TEXT("SSlaterSurface")))->GetSurface();
		const FSlaterSurface& B = StaticCastSharedPtr<SSlaterSurface>(VwFind(Dusk.Root.ToSharedRef(), TEXT("SSlaterSurface")))->GetSurface();
		ASSERT_THAT(IsFalse(VwSame(A.Fill, B.Fill)));
		ASSERT_THAT(IsTrue(A.CornerRadius == B.CornerRadius));
	}

	TEST_METHOD(Surface_InsetAndChildMargin_AddUpInsideTheBorder)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<surface role='UI.Surface.Panel' inset='6'><spacer margin='2'/></surface>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SWidget> Found = VwFind(Result.Root.ToSharedRef(), TEXT("SSlaterSurface"));
		ASSERT_THAT(IsTrue(Found.IsValid()));
		// Border padding of 8 on each side around a spacer with no size.
		const FVector2D Desired = VwDesired(Found.ToSharedRef());
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Desired.X, 16.f) && FMath::IsNearlyEqual(Desired.Y, 16.f)));
	}

	TEST_METHOD(Surface_NeedsARole_AndRefusesAnUnknownOne)
	{
		FSlaterBuildResult Result = VwBuild(TEXT("<surface/>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("needs the attribute 'role'"))));

		Result = VwBuild(TEXT("<surface role='UI.Surface.Pnael'/>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("did you mean 'UI.Surface.Panel'"))));
	}

	TEST_METHOD(Surface_UnknownState_ListsTheWords)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<surface role='UI.Surface.Panel' state='hovered'/>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("allowed: normal, hover, pressed, disabled"))));
	}

	TEST_METHOD(Surface_TakesOneChild)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<surface role='UI.Surface.Panel'><spacer/><spacer/></surface>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("takes one child"))));
	}

	TEST_METHOD(Text_ShowsItsCharacterData)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<text>Hello &amp; welcome</text>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SWidget> Found = VwFind(Result.Root.ToSharedRef(), TEXT("STextBlock"));
		ASSERT_THAT(IsTrue(Found.IsValid()));
		ASSERT_THAT(AreEqual(StaticCastSharedPtr<STextBlock>(Found)->GetText().ToString(), FString(TEXT("Hello & welcome"))));
	}

	TEST_METHOD(Text_DefaultsToTheBodyFontAndTextColor)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<text>x</text>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedRef<STextBlock> Block = StaticCastSharedPtr<STextBlock>(VwFind(Result.Root.ToSharedRef(), TEXT("STextBlock"))).ToSharedRef();
		FLinearColor Expected;
		VwStack().FindColor(TEXT("UI.Color.Text"), Expected);
		ASSERT_THAT(IsTrue(VwSame(Block->GetColorAndOpacity().GetSpecifiedColor(), Expected)));
		ASSERT_THAT(IsTrue(Block->GetFont().Size == 14.f));
	}

	TEST_METHOD(Text_FontAndColorTokens_AreApplied)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<text font='UI.Font.Heading' color='UI.Color.Danger'>x</text>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedRef<STextBlock> Block = StaticCastSharedPtr<STextBlock>(VwFind(Result.Root.ToSharedRef(), TEXT("STextBlock"))).ToSharedRef();
		FLinearColor Expected;
		VwStack().FindColor(TEXT("UI.Color.Danger"), Expected);
		ASSERT_THAT(IsTrue(VwSame(Block->GetColorAndOpacity().GetSpecifiedColor(), Expected)));
		ASSERT_THAT(IsTrue(Block->GetFont().Size == 20.f));
	}

	TEST_METHOD(Text_Wrap_AndJustify)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<text wrap='true' justify='center'>x</text>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(VwFind(Result.Root.ToSharedRef(), TEXT("STextBlock")).IsValid()));
	}

	TEST_METHOD(Text_RefusesChildren_AndAnUnknownJustify)
	{
		FSlaterBuildResult Result = VwBuild(TEXT("<text>x<spacer/></text>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("cannot have child elements"))));

		Result = VwBuild(TEXT("<text justify='middle'>x</text>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("allowed: left, center, right"))));
	}

	TEST_METHOD(Text_RefusesAnUnknownFontToken_WithASuggestion)
	{
		const FSlaterBuildResult Result = VwBuild(TEXT("<text font='UI.Font.Bodyy'>x</text>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("did you mean 'UI.Font.Body'"))));
	}

	TEST_METHOD(ContentElements_AreRegistered)
	{
		TArray<FString> Tags;
		Slater::Elements().GetTags(Tags);
		ASSERT_THAT(IsTrue(Tags.Contains(TEXT("surface"))));
		ASSERT_THAT(IsTrue(Tags.Contains(TEXT("text"))));
	}
};

/** Finding, size-checking, parsing and building layout files. */
TEST_CLASS(SlaterLayoutLoaderTests, "BRS.SlaterUI.Content.Loader")
{
	FString TempDir;

	BEFORE_EACH()
	{
		TempDir = FPaths::ProjectSavedDir() / TEXT("SlaterUI/Tests");
		IFileManager::Get().MakeDirectory(*TempDir, true);
	}

	AFTER_EACH()
	{
		IFileManager::Get().DeleteDirectory(*TempDir, false, true);
	}

	TEST_METHOD(LayoutIds_AcceptSegmentsOfWordCharacters_AndNothingElse)
	{
		for (const TCHAR* Good : {TEXT("Panel"), TEXT("Hud/Health"), TEXT("a_b-c/d9")})
		{
			ASSERT_THAT(IsTrue(Slater::IsValidLayoutId(Good)));
		}
		for (const TCHAR* Bad : {TEXT(""), TEXT("/Panel"), TEXT("Panel/"), TEXT("a//b"), TEXT("../Panel"), TEXT("Panel.sltr"), TEXT("a b"), TEXT("a\\b")})
		{
			ASSERT_THAT(IsFalse(Slater::IsValidLayoutId(Bad)));
		}
	}

	TEST_METHOD(ShippedLayouts_AreFound)
	{
		ASSERT_THAT(IsTrue(Slater::FindLayoutFile(TEXT("Gallery")).EndsWith(TEXT("Gallery.sltr"))));
		ASSERT_THAT(IsTrue(Slater::FindLayoutFile(TEXT("Panel")).EndsWith(TEXT("Panel.sltr"))));
		ASSERT_THAT(IsTrue(Slater::FindLayoutFile(TEXT("NoSuchLayout")).IsEmpty()));
		ASSERT_THAT(IsTrue(Slater::FindLayoutFile(TEXT("../Panel")).IsEmpty()));
	}

	TEST_METHOD(ShippedLayouts_BuildInBothStyles)
	{
		for (const TCHAR* Style : {TEXT("Base"), TEXT("Dusk")})
		{
			for (const TCHAR* Id : {TEXT("Gallery"), TEXT("Panel")})
			{
				const FSlaterStyleStack Stack = VwStack(Style);
				FSlaterBuildResult Result;
				const bool bOk = Slater::LoadLayout(Id, Stack, Result);
				for (const FSlaterError& Error : Result.Errors)
				{
					AddError(Error.ToString());
				}
				for (const FSlaterError& Warning : Result.Warnings)
				{
					AddError(Warning.ToString());
				}
				ASSERT_THAT(IsTrue(bOk));
				ASSERT_THAT(IsTrue(Result.WidgetCount > 5));
			}
		}
	}

	TEST_METHOD(MissingLayout_IsAnError_NamingTheFoldersSearched)
	{
		FSlaterBuildResult Result;
		ASSERT_THAT(IsFalse(Slater::LoadLayout(TEXT("NoSuchLayout"), VwStack(), Result)));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("no such layout; searched"))));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("UI/Layouts"))));
	}

	TEST_METHOD(InvalidId_IsAnError)
	{
		FSlaterBuildResult Result;
		ASSERT_THAT(IsFalse(Slater::LoadLayout(TEXT("../Panel"), VwStack(), Result)));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("is not a layout ID"))));
	}

	TEST_METHOD(FileOverTheCap_IsRefusedBeforeItIsRead)
	{
		// 200 bytes against a cap of 10 characters (40 bytes). The text would parse, so only the size check can fail it.
		const FString File = TempDir / TEXT("Big.sltr");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(FString(TEXT("<spacer/>")) + FString::ChrN(191, ' '), *File)));
		FSlaterLimits Limits;
		Limits.MaxChars = 10;
		FSlaterBuildResult Result;
		ASSERT_THAT(IsFalse(Slater::LoadLayoutFile(File, TEXT("Big.sltr"), VwStack(), Result, FSlaterBudget(), Slater::Elements(), Limits)));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("larger than 10 characters"))));
	}

	TEST_METHOD(FileUnderTheByteBound_ButOverTheCharCap_FailsInTheParser)
	{
		// 30 bytes is under 10 * 4, so the size check passes and the parser's own cap refuses it.
		const FString File = TempDir / TEXT("Mid.sltr");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(FString(TEXT("<spacer/>")) + FString::ChrN(21, ' '), *File)));
		FSlaterLimits Limits;
		Limits.MaxChars = 10;
		FSlaterBuildResult Result;
		ASSERT_THAT(IsFalse(Slater::LoadLayoutFile(File, TEXT("Mid.sltr"), VwStack(), Result, FSlaterBudget(), Slater::Elements(), Limits)));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(IsFalse(Result.Errors[0].Message.Contains(TEXT("larger than"))));
	}

	TEST_METHOD(SyntaxError_CarriesTheLabelAndPosition)
	{
		const FString File = TempDir / TEXT("Bad.sltr");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(TEXT("<vbox>\n  <spacer>\n</vbox>"), *File)));
		FSlaterBuildResult Result;
		ASSERT_THAT(IsFalse(Slater::LoadLayoutFile(File, TEXT("UI/Layouts/Bad.sltr"), VwStack(), Result)));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(IsTrue(Result.Errors[0].ToString().StartsWith(TEXT("UI/Layouts/Bad.sltr("))));
		ASSERT_THAT(AreEqual(Result.Errors[0].Line, 3));
	}

	TEST_METHOD(BuildError_CarriesTheLabelAndPosition)
	{
		const FString File = TempDir / TEXT("Typo.sltr");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(TEXT("<vbox>\n  <txet>x</txet>\n</vbox>"), *File)));
		FSlaterBuildResult Result;
		ASSERT_THAT(IsFalse(Slater::LoadLayoutFile(File, TEXT("Typo.sltr"), VwStack(), Result)));
		ASSERT_THAT(AreEqual(Result.Errors[0].Line, 2));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("did you mean 'text'"))));
	}

	TEST_METHOD(MissingFile_IsAnError)
	{
		FSlaterBuildResult Result;
		ASSERT_THAT(IsFalse(Slater::LoadLayoutFile(TempDir / TEXT("Nope.sltr"), TEXT("Nope.sltr"), VwStack(), Result)));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("does not exist"))));
	}
};

/** The widget that shows a layout. */
TEST_CLASS(SlaterViewWidgetTests, "BRS.SlaterUI.Content.View")
{
	TEST_METHOD(ValidLayout_IsShown)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Panel"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->GetResult().WidgetCount > 5));
		ASSERT_THAT(IsTrue(VwFind(View, TEXT("SSlaterSurface")).IsValid()));
		ASSERT_THAT(IsTrue(VwDesired(View).X >= 360.f));
	}

	TEST_METHOD(BrokenLayout_ShowsItsErrorsAsText)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("NoSuchLayout"));
		ASSERT_THAT(IsTrue(View->HasErrors()));
		const TSharedPtr<SWidget> Text = VwFind(View, TEXT("STextBlock"));
		ASSERT_THAT(IsTrue(Text.IsValid()));
		ASSERT_THAT(IsTrue(StaticCastSharedPtr<STextBlock>(Text)->GetText().ToString().Contains(TEXT("no such layout"))));
	}

	TEST_METHOD(SetLayout_AfterAFailure_Recovers)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("NoSuchLayout"));
		ASSERT_THAT(IsTrue(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->SetLayout(TEXT("Gallery"))));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(AreEqual(View->GetLayoutId(), FString(TEXT("Gallery"))));
	}

	TEST_METHOD(SetStyle_RebuildsInTheOtherStyle)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Panel")).StyleName(TEXT("Base"));
		const FSlaterSurface Before = StaticCastSharedPtr<SSlaterSurface>(VwFind(View, TEXT("SSlaterSurface")))->GetSurface();
		ASSERT_THAT(IsTrue(View->SetStyle(TEXT("Dusk"))));
		const FSlaterSurface After = StaticCastSharedPtr<SSlaterSurface>(VwFind(View, TEXT("SSlaterSurface")))->GetSurface();
		ASSERT_THAT(IsFalse(VwSame(Before.Fill, After.Fill)));
	}

	TEST_METHOD(SetStyle_Unknown_FailsAndKeepsTheOldStyle)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Panel"));
		ASSERT_THAT(IsFalse(View->SetStyle(TEXT("NoSuchStyle"))));
		ASSERT_THAT(IsTrue(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->SetStyle(TEXT("Base"))));
		ASSERT_THAT(IsFalse(View->HasErrors()));
	}

	TEST_METHOD(NoLayoutId_ShowsNothing)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView);
		ASSERT_THAT(IsTrue(View->GetResult().Errors.Num() == 0));
	}
};

#endif
