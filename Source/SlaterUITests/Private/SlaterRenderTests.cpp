// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "SlaterImages.h"
#include "SlaterSurface.h"
#include "SlaterView.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

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

	/** Vector icons are rasterized by the application's own renderer when it draws a window (FWidgetRenderer does not),
	 *  so show the view in a real window for a few ticks after a draw has asked for them; the cache is keyed by file, size and scale. */
	void RdShowInWindow(const TSharedRef<SWidget>& View)
	{
		View->SlatePrepass(1.f);
		const TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(View->GetDesiredSize()).SupportsMaximize(false).SupportsMinimize(false)[View];
		FSlateApplication::Get().AddWindow(Window);
		for (int32 Tick = 0; Tick < 12; ++Tick)
		{
			FSlateApplication::Get().Tick();
			FPlatformProcess::Sleep(0.02f);
			FlushRenderingCommands();
		}
		Window->RequestDestroyWindow();
		FSlateApplication::Get().Tick();
		View->SlatePrepass(1.f);
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

/**
 * Renders the sidebar sample expanded and collapsed to Saved/SlaterUI/Render/Sidebar_<state>.png for review (T-403). The SVG
 * icons only draw with a real RHI, so under -nullrhi this skips. The pixel check: the collapsed image is narrower.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterRenderSidebarTest, "BRS.SlaterUI.Render.Sidebar", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterRenderSidebarTest::RunTest(const FString&)
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
	Slater::SetImageSource(Slater::MakePluginIconSource());
	FWidgetRenderer Renderer(false);
	const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Sidebar"));
	if (TestFalse(TEXT("Sidebar builds"), View->HasErrors()))
	{
		TMap<FString, int32> Widths;
		for (const TCHAR* State : {TEXT("Expanded"), TEXT("Collapsed")})
		{
			if (FCString::Strcmp(State, TEXT("Collapsed")) == 0)
			{
				const TSharedPtr<SSlaterSurface> Hamburger = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("Hamburger")));
				TestTrue(TEXT("Hamburger clicked"), Hamburger.IsValid() && Hamburger->Activate());
			}
			UTextureRenderTarget2D* Target = nullptr;
			// Pass 0 queues the icons for rasterizing, the window ticks do it, pass 1 draws the result.
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				View->SlatePrepass(1.f);
				const FVector2D Size = FVector2D(FMath::CeilToInt(View->GetDesiredSize().X), FMath::CeilToInt(View->GetDesiredSize().Y));
				Target = Renderer.DrawWidget(View, Size);
				FlushRenderingCommands();
				if (Pass == 0)
				{
					RdShowInWindow(View);
				}
			}
			TArray<FColor> Pixels;
			FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
			if (!TestTrue(TEXT("Pixels read back"), Resource && Resource->ReadPixels(Pixels)))
			{
				continue;
			}
			for (FColor& Pixel : Pixels)
			{
				const FColor Encoded = FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f).ToFColorSRGB();
				Pixel = FColor(Encoded.R, Encoded.G, Encoded.B, 255);
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			const FString File = RenderDir() / FString::Printf(TEXT("Sidebar_%s.png"), State);
			TestTrue(*FString::Printf(TEXT("%s written"), *File), FFileHelper::SaveArrayToFile(Png, *File));
			AddInfo(FString::Printf(TEXT("%s (%d x %d)"), *File, Target->SizeX, Target->SizeY));
			Widths.Add(State, Target->SizeX);
		}
		if (Widths.Contains(TEXT("Expanded")) && Widths.Contains(TEXT("Collapsed")))
		{
			TestTrue(TEXT("Collapsed image is narrower"), Widths[TEXT("Collapsed")] < Widths[TEXT("Expanded")]);
		}
	}
	Slater::SetImageSource(nullptr);
	return true;
}

/**
 * Renders the account dashboard sample in Skyline to Saved/SlaterUI/Render/Account_Skyline.png for review (T-408). Icons need a real
 * RHI, so under -nullrhi this skips. The pixel check: the page corner is Skyline's navy background.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterRenderAccountTest, "BRS.SlaterUI.Render.Account", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterRenderAccountTest::RunTest(const FString&)
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
	Slater::SetImageSource(Slater::MakePluginIconSource());
	FWidgetRenderer Renderer(false);
	const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Account")).StyleName(TEXT("Skyline")).UseMockContext(true);
	if (TestFalse(TEXT("Account builds"), View->HasErrors()))
	{
		UTextureRenderTarget2D* Target = nullptr;
		// Pass 0 queues the icons for rasterizing and measures wrapped text, the window ticks rasterize, pass 1 draws the result.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			View->SlatePrepass(1.f);
			const FVector2D Size = FVector2D(FMath::CeilToInt(View->GetDesiredSize().X), FMath::CeilToInt(View->GetDesiredSize().Y));
			Target = Renderer.DrawWidget(View, Size);
			FlushRenderingCommands();
			if (Pass == 0)
			{
				RdShowInWindow(View);
			}
		}
		TArray<FColor> Pixels;
		FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
		if (TestTrue(TEXT("Pixels read back"), Resource && Resource->ReadPixels(Pixels)) && TestEqual(TEXT("Pixel count"), Pixels.Num(), Target->SizeX * Target->SizeY))
		{
			for (FColor& Pixel : Pixels)
			{
				const FColor Encoded = FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f).ToFColorSRGB();
				Pixel = FColor(Encoded.R, Encoded.G, Encoded.B, 255);
			}
			TestTrue(TEXT("Page corner is the Skyline background"), RdClose(Pixels[2 * Target->SizeX + 2], FColor(11, 22, 36), 12));
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			const FString File = RenderDir() / TEXT("Account_Skyline.png");
			TestTrue(*FString::Printf(TEXT("%s written"), *File), FFileHelper::SaveArrayToFile(Png, *File));
			AddInfo(FString::Printf(TEXT("%s (%d x %d)"), *File, Target->SizeX, Target->SizeY));
		}
	}
	Slater::SetImageSource(nullptr);
	return true;
}

/**
 * Renders the controls sample (T-381) in Base and Dusk to Saved/SlaterUI/Render/Settings_<Style>.png for review: the slider,
 * checkbox and text box draw with the engine's core style, so this is where a clash with a SlaterUI style shows.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterRenderSettingsTest, "BRS.SlaterUI.Render.Settings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterRenderSettingsTest::RunTest(const FString&)
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
	for (const TCHAR* Style : {TEXT("Base"), TEXT("Dusk")})
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Settings")).StyleName(Style).UseMockContext(true);
		if (!TestFalse(*FString::Printf(TEXT("Settings builds in %s"), Style), View->HasErrors()))
		{
			continue;
		}
		UTextureRenderTarget2D* Target = nullptr;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			View->SlatePrepass(1.f);
			const FVector2D Size = FVector2D(FMath::CeilToInt(View->GetDesiredSize().X), FMath::CeilToInt(View->GetDesiredSize().Y));
			Target = Renderer.DrawWidget(View, Size);
			FlushRenderingCommands();
		}
		TArray<FColor> Pixels;
		FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
		if (TestTrue(TEXT("Pixels read back"), Resource && Resource->ReadPixels(Pixels)) && TestEqual(TEXT("Pixel count"), Pixels.Num(), Target->SizeX * Target->SizeY))
		{
			for (FColor& Pixel : Pixels)
			{
				const FColor Encoded = FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f).ToFColorSRGB();
				Pixel = FColor(Encoded.R, Encoded.G, Encoded.B, 255);
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			const FString File = RenderDir() / FString::Printf(TEXT("Settings_%s.png"), Style);
			TestTrue(*FString::Printf(TEXT("%s written"), *File), FFileHelper::SaveArrayToFile(Png, *File));
			AddInfo(FString::Printf(TEXT("%s (%d x %d)"), *File, Target->SizeX, Target->SizeY));
		}
	}
	return true;
}

/**
 * Renders the popup menu sample closed, open and with the submenu open to Saved/SlaterUI/Render/ContextMenu_<state>.png for review
 * (T-415). Icons need a real RHI, so under -nullrhi this skips. The pixel check: the open menu changes the picture.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterRenderContextMenuTest, "BRS.SlaterUI.Render.ContextMenu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterRenderContextMenuTest::RunTest(const FString&)
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
	Slater::SetImageSource(Slater::MakePluginIconSource());
	FWidgetRenderer Renderer(false);
	const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ContextMenu"));
	if (TestFalse(TEXT("ContextMenu builds"), View->HasErrors()))
	{
		TArray<TArray<FColor>> Pictures;
		const TArray<TTuple<const TCHAR*, const TCHAR*, const TCHAR*>> Steps = {
			MakeTuple(TEXT("Closed"), TEXT("false"), TEXT("none")),
			MakeTuple(TEXT("Open"), TEXT("true"), TEXT("none")),
			MakeTuple(TEXT("Submenu"), TEXT("true"), TEXT("learn")),
		};
		for (const TTuple<const TCHAR*, const TCHAR*, const TCHAR*>& Step : Steps)
		{
			View->SetState(TEXT("menu"), Step.Get<1>());
			View->SetState(TEXT("sub"), Step.Get<2>());
			UTextureRenderTarget2D* Target = nullptr;
			// Pass 0 queues the icons for rasterizing, the window ticks do it, pass 1 draws the result.
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				View->SlatePrepass(1.f);
				const FVector2D Size = FVector2D(FMath::CeilToInt(View->GetDesiredSize().X), FMath::CeilToInt(View->GetDesiredSize().Y));
				Target = Renderer.DrawWidget(View, Size);
				FlushRenderingCommands();
				if (Pass == 0)
				{
					RdShowInWindow(View);
				}
			}
			TArray<FColor> Pixels;
			FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
			if (!TestTrue(TEXT("Pixels read back"), Resource && Resource->ReadPixels(Pixels)))
			{
				continue;
			}
			for (FColor& Pixel : Pixels)
			{
				const FColor Encoded = FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f).ToFColorSRGB();
				Pixel = FColor(Encoded.R, Encoded.G, Encoded.B, 255);
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			const FString File = RenderDir() / FString::Printf(TEXT("ContextMenu_%s.png"), Step.Get<0>());
			TestTrue(*FString::Printf(TEXT("%s written"), *File), FFileHelper::SaveArrayToFile(Png, *File));
			AddInfo(FString::Printf(TEXT("%s (%d x %d)"), *File, Target->SizeX, Target->SizeY));
			Pictures.Add(MoveTemp(Pixels));
		}
		if (Pictures.Num() == 3)
		{
			TestTrue(TEXT("Opening the menu changes the picture"), Pictures[0] != Pictures[1]);
			TestTrue(TEXT("Opening the submenu changes the picture"), Pictures[1] != Pictures[2]);
		}
	}
	Slater::SetImageSource(nullptr);
	return true;
}

#endif
