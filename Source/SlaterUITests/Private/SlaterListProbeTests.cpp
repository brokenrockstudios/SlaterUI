// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "SlaterListTestKit.h"
#include "SlaterModel.h"
#include "SlaterSurface.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

/*
 * T-351 measurements: the same inventory grid three ways, driven through the same four scenarios, timed on the game
 * thread. Needs a real RHI (`-WithRHI`) because the frame is drawn with FWidgetRenderer. The numbers are written to
 * Saved/SlaterUI/ListProbe.txt and the log for Docs/SlaterUI/DevNotes.md.
 */
namespace SlaterListProbeTestsPrivate
{
	/** What every variant must be able to do; items are named i<generation>_<n>. */
	class IPbGrid
	{
	public:
		virtual ~IPbGrid() = default;
		virtual TSharedRef<SWidget> GetRoot() const = 0;
		virtual void SetCellHover(int32 Cell, bool bHover) = 0;
		virtual void MoveItem(int32 N, int32 Slot) = 0;
		/** Removes every item and adds NumItems new ones (a container opening). */
		virtual void ReplaceItems(int32 Generation) = 0;
		/** Called before the timed frame of `ReplaceItems`, for work that is the data's, not the grid's (the layout variant builds its JSON items here). */
		virtual void PrepareItems(int32 /*Generation*/) {}
	};

	struct FPbItemParams
	{
		FString Id;
		int32 Slot = 0;
		int32 Icon = 0;
		int32 Count = 1;
		bool bLocked = false;
	};

	FPbItemParams PbItem(int32 Generation, int32 N, int32 NumCells, int32 NumItems)
	{
		FPbItemParams Item;
		Item.Id = FString::Printf(TEXT("i%d_%d"), Generation, N);
		Item.Slot = (N * FMath::Max(1, NumCells / FMath::Max(1, NumItems)) + Generation) % NumCells;
		Item.Icon = N % 16;
		Item.Count = N + 1;
		Item.bLocked = (N % 5 == 0);
		return Item;
	}

	/** The layout and the model, bound in one mode. */
	class FPbSlaterGrid : public IPbGrid
	{
	public:
		FPbSlaterGrid(const TSharedPtr<FSlaterTestImages>& Images, int32 InNumCells, int32 InNumItems, int32 Columns, ESlaterBindMode Mode)
			: NumCells(InNumCells), NumItems(InNumItems)
		{
			Rig.Images = Images;
			// Generation 0 items are created by the mock data itself, so the first build is the whole cost of opening.
			FString Json = TEXT("{\"Title\":\"Backpack\",\"Cells\":[");
			for (int32 I = 0; I < NumCells; ++I)
			{
				Json += FString::Printf(TEXT("%s{\"Index\":%d,\"State\":\"normal\"}"), I ? TEXT(",") : TEXT(""), I);
			}
			Json += TEXT("],\"Items\":[");
			for (int32 N = 0; N < NumItems; ++N)
			{
				const FPbItemParams P = PbItem(0, N, NumCells, NumItems);
				Json += FString::Printf(TEXT("%s{\"Id\":\"%s\",\"Slot\":%d,\"Icon\":\"icon%d\",\"Count\":%d,\"Locked\":%s}"), N ? TEXT(",") : TEXT(""), *P.Id, P.Slot, P.Icon, P.Count, P.bLocked ? TEXT("true") : TEXT("false"));
				Ids.Add(P.Id);
			}
			Json += TEXT("]}");
			bOk = Rig.Init(SlaterListKit::LayoutText(Columns), Json, Mode);
		}

		~FPbSlaterGrid()
		{
			Rig.Shutdown();
		}

		bool bOk = false;
		SlaterListKit::FRig Rig;

		virtual TSharedRef<SWidget> GetRoot() const override { return Rig.Result.Root.ToSharedRef(); }

		virtual void SetCellHover(int32 Cell, bool bHover) override
		{
			Rig.Model->SetItemString(TEXT("Cells"), TEXT("Index"), FString::FromInt(Cell), TEXT("State"), bHover ? TEXT("hover") : TEXT("normal"));
		}

		virtual void MoveItem(int32 N, int32 Slot) override
		{
			Rig.Model->SetItemNumber(TEXT("Items"), TEXT("Id"), Ids[N], TEXT("Slot"), Slot);
		}

		virtual void PrepareItems(int32 Generation) override
		{
			Prepared.Reset();
			for (int32 N = 0; N < NumItems; ++N)
			{
				const FPbItemParams P = PbItem(Generation, N, NumCells, NumItems);
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("Id"), P.Id);
				Item->SetNumberField(TEXT("Slot"), P.Slot);
				Item->SetStringField(TEXT("Icon"), FString::Printf(TEXT("icon%d"), P.Icon));
				Item->SetNumberField(TEXT("Count"), P.Count);
				Item->SetBoolField(TEXT("Locked"), P.bLocked);
				Prepared.Add(Item);
			}
		}

		virtual void ReplaceItems(int32 Generation) override
		{
			if (Prepared.Num() != NumItems)
			{
				PrepareItems(Generation);
			}
			for (const FString& Id : Ids)
			{
				Rig.Model->RemoveItem(TEXT("Items"), TEXT("Id"), Id);
			}
			Ids.Reset();
			for (const TSharedRef<FJsonObject>& Item : Prepared)
			{
				FString Id;
				FSlaterModel::ReadField(*Item, TEXT("Id"), Id);
				Rig.Model->AddItem(TEXT("Items"), TEXT("Id"), Item);
				Ids.Add(Id);
			}
			Prepared.Reset();
		}

	private:
		int32 NumCells;
		int32 NumItems;
		TArray<FString> Ids;
		TArray<TSharedRef<FJsonObject>> Prepared;
	};

	/** One widget that paints every cell itself, the way a host-owned grid would (T-151's idea): no widget per cell. */
	class SPbCellPainter : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPbCellPainter) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, int32 InNumCells, int32 InColumns, const FSlateBrush* InNormal, const FSlateBrush* InHover)
		{
			NumCells = InNumCells;
			Columns = InColumns;
			Normal = InNormal;
			Hover = InHover;
		}

		void SetHover(int32 Cell, bool bHover)
		{
			if (bHover)
			{
				Hovered.Add(Cell);
			}
			else
			{
				Hovered.Remove(Cell);
			}
			Invalidate(EInvalidateWidgetReason::Paint);
		}

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool) const override
		{
			for (int32 I = 0; I < NumCells; ++I)
			{
				const FSlateBrush* Brush = Hovered.Contains(I) ? Hover : Normal;
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(48.f, 48.f), FSlateLayoutTransform(FVector2f(float(I % Columns) * 48.f, float(I / Columns) * 48.f))), Brush, ESlateDrawEffect::None, Brush->GetTint(InWidgetStyle));
			}
			return LayerId;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(Columns * 48.0, ((NumCells + Columns - 1) / Columns) * 48.0);
		}

	private:
		int32 NumCells = 0;
		int32 Columns = 1;
		const FSlateBrush* Normal = nullptr;
		const FSlateBrush* Hover = nullptr;
		TSet<int32> Hovered;
	};

	/**
	 * The same grid written by hand in plain Slate with pushed updates: what an agent writes today, and the
	 * interpreter's baseline. Widgets, fonts, brushes and the update path match the layout's; there is no parsing,
	 * registry, model or binding layer.
	 */
	class FPbPlainGrid : public IPbGrid
	{
	public:
		FPbPlainGrid(const FSlaterStyleStack& Stack, int32 InNumCells, int32 InNumItems, int32 InColumns, bool bInHostPaintedCells = false)
			: NumCells(InNumCells), NumItems(InNumItems), Columns(InColumns), bHostPaintedCells(bInHostPaintedCells)
		{
			FSlaterSurface Panel;
			FSlaterSurface Normal;
			FSlaterSurface Hover;
			FSlaterFont Heading;
			FSlaterFont Caption;
			FLinearColor TextColor = FLinearColor::White;
			float Space = 0.f;
			Stack.FindSurface(TEXT("UI.Surface.Panel"), Panel);
			Stack.FindSurface(TEXT("UI.Surface.Button"), Normal);
			Stack.FindSurface(TEXT("UI.Surface.Button.Hover"), Hover);
			Stack.FindFont(TEXT("UI.Font.Heading"), Heading);
			Stack.FindFont(TEXT("UI.Font.Caption"), Caption);
			Stack.FindColor(TEXT("UI.Color.Text"), TextColor);
			Stack.FindMetric(TEXT("UI.Space.M"), Space);
			PanelBrush = SSlaterSurface::MakeBrush(Panel);
			NormalBrush = SSlaterSurface::MakeBrush(Normal);
			HoverBrush = SSlaterSurface::MakeBrush(Hover);
			CaptionFont = FCoreStyle::GetDefaultFontStyle(Caption.Role, Caption.Size);
			Color = FSlateColor(TextColor);

			TSharedPtr<SUniformGridPanel> CellGrid;
			SAssignNew(CellGrid, SUniformGridPanel).MinDesiredSlotWidth(48.f).MinDesiredSlotHeight(48.f);
			SAssignNew(ItemGrid, SUniformGridPanel).MinDesiredSlotWidth(48.f).MinDesiredSlotHeight(48.f);
			CellGrid->SetVisibility(EVisibility::HitTestInvisible);
			ItemGrid->SetVisibility(EVisibility::HitTestInvisible);
			TSharedPtr<SWidget> CellWidget = CellGrid;
			if (bHostPaintedCells)
			{
				SAssignNew(Painter, SPbCellPainter, NumCells, Columns, &NormalBrush, &HoverBrush);
				Painter->SetVisibility(EVisibility::HitTestInvisible);
				CellWidget = Painter;
			}
			else
			{
				for (int32 I = 0; I < NumCells; ++I)
				{
					TSharedRef<SBorder> Cell = SNew(SBorder).BorderImage(&NormalBrush).Padding(0.f);
					Cells.Add(Cell);
					CellGrid->AddSlot(I % Columns, I / Columns)[Cell];
				}
			}
			for (int32 N = 0; N < NumItems; ++N)
			{
				AddItem(PbItem(0, N, NumCells, NumItems));
			}
			Root = SNew(SBorder)
				.BorderImage(&PanelBrush)
				.Padding(Space)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Backpack"))).Font(FCoreStyle::GetDefaultFontStyle(Heading.Role, Heading.Size)).ColorAndOpacity(Color)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[CellWidget.ToSharedRef()]
						+ SOverlay::Slot()[ItemGrid.ToSharedRef()]
					]
				];
		}

		virtual TSharedRef<SWidget> GetRoot() const override { return Root.ToSharedRef(); }

		virtual void SetCellHover(int32 Cell, bool bHover) override
		{
			if (Painter.IsValid())
			{
				Painter->SetHover(Cell, bHover);
				return;
			}
			Cells[Cell]->SetBorderImage(bHover ? &HoverBrush : &NormalBrush);
		}

		virtual void MoveItem(int32 N, int32 Slot) override
		{
			FPlainItem& Item = Items.FindChecked(Ids[N]);
			ItemGrid->RemoveSlot(Item.Widget.ToSharedRef());
			ItemGrid->AddSlot(Slot % Columns, Slot / Columns)[Item.Widget.ToSharedRef()];
		}

		virtual void ReplaceItems(int32 Generation) override
		{
			for (const FString& Id : Ids)
			{
				ItemGrid->RemoveSlot(Items.FindChecked(Id).Widget.ToSharedRef());
				Items.Remove(Id);
			}
			Ids.Reset();
			for (int32 N = 0; N < NumItems; ++N)
			{
				AddItem(PbItem(Generation, N, NumCells, NumItems));
			}
		}

	private:
		struct FPlainItem
		{
			TSharedPtr<SWidget> Widget;
		};

		int32 NumCells;
		int32 NumItems;
		int32 Columns;
		bool bHostPaintedCells;
		TSharedPtr<SPbCellPainter> Painter;
		FSlateBrush PanelBrush;
		FSlateBrush NormalBrush;
		FSlateBrush HoverBrush;
		FSlateFontInfo CaptionFont;
		FSlateColor Color;
		TSharedPtr<SWidget> Root;
		TSharedPtr<SUniformGridPanel> ItemGrid;
		TArray<TSharedRef<SBorder>> Cells;
		TMap<FString, FPlainItem> Items;
		TArray<FString> Ids;

		void AddItem(const FPbItemParams& P)
		{
			TSharedRef<SWidget> Widget = SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SImage).Image(Slater::FindImage(FName(*FString::Printf(TEXT("icon%d"), P.Icon)))).DesiredSizeOverride(FVector2D(36.0, 36.0)).Visibility(EVisibility::HitTestInvisible)
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
				[
					SNew(STextBlock).Text(FText::FromString(FString::FromInt(P.Count))).Font(CaptionFont).ColorAndOpacity(Color)
				]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
				[
					SNew(SImage).Image(Slater::FindImage(TEXT("lock"))).DesiredSizeOverride(FVector2D(12.0, 12.0)).Visibility(P.bLocked ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				];
			Items.Add(P.Id, {Widget});
			Ids.Add(P.Id);
			ItemGrid->AddSlot(P.Slot % Columns, P.Slot / Columns)[Widget];
		}
	};

	double PbMedianMs(TArray<double> Seconds)
	{
		if (Seconds.IsEmpty())
		{
			return 0.0;
		}
		Seconds.Sort();
		return Seconds[Seconds.Num() / 2] * 1000.0;
	}

	double PbP95Ms(TArray<double> Seconds)
	{
		if (Seconds.IsEmpty())
		{
			return 0.0;
		}
		Seconds.Sort();
		return Seconds[FMath::Min(Seconds.Num() - 1, Seconds.Num() * 95 / 100)] * 1000.0;
	}

	struct FPbFrames
	{
		TArray<double> Mutate;
		TArray<double> Draw;
		TArray<double> Total;
	};

	/** One frame: the changes, then Slate's prepass and paint of the whole tree. */
	template <typename TMutate>
	void PbFrame(FWidgetRenderer& Renderer, IPbGrid& Grid, const FVector2D& Size, FPbFrames& Out, bool bRecord, TMutate&& Mutate)
	{
		const double T0 = FPlatformTime::Seconds();
		Mutate();
		const double T1 = FPlatformTime::Seconds();
		Renderer.DrawWidget(Grid.GetRoot(), Size);
		const double T2 = FPlatformTime::Seconds();
		if (bRecord)
		{
			Out.Mutate.Add(T1 - T0);
			Out.Draw.Add(T2 - T1);
			Out.Total.Add(T2 - T0);
		}
	}

	FString PbRow(const TCHAR* Variant, const TCHAR* Scenario, const FPbFrames& F, int32 Widgets, int32 Polled)
	{
		return FString::Printf(TEXT("| %s | %s | %d | %.3f | %.3f | %.3f | %.3f | %d |\n"), Variant, Scenario, Widgets, PbMedianMs(F.Total), PbP95Ms(F.Total), PbMedianMs(F.Mutate), PbMedianMs(F.Draw), Polled);
	}
}
using namespace SlaterListProbeTestsPrivate;

/** Renders the shipped ListProbe layout, bound to mock data, to Saved/SlaterUI/Render/ListProbe_<Style>.png for review. */
TEST_CLASS(SlaterListRenderTests, "BRS.SlaterUI.List.Render")
{
	TEST_METHOD(ListProbe_RendersToPng)
	{
		if (GUsingNullRHI || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
		{
			AddInfo(TEXT("Skipped: requires a real RHI (run with -WithRHI)."));
			return;
		}
		SlaterListKit::FRig Rig;
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("SlaterUI/Render");
		IFileManager::Get().MakeDirectory(*Dir, true);
		FWidgetRenderer Renderer(false);
		ASSERT_THAT(IsTrue(Rig.Init(SlaterListKit::LayoutText(10), SlaterListKit::MakeMockJson(40, 9), ESlaterBindMode::Pushed)));
		// A hovered cell and a pressed one, to see the states.
		Rig.Model->SetItemString(TEXT("Cells"), TEXT("Index"), TEXT("13"), TEXT("State"), TEXT("hover"));
		Rig.Model->SetItemString(TEXT("Cells"), TEXT("Index"), TEXT("22"), TEXT("State"), TEXT("pressed"));
		UTextureRenderTarget2D* Target = nullptr;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Rig.Result.Root->SlatePrepass(1.f);
			Target = Renderer.DrawWidget(Rig.Result.Root.ToSharedRef(), FVector2D(FMath::CeilToDouble(Rig.Result.Root->GetDesiredSize().X), FMath::CeilToDouble(Rig.Result.Root->GetDesiredSize().Y)));
			FlushRenderingCommands();
		}
		ASSERT_THAT(IsNotNull(Target));
		TArray<FColor> Pixels;
		ASSERT_THAT(IsTrue(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)));
		for (FColor& Pixel : Pixels)
		{
			const FColor Encoded = FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f).ToFColorSRGB();
			Pixel = FColor(Encoded.R, Encoded.G, Encoded.B, 255);
		}
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		ASSERT_THAT(IsTrue(FFileHelper::SaveArrayToFile(Png, *(Dir / TEXT("ListProbe_Base.png")))));
		AddInfo(Dir / TEXT("ListProbe_Base.png"));
	}
};

TEST_CLASS(SlaterListProbeTests, "BRS.SlaterUI.List.Probe")
{
	TEST_METHOD(InventoryGrid_ThreeWays_FourScenarios_Measured)
	{
		if (GUsingNullRHI || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
		{
			AddInfo(TEXT("Skipped: requires a real RHI (run with -WithRHI)."));
			return;
		}

		FString Report = TEXT("Median / p95 of whole frames in ms (changes + prepass + paint, game thread), changes alone, paint alone; polled getter reads per frame.\n\n");
		Report += TEXT("| Variant | Scenario | Widgets | Frame median | Frame p95 | Changes | Paint | Polled reads/frame |\n|---|---|---|---|---|---|---|---|\n");
		FString PrepassReport = TEXT("\n| Variant | Grid | Idle prepass alone, median ms |\n|---|---|---|\n");
		FString BuildReport = TEXT("\n| Variant | Cells | Items | Parse + check | Build instances + bind | Total | Widgets |\n|---|---|---|---|---|---|---|\n");

		const TSharedPtr<FSlaterTestImages> Images = MakeShared<FSlaterTestImages>(16);
		FWidgetRenderer Renderer(false);
		FSlaterStyleStack Stack;
		ASSERT_THAT(IsTrue(Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack)));

		struct FSize
		{
			int32 Cells;
			int32 Items;
			int32 Columns;
			const TCHAR* Name;
		};
		const FSize Sizes[] = {{100, 24, 10, TEXT("backpack")}, {600, 120, 30, TEXT("stash")}};
		enum class EVariant { SlaterPushed, SlaterPolled, PlainPushed, HostPainted };
		const TCHAR* const VariantNames[] = {TEXT("layout, pushed"), TEXT("layout, polled"), TEXT("hand-written Slate, pushed"), TEXT("host-painted cells, pushed")};

		for (const FSize& Size : Sizes)
		{
			for (const EVariant Variant : {EVariant::SlaterPushed, EVariant::SlaterPolled, EVariant::PlainPushed, EVariant::HostPainted})
			{
				const TCHAR* VariantName = VariantNames[static_cast<int32>(Variant)];
				const bool bPlain = Variant == EVariant::PlainPushed || Variant == EVariant::HostPainted;
				const bool bHostPainted = Variant == EVariant::HostPainted;

				// --- Build cost, median of 21 builds after one warm-up (a busy machine moves a median of 7 by 30%).
				TArray<double> ParseTimes;
				TArray<double> BindTimes;
				TArray<double> PlainTimes;
				int32 Widgets = 0;
				for (int32 Run = 0; Run < 22; ++Run)
				{
					Slater::SetImageSource(Images);
					if (bPlain)
					{
						const double T0 = FPlatformTime::Seconds();
						FPbPlainGrid Plain(Stack, Size.Cells, Size.Items, Size.Columns, bHostPainted);
						const double T1 = FPlatformTime::Seconds();
						int32 Depth = 0;
						Slater::MeasureWidgetTree(Plain.GetRoot(), Widgets, Depth);
						if (Run > 0)
						{
							PlainTimes.Add(T1 - T0);
						}
					}
					else
					{
						// Parse and check the layout, then bind (builds one instance per cell and per item).
						SlaterListKit::FRig Rig;
						Rig.Images = Images;
						Rig.Stack = Stack;
						Rig.Model = MakeShared<FSlaterModel>();
						FSlaterError Error;
						FString Json = SlaterListKit::MakeMockJson(Size.Cells, Size.Items);
						ASSERT_THAT(IsTrue(Rig.Model->LoadMock(Json, TEXT("mock"), Error)));
						const double T0 = FPlatformTime::Seconds();
						ASSERT_THAT(IsTrue(Slater::BuildLayoutFromText(SlaterListKit::LayoutText(Size.Columns), TEXT("ListProbe"), Stack, Rig.Result)));
						const double T1 = FPlatformTime::Seconds();
						Rig.Binder = Slater::Bind(Rig.Result, Rig.Model.ToSharedRef(), Variant == EVariant::SlaterPushed ? ESlaterBindMode::Pushed : ESlaterBindMode::Polled, Stack);
						const double T2 = FPlatformTime::Seconds();
						int32 Depth = 0;
						Slater::MeasureWidgetTree(Rig.Result.Root.ToSharedRef(), Widgets, Depth);
						if (Run > 0)
						{
							ParseTimes.Add(T1 - T0);
							BindTimes.Add(T2 - T1);
						}
						Rig.Shutdown();
					}
				}
				if (bPlain)
				{
					BuildReport += FString::Printf(TEXT("| %s | %d | %d | n/a | %.3f | %.3f | %d |\n"), VariantName, Size.Cells, Size.Items, PbMedianMs(PlainTimes), PbMedianMs(PlainTimes), Widgets);
				}
				else
				{
					BuildReport += FString::Printf(TEXT("| %s | %d | %d | %.3f | %.3f | %.3f | %d |\n"), VariantName, Size.Cells, Size.Items, PbMedianMs(ParseTimes), PbMedianMs(BindTimes), PbMedianMs(ParseTimes) + PbMedianMs(BindTimes), Widgets);
				}

				// --- Scenarios on one live grid.
				Slater::SetImageSource(Images);
				TUniquePtr<IPbGrid> Grid;
				if (bPlain)
				{
					Grid = MakeUnique<FPbPlainGrid>(Stack, Size.Cells, Size.Items, Size.Columns, bHostPainted);
				}
				else
				{
					TUniquePtr<FPbSlaterGrid> SlaterGrid = MakeUnique<FPbSlaterGrid>(Images, Size.Cells, Size.Items, Size.Columns, Variant == EVariant::SlaterPushed ? ESlaterBindMode::Pushed : ESlaterBindMode::Polled);
					ASSERT_THAT(IsTrue(SlaterGrid->bOk));
					Grid = MoveTemp(SlaterGrid);
				}
				Grid->GetRoot()->SlatePrepass(1.f);
				const FVector2D DrawSize(FMath::CeilToDouble(Grid->GetRoot()->GetDesiredSize().X), FMath::CeilToDouble(Grid->GetRoot()->GetDesiredSize().Y));
				const int32 Frames = 150;
				const int32 Warmup = 10;

				// idle
				FPbFrames Idle;
				for (int32 F = 0; F < Frames + Warmup; ++F)
				{
					PbFrame(Renderer, *Grid, DrawSize, Idle, F >= Warmup, [] {});
				}
				FlushRenderingCommands();
				Slater::ListStats().Reset();
				FPbFrames IdleCounted;
				for (int32 F = 0; F < 20; ++F)
				{
					PbFrame(Renderer, *Grid, DrawSize, IdleCounted, true, [] {});
				}
				const int32 PolledPerFrame = Slater::ListStats().PolledReads / 20;
				{
					TArray<double> Prepass;
					for (int32 F = 0; F < 100; ++F)
					{
						const double T0 = FPlatformTime::Seconds();
						Grid->GetRoot()->SlatePrepass(1.f);
						Prepass.Add(FPlatformTime::Seconds() - T0);
					}
					PrepassReport += FString::Printf(TEXT("| %s | %s | %.3f |\n"), VariantName, Size.Name, PbMedianMs(Prepass));
				}
				Report += PbRow(VariantName, *FString::Printf(TEXT("idle, %s"), Size.Name), Idle, Widgets, PolledPerFrame);

				// hover sweep: the previous cell goes normal and the next goes hover, every frame.
				FPbFrames Hover;
				for (int32 F = 0; F < Frames + Warmup; ++F)
				{
					PbFrame(Renderer, *Grid, DrawSize, Hover, F >= Warmup, [&]
					{
						if (F > 0)
						{
							Grid->SetCellHover((F - 1) % Size.Cells, false);
						}
						Grid->SetCellHover(F % Size.Cells, true);
					});
				}
				Grid->SetCellHover((Frames + Warmup - 1) % Size.Cells, false);
				FlushRenderingCommands();
				Report += PbRow(VariantName, *FString::Printf(TEXT("hover sweep, %s"), Size.Name), Hover, Widgets, PolledPerFrame);

				// item drag: one item moves to another cell every frame while the cell under it highlights.
				FPbFrames Drag;
				for (int32 F = 0; F < Frames + Warmup; ++F)
				{
					PbFrame(Renderer, *Grid, DrawSize, Drag, F >= Warmup, [&]
					{
						const int32 Cell = (F * 3 + 1) % Size.Cells;
						if (F > 0)
						{
							Grid->SetCellHover(((F - 1) * 3 + 1) % Size.Cells, false);
						}
						Grid->SetCellHover(Cell, true);
						Grid->MoveItem(0, Cell);
					});
				}
				Grid->SetCellHover(((Frames + Warmup - 1) * 3 + 1) % Size.Cells, false);
				FlushRenderingCommands();
				Report += PbRow(VariantName, *FString::Printf(TEXT("item drag, %s"), Size.Name), Drag, Widgets, PolledPerFrame);

				// open-container burst: every item is removed and as many new ones added, in one frame.
				FPbFrames Burst;
				for (int32 F = 0; F < 25; ++F)
				{
					Grid->PrepareItems(F + 1);
					PbFrame(Renderer, *Grid, DrawSize, Burst, F >= 5, [&] { Grid->ReplaceItems(F + 1); });
				}
				FlushRenderingCommands();
				Report += PbRow(VariantName, *FString::Printf(TEXT("open burst (%d items), %s"), Size.Items, Size.Name), Burst, Widgets, PolledPerFrame);

				Grid.Reset();
				Slater::SetImageSource(nullptr);
			}
		}

		Report += PrepassReport + BuildReport;
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("SlaterUI");
		IFileManager::Get().MakeDirectory(*Dir, true);
		FFileHelper::SaveStringToFile(Report, *(Dir / TEXT("ListProbe.txt")));
		AddInfo(Report);
	}
};

#endif
