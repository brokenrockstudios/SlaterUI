// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "SlaterListTestKit.h"
#include "SlaterSurface.h"
#include "SlaterView.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

namespace
{
	FSlaterBuildResult ChBuild(const TCHAR* Layout)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}

	TSharedPtr<SSlaterSurface> ChSurface(const FSlaterBuildResult& Result)
	{
		return Result.Root.IsValid() ? StaticCastSharedPtr<SSlaterSurface>(SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SSlaterSurface"))) : nullptr;
	}

	bool ChHasError(const FSlaterBuildResult& Result, const TCHAR* Text)
	{
		for (const FSlaterError& Error : Result.Errors)
		{
			if (Error.ToString().Contains(Text))
			{
				return true;
			}
		}
		return false;
	}

	bool ChNear(const FVector2f& A, float X, float Y)
	{
		return FMath::IsNearlyEqual(A.X, X, 0.001f) && FMath::IsNearlyEqual(A.Y, Y, 0.001f);
	}

	bool ChColorNear(const FColor& A, const FColor& B, int32 Tolerance)
	{
		return FMath::Abs(int32(A.R) - B.R) <= Tolerance && FMath::Abs(int32(A.G) - B.G) <= Tolerance && FMath::Abs(int32(A.B) - B.B) <= Tolerance;
	}

	/** Draws a widget twice (text and vector images settle on the second pass) and reads it back as sRGB; empty when there is no target. */
	bool ChRender(const TSharedRef<SWidget>& Widget, const FVector2D& Size, TArray<FColor>& OutPixels, int32& OutWidth)
	{
		FWidgetRenderer Renderer(false);
		UTextureRenderTarget2D* Target = nullptr;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Widget->SlatePrepass(1.f);
			Target = Renderer.DrawWidget(Widget, Size);
			FlushRenderingCommands();
		}
		FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
		if (!Resource || !Resource->ReadPixels(OutPixels) || OutPixels.Num() != Target->SizeX * Target->SizeY)
		{
			return false;
		}
		OutWidth = Target->SizeX;
		for (FColor& Pixel : OutPixels)
		{
			const FColor Encoded = FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f).ToFColorSRGB();
			Pixel = FColor(Encoded.R, Encoded.G, Encoded.B, 255);
		}
		return true;
	}

	void ChSavePng(const TArray<FColor>& Pixels, int32 Width, const TCHAR* Name)
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("SlaterUI/Render");
		IFileManager::Get().MakeDirectory(*Dir, true);
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Width, Pixels.Num() / Width, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		FFileHelper::SaveArrayToFile(Png, *(Dir / Name));
	}
}

/** `shape="chamfer"` on `surface`: the outline path, the attributes, and the refusals. No world, no RHI. */
TEST_CLASS(SlaterChamferTests, "BRS.SlaterUI.Chamfer")
{
	TEST_METHOD(Path_WithOneCut_HasTheCutCornerAsTwoPoints_ClockwiseFromTheTopLeft)
	{
		FSlaterChamfer Cuts;
		Cuts.TopLeft = 10.f;
		TArray<FVector2f> Path;
		Slater::MakeChamferPath(FVector2f(100.f, 50.f), Cuts, 0.f, Path);
		ASSERT_THAT(AreEqual(Path.Num(), 5));
		ASSERT_THAT(IsTrue(ChNear(Path[0], 0.f, 10.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[1], 10.f, 0.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[2], 100.f, 0.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[3], 100.f, 50.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[4], 0.f, 50.f)));
	}

	TEST_METHOD(Path_WithAllCuts_HasEightPoints_AndEachCornerIsCutBySizeOfItsOwn)
	{
		FSlaterChamfer Cuts;
		Cuts.TopLeft = 5.f;
		Cuts.TopRight = 6.f;
		Cuts.BottomRight = 7.f;
		Cuts.BottomLeft = 8.f;
		TArray<FVector2f> Path;
		Slater::MakeChamferPath(FVector2f(100.f, 50.f), Cuts, 0.f, Path);
		ASSERT_THAT(AreEqual(Path.Num(), 8));
		ASSERT_THAT(IsTrue(ChNear(Path[1], 5.f, 0.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[2], 94.f, 0.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[3], 100.f, 6.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[4], 100.f, 43.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[5], 93.f, 50.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[6], 8.f, 50.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[7], 0.f, 42.f)));
	}

	TEST_METHOD(Path_Inset_MovesEveryEdgeInwardsByTheInset)
	{
		// A diagonal x + y = c moves inwards by d to x + y = c + d * sqrt(2); its ends stay on the shrunk box.
		FSlaterChamfer Cuts;
		Cuts.TopLeft = 10.f;
		TArray<FVector2f> Path;
		Slater::MakeChamferPath(FVector2f(100.f, 50.f), Cuts, 2.f, Path);
		ASSERT_THAT(AreEqual(Path.Num(), 5));
		const float Cut = 10.f + 2.f * (UE_SQRT_2 - 2.f);
		ASSERT_THAT(IsTrue(ChNear(Path[0], 2.f, 2.f + Cut)));
		ASSERT_THAT(IsTrue(ChNear(Path[1], 2.f + Cut, 2.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[3], 98.f, 48.f)));
		// The inset diagonal is 2 units from the original one.
		const FVector2f Along = Path[1] - Path[0];
		const FVector2f Normal = FVector2f(1.f, 1.f).GetSafeNormal();
		const float Distance = FVector2f::DotProduct(Path[0] - FVector2f(0.f, 10.f), Normal);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Distance, 2.f, 0.001f)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyZero(FVector2f::DotProduct(Along, Normal), 0.001f)));
	}

	TEST_METHOD(Path_CutsThatDoNotFitAnEdge_AreScaledDownTogether)
	{
		FSlaterChamfer Cuts;
		Cuts.TopLeft = 60.f;
		Cuts.TopRight = 60.f;
		TArray<FVector2f> Path;
		Slater::MakeChamferPath(FVector2f(100.f, 100.f), Cuts, 0.f, Path);
		ASSERT_THAT(AreEqual(Path.Num(), 6));
		// 60 + 60 on a 100 edge scales both by 100 / 120.
		ASSERT_THAT(IsTrue(ChNear(Path[1], 50.f, 0.f)));
		ASSERT_THAT(IsTrue(ChNear(Path[2], 50.f, 0.f)));
	}

	TEST_METHOD(Path_ABoxSmallerThanItsInset_IsEmpty)
	{
		TArray<FVector2f> Path;
		FSlaterChamfer Cuts;
		Cuts.TopLeft = 4.f;
		Slater::MakeChamferPath(FVector2f(3.f, 3.f), Cuts, 2.f, Path);
		ASSERT_THAT(IsTrue(Path.IsEmpty()));
	}

	TEST_METHOD(Surface_WithCut_IsChamfered_AllCornersTheSame)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='chamfer' cut='12'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SSlaterSurface> Surface = ChSurface(Result);
		ASSERT_THAT(IsTrue(Surface.IsValid()));
		ASSERT_THAT(IsTrue(Surface->IsChamfered()));
		ASSERT_THAT(IsTrue(Surface->GetChamfer() == FSlaterChamfer{12.f, 12.f, 12.f, 12.f}));
	}

	TEST_METHOD(Surface_CornerCuts_OverrideCut_AndAnOmittedCornerTakesIt)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='chamfer' cut='6' cut-tl='20' cut-br='0'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SSlaterSurface> Surface = ChSurface(Result);
		ASSERT_THAT(IsTrue(Surface.IsValid()));
		ASSERT_THAT(IsTrue(Surface->GetChamfer() == FSlaterChamfer{20.f, 6.f, 0.f, 6.f}));
	}

	TEST_METHOD(Surface_OnlyCornerCuts_LeavesTheOthersSquare)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='chamfer' cut-tr='9'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(ChSurface(Result)->GetChamfer() == FSlaterChamfer{0.f, 9.f, 0.f, 0.f}));
	}

	TEST_METHOD(Surface_CutTakesAMetricToken)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='chamfer' cut='UI.Space.L'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		float Space = 0.f;
		ASSERT_THAT(IsTrue(Stack.FindMetric(TEXT("UI.Space.L"), Space) && Space > 0.f));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ChSurface(Result)->GetChamfer().TopLeft, Space)));
	}

	TEST_METHOD(Surface_WithoutShape_OrRounded_IsTheRoundedBox)
	{
		const FSlaterBuildResult Plain = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsTrue(Plain.Succeeded()));
		ASSERT_THAT(IsFalse(ChSurface(Plain)->IsChamfered()));
		const FSlaterBuildResult Rounded = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='rounded'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsTrue(Rounded.Succeeded()));
		ASSERT_THAT(IsFalse(ChSurface(Rounded)->IsChamfered()));
	}

	TEST_METHOD(Surface_ChamferWithNoCut_IsRefused_AndSaysWhatToWrite)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='chamfer'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(ChHasError(Result, TEXT("needs a cut above 0"))));
	}

	TEST_METHOD(Surface_ChamferWithZeroCuts_IsRefusedToo)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='chamfer' cut='0'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(ChHasError(Result, TEXT("needs a cut above 0"))));
	}

	TEST_METHOD(Surface_CutWithoutTheChamferShape_IsRefused)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' cut='8'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(ChHasError(Result, TEXT("only apply to shape=\"chamfer\""))));
	}

	TEST_METHOD(Surface_ANegativeCut_IsRefused)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='chamfer' cut='8' cut-bl='-2'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(ChHasError(Result, TEXT("must not be negative"))));
	}

	TEST_METHOD(Surface_AnUnknownShape_IsRefused_AndListsTheWords)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='round'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(ChHasError(Result, TEXT("chamfer"))));
	}

	TEST_METHOD(Surface_InsideATemplate_TakesTheCutAsAParameter)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><template name='tag' params='size'><surface role='UI.Surface.Panel' shape='chamfer' cut='$size'><text>x</text></surface></template><tag size='14'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ChSurface(Result)->GetChamfer().TopLeft, 14.f)));
	}

	TEST_METHOD(SetSurface_OnAChamferedSurface_ChangesTheDrawnSurface_AndKeepsTheShape)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='chamfer' cut='12'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SSlaterSurface> Surface = ChSurface(Result);
		ASSERT_THAT(IsTrue(Surface.IsValid()));
		FSlaterSurface Other;
		Other.Fill = FLinearColor(0.f, 1.f, 0.f);
		Surface->SetSurface(Other);
		ASSERT_THAT(IsTrue(Surface->GetSurface() == Other));
		ASSERT_THAT(IsTrue(Surface->IsChamfered()));
	}

	TEST_METHOD(SetChamfer_WithNoCut_GoesBackToTheRoundedBox)
	{
		const FSlaterBuildResult Result = ChBuild(TEXT("<vbox><surface role='UI.Surface.Panel' shape='chamfer' cut='12'><text>x</text></surface></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SSlaterSurface> Surface = ChSurface(Result);
		ASSERT_THAT(IsTrue(Surface.IsValid()));
		Surface->SetChamfer(FSlaterChamfer());
		ASSERT_THAT(IsFalse(Surface->IsChamfered()));
	}
};

/**
 * Draws chamfered surfaces on a blue page and checks the corners (T-413): the cut corner shows the page, a square corner
 * and the middle show the fill, a border is drawn on the cut edge, and the cut edge is anti-aliased. Writes
 * Saved/SlaterUI/Render/Chamfer.png. Needs a real RHI (`-WithRHI`).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterRenderChamferTest, "BRS.SlaterUI.Render.Chamfer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterRenderChamferTest::RunTest(const FString&)
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

	// Left: red, no border, top left cut 20 and bottom right cut 12. Right: red with a 3 px green border, the same cuts.
	FSlaterSurface Plain;
	Plain.Fill = FLinearColor(1.f, 0.f, 0.f);
	FSlaterSurface Bordered = Plain;
	Bordered.Border = FLinearColor(0.f, 1.f, 0.f);
	Bordered.BorderWidth = 3.f;
	FSlaterChamfer Cuts;
	Cuts.TopLeft = 20.f;
	Cuts.BottomRight = 12.f;
	const TSharedRef<SSlaterSurface> Left = SNew(SSlaterSurface).Surface(Plain);
	Left->SetChamfer(Cuts);
	const TSharedRef<SSlaterSurface> Right = SNew(SSlaterSurface).Surface(Bordered);
	Right->SetChamfer(Cuts);

	const TSharedRef<SWidget> Scene = SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SColorBlock).Color(FLinearColor(0.f, 0.f, 1.f)).Size(FVector2D(160.0, 60.0))]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(10.f, 10.f, 0.f, 0.f)[SNew(SBox).WidthOverride(60).HeightOverride(40)[Left]]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(90.f, 10.f, 0.f, 0.f)[SNew(SBox).WidthOverride(60).HeightOverride(40)[Right]];

	TArray<FColor> Pixels;
	int32 Width = 0;
	if (!TestTrue(TEXT("Rendered and read back"), ChRender(Scene, FVector2D(160.0, 60.0), Pixels, Width)))
	{
		return false;
	}
	ChSavePng(Pixels, Width, TEXT("Chamfer.png"));
	AddInfo(FPaths::ProjectSavedDir() / TEXT("SlaterUI/Render/Chamfer.png"));

	const FColor Blue(0, 0, 255);
	const FColor Red(255, 0, 0);
	const FColor Green(0, 255, 0);
	auto At = [&](int32 X, int32 Y) { return Pixels[Y * Width + X]; };

	// Left card occupies x 10..70, y 10..50.
	TestTrue(TEXT("Cut top-left corner shows the page"), ChColorNear(At(11, 11), Blue, 8));
	TestTrue(TEXT("Cut bottom-right corner shows the page"), ChColorNear(At(68, 48), Blue, 8));
	TestTrue(TEXT("Square top-right corner is filled"), ChColorNear(At(68, 12), Red, 8));
	TestTrue(TEXT("Square bottom-left corner is filled"), ChColorNear(At(12, 48), Red, 8));
	TestTrue(TEXT("The middle is filled"), ChColorNear(At(40, 30), Red, 8));
	TestTrue(TEXT("Just inside the top-left diagonal is filled"), ChColorNear(At(10 + 14, 10 + 12), Red, 8));
	TestTrue(TEXT("Just outside the top-left diagonal is the page"), ChColorNear(At(10 + 9, 10 + 8), Blue, 8));

	// Right card occupies x 90..150: a green border of 3 px on every side, the cut corner still shows the page.
	TestTrue(TEXT("Border is drawn on the top edge"), ChColorNear(At(125, 11), Green, 24));
	TestTrue(TEXT("Border is drawn on the left edge"), ChColorNear(At(91, 40), Green, 24));
	TestTrue(TEXT("Inside the border is the fill"), ChColorNear(At(120, 30), Red, 8));
	TestTrue(TEXT("Cut top-left corner of the bordered card shows the page"), ChColorNear(At(91, 11), Blue, 8));

	// Edge quality of the polygon fill plus its one pixel edge line: count the rows of the top-left cut where the
	// pixel on the diagonal is neither the page nor the fill (an anti-aliased edge has such pixels, a plain polygon does not).
	int32 BlendedRows = 0;
	for (int32 Row = 0; Row < 18; ++Row)
	{
		bool bBlended = false;
		for (int32 Col = 0; Col < 22 && !bBlended; ++Col)
		{
			const FColor Pixel = At(10 + Col, 10 + Row);
			bBlended = !ChColorNear(Pixel, Blue, 24) && !ChColorNear(Pixel, Red, 24);
		}
		BlendedRows += bBlended ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("Cut edge: %d of 18 rows have a blended pixel on the diagonal"), BlendedRows));
	TestTrue(TEXT("The cut edge is anti-aliased"), BlendedRows >= 14);
	return true;
}

/**
 * The Gallery's chamfered card in both shipped styles (T-413): the cut corner shows the page behind, the middle is the
 * style's panel fill. Needs a real RHI (`-WithRHI`).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterRenderChamferGalleryTest, "BRS.SlaterUI.Render.ChamferGallery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterRenderChamferGalleryTest::RunTest(const FString&)
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

	for (const TCHAR* StyleName : {TEXT("Base"), TEXT("Dusk")})
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Gallery")).StyleName(StyleName);
		if (!TestFalse(FString::Printf(TEXT("Gallery builds in %s"), StyleName), View->HasErrors()))
		{
			continue;
		}
		const TSharedPtr<SWidget> Card = View->FindById(TEXT("ChamferCard"));
		if (!TestTrue(TEXT("The Gallery has a ChamferCard"), Card.IsValid()))
		{
			continue;
		}
		View->SlatePrepass(1.f);
		const FVector2D Size(FMath::CeilToDouble(View->GetDesiredSize().X), FMath::CeilToDouble(View->GetDesiredSize().Y));
		TArray<FColor> Pixels;
		int32 Width = 0;
		if (!TestTrue(TEXT("Rendered and read back"), ChRender(View, Size, Pixels, Width)))
		{
			continue;
		}
		ChSavePng(Pixels, Width, *FString::Printf(TEXT("ChamferGallery_%s.png"), StyleName));

		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(StyleName, Stack);
		FSlaterSurface Panel, Background;
		TestTrue(TEXT("Panel and Background surfaces exist"), Stack.FindSurface(TEXT("UI.Surface.Panel"), Panel) && Stack.FindSurface(TEXT("UI.Surface.Background"), Background));
		const FColor PanelFill = Panel.Fill.ToFColorSRGB();
		const FColor PageFill = Background.Fill.ToFColorSRGB();

		const FGeometry Geometry = Card->GetPaintSpaceGeometry();
		const FVector2D Origin = Geometry.GetAbsolutePosition();
		const FVector2D Extent = Geometry.GetAbsoluteSize();
		auto At = [&](int32 X, int32 Y) { return Pixels[FMath::Clamp(Y, 0, Pixels.Num() / Width - 1) * Width + FMath::Clamp(X, 0, Width - 1)]; };
		const int32 Left = FMath::RoundToInt(Origin.X);
		const int32 Top = FMath::RoundToInt(Origin.Y);
		const int32 Right = FMath::RoundToInt(Origin.X + Extent.X);
		const int32 Bottom = FMath::RoundToInt(Origin.Y + Extent.Y);
		TestTrue(*FString::Printf(TEXT("%s: the cut top-left corner shows the page"), StyleName), ChColorNear(At(Left + 2, Top + 2), PageFill, 10));
		TestTrue(*FString::Printf(TEXT("%s: the cut bottom-right corner shows the page"), StyleName), ChColorNear(At(Right - 3, Bottom - 3), PageFill, 10));
		TestTrue(*FString::Printf(TEXT("%s: the middle is the panel fill"), StyleName), ChColorNear(At((Left + Right) / 2, Top + 6), PanelFill, 10));
	}
	return true;
}

#endif
