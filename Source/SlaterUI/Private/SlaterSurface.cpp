// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterSurface.h"

#include "SlaterCommands.h"
#include "SlaterEvents.h"
#include "SlaterView.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterSurface, Log, All);

void Slater::MakeChamferPath(const FVector2f& Size, const FSlaterChamfer& Cuts, float Inset, TArray<FVector2f>& OutPoints)
{
	OutPoints.Reset();
	const float X0 = Inset;
	const float Y0 = Inset;
	const float X1 = Size.X - Inset;
	const float Y1 = Size.Y - Inset;
	if (X1 <= X0 || Y1 <= Y0)
	{
		return;
	}

	// Two cuts along one edge must fit in it: shrink all four by the same factor, which keeps the shape.
	float TL = FMath::Max(Cuts.TopLeft, 0.f);
	float TR = FMath::Max(Cuts.TopRight, 0.f);
	float BR = FMath::Max(Cuts.BottomRight, 0.f);
	float BL = FMath::Max(Cuts.BottomLeft, 0.f);
	float Scale = 1.f;
	const auto Fit = [&Scale](float Sum, float Length)
	{
		if (Sum > Length)
		{
			Scale = FMath::Min(Scale, Length / Sum);
		}
	};
	Fit(TL + TR, Size.X);
	Fit(BL + BR, Size.X);
	Fit(TL + BL, Size.Y);
	Fit(TR + BR, Size.Y);

	// The diagonal x + y = c moved inwards by Inset is x + y = c + Inset * sqrt(2). It meets the shrunk box's left edge
	// (x = Inset) at y = c + Inset * (sqrt(2) - 1), which is a cut of c + Inset * (sqrt(2) - 2) from the shrunk corner.
	const float Grow = Inset * (UE_SQRT_2 - 2.f);
	const auto Cut = [Scale, Grow](float C) { return C > 0.f ? FMath::Max(C * Scale + Grow, 0.f) : 0.f; };
	TL = Cut(TL);
	TR = Cut(TR);
	BR = Cut(BR);
	BL = Cut(BL);

	if (TL > 0.f) { OutPoints.Add(FVector2f(X0, Y0 + TL)); OutPoints.Add(FVector2f(X0 + TL, Y0)); } else { OutPoints.Add(FVector2f(X0, Y0)); }
	if (TR > 0.f) { OutPoints.Add(FVector2f(X1 - TR, Y0)); OutPoints.Add(FVector2f(X1, Y0 + TR)); } else { OutPoints.Add(FVector2f(X1, Y0)); }
	if (BR > 0.f) { OutPoints.Add(FVector2f(X1, Y1 - BR)); OutPoints.Add(FVector2f(X1 - BR, Y1)); } else { OutPoints.Add(FVector2f(X1, Y1)); }
	if (BL > 0.f) { OutPoints.Add(FVector2f(X0 + BL, Y1)); OutPoints.Add(FVector2f(X0, Y1 - BL)); } else { OutPoints.Add(FVector2f(X0, Y1)); }
}

FSlateBrush SSlaterSurface::MakeBrush(const FSlaterSurface& InSurface)
{
	FSlateBrush Result;
	Result.DrawAs = ESlateBrushDrawType::RoundedBox;
	Result.ImageType = ESlateBrushImageType::NoImage;
	Result.TintColor = InSurface.Fill;
	Result.OutlineSettings = FSlateBrushOutlineSettings(InSurface.CornerRadius, InSurface.Border, InSurface.BorderWidth);
	return Result;
}

TSharedRef<const FSlateBrush> SSlaterSurface::BrushFor(const FSlaterSurface& InSurface)
{
	static TArray<TPair<FSlaterSurface, TSharedRef<const FSlateBrush>>> Cache;
	for (const TPair<FSlaterSurface, TSharedRef<const FSlateBrush>>& Entry : Cache)
	{
		if (Entry.Key == InSurface)
		{
			return Entry.Value;
		}
	}
	if (Cache.Num() >= 64)
	{
		// Widgets keep the brushes they use; this only forgets the oldest.
		Cache.RemoveAt(0);
	}
	const TSharedRef<const FSlateBrush> Made = MakeShared<const FSlateBrush>(MakeBrush(InSurface));
	Cache.Emplace(InSurface, Made);
	return Made;
}

void SSlaterSurface::Construct(const FArguments& InArgs)
{
	Surface = InArgs._Surface;
	RestingSurface = Surface;
	Brush = BrushFor(Surface);

	SBorder::Construct(SBorder::FArguments()
		.BorderImage(Brush.Get())
		.Padding(InArgs._Inset)
		[
			InArgs._Content.Widget
		]);
}

void SSlaterSurface::SetSurface(const FSlaterSurface& InSurface)
{
	RestingSurface = InSurface;
	Refresh();
}

void SSlaterSurface::Refresh()
{
	const FSlaterSurface& Wanted = !bInteractive ? RestingSurface : (bHovered && bPressed) ? PressedSurface : bHovered ? HoverSurface : RestingSurface;
	if (Surface == Wanted)
	{
		return;
	}
	Surface = Wanted;
	if (IsChamfered())
	{
		// Drawn in OnPaint from the surface values; the border keeps its empty brush.
		Invalidate(EInvalidateWidgetReason::Paint);
		return;
	}
	Brush = BrushFor(Surface);
	SetBorderImage(Brush.Get());
}

void SSlaterSurface::SetChamfer(const FSlaterChamfer& InCuts)
{
	Chamfer = InCuts;
	if (IsChamfered())
	{
		SetBorderImage(FStyleDefaults::GetNoBrush());
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	else
	{
		Brush = BrushFor(Surface);
		SetBorderImage(Brush.Get());
	}
}

int32 SSlaterSurface::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!IsChamfered())
	{
		return SBorder::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	const ESlateDrawEffect Effects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	const FVector2f Size = AllottedGeometry.GetLocalSize();

	// The outline sits inside the box, its center line half a border width in. With no border a one pixel line in the
	// fill color is drawn on the same path: the polygon's edge has no anti-aliasing of its own, the line has.
	const bool bBorder = Surface.BorderWidth > 0.f && Surface.Border.A > 0.f;
	const float EdgeWidth = bBorder ? Surface.BorderWidth : 1.f;
	FLinearColor EdgeColor = bBorder ? Surface.Border : Surface.Fill;
	TArray<FVector2f> Path;
	Slater::MakeChamferPath(Size, Chamfer, EdgeWidth * 0.5f, Path);
	if (Path.Num() < 3)
	{
		return SBorder::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
	}

	if (Surface.Fill.A > 0.f && FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		// Custom vertices need a texture to sample; the core style's white box, sampled at the middle of its area.
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("GenericWhiteBox");
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
		const FSlateShaderResourceProxy* Proxy = Handle.GetResourceProxy();
		if (Proxy)
		{
			FLinearColor FillColor = Surface.Fill;
			FillColor.A *= Opacity;
			const FColor Vertex = FillColor.ToFColorSRGB();
			const FVector2f Uv = Proxy->StartUV + Proxy->SizeUV * 0.5f;
			const FSlateRenderTransform& Transform = AllottedGeometry.GetAccumulatedRenderTransform();
			TArray<FSlateVertex> Vertices;
			TArray<SlateIndex> Indices;
			for (const FVector2f& Point : Path)
			{
				Vertices.Add(FSlateVertex::Make(Transform, Point, Uv, Vertex));
			}
			// The polygon is convex, so a fan from the first point covers it.
			for (int32 Index = 1; Index + 1 < Path.Num(); ++Index)
			{
				Indices.Add(0);
				Indices.Add(static_cast<SlateIndex>(Index));
				Indices.Add(static_cast<SlateIndex>(Index + 1));
			}
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, Vertices, Indices, nullptr, 0, 0, Effects);
		}
	}

	EdgeColor.A *= Opacity;
	if (EdgeColor.A > 0.f)
	{
		const FVector2f First = Path[0];
		Path.Add(First);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Path, Effects, EdgeColor, true, EdgeWidth);
	}
	return SBorder::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
}

void SSlaterSurface::SetInteractive(const FSlaterSurface& InHover, const FSlaterSurface& InPressed)
{
	bInteractive = true;
	HoverSurface = InHover;
	PressedSurface = InPressed;
	// A border is not hit-testable on its own.
	SetVisibility(EVisibility::Visible);
	Refresh();
}

void SSlaterSurface::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SBorder::OnMouseEnter(MyGeometry, MouseEvent);
	if (bInteractive && IsEnabled())
	{
		bHovered = true;
		Refresh();
	}
	if (!HoverStateKey.IsNone() && IsEnabled())
	{
		ApplyPointerState(HoverStateKey, HoverStateValue, TEXT("hover-set-state"));
	}
}

void SSlaterSurface::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SBorder::OnMouseLeave(MouseEvent);
	if (bInteractive)
	{
		bHovered = false;
		bPressed = false;
		Refresh();
	}
	if (!LeaveStateKey.IsNone() && IsEnabled())
	{
		ApplyPointerState(LeaveStateKey, LeaveStateValue, TEXT("leave-set-state"));
	}
}

TSharedPtr<SSlaterView> SSlaterSurface::FindView() const
{
	// Also finds the view from inside a popup's menu (T-439), which the engine's menu stack parents elsewhere.
	return Slater::FindOwningView(*this);
}

void SSlaterSurface::ApplyPointerState(FName Key, const FString& Value, const TCHAR* Attribute) const
{
	if (const TSharedPtr<SSlaterView> View = FindView())
	{
		View->SetState(Key, Value);
	}
	else
	{
		UE_LOG(LogSlaterSurface, Warning, TEXT("SlaterUI: %s=\"%s=%s\" needs the surface to be inside an SSlaterView, which holds the state"), Attribute, *Key.ToString(), *Value);
	}
}

void SSlaterSurface::SetHoverStateChange(FName InKey, const FString& InValue)
{
	HoverStateKey = InKey;
	HoverStateValue = InValue;
	// A border is not hit-testable on its own; without it the pointer would never enter.
	SetVisibility(EVisibility::Visible);
}

void SSlaterSurface::SetLeaveStateChange(FName InKey, const FString& InValue)
{
	LeaveStateKey = InKey;
	LeaveStateValue = InValue;
	SetVisibility(EVisibility::Visible);
}

void SSlaterSurface::SetCommand(FName InCommand, const FString& InArgument)
{
	Command = InCommand;
	CommandArgument = InArgument;
	if (HasAction())
	{
		// A border is not hit-testable on its own; with an action it is the button.
		SetVisibility(EVisibility::Visible);
	}
}

void SSlaterSurface::SetStateChange(FName InKey, const FString& InValue)
{
	StateKey = InKey;
	StateValue = InValue;
	if (HasAction())
	{
		SetVisibility(EVisibility::Visible);
	}
}

bool Slater::ActivateAction(const TSharedRef<SWidget>& Source, FName StateKey, const FString& StateValue, FName Command, const FString& CommandArgument)
{
	const TSharedPtr<SSlaterView> View = Slater::FindOwningView(*Source);
	bool bOk = true;
	if (!StateKey.IsNone())
	{
		if (View.IsValid())
		{
			bOk = View->SetState(StateKey, StateValue);
		}
		else
		{
			UE_LOG(LogSlaterSurface, Warning, TEXT("SlaterUI: set-state=\"%s=%s\" needs the surface to be inside an SSlaterView, which holds the state"), *StateKey.ToString(), *StateValue);
			bOk = false;
		}
	}
	if (!Command.IsNone())
	{
		const FSlaterCommandCall Call{Command, Source, CommandArgument};
		const bool bRan = View.IsValid() ? View->RunCommand(Call) : Slater::RunCommand(Slater::Commands(), Call);
		bOk = bOk && bRan;
	}
	return bOk;
}

bool SSlaterSurface::Activate()
{
	if (!HasAction() || !IsEnabled())
	{
		return false;
	}
	return Slater::ActivateAction(AsShared(), StateKey, StateValue, Command, CommandArgument);
}

FReply SSlaterSurface::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bInteractive && IsEnabled() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPressed = true;
		Refresh();
		return FReply::Handled();
	}
	if (HasAction() && IsEnabled() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}
	return SBorder::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SSlaterSurface::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bInteractive && bPressed && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPressed = false;
		Refresh();
		if (HasAction() && IsEnabled())
		{
			Activate();
		}
		return FReply::Handled();
	}
	if (HasAction() && IsEnabled() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Activate();
		return FReply::Handled();
	}
	return SBorder::OnMouseButtonUp(MyGeometry, MouseEvent);
}
