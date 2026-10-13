// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterElements.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SSpacer.h"

namespace
{
	/** Alignments only where the markup said so. */
	template <typename SlotType>
	void ApplyAlign(SlotType& Slot, const FSlaterSlotValues& Values)
	{
		if (Values.HAlign.IsSet())
		{
			Slot.HAlign(Values.HAlign.GetValue());
		}
		if (Values.VAlign.IsSet())
		{
			Slot.VAlign(Values.VAlign.GetValue());
		}
	}

	/** Margin, then the alignments. For slots that have padding. */
	template <typename SlotType>
	void ApplyCommon(SlotType& Slot, const FSlaterSlotValues& Values)
	{
		if (Values.Margin != 0.f)
		{
			Slot.Padding(FMargin(Values.Margin));
		}
		ApplyAlign(Slot, Values);
	}

	TSharedPtr<SWidget> BuildVBox(FSlaterBuildContext& Context)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (const FSlaterBuiltChild& Child : Context.Children)
		{
			SVerticalBox::FScopedWidgetSlotArguments Slot = Box->AddSlot();
			if (Child.Slot.Fill.IsSet())
			{
				Slot.FillHeight(Child.Slot.Fill.GetValue());
			}
			else
			{
				Slot.AutoHeight();
			}
			ApplyCommon(Slot, Child.Slot);
			Slot[Child.Widget];
		}
		return Box;
	}

	TSharedPtr<SWidget> BuildHBox(FSlaterBuildContext& Context)
	{
		TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
		for (const FSlaterBuiltChild& Child : Context.Children)
		{
			SHorizontalBox::FScopedWidgetSlotArguments Slot = Box->AddSlot();
			if (Child.Slot.Fill.IsSet())
			{
				Slot.FillWidth(Child.Slot.Fill.GetValue());
			}
			else
			{
				Slot.AutoWidth();
			}
			ApplyCommon(Slot, Child.Slot);
			Slot[Child.Widget];
		}
		return Box;
	}

	TSharedPtr<SWidget> BuildOverlay(FSlaterBuildContext& Context)
	{
		TSharedRef<SOverlay> Overlay = SNew(SOverlay);
		for (const FSlaterBuiltChild& Child : Context.Children)
		{
			SOverlay::FScopedWidgetSlotArguments Slot = Overlay->AddSlot();
			ApplyCommon(Slot, Child.Slot);
			Slot[Child.Widget];
		}
		return Overlay;
	}

	TSharedPtr<SWidget> BuildBox(FSlaterBuildContext& Context)
	{
		TSharedRef<SBox> Box = SNew(SBox);
		Slater::ApplySizing(Context, *Box);
		if (Context.Children.Num() > 0)
		{
			const FSlaterBuiltChild& Child = Context.Children[0];
			if (Child.Slot.HAlign.IsSet())
			{
				Box->SetHAlign(Child.Slot.HAlign.GetValue());
			}
			if (Child.Slot.VAlign.IsSet())
			{
				Box->SetVAlign(Child.Slot.VAlign.GetValue());
			}
			if (Child.Slot.Margin != 0.f)
			{
				// The element's own padding and the child's margin share the box's one padding; they add.
				Box->SetPadding(FMargin(Context.GetMetric(TEXT("padding")) + Child.Slot.Margin));
			}
			Box->SetContent(Child.Widget);
		}
		return Box;
	}

	TSharedPtr<SWidget> BuildScroll(FSlaterBuildContext& Context)
	{
		const bool bHorizontal = Context.GetEnum(TEXT("orientation"), TEXT("vertical")) == TEXT("horizontal");
		TSharedRef<SScrollBox> Scroll = SNew(SScrollBox).Orientation(bHorizontal ? Orient_Horizontal : Orient_Vertical);
		for (const FSlaterBuiltChild& Child : Context.Children)
		{
			SScrollBox::FScopedWidgetSlotArguments Slot = Scroll->AddSlot();
			ApplyCommon(Slot, Child.Slot);
			Slot[Child.Widget];
		}
		return Scroll;
	}

	TSharedPtr<SWidget> BuildUniformGrid(FSlaterBuildContext& Context)
	{
		const int32 Columns = FMath::Max(1, Context.GetInt(TEXT("columns"), 1));
		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel);
		if (Context.Has(TEXT("gap")))
		{
			Grid->SetSlotPadding(FMargin(Context.GetMetric(TEXT("gap")) * 0.5f));
		}
		if (Context.Has(TEXT("min-cell-width")))
		{
			Grid->SetMinDesiredSlotWidth(Context.GetMetric(TEXT("min-cell-width")));
		}
		if (Context.Has(TEXT("min-cell-height")))
		{
			Grid->SetMinDesiredSlotHeight(Context.GetMetric(TEXT("min-cell-height")));
		}
		int32 Index = 0;
		for (const FSlaterBuiltChild& Child : Context.Children)
		{
			SUniformGridPanel::FScopedWidgetSlotArguments Slot = Grid->AddSlot(Index % Columns, Index / Columns);
			ApplyAlign(Slot, Child.Slot);
			Slot[Child.Widget];
			++Index;
		}
		return Grid;
	}

	TSharedPtr<SWidget> BuildWrap(FSlaterBuildContext& Context)
	{
		const bool bHorizontal = Context.GetEnum(TEXT("orientation"), TEXT("horizontal")) == TEXT("horizontal");
		TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).Orientation(bHorizontal ? Orient_Horizontal : Orient_Vertical);
		if (Context.Has(TEXT("gap")))
		{
			Wrap->SetInnerSlotPadding(FVector2D(Context.GetMetric(TEXT("gap"))));
		}
		for (const FSlaterBuiltChild& Child : Context.Children)
		{
			SWrapBox::FScopedWidgetSlotArguments Slot = Wrap->AddSlot();
			ApplyCommon(Slot, Child.Slot);
			Slot[Child.Widget];
		}
		return Wrap;
	}

	TSharedPtr<SWidget> BuildSpacer(FSlaterBuildContext&)
	{
		return SNew(SSpacer);
	}

	FSlaterAttrDesc OrientationAttr(const TCHAR* First, const TCHAR* Second)
	{
		FSlaterAttrDesc Attr;
		Attr.Name = TEXT("orientation");
		Attr.Type = ESlaterAttrType::Enum;
		Attr.EnumValues = {FName(First), FName(Second)};
		return Attr;
	}

	FSlaterAttrDesc MetricAttr(const TCHAR* Name)
	{
		FSlaterAttrDesc Attr;
		Attr.Name = Name;
		Attr.Type = ESlaterAttrType::Metric;
		return Attr;
	}

	const TCHAR* PurposeOf(const FString& Tag)
	{
		if (Tag == TEXT("vbox")) return TEXT("stacks children top to bottom");
		if (Tag == TEXT("hbox")) return TEXT("lays children out left to right");
		if (Tag == TEXT("overlay")) return TEXT("layers children on top of each other");
		if (Tag == TEXT("box")) return TEXT("one child with a size or padding");
		if (Tag == TEXT("scroll")) return TEXT("scrolls children that do not fit");
		if (Tag == TEXT("uniform-grid")) return TEXT("a grid of equal cells");
		if (Tag == TEXT("wrap")) return TEXT("flows children onto new lines");
		if (Tag == TEXT("spacer")) return TEXT("empty space");
		return TEXT("");
	}

	void Add(FSlaterElementRegistry& Registry, const TCHAR* Tag, ESlaterChildRule Children, ESlaterSlotAttrs SlotAttrs, FSlaterBuilder Builder, TArray<FSlaterAttrDesc> Attributes = {}, bool bAppliesSizing = false)
	{
		FSlaterElementDesc Desc;
		Desc.Tag = Tag;
		Desc.Children = Children;
		Desc.SlotAttrs = SlotAttrs;
		Desc.Builder = MoveTemp(Builder);
		Desc.Attributes = MoveTemp(Attributes);
		Desc.bAppliesSizing = bAppliesSizing;
		Desc.Purpose = PurposeOf(Tag);
		FString Error;
		const bool bOk = Registry.Register(MoveTemp(Desc), &Error);
		ensureMsgf(bOk, TEXT("SlaterUI stock element <%s>: %s"), Tag, *Error);
	}
}

void Slater::RegisterLayoutElements(FSlaterElementRegistry& Registry)
{
	const ESlaterSlotAttrs Margins = ESlaterSlotAttrs::Margin | ESlaterSlotAttrs::HAlign | ESlaterSlotAttrs::VAlign;

	Add(Registry, TEXT("vbox"), ESlaterChildRule::Many, ESlaterSlotAttrs::All, &BuildVBox);
	Add(Registry, TEXT("hbox"), ESlaterChildRule::Many, ESlaterSlotAttrs::All, &BuildHBox);
	Add(Registry, TEXT("overlay"), ESlaterChildRule::Many, Margins, &BuildOverlay);
	Add(Registry, TEXT("box"), ESlaterChildRule::One, Margins, &BuildBox, {}, true);
	Add(Registry, TEXT("scroll"), ESlaterChildRule::Many, ESlaterSlotAttrs::Margin | ESlaterSlotAttrs::HAlign, &BuildScroll, {OrientationAttr(TEXT("vertical"), TEXT("horizontal"))});

	FSlaterAttrDesc Columns;
	Columns.Name = TEXT("columns");
	Columns.Type = ESlaterAttrType::Int;
	Columns.bRequired = true;
	Add(Registry, TEXT("uniform-grid"), ESlaterChildRule::Many, ESlaterSlotAttrs::HAlign | ESlaterSlotAttrs::VAlign, &BuildUniformGrid,
		{Columns, MetricAttr(TEXT("gap")), MetricAttr(TEXT("min-cell-width")), MetricAttr(TEXT("min-cell-height"))});

	Add(Registry, TEXT("wrap"), ESlaterChildRule::Many, Margins, &BuildWrap, {MetricAttr(TEXT("gap")), OrientationAttr(TEXT("horizontal"), TEXT("vertical"))});
	Add(Registry, TEXT("spacer"), ESlaterChildRule::None, ESlaterSlotAttrs::None, &BuildSpacer);
}
