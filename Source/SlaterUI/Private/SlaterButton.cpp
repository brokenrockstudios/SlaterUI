// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterButton.h"

#include "SlaterSurface.h"

#include "Styling/SlateTypes.h"
#include "Styling/StyleDefaults.h"

namespace SlaterButtonPrivate
{
	/** The engine button with nothing drawn: the face is the look. */
	const FButtonStyle& BareButtonStyle()
	{
		static const FButtonStyle Style = []()
		{
			FButtonStyle Result;
			const FSlateBrush& None = *FStyleDefaults::GetNoBrush();
			Result.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None);
			Result.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
			return Result;
		}();
		return Style;
	}
}
using namespace SlaterButtonPrivate;

void SSlaterButton::Construct(const FArguments& InArgs)
{
	Looks = InArgs._Looks;
	Face = SNew(SSlaterSurface)
		.Surface(Looks.Rest)
		.Inset(InArgs._Inset)
		[
			InArgs._Content.Widget
		];
	// The button is the hit target; the face lets the pointer through to it, and to anything clickable inside it.
	Face->SetVisibility(EVisibility::SelfHitTestInvisible);

	SButton::Construct(SButton::FArguments()
		.ButtonStyle(&BareButtonStyle())
		.ContentPadding(FMargin(0.f))
		.OnClicked(FOnClicked::CreateSP(this, &SSlaterButton::HandleClicked))
		[
			Face.ToSharedRef()
		]);
	UpdateLook();
}

void SSlaterButton::SetCommand(FName InCommand, const FString& InArgument)
{
	Command = InCommand;
	CommandArgument = InArgument;
}

void SSlaterButton::SetStateChange(FName InKey, const FString& InValue)
{
	StateKey = InKey;
	StateValue = InValue;
}

bool SSlaterButton::Activate()
{
	if (!HasAction() || !IsEnabled())
	{
		return false;
	}
	return Slater::ActivateAction(AsShared(), StateKey, StateValue, Command, CommandArgument);
}

FReply SSlaterButton::HandleClicked()
{
	Activate();
	return FReply::Handled();
}

void SSlaterButton::UpdateLook()
{
	const ESlaterButtonLook Wanted =
		!IsEnabled() ? ESlaterButtonLook::Disabled :
		IsPressed() ? ESlaterButtonLook::Pressed :
		bFocused ? ESlaterButtonLook::Focused :
		bHovered ? ESlaterButtonLook::Hover : ESlaterButtonLook::Rest;
	if (Wanted == Look)
	{
		return;
	}
	Look = Wanted;
	switch (Look)
	{
	case ESlaterButtonLook::Hover: Face->SetSurface(Looks.Hover); break;
	case ESlaterButtonLook::Pressed: Face->SetSurface(Looks.Pressed); break;
	case ESlaterButtonLook::Focused: Face->SetSurface(Looks.Focused); break;
	case ESlaterButtonLook::Disabled: Face->SetSurface(Looks.Disabled); break;
	default: Face->SetSurface(Looks.Rest); break;
	}
}

void SSlaterButton::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SButton::OnMouseEnter(MyGeometry, MouseEvent);
	bHovered = true;
	UpdateLook();
}

void SSlaterButton::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SButton::OnMouseLeave(MouseEvent);
	bHovered = false;
	UpdateLook();
}

FReply SSlaterButton::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	const FReply Reply = SButton::OnFocusReceived(MyGeometry, InFocusEvent);
	// A click focuses the button too, and should not leave a focus ring behind; navigation and the keyboard should.
	bFocused = InFocusEvent.GetCause() != EFocusCause::Mouse;
	UpdateLook();
	return Reply;
}

void SSlaterButton::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SButton::OnFocusLost(InFocusEvent);
	bFocused = false;
	UpdateLook();
}

void SSlaterButton::Press()
{
	SButton::Press();
	UpdateLook();
}

void SSlaterButton::Release()
{
	SButton::Release();
	UpdateLook();
}

void SSlaterButton::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SButton::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// `enabled` may be bound; nothing tells the button when it changes.
	const bool bShowsDisabled = Look == ESlaterButtonLook::Disabled;
	if (bShowsDisabled == IsEnabled())
	{
		UpdateLook();
	}
}
