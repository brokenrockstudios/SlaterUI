// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "SlaterListTestKit.h"
#include "SlaterMeter.h"
#include "SlaterModel.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"

namespace SlaterMeterTestsPrivate
{
	FString MtErrors(const FSlaterBuildResult& Result)
	{
		FString Text;
		for (const FSlaterError& Error : Result.Errors)
		{
			Text += Error.ToString() + TEXT("\n");
		}
		return Text;
	}

	FSlaterBuildResult MtBuild(const TCHAR* Layout)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}

	TSharedPtr<SSlaterMeter> MtMeter(const FSlaterBuildResult& Result)
	{
		return Result.Root.IsValid() ? StaticCastSharedPtr<SSlaterMeter>(SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SSlaterMeter"))) : nullptr;
	}
}
using namespace SlaterMeterTestsPrivate;

/** `<meter>` and `<rule>`: build, defaults from the style, clamping and bound values. */
TEST_CLASS(SlaterMeterTests, "BRS.SlaterUI.Meter")
{
	SlaterListKit::FRig Rig;

	AFTER_EACH()
	{
		Rig.Shutdown();
	}

	TEST_METHOD(Meter_Builds_WithTheValueAndTheStyleColorsByDefault)
	{
		const FSlaterBuildResult Result = MtBuild(TEXT("<vbox><meter value='0.25'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SSlaterMeter> Meter = MtMeter(Result);
		ASSERT_THAT(IsTrue(Meter.IsValid()));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Meter->GetValue(), 0.25f)));
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FLinearColor Accent, Border;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("UI.Color.Accent"), Accent) && Stack.FindColor(TEXT("UI.Color.Border"), Border)));
		ASSERT_THAT(IsTrue(Meter->GetFillColor() == Accent));
		ASSERT_THAT(IsTrue(Meter->GetTrackColor() == Border));
	}

	TEST_METHOD(Meter_ColorAndTrack_TakeTokensAndLiterals)
	{
		const FSlaterBuildResult Result = MtBuild(TEXT("<vbox><meter value='1' color='UI.Color.Danger' track='#102030'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedPtr<SSlaterMeter> Meter = MtMeter(Result);
		ASSERT_THAT(IsTrue(Meter.IsValid()));
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FLinearColor Danger;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("UI.Color.Danger"), Danger)));
		ASSERT_THAT(IsTrue(Meter->GetFillColor() == Danger));
		ASSERT_THAT(IsTrue(Meter->GetTrackColor() == FLinearColor::FromSRGBColor(FColor(0x10, 0x20, 0x30))));
	}

	TEST_METHOD(Meter_HeightAttribute_SizesIt_AndWithoutOneItHasADefault)
	{
		const FSlaterBuildResult Plain = MtBuild(TEXT("<vbox><meter value='0.5'/></vbox>"));
		ASSERT_THAT(IsTrue(Plain.Succeeded()));
		Plain.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(MtMeter(Plain)->GetDesiredSize().Y > 0.f));
		const FSlaterBuildResult Tall = MtBuild(TEXT("<vbox><meter value='0.5' height='20'/></vbox>"));
		ASSERT_THAT(IsTrue(Tall.Succeeded()));
		Tall.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(float(Tall.Root->GetDesiredSize().Y), 20.f)));
	}

	TEST_METHOD(Meter_ValueOutsideZeroToOne_IsClamped_WithNoError)
	{
		const FSlaterBuildResult Over = MtBuild(TEXT("<vbox><meter value='1.7'/></vbox>"));
		ASSERT_THAT(IsTrue(Over.Succeeded()));
		ASSERT_THAT(IsTrue(Over.Warnings.IsEmpty()));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(MtMeter(Over)->GetValue(), 1.f)));
		const FSlaterBuildResult Under = MtBuild(TEXT("<vbox><meter value='-3'/></vbox>"));
		ASSERT_THAT(IsTrue(Under.Succeeded()));
		ASSERT_THAT(IsTrue(Under.Warnings.IsEmpty()));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(MtMeter(Under)->GetValue(), 0.f)));
		const FSlaterBuildResult None = MtBuild(TEXT("<vbox><meter/></vbox>"));
		ASSERT_THAT(IsTrue(None.Succeeded()));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(MtMeter(None)->GetValue(), 0.f)));
	}

	TEST_METHOD(Meter_SetValue_ClampsAndIgnoresNaN)
	{
		TSharedRef<SSlaterMeter> Meter = SNew(SSlaterMeter).Value(0.4f);
		Meter->SetValue(2.f);
		ASSERT_THAT(IsTrue(Meter->GetValue() == 1.f));
		Meter->SetValue(-1.f);
		ASSERT_THAT(IsTrue(Meter->GetValue() == 0.f));
		Meter->SetValue(0.6f);
		Meter->SetValue(std::numeric_limits<float>::quiet_NaN());
		ASSERT_THAT(IsTrue(Meter->GetValue() == 0.f));
	}

	TEST_METHOD(Meter_BoundValue_FollowsTheMockModel_AndClamps)
	{
		ASSERT_THAT(IsTrue(Rig.Init(TEXT("<vbox><meter value='{Progress}'/></vbox>"), TEXT("{\"Progress\":0.04}"), ESlaterBindMode::Pushed)));
		const TSharedPtr<SSlaterMeter> Meter = MtMeter(Rig.Result);
		ASSERT_THAT(IsTrue(Meter.IsValid()));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Meter->GetValue(), 0.04f)));
		ASSERT_THAT(IsTrue(Rig.Model->SetValue(TEXT("Progress"), TEXT("0.75"))));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Meter->GetValue(), 0.75f)));
		ASSERT_THAT(IsTrue(Rig.Model->SetValue(TEXT("Progress"), TEXT("4"))));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Meter->GetValue(), 1.f)));
	}

	TEST_METHOD(Meter_BoundValue_OfAnUnknownPath_IsRefusedByCheckBindings)
	{
		ASSERT_THAT(IsTrue(Rig.Init(TEXT("<vbox><meter value='{Missing}'/></vbox>"), TEXT("{\"Progress\":0.5}"), ESlaterBindMode::Pushed)));
		TArray<FSlaterError> Errors;
		const TSharedRef<ISlaterScope> Scope = Slater::MakeModelScope(Rig.Model.ToSharedRef());
		ASSERT_THAT(IsFalse(Slater::CheckBindings(Rig.Result, *Scope, TEXT("Test"), Errors)));
	}

	TEST_METHOD(Meter_BadAttributes_AreReportedWithTheirLine)
	{
		const FSlaterBuildResult Result = MtBuild(TEXT("<vbox><meter value='lots' tint='#fff'/></vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		const FString Errors = MtErrors(Result);
		ASSERT_THAT(IsTrue(Errors.Contains(TEXT("Layouts/Test.sltr(1,"))));
		ASSERT_THAT(IsTrue(Errors.Contains(TEXT("tint"))));
	}

	TEST_METHOD(Rule_Builds_WithDefaultsAndThickness)
	{
		const FSlaterBuildResult Thin = MtBuild(TEXT("<vbox><rule/></vbox>"));
		ASSERT_THAT(IsTrue(Thin.Succeeded()));
		const TSharedPtr<SWidget> Line = SlaterListKit::Find(Thin.Root.ToSharedRef(), TEXT("SColorBlock"));
		ASSERT_THAT(IsTrue(Line.IsValid()));
		Thin.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(float(Line->GetDesiredSize().Y), 1.f)));

		const FSlaterBuildResult Thick = MtBuild(TEXT("<vbox><rule thickness='3' color='#FF0000'/></vbox>"));
		ASSERT_THAT(IsTrue(Thick.Succeeded()));
		const TSharedPtr<SWidget> Bar = SlaterListKit::Find(Thick.Root.ToSharedRef(), TEXT("SColorBlock"));
		Thick.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(float(Bar->GetDesiredSize().Y), 3.f)));
	}

	TEST_METHOD(Rule_WithAChild_OrANegativeThickness_IsHandled)
	{
		const FSlaterBuildResult WithChild = MtBuild(TEXT("<vbox><rule><spacer/></rule></vbox>"));
		ASSERT_THAT(IsFalse(WithChild.Succeeded()));
		const FSlaterBuildResult Negative = MtBuild(TEXT("<vbox><rule thickness='-2'/></vbox>"));
		ASSERT_THAT(IsTrue(Negative.Succeeded()));
		const TSharedPtr<SWidget> Line = SlaterListKit::Find(Negative.Root.ToSharedRef(), TEXT("SColorBlock"));
		Negative.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(Line->GetDesiredSize().Y == 0.f));
	}

	TEST_METHOD(MeterAndRule_InsideATemplate_BindPerItem)
	{
		const TCHAR* Layout = TEXT("<vbox><repeat source='{L}' key='Id' columns='1'><vbox><meter value='{P}'/><rule/></vbox></repeat></vbox>");
		ASSERT_THAT(IsTrue(Rig.Init(Layout, TEXT("{\"L\":[{\"Id\":\"a\",\"P\":0.25},{\"Id\":\"b\",\"P\":0.9}]}"), ESlaterBindMode::Pushed)));
		const TSharedPtr<SWidget> A = Rig.Repeat(0)->FindItemWidget(TEXT("a"));
		const TSharedPtr<SWidget> B = Rig.Repeat(0)->FindItemWidget(TEXT("b"));
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid()));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StaticCastSharedPtr<SSlaterMeter>(SlaterListKit::Find(A.ToSharedRef(), TEXT("SSlaterMeter")))->GetValue(), 0.25f)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StaticCastSharedPtr<SSlaterMeter>(SlaterListKit::Find(B.ToSharedRef(), TEXT("SSlaterMeter")))->GetValue(), 0.9f)));
	}
};

/**
 * Renders a meter and a rounded-box brush with a texture (the avatar question of T-409) to PNGs. Pixel checks: the
 * meter's fill and track at known positions, and whether the avatar's corner shows the page behind it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterRenderMeterTest, "BRS.SlaterUI.Render.Meter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterRenderMeterTest::RunTest(const FString&)
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

	// A solid red 8x8 texture, drawn through a rounded-box brush with a 16 px radius at 32 x 32.
	TArray<uint8> Pixels;
	for (int32 P = 0; P < 64; ++P)
	{
		Pixels.Append({0, 0, 255, 255});
	}
	UTexture2D* Texture = UTexture2D::CreateTransient(8, 8, PF_B8G8R8A8, NAME_None, Pixels);
	Texture->UpdateResource();
	TStrongObjectPtr<UTexture2D> Keep(Texture);
	FSlateBrush Avatar;
	Avatar.DrawAs = ESlateBrushDrawType::RoundedBox;
	Avatar.ImageType = ESlateBrushImageType::FullColor;
	Avatar.SetResourceObject(Texture);
	Avatar.ImageSize = FVector2D(32.0, 32.0);
	Avatar.OutlineSettings = FSlateBrushOutlineSettings(16.f);
	Avatar.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;

	FSlaterStyleStack Stack;
	Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
	FSlaterBuildResult Meter;
	Slater::BuildLayoutFromText(TEXT("<box width='100' height='10'><meter value='0.5' color='#00FF00' track='#FFFFFF'/></box>"), TEXT("Layouts/Test.sltr"), Stack, Meter);
	if (!TestTrue(TEXT("Meter layout builds"), Meter.Succeeded()))
	{
		return false;
	}

	const TSharedRef<SWidget> Scene = SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SColorBlock).Color(FLinearColor(0.f, 0.f, 1.f)).Size(FVector2D(100.0, 60.0))]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(0.f)[SNew(SBox).WidthOverride(32).HeightOverride(32)[SNew(SImage).Image(&Avatar)]]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)[Meter.Root.ToSharedRef()];

	IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir() / TEXT("SlaterUI/Render")), true);
	FWidgetRenderer Renderer(false);
	UTextureRenderTarget2D* Target = nullptr;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		Scene->SlatePrepass(1.f);
		Target = Renderer.DrawWidget(Scene, FVector2D(100.0, 60.0));
		FlushRenderingCommands();
	}
	if (!TestNotNull(TEXT("Render target"), Target))
	{
		return false;
	}
	TArray<FColor> Read;
	FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
	if (!TestTrue(TEXT("Pixels read back"), Resource && Resource->ReadPixels(Read)) || !TestEqual(TEXT("Pixel count"), Read.Num(), Target->SizeX * Target->SizeY))
	{
		return false;
	}
	const int32 Width = Target->SizeX;
	for (FColor& Pixel : Read)
	{
		const FColor Encoded = FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f).ToFColorSRGB();
		Pixel = FColor(Encoded.R, Encoded.G, Encoded.B, 255);
	}
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Width, Target->SizeY, TArrayView64<const FColor>(Read.GetData(), Read.Num()), Png);
	const FString File = FPaths::ProjectSavedDir() / TEXT("SlaterUI/Render/Meter.png");
	TestTrue(TEXT("Meter.png written"), FFileHelper::SaveArrayToFile(Png, *File));
	AddInfo(File);

	auto At = [&](int32 X, int32 Y) { return Read[Y * Width + X]; };
	auto Near = [](const FColor& A, const FColor& B) { return FMath::Abs(int32(A.R) - B.R) <= 16 && FMath::Abs(int32(A.G) - B.G) <= 16 && FMath::Abs(int32(A.B) - B.B) <= 16; };
	// Meter at the bottom-left: 100 x 10, the fill covers x 0..50 (rounded ends), the track the rest.
	TestTrue(TEXT("Fill is green at 25%"), Near(At(25, 55), FColor(0, 255, 0)));
	TestTrue(TEXT("Track is white at 75%"), Near(At(75, 55), FColor(255, 255, 255)));
	// Fully rounded ends: the very corner of the bar shows the page behind it.
	TestTrue(TEXT("Meter corner is rounded away"), Near(At(0, 50), FColor(0, 0, 255)));
	// Avatar: the center is the texture; the answer for the corner goes to DevNotes.
	TestTrue(TEXT("Avatar center shows the texture"), Near(At(16, 16), FColor(255, 0, 0)));
	const bool bCornerRounded = Near(At(0, 0), FColor(0, 0, 255));
	AddInfo(FString::Printf(TEXT("Avatar corner pixel is %s (rounded box with a texture %s)"), *At(0, 0).ToString(), bCornerRounded ? TEXT("rounds the image") : TEXT("does NOT round the image")));
	TestTrue(TEXT("A rounded-box brush with a texture rounds its corners"), bCornerRounded);
	return true;
}

#endif
