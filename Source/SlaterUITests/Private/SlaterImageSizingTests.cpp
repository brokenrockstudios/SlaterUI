// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Layout/ArrangedChildren.h"
#include "Misc/AutomationTest.h"
#include "SlaterListTestKit.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SScaleBox.h"

namespace
{
	FSlaterBuildResult IsBuild(const TCHAR* Layout)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}

	/** The size the child of the layout's `<scale>` is given when the scale box is allotted Room. */
	FVector2D ScaledChildSize(const TCHAR* ScaleAttrs, const FVector2D& Room, bool& bOk)
	{
		const FString Layout = FString::Printf(TEXT("<vbox><scale %s><box width='40' height='20'/></scale></vbox>"), ScaleAttrs);
		const FSlaterBuildResult Result = IsBuild(*Layout);
		bOk = Result.Succeeded();
		const TSharedPtr<SWidget> Scale = bOk ? SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SScaleBox")) : nullptr;
		bOk = bOk && Scale.IsValid();
		if (!bOk)
		{
			return FVector2D::ZeroVector;
		}
		Result.Root->SlatePrepass(1.f);
		FArrangedChildren Arranged(EVisibility::All);
		Scale->ArrangeChildren(FGeometry::MakeRoot(FVector2f(Room), FSlateLayoutTransform()), Arranged);
		bOk = Arranged.Num() == 1;
		return bOk ? FVector2D(Arranged[0].Geometry.GetAbsoluteSize()) : FVector2D::ZeroVector;
	}

	/** SImage keeps its tint in a protected attribute; read it through a derived view (adds no data, nothing is constructed). */
	struct FPeekImage : SImage
	{
		static FLinearColor TintOf(const TSharedPtr<SWidget>& Widget)
		{
			return static_cast<const FPeekImage&>(*Widget).GetColorAndOpacityAttribute().Get().GetSpecifiedColor();
		}
	};

	bool NearSize(const FVector2D& Size, float X, float Y)
	{
		return FMath::IsNearlyEqual(float(Size.X), X, 0.5f) && FMath::IsNearlyEqual(float(Size.Y), Y, 0.5f);
	}
}

/** `<scale>`, `<image tint width height>` and `<throbber>` (T-528). */
TEST_CLASS(SlaterImageSizingTests, "BRS.SlaterUI.ImageSizing")
{
	TEST_METHOD(Scale_None_KeepsTheChildsOwnSize)
	{
		bool bOk = false;
		const FVector2D Size = ScaledChildSize(TEXT("stretch='none'"), FVector2D(200, 50), bOk);
		ASSERT_THAT(IsTrue(bOk));
		ASSERT_THAT(IsTrue(NearSize(Size, 40.f, 20.f)));
	}

	TEST_METHOD(Scale_Fit_ScalesUniformlyUntilOneSideTouches)
	{
		bool bOk = false;
		// Default is fit; 40x20 in 200x50 scales by min(5, 2.5).
		const FVector2D Size = ScaledChildSize(TEXT(""), FVector2D(200, 50), bOk);
		ASSERT_THAT(IsTrue(bOk));
		ASSERT_THAT(IsTrue(NearSize(Size, 100.f, 50.f)));
		const FVector2D Explicit = ScaledChildSize(TEXT("stretch='fit'"), FVector2D(200, 50), bOk);
		ASSERT_THAT(IsTrue(bOk && NearSize(Explicit, 100.f, 50.f)));
	}

	TEST_METHOD(Scale_Fill_ScalesUniformlyUntilTheRoomIsCovered)
	{
		bool bOk = false;
		const FVector2D Size = ScaledChildSize(TEXT("stretch='fill'"), FVector2D(200, 50), bOk);
		ASSERT_THAT(IsTrue(bOk));
		ASSERT_THAT(IsTrue(NearSize(Size, 200.f, 100.f)));
	}

	TEST_METHOD(Scale_Stretch_TakesTheWholeRoomWithoutKeepingTheShape)
	{
		bool bOk = false;
		const FVector2D Size = ScaledChildSize(TEXT("stretch='stretch'"), FVector2D(200, 50), bOk);
		ASSERT_THAT(IsTrue(bOk));
		ASSERT_THAT(IsTrue(NearSize(Size, 200.f, 50.f)));
	}

	TEST_METHOD(Scale_User_UsesTheScaleAttribute)
	{
		bool bOk = false;
		const FVector2D Size = ScaledChildSize(TEXT("stretch='user' scale='3'"), FVector2D(200, 50), bOk);
		ASSERT_THAT(IsTrue(bOk));
		ASSERT_THAT(IsTrue(NearSize(Size, 120.f, 60.f)));
	}

	TEST_METHOD(Scale_BadStretchWord_IsRefused)
	{
		const FSlaterBuildResult Result = IsBuild(TEXT("<vbox><scale stretch='zoom'><box/></scale></vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
	}

	TEST_METHOD(Image_Tint_IsAppliedToTheWidget_AndDefaultsToWhite)
	{
		const FSlaterBuildResult Tinted = IsBuild(TEXT("<vbox><image tint='#FF0000'/></vbox>"));
		ASSERT_THAT(IsTrue(Tinted.Succeeded()));
		const TSharedPtr<SWidget> Image = SlaterListKit::Find(Tinted.Root.ToSharedRef(), TEXT("SImage"));
		ASSERT_THAT(IsTrue(Image.IsValid()));
		const FLinearColor Red = FPeekImage::TintOf(Image);
		ASSERT_THAT(IsTrue(Red.R > 0.99f && Red.G < 0.01f && Red.B < 0.01f));

		const FSlaterBuildResult Plain = IsBuild(TEXT("<vbox><image/></vbox>"));
		ASSERT_THAT(IsTrue(Plain.Succeeded()));
		const FLinearColor White = FPeekImage::TintOf(SlaterListKit::Find(Plain.Root.ToSharedRef(), TEXT("SImage")));
		ASSERT_THAT(IsTrue(White == FLinearColor::White));
	}

	TEST_METHOD(Image_WidthAndHeight_GiveANonSquareSize)
	{
		const FSlaterBuildResult Result = IsBuild(TEXT("<vbox><image width='48' height='16'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SWidget> Image = SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SImage"));
		ASSERT_THAT(IsTrue(Image.IsValid()));
		Result.Root->SlatePrepass(1.f);
		const FVector2D Desired = Result.Root->GetDesiredSize();
		ASSERT_THAT(IsTrue(NearSize(Desired, 48.f, 16.f)));
	}

	TEST_METHOD(Image_InsideAScale_Fits)
	{
		const FSlaterBuildResult Result = IsBuild(TEXT("<vbox><scale><image width='48' height='16'/></scale></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SScaleBox")).IsValid()));
	}

	TEST_METHOD(Throbber_Builds_AndSizeSetsItsDiameter)
	{
		const FSlaterBuildResult Small = IsBuild(TEXT("<vbox><throbber size='16'/></vbox>"));
		const FSlaterBuildResult Big = IsBuild(TEXT("<vbox><throbber size='64' color='#00FF00'/></vbox>"));
		ASSERT_THAT(IsTrue(Small.Succeeded() && Big.Succeeded()));
		const TSharedPtr<SWidget> SmallThrobber = SlaterListKit::Find(Small.Root.ToSharedRef(), TEXT("SCircularThrobber"));
		const TSharedPtr<SWidget> BigThrobber = SlaterListKit::Find(Big.Root.ToSharedRef(), TEXT("SCircularThrobber"));
		ASSERT_THAT(IsTrue(SmallThrobber.IsValid() && BigThrobber.IsValid()));
		Small.Root->SlatePrepass(1.f);
		Big.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(NearSize(SmallThrobber->GetDesiredSize(), 16.f, 16.f)));
		ASSERT_THAT(IsTrue(NearSize(BigThrobber->GetDesiredSize(), 64.f, 64.f)));
	}

	TEST_METHOD(Throbber_WithNoAttributes_UsesTheDefaultSize)
	{
		const FSlaterBuildResult Result = IsBuild(TEXT("<vbox><throbber/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SWidget> Throbber = SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SCircularThrobber"));
		ASSERT_THAT(IsTrue(Throbber.IsValid()));
		Result.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(NearSize(Throbber->GetDesiredSize(), 32.f, 32.f)));
	}
};

#endif
