// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterSurface.h"

#include "SlaterCommands.h"
#include "SlaterView.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterSurface, Log, All);

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
	Brush = BrushFor(Surface);
	SetBorderImage(Brush.Get());
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

bool SSlaterSurface::Activate()
{
	if (!HasAction() || !IsEnabled())
	{
		return false;
	}
	TSharedPtr<SSlaterView> View;
	for (TSharedPtr<SWidget> Parent = GetParentWidget(); Parent.IsValid(); Parent = Parent->GetParentWidget())
	{
		if (Parent->GetType() == FName(TEXT("SSlaterView")))
		{
			View = StaticCastSharedPtr<SSlaterView>(Parent);
			break;
		}
	}
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
		// The command may rebuild the tree this surface is in, so nothing of the surface is touched after it runs.
		const FSlaterCommandCall Call{Command, AsShared(), CommandArgument};
		const bool bRan = View.IsValid() ? View->RunCommand(Call) : Slater::RunCommand(Slater::Commands(), Call);
		bOk = bOk && bRan;
	}
	return bOk;
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
