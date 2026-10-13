// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "AgentProbe/AgentProbeCpp.h"
#include "CQTest.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/SecureHash.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "SlaterList.h"
#include "SlaterModel.h"
#include "SlaterView.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

/**
 * Harness of the agent fluency probe (T-355, Docs/SlaterUI/AgentProbe/README.md). An agent drops its work in
 * `Saved/SlaterUI/AgentProbe/...`, runs `RunTests.ps1 -Filter BRS.SlaterUI.AgentProbe -WithRHI`, and reads `Result.txt`
 * (and looks at `Out.png`) next to its files. With nothing dropped there, the tests do nothing and pass.
 */
namespace
{
	FString ProbeRoot()
	{
		return FPaths::ProjectSavedDir() / TEXT("SlaterUI/AgentProbe");
	}

	void ProbeWalk(const TSharedRef<SWidget>& Widget, int32 Depth, int32& OutCount, int32& OutDepth, TArray<FString>& OutTexts)
	{
		if (Widget == SNullWidget::NullWidget)
		{
			return;
		}
		++OutCount;
		OutDepth = FMath::Max(OutDepth, Depth);
		if (Widget->GetType() == FName(TEXT("STextBlock")))
		{
			OutTexts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
		}
		if (FChildren* Children = Widget->GetChildren())
		{
			Children->ForEachWidget([&](SWidget& Child)
			{
				ProbeWalk(Child.AsShared(), Depth + 1, OutCount, OutDepth, OutTexts);
			});
		}
	}

	FString ProbeTreeLine(const TSharedRef<SWidget>& Root, const TCHAR* Label, TArray<FString>& OutTexts)
	{
		int32 Count = 0;
		int32 Depth = 0;
		OutTexts.Reset();
		ProbeWalk(Root, 1, Count, Depth, OutTexts);
		TArray<FString> Quoted;
		for (const FString& Text : OutTexts)
		{
			Quoted.Add(FString::Printf(TEXT("\"%s\""), *Text));
		}
		return FString::Printf(TEXT("%s: %d widgets, %d deep\n%s texts: %s\n"), Label, Count, Depth, Label, *FString::Join(Quoted, TEXT(", ")));
	}

	/** Renders on the Base background so transparent areas are readable; encodes to sRGB. False without a renderer. */
	bool ProbeRender(const TSharedRef<SWidget>& Content, int32& OutWidth, int32& OutHeight, TArray<FColor>& OutPixels)
	{
		if (GUsingNullRHI || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
		{
			return false;
		}
		const TSharedRef<SWidget> Root = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(0xF2, 0xF3, 0xF5)))
			.Padding(0.f)
			[
				Content
			];
		FWidgetRenderer Renderer(false);
		UTextureRenderTarget2D* Target = nullptr;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Root->SlatePrepass(1.f);
			const FVector2D Size(FMath::CeilToInt(Root->GetDesiredSize().X), FMath::CeilToInt(Root->GetDesiredSize().Y));
			if (Size.X < 1.0 || Size.Y < 1.0)
			{
				return false;
			}
			Target = Renderer.DrawWidget(Root, Size);
			FlushRenderingCommands();
		}
		FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
		if (!Resource || !Resource->ReadPixels(OutPixels))
		{
			return false;
		}
		OutWidth = Target->SizeX;
		OutHeight = Target->SizeY;
		if (OutPixels.Num() != OutWidth * OutHeight)
		{
			return false;
		}
		for (FColor& Pixel : OutPixels)
		{
			const FColor Encoded = FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f).ToFColorSRGB();
			Pixel = FColor(Encoded.R, Encoded.G, Encoded.B, 255);
		}
		return true;
	}

	bool ProbeSavePng(const FString& File, int32 Width, int32 Height, const TArray<FColor>& Pixels)
	{
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		return FFileHelper::SaveArrayToFile(Png, *File);
	}

	bool ProbeLoadPng(const FString& File, int32& OutWidth, int32& OutHeight, TArray<FColor>& OutPixels)
	{
		TArray<uint8> Data;
		if (!FFileHelper::LoadFileToArray(Data, *File))
		{
			return false;
		}
		IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
		TArray64<uint8> Raw;
		if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Data.GetData(), Data.Num()) || !Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
		{
			return false;
		}
		OutWidth = Wrapper->GetWidth();
		OutHeight = Wrapper->GetHeight();
		OutPixels.SetNumUninitialized(OutWidth * OutHeight);
		FMemory::Memcpy(OutPixels.GetData(), Raw.GetData(), OutPixels.Num() * sizeof(FColor));
		return true;
	}

	/** One line: whether the rendering has the reference's size and how many pixels differ (by more than 32 on a channel), plus the mean channel error. */
	FString ProbeCompare(const FString& ReferenceFile, int32 Width, int32 Height, const TArray<FColor>& Pixels)
	{
		if (!FPaths::FileExists(ReferenceFile))
		{
			return FString();
		}
		int32 RefWidth = 0;
		int32 RefHeight = 0;
		TArray<FColor> Ref;
		if (!ProbeLoadPng(ReferenceFile, RefWidth, RefHeight, Ref))
		{
			return TEXT("Reference: could not be read\n");
		}
		// Compared on the overlap, so a wrong size still gives a number; the size line says it is wrong.
		const int32 W = FMath::Min(Width, RefWidth);
		const int32 H = FMath::Min(Height, RefHeight);
		int64 Differing = 0;
		int64 ErrorSum = 0;
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const FColor A = Pixels[Y * Width + X];
				const FColor B = Ref[Y * RefWidth + X];
				const int32 Dr = FMath::Abs(int32(A.R) - int32(B.R));
				const int32 Dg = FMath::Abs(int32(A.G) - int32(B.G));
				const int32 Db = FMath::Abs(int32(A.B) - int32(B.B));
				ErrorSum += Dr + Dg + Db;
				Differing += (FMath::Max3(Dr, Dg, Db) > 32) ? 1 : 0;
			}
		}
		const double Total = FMath::Max(1, W * H);
		const bool bSize = Width == RefWidth && Height == RefHeight;
		return FString::Printf(TEXT("Reference: %s (got %d x %d, want %d x %d); %.2f%% of pixels differ, mean channel error %.2f of 255\n"), bSize ? TEXT("same size") : TEXT("SIZE DIFFERS"), Width, Height, RefWidth, RefHeight, 100.0 * Differing / Total, double(ErrorSum) / (3.0 * Total));
	}

	/** Renders Content to <Dir>/<Name>.png, appends the comparison with ReferenceFile (if there is one) to ReferenceFile.compare.txt, returns report lines. */
	FString ProbeRenderReport(const TSharedRef<SWidget>& Content, const FString& Dir, const TCHAR* Name, const FString& ReferenceFile)
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<FColor> Pixels;
		if (!ProbeRender(Content, Width, Height, Pixels))
		{
			return FString::Printf(TEXT("%s.png: not rendered (needs -WithRHI)\n"), Name);
		}
		const FString File = Dir / FString::Printf(TEXT("%s.png"), Name);
		FString Report = ProbeSavePng(File, Width, Height, Pixels) ? FString::Printf(TEXT("%s.png: %d x %d\n"), Name, Width, Height) : FString::Printf(TEXT("%s.png: could not be written\n"), Name);
		if (!ReferenceFile.IsEmpty())
		{
			// Kept out of Result.txt, which the agent reads: the number would steer it. One line per run, next to the reference.
			const FString Line = ProbeCompare(ReferenceFile, Width, Height, Pixels);
			if (!Line.IsEmpty())
			{
				FFileHelper::SaveStringToFile(Line, *(ReferenceFile + TEXT(".compare.txt")), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
			}
		}
		return Report;
	}

	/** Result.txt is the latest run; History.txt keeps every run on changed input, one `=== Round n ===` block each (the round count of the probe). */
	void ProbeSaveResult(const FString& Dir, const FString& Report)
	{
		FFileHelper::SaveStringToFile(Report, *(Dir / TEXT("Result.txt")));
		FString History;
		FFileHelper::LoadFileToString(History, *(Dir / TEXT("History.txt")));
		int32 Round = 1;
		for (int32 At = History.Find(TEXT("=== Round ")); At != INDEX_NONE; At = History.Find(TEXT("=== Round "), ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
		{
			++Round;
		}
		History += FString::Printf(TEXT("=== Round %d ===\n%s\n"), Round, *Report);
		FFileHelper::SaveStringToFile(History, *(Dir / TEXT("History.txt")));
	}

	FString ProbeReadLine(const FString& File, const TCHAR* Default)
	{
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *File))
		{
			Text.TrimStartAndEndInline();
			if (!Text.IsEmpty())
			{
				return Text;
			}
		}
		return Default;
	}
}

/**
 * Builds every `Saved/SlaterUI/AgentProbe/Sltr/<Run>/Layout.sltr` against the shipped styles plus any `*.sltrstyle` in
 * the run's folder (`Style.txt` names the style, default Base; `Mock.json` is the model; `Changes.txt` lines
 * `set <Array> <KeyField> <Key> <Field> <Number>` run after the first render), writes `Result.txt` and `Out.png`
 * (`OutAfter.png` after the changes) and compares with `Saved/SlaterUI/AgentProbe/Reference/<Run>.png` when there is one.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterAgentProbeSltrTest, "BRS.SlaterUI.AgentProbe.Sltr", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterAgentProbeSltrTest::RunTest(const FString&)
{
	const FString SltrRoot = ProbeRoot() / TEXT("Sltr");
	TArray<FString> Runs;
	IFileManager::Get().FindFiles(Runs, *(SltrRoot / TEXT("*")), false, true);
	for (const FString& Run : Runs)
	{
		const FString Dir = SltrRoot / Run;
		if (!FPaths::FileExists(Dir / TEXT("Layout.sltr")))
		{
			continue;
		}

		// A round is a run on changed input. The same files give the same result, so skip them and keep the last Result.txt.
		FString InputHash;
		{
			TArray<FString> Inputs;
			IFileManager::Get().FindFiles(Inputs, *(Dir / TEXT("*.*")), true, false);
			Inputs.Sort();
			FMD5 Md5;
			for (const FString& Input : Inputs)
			{
				if (Input.EndsWith(TEXT(".sltr")) || Input.EndsWith(TEXT(".sltrstyle")) || Input == TEXT("Mock.json") || Input == TEXT("Changes.txt") || Input == TEXT("Style.txt"))
				{
					TArray<uint8> Bytes;
					FFileHelper::LoadFileToArray(Bytes, *(Dir / Input));
					Md5.Update(reinterpret_cast<const uint8*>(*Input), Input.Len() * sizeof(TCHAR));
					Md5.Update(Bytes.GetData(), Bytes.Num());
				}
			}
			uint8 Digest[16];
			Md5.Final(Digest);
			InputHash = BytesToHex(Digest, 16);
		}
		FString LastHash;
		if (FFileHelper::LoadFileToString(LastHash, *(Dir / TEXT("Result.hash"))) && LastHash == InputHash && FPaths::FileExists(Dir / TEXT("Result.txt")))
		{
			continue;
		}
		FFileHelper::SaveStringToFile(InputHash, *(Dir / TEXT("Result.hash")));

		FString Report;

		FSlaterStyleLibrary Library;
		TArray<FSlaterError> StyleErrors;
		Library.LoadPluginStyles(StyleErrors);
		TArray<FString> StyleFiles;
		IFileManager::Get().FindFiles(StyleFiles, *(Dir / TEXT("*.sltrstyle")), true, false);
		if (StyleFiles.Num() > 0)
		{
			Library.LoadDirectory(Dir, StyleErrors);
		}
		for (const FSlaterError& Error : StyleErrors)
		{
			Report += FString::Printf(TEXT("STYLE ERROR %s\n"), *Error.ToString());
		}

		const FString StyleName = ProbeReadLine(Dir / TEXT("Style.txt"), TEXT("Base"));
		Report += FString::Printf(TEXT("Style: %s\n"), *StyleName);
		FSlaterStyleStack Stack;
		if (!Library.BuildStack(*StyleName, Stack))
		{
			Report += FString::Printf(TEXT("FAILED: unknown style '%s'\n"), *StyleName);
			ProbeSaveResult(Dir, Report);
			continue;
		}

		FSlaterBuildResult Result;
		const bool bBuilt = Slater::LoadLayoutFile(Dir / TEXT("Layout.sltr"), TEXT("Layout.sltr"), Stack, Result);
		for (const FSlaterError& Error : Result.Errors)
		{
			Report += FString::Printf(TEXT("ERROR %s\n"), *Error.ToString());
		}
		for (const FSlaterError& Warning : Result.Warnings)
		{
			Report += FString::Printf(TEXT("WARNING %s\n"), *Warning.ToString());
		}
		if (!bBuilt || !Result.Root.IsValid())
		{
			Report += FString::Printf(TEXT("FAILED: %d error(s)\n"), Result.Errors.Num());
			ProbeSaveResult(Dir, Report);
			continue;
		}

		TSharedPtr<FSlaterModel> Model;
		TSharedPtr<FSlaterBinder> Binder;
		const FString MockFile = Dir / TEXT("Mock.json");
		if (FPaths::FileExists(MockFile))
		{
			Model = MakeShared<FSlaterModel>();
			FSlaterError ModelError;
			if (Model->LoadMockFile(MockFile, ModelError))
			{
				Binder = Slater::Bind(Result, Model.ToSharedRef(), ESlaterBindMode::Pushed, Stack);
			}
			else
			{
				Report += FString::Printf(TEXT("ERROR Mock.json: %s\n"), *ModelError.ToString());
			}
		}

		Report += TEXT("OK: the layout builds\n");
		TArray<FString> Texts;
		Report += ProbeTreeLine(Result.Root.ToSharedRef(), TEXT("Tree"), Texts);
		Report += ProbeRenderReport(Result.Root.ToSharedRef(), Dir, TEXT("Out"), ProbeRoot() / TEXT("Reference") / (Run + TEXT(".png")));

		FString ChangesText;
		if (Model.IsValid() && FFileHelper::LoadFileToString(ChangesText, *(Dir / TEXT("Changes.txt"))))
		{
			TArray<FString> Lines;
			ChangesText.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				TArray<FString> Parts;
				Line.ParseIntoArray(Parts, TEXT(" "));
				if (Parts.Num() == 6 && Parts[0] == TEXT("set"))
				{
					const bool bSet = Model->SetItemNumber(Parts[1], Parts[2], Parts[3], Parts[4], FCString::Atod(*Parts[5]));
					Report += FString::Printf(TEXT("Change %s: %s\n"), *Line, bSet ? TEXT("applied") : TEXT("not applied (unknown item, or the value did not change)"));
				}
			}
			Report += ProbeTreeLine(Result.Root.ToSharedRef(), TEXT("After"), Texts);
			Report += ProbeRenderReport(Result.Root.ToSharedRef(), Dir, TEXT("OutAfter"), FString());
		}
		ProbeSaveResult(Dir, Report);
		AddInfo(FString::Printf(TEXT("%s:\n%s"), *Run, *Report));
	}
	return true;
}

/**
 * Renders `SlaterProbeCpp::TaskN()` (the plain-Slate variants, `Private/AgentProbe/CppTaskN.cpp`) to
 * `Saved/SlaterUI/AgentProbe/Cpp/TaskN/Out.png`, with `Result.txt` and a comparison with `Reference/Cpp<N>.png` if present.
 * Task 2 changes cell 3 to 99 after the first render; Task 5 gets the Base theme. The tests pass whatever the widgets
 * look like; this is a probe, the report is the result. Runs only the task named by the environment variable
 * SLATER_PROBE_TASK (set by Tools/Scripts/AgentProbeCpp.ps1, which also swaps the agent's file in for the build), so
 * a plain run of the filter does nothing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlaterAgentProbeCppTest, "BRS.SlaterUI.AgentProbe.Cpp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlaterAgentProbeCppTest::RunTest(const FString&)
{
	const int32 Only = FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("SLATER_PROBE_TASK")));
	for (int32 Task = 1; Task <= 5; ++Task)
	{
		if (Task != Only)
		{
			continue;
		}
		const FString Dir = ProbeRoot() / TEXT("Cpp") / FString::Printf(TEXT("Task%d"), Task);
		if (!IFileManager::Get().DirectoryExists(*Dir))
		{
			continue;
		}
		TFunction<void(int32, int32)> SetCount;
		TSharedRef<SWidget> Widget = SNullWidget::NullWidget;
		switch (Task)
		{
		case 1: Widget = SlaterProbeCpp::Task1(); break;
		case 2: Widget = SlaterProbeCpp::Task2(SetCount); break;
		case 3: Widget = SlaterProbeCpp::Task3(); break;
		case 4: Widget = SlaterProbeCpp::Task4(); break;
		default: Widget = SlaterProbeCpp::Task5(SlaterProbeCpp::FProbeTheme::MakeBase()); break;
		}

		FString Report;
		if (Widget == SNullWidget::NullWidget)
		{
			Report = TEXT("FAILED: the task returned a null widget (still the stub)\n");
		}
		else
		{
			Report = TEXT("OK: the task compiled and returned a widget\n");
			TArray<FString> Texts;
			Report += ProbeTreeLine(Widget, TEXT("Tree"), Texts);
			Report += ProbeRenderReport(Widget, Dir, TEXT("Out"), ProbeRoot() / TEXT("Reference") / FString::Printf(TEXT("Cpp%d.png"), Task));
			if (Task == 2 && SetCount)
			{
				SetCount(3, 99);
				Report += ProbeTreeLine(Widget, TEXT("After"), Texts);
				Report += ProbeRenderReport(Widget, Dir, TEXT("OutAfter"), FString());
			}
		}
		ProbeSaveResult(Dir, Report);
		AddInfo(FString::Printf(TEXT("Task%d:\n%s"), Task, *Report));
	}
	return true;
}

#endif
