// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterElements.h"

#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSafeZone.h"

namespace SlaterPanelElementsPrivate
{
	/** Where each anchor word pins the child: minimum and maximum corner of the parent, 0 to 1 on each axis. */
	FAnchors AnchorsOf(const TOptional<FName>& Word)
	{
		struct FEntry { const TCHAR* Name; FAnchors Anchors; };
		static const FEntry Entries[] = {
			{TEXT("top-left"), FAnchors(0.f, 0.f)},
			{TEXT("top"), FAnchors(0.5f, 0.f)},
			{TEXT("top-right"), FAnchors(1.f, 0.f)},
			{TEXT("left"), FAnchors(0.f, 0.5f)},
			{TEXT("center"), FAnchors(0.5f, 0.5f)},
			{TEXT("right"), FAnchors(1.f, 0.5f)},
			{TEXT("bottom-left"), FAnchors(0.f, 1.f)},
			{TEXT("bottom"), FAnchors(0.5f, 1.f)},
			{TEXT("bottom-right"), FAnchors(1.f, 1.f)},
			{TEXT("top-wide"), FAnchors(0.f, 0.f, 1.f, 0.f)},
			{TEXT("bottom-wide"), FAnchors(0.f, 1.f, 1.f, 1.f)},
			{TEXT("left-tall"), FAnchors(0.f, 0.f, 0.f, 1.f)},
			{TEXT("right-tall"), FAnchors(1.f, 0.f, 1.f, 1.f)},
			{TEXT("fill"), FAnchors(0.f, 0.f, 1.f, 1.f)},
		};
		if (Word.IsSet())
		{
			for (const FEntry& Entry : Entries)
			{
				if (Word.GetValue() == Entry.Name)
				{
					return Entry.Anchors;
				}
			}
		}
		return Entries[0].Anchors;
	}

	TSharedPtr<SWidget> BuildCanvas(FSlaterBuildContext& Context)
	{
		TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);
		for (const FSlaterBuiltChild& Child : Context.Children)
		{
			const FSlaterSlotValues& Values = Child.Slot;
			const FAnchors Anchors = AnchorsOf(Values.Anchor);
			const bool bStretchX = Anchors.Minimum.X != Anchors.Maximum.X;
			const bool bStretchY = Anchors.Minimum.Y != Anchors.Maximum.Y;
			const FVector2D Size = Values.Size.Get(FVector2D::ZeroVector);

			// A point axis reads Left/Top as the position and Right/Bottom as the size; a stretched axis reads them as the two insets.
			const float Left = static_cast<float>(Values.Offset.X);
			const float Top = static_cast<float>(Values.Offset.Y);
			const FMargin Offset(Left, Top, bStretchX ? Left : static_cast<float>(Size.X), bStretchY ? Top : static_cast<float>(Size.Y));
			const FVector2D Alignment = Values.Align.Get(FVector2D(bStretchX ? 0.f : Anchors.Minimum.X, bStretchY ? 0.f : Anchors.Minimum.Y));

			SConstraintCanvas::FScopedWidgetSlotArguments Slot = Canvas->AddSlot();
			Slot.Anchors(Anchors);
			Slot.Offset(Offset);
			Slot.Alignment(Alignment);
			// Without a size the child keeps its own, which only a point anchor on both axes can honor.
			Slot.AutoSize(!Values.Size.IsSet() && !bStretchX && !bStretchY);
			Slot[Child.Widget];
		}
		return Canvas;
	}

	/** Weights like `"1, 0, 2"`: the fill coefficient of column or row 0, 1, 2. Empty text sets none. */
	FString CheckWeights(const FString& Text)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			const FString Trimmed = Part.TrimStartAndEnd();
			if (!Trimmed.IsNumeric() || FCString::Atof(*Trimmed) < 0.f)
			{
				return FString::Printf(TEXT("'%s' is not a list of weights: write numbers of 0 or more separated by commas, for example \"1, 0, 2\""), *Text);
			}
		}
		return FString();
	}

	TArray<float> WeightsOf(const FString& Text)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), true);
		TArray<float> Weights;
		for (const FString& Part : Parts)
		{
			Weights.Add(FCString::Atof(*Part.TrimStartAndEnd()));
		}
		return Weights;
	}

	TSharedPtr<SWidget> BuildGrid(FSlaterBuildContext& Context)
	{
		TSharedRef<SGridPanel> Grid = SNew(SGridPanel);
		const float HalfGap = Context.GetMetric(TEXT("gap")) * 0.5f;
		for (const FSlaterBuiltChild& Child : Context.Children)
		{
			const FSlaterSlotValues& Values = Child.Slot;
			SGridPanel::FScopedWidgetSlotArguments Slot = Grid->AddSlot(Values.Column, Values.Row);
			Slot.ColumnSpan(Values.ColumnSpan);
			Slot.RowSpan(Values.RowSpan);
			Slot.Padding(FMargin(HalfGap + Values.Margin));
			if (Values.HAlign.IsSet())
			{
				Slot.HAlign(Values.HAlign.GetValue());
			}
			if (Values.VAlign.IsSet())
			{
				Slot.VAlign(Values.VAlign.GetValue());
			}
			Slot[Child.Widget];
		}
		const TArray<float> Columns = WeightsOf(Context.GetText(TEXT("column-fill")));
		for (int32 Index = 0; Index < Columns.Num(); ++Index)
		{
			Grid->SetColumnFill(Index, Columns[Index]);
		}
		const TArray<float> Rows = WeightsOf(Context.GetText(TEXT("row-fill")));
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			Grid->SetRowFill(Index, Rows[Index]);
		}
		return Grid;
	}

	TSharedPtr<SWidget> BuildSafeZone(FSlaterBuildContext& Context)
	{
		TSharedRef<SSafeZone> Zone = SNew(SSafeZone)
			.IsTitleSafe(Context.GetBool(TEXT("title-safe"), true))
			.PadLeft(Context.GetBool(TEXT("pad-left"), true))
			.PadRight(Context.GetBool(TEXT("pad-right"), true))
			.PadTop(Context.GetBool(TEXT("pad-top"), true))
			.PadBottom(Context.GetBool(TEXT("pad-bottom"), true));
		if (Context.Children.Num() > 0)
		{
			Zone->SetContent(Context.Children[0].Widget);
		}
		return Zone;
	}

	FSlaterAttrDesc PanelAttr(const TCHAR* Name, ESlaterAttrType Type)
	{
		FSlaterAttrDesc Attr;
		Attr.Name = Name;
		Attr.Type = Type;
		return Attr;
	}

	FSlaterAttrDesc WeightsAttr(const TCHAR* Name)
	{
		FSlaterAttrDesc Attr = PanelAttr(Name, ESlaterAttrType::Text);
		Attr.CheckText = &CheckWeights;
		return Attr;
	}

	void AddPanel(FSlaterElementRegistry& Registry, const TCHAR* Tag, const TCHAR* Purpose, ESlaterChildRule Children, ESlaterSlotAttrs SlotAttrs, FSlaterBuilder Builder, TArray<FSlaterAttrDesc> Attributes)
	{
		FSlaterElementDesc Desc;
		Desc.Tag = Tag;
		Desc.Purpose = Purpose;
		Desc.Children = Children;
		Desc.SlotAttrs = SlotAttrs;
		Desc.Builder = MoveTemp(Builder);
		Desc.Attributes = MoveTemp(Attributes);
		FString Error;
		const bool bOk = Registry.Register(MoveTemp(Desc), &Error);
		ensureMsgf(bOk, TEXT("SlaterUI stock element <%s>: %s"), Tag, *Error);
	}
}
using namespace SlaterPanelElementsPrivate;

void Slater::RegisterPanelElements(FSlaterElementRegistry& Registry)
{
	AddPanel(Registry, TEXT("canvas"), TEXT("children pinned to the edges, corners or center of the parent (an anchored HUD)"),
		ESlaterChildRule::Many, ESlaterSlotAttrs::CanvasAll, &BuildCanvas, {});

	AddPanel(Registry, TEXT("grid"), TEXT("rows and columns, a child may span several cells"),
		ESlaterChildRule::Many, ESlaterSlotAttrs::GridAll | ESlaterSlotAttrs::Margin | ESlaterSlotAttrs::HAlign | ESlaterSlotAttrs::VAlign, &BuildGrid,
		{PanelAttr(TEXT("gap"), ESlaterAttrType::Metric), WeightsAttr(TEXT("column-fill")), WeightsAttr(TEXT("row-fill"))});

	AddPanel(Registry, TEXT("safe-zone"), TEXT("keeps its child inside the TV and console safe area"),
		ESlaterChildRule::One, ESlaterSlotAttrs::None, &BuildSafeZone,
		{PanelAttr(TEXT("title-safe"), ESlaterAttrType::Bool), PanelAttr(TEXT("pad-left"), ESlaterAttrType::Bool), PanelAttr(TEXT("pad-right"), ESlaterAttrType::Bool),
		 PanelAttr(TEXT("pad-top"), ESlaterAttrType::Bool), PanelAttr(TEXT("pad-bottom"), ESlaterAttrType::Bool)});
}
