// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "SlaterView.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SWidget.h"

namespace
{
	const FString RenderDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("SlaterUI/Render");
	}

	/** True when A is within Tolerance of B on every channel. */
	bool RdClose(const FColor& A, const FColor& B, int32 Tolerance)
	{
		return FMath::Abs(int32(A.R) - int32(B.R)) <= Tolerance && FMath::Abs(int32(A.G) - int32(B.G)) <= Tolerance && FMath::Abs(int32(A.B) - int32(B.B)) <= Tolerance;
	}

	double RdMedianMs(TArray<double>& Seconds)
	{
		Seconds.Sort();
		return Seconds[Seconds.Num() / 2] * 1000.0;
	}
}

/**
 * Renders the gallery and the panel in both shipped styles to PNGs in Saved/SlaterUI/Render for review. There is no
 * golden image: the only pixel checks are that the background is the style's background and the two styles differ.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterRenderLayoutsTest, "BRS.SlaterUI.Render.GalleryAndPanel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterRenderLayoutsTest::RunTest(const FString&)
{
	if (GUsingNullRHI)
	{
		AddInfo(TEXT("Skipped: requires a real RHI (run with -WithRHI)."));
		return true;
	}
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		AddInfo(TEXT("Skipped: no Slate renderer in this run."));
		return true;
	}

	IFileManager::Get().MakeDirectory(*RenderDir(), true);
	FWidgetRenderer Renderer(false);

	TMap<FString, FColor> Corners;
	for (const TCHAR* Style : {TEXT("Base"), TEXT("Dusk")})
	{
		for (const TCHAR* Layout : {TEXT("Gallery"), TEXT("Panel")})
		{
			const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(Layout).StyleName(Style);
			if (!TestFalse(FString::Printf(TEXT("%s in %s builds"), Layout, Style), View->HasErrors()))
			{
				for (const FSlaterError& Error : View->GetResult().Errors)
				{
					AddError(Error.ToString());
				}
				continue;
			}

			// Wrapped text only knows its width after a paint, so draw once, measure again and draw the final image.
			UTextureRenderTarget2D* Target = nullptr;
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				View->SlatePrepass(1.f);
				const FVector2D Size = FVector2D(FMath::CeilToInt(View->GetDesiredSize().X), FMath::CeilToInt(View->GetDesiredSize().Y));
				Target = Renderer.DrawWidget(View, Size);
				FlushRenderingCommands();
			}
			if (!TestNotNull(TEXT("Render target"), Target))
			{
				continue;
			}

			TArray<FColor> Pixels;
			FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
			if (!TestTrue(TEXT("Pixels read back"), Resource && Resource->ReadPixels(Pixels)))
			{
				continue;
			}
			const int32 Width = Target->SizeX;
			const int32 Height = Target->SizeY;
			TestEqual(TEXT("Pixel count"), Pixels.Num(), Width * Height);
			if (Pixels.Num() != Width * Height)
			{
				continue;
			}

			// The target holds linear values (no gamma pass), so encode to sRGB for the PNG and the checks.
			for (FColor& Pixel : Pixels)
			{
				const FColor Encoded = FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f).ToFColorSRGB();
				Pixel = FColor(Encoded.R, Encoded.G, Encoded.B, 255);
			}

			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			const FString File = RenderDir() / FString::Printf(TEXT("%s_%s.png"), Layout, Style);
			TestTrue(*FString::Printf(TEXT("%s written"), *File), FFileHelper::SaveArrayToFile(Png, *File));
			AddInfo(FString::Printf(TEXT("%s (%d x %d)"), *File, Width, Height));

			// The gallery's root surface is the style background; the panel's corner is the page behind its rounded corner.
			if (FCString::Strcmp(Layout, TEXT("Gallery")) == 0)
			{
				Corners.Add(Style, Pixels[2 * Width + Width / 2]);
			}
		}
	}

	if (Corners.Contains(TEXT("Base")) && Corners.Contains(TEXT("Dusk")))
	{
		TestTrue(TEXT("Base background is light"), RdClose(Corners[TEXT("Base")], FColor(242, 243, 245), 12));
		TestTrue(TEXT("Dusk background is dark"), RdClose(Corners[TEXT("Dusk")], FColor(20, 22, 27), 12));
	}
	return true;
}

/** Parse and build cost of the two layouts, written to Saved/SlaterUI/Metrics.txt and the log, for DevNotes. */
TEST_CLASS(SlaterLayoutCostTests, "BRS.SlaterUI.Render.Cost")
{
	TEST_METHOD(ParseAndBuild_AreMeasured)
	{
		IFileManager::Get().MakeDirectory(*RenderDir(), true);
		FSlaterStyleStack Stack;
		ASSERT_THAT(IsTrue(Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack)));

		FString Report;
		for (const TCHAR* Layout : {TEXT("Gallery"), TEXT("Panel")})
		{
			FString Text;
			ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Text, *Slater::FindLayoutFile(Layout))));

			TArray<double> ParseTimes;
			TArray<double> BuildTimes;
			int32 Widgets = 0;
			int32 Depth = 0;
			for (int32 Run = 0; Run < 101; ++Run)
			{
				FSlaterNode Root;
				FSlaterError Error;
				const double T0 = FPlatformTime::Seconds();
				ASSERT_THAT(IsTrue(Slater::ParseMarkup(Text, Layout, Root, Error)));
				const double T1 = FPlatformTime::Seconds();
				FSlaterBuildResult Result;
				ASSERT_THAT(IsTrue(Slater::BuildLayout(Root, Layout, Stack, Result)));
				const double T2 = FPlatformTime::Seconds();
				if (Run > 0)
				{
					// The first run warms caches (fonts, FName tables) and is not counted.
					ParseTimes.Add(T1 - T0);
					BuildTimes.Add(T2 - T1);
				}
				Widgets = Result.WidgetCount;
				Depth = Result.WidgetDepth;
			}
			Report += FString::Printf(TEXT("%s: %d chars, %d widgets, %d deep, parse %.3f ms, build %.3f ms (median of %d runs)\n"), Layout, Text.Len(), Widgets, Depth, RdMedianMs(ParseTimes), RdMedianMs(BuildTimes), ParseTimes.Num());
		}
		AddInfo(Report);
		FFileHelper::SaveStringToFile(Report, *(RenderDir() / TEXT("Metrics.txt")));
	}
};

#endif
