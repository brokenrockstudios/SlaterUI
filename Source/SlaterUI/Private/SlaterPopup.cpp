// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterPopup.h"

#include "SlaterElements.h"
#include "SlaterEvents.h"
#include "SlaterState.h"
#include "SlaterView.h"

#include "Input/Events.h"
#include "Widgets/SNullWidget.h"

namespace
{
	const FName AttrPlacement(TEXT("placement"));
	const FName AttrOpenWhen(TEXT("open-when"));
	const FName AttrCloseState(TEXT("close-state"));

	EMenuPlacement PlacementFromWord(FName Word)
	{
		if (Word == TEXT("above")) return MenuPlacement_AboveAnchor;
		if (Word == TEXT("right")) return MenuPlacement_MenuRight;
		return MenuPlacement_BelowAnchor;
	}

	FSlaterAttrDesc PopupStateAttr(const TCHAR* Name)
	{
		FSlaterAttrDesc Desc;
		Desc.Name = Name;
		Desc.Type = ESlaterAttrType::Text;
		Desc.CheckText = [Name](const FString& Text) { return Slater::CheckStatePair(Text, Name); };
		return Desc;
	}

	TSharedPtr<SWidget> BuildPopup(FSlaterBuildContext& Context)
	{
		const int32 Count = Context.Children.Num();
		if (Count == 0)
		{
			Context.Failure = TEXT("<popup> needs an anchor: its first child is the element the menu opens from, its second child is the menu");
			return nullptr;
		}
		if (Count == 1)
		{
			Context.Failure = TEXT("<popup> needs a menu: its second child is the menu that opens from the anchor (the first child)");
			return nullptr;
		}
		if (Count > 2)
		{
			Context.Failure = FString::Printf(TEXT("<popup> takes exactly two children, the anchor and then the menu (found %d); put the menu rows in one vbox"), Count);
			return nullptr;
		}

		FString OpenKey, OpenValue, CloseKey, CloseValue;
		const bool bHasOpen = Context.Has(AttrOpenWhen) && Slater::ParseStatePair(Context.GetText(AttrOpenWhen), OpenKey, OpenValue);
		bool bHasClose = Context.Has(AttrCloseState) && Slater::ParseStatePair(Context.GetText(AttrCloseState), CloseKey, CloseValue);
		if (bHasClose && !bHasOpen)
		{
			Context.Failure = TEXT("<popup> has close-state but no open-when: close-state says what the engine's close (outside click, Escape) writes back, so name the state with open-when");
			return nullptr;
		}
		if (bHasClose && !CloseKey.Equals(OpenKey, ESearchCase::CaseSensitive))
		{
			Context.Failure = FString::Printf(TEXT("<popup> open-when and close-state name different states ('%s' and '%s'); the menu follows one state"), *OpenKey, *CloseKey);
			return nullptr;
		}
		if (bHasOpen && !bHasClose && OpenValue == TEXT("true"))
		{
			// The common case, a boolean state: closing it writes false.
			CloseKey = OpenKey;
			CloseValue = TEXT("false");
			bHasClose = true;
		}

		const TSharedRef<SSlaterPopup> Popup = SNew(SSlaterPopup)
			.Placement(PlacementFromWord(Context.GetEnum(AttrPlacement, TEXT("below"))))
			.Menu(Context.Children[1].Widget)
			[
				Context.Children[0].Widget
			];
		if (bHasOpen)
		{
			Popup->SetOpenState(FName(*OpenKey), OpenValue);
		}
		if (bHasClose)
		{
			Popup->SetCloseState(FName(*CloseKey), CloseValue);
		}
		return Popup;
	}
}

void SSlaterPopupMenu::Construct(const FArguments& InArgs, const TSharedRef<SSlaterPopup>& InPopup)
{
	Popup = InPopup;
	ChildSlot[InArgs._Content.Widget];
}

void SSlaterPopup::Construct(const FArguments& InArgs)
{
	Placement = InArgs._Placement;
	MenuRoot = SNew(SSlaterPopupMenu, SharedThis(this))
		[
			InArgs._Menu.IsValid() ? InArgs._Menu.ToSharedRef() : SNullWidget::NullWidget
		];
	// FitInWindow flips the menu to the other side at the edge of the window (T-416: works in a shipped game, not for a view smaller than its window).
	SMenuAnchor::Construct(SMenuAnchor::FArguments()
		.Placement(InArgs._Placement)
		.FitInWindow(true)
		.OnMenuOpenChanged(FOnIsOpenChanged::CreateSP(this, &SSlaterPopup::HandleMenuOpenChanged))
		.MenuContent(MenuRoot)
		[
			InArgs._Content.Widget
		]);
}

void SSlaterPopup::SetOpenState(FName InKey, const FString& InValue)
{
	OpenKey = InKey;
	OpenValue = InValue;
}

void SSlaterPopup::SetCloseState(FName InKey, const FString& InValue)
{
	CloseKey = InKey;
	CloseValue = InValue;
}

void SSlaterPopup::WriteState(FName Key, const FString& Value)
{
	if (Key.IsNone())
	{
		return;
	}
	if (const TSharedPtr<SSlaterView> View = Slater::FindOwningView(*this))
	{
		View->SetState(Key, Value);
	}
}

void SSlaterPopup::HandleMenuOpenChanged(bool bOpen)
{
	// The engine calls this with true before it knows whether a window will show the menu; only a close matters here.
	if (bOpen || !bOpenRequested)
	{
		return;
	}
	bOpenRequested = false;
	WriteState(CloseKey, CloseValue);
	CloseNestedPopups();
}

void SSlaterPopup::CloseFromParent()
{
	if (bOpenRequested || IsOpen())
	{
		bOpenRequested = false;
		SetIsOpen(false);
		WriteState(CloseKey, CloseValue);
	}
	CloseNestedPopups();
}

void SSlaterPopup::CloseNestedPopups()
{
	static const FName PopupType(TEXT("SSlaterPopup"));
	TFunction<void(SWidget&)> Walk = [&Walk](SWidget& Widget)
	{
		if (Widget.GetType() == PopupType)
		{
			static_cast<SSlaterPopup&>(Widget).CloseFromParent();
			return;
		}
		if (FChildren* Children = Widget.GetChildren())
		{
			Children->ForEachWidget([&Walk](SWidget& Child) { Walk(Child); });
		}
	};
	if (MenuRoot.IsValid())
	{
		MenuRoot->GetChildren()->ForEachWidget([&Walk](SWidget& Child) { Walk(Child); });
	}
}

void SSlaterPopup::SetOpenRequested(bool bOpen)
{
	if (bOpen)
	{
		if (bOpenRequested)
		{
			return;
		}
		if (!ShouldOpenDueToClick())
		{
			// A press just dismissed the menu: the click that set the state must not reopen it (T-416). Put the state back.
			WriteState(CloseKey, CloseValue);
			return;
		}
		bOpenRequested = true;
		SetIsOpen(true);
		return;
	}
	if (!bOpenRequested && !IsOpen())
	{
		return;
	}
	// The state already says closed; the engine's close callback must not write it a second time.
	bOpenRequested = false;
	SetIsOpen(false);
	CloseNestedPopups();
}

bool SSlaterPopup::ToggleFromClick()
{
	const bool bBlocked = bBlockOpenOnRelease;
	bBlockOpenOnRelease = false;
	if (IsOpen() || bOpenRequested)
	{
		bOpenRequested = false;
		SetIsOpen(false);
		WriteState(CloseKey, CloseValue);
		CloseNestedPopups();
		return false;
	}
	if (bBlocked || !ShouldOpenDueToClick())
	{
		return false;
	}
	bOpenRequested = true;
	SetIsOpen(true);
	WriteState(OpenKey, OpenValue);
	return true;
}

FReply SSlaterPopup::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Seen before the press reaches the anchor's content: the menu is open, or the engine just dismissed it for this very press.
		bBlockOpenOnRelease = !ShouldOpenDueToClick() || bOpenRequested;
	}
	return FReply::Unhandled();
}

FReply SSlaterPopup::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ToggleFromClick();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void Slater::RegisterPopupElements(FSlaterElementRegistry& Registry)
{
	FSlaterElementDesc Desc;
	Desc.Tag = TEXT("popup");
	Desc.Purpose = TEXT("a menu that opens from an anchor (first child) over the page; the second child is the menu");
	FSlaterAttrDesc Placement;
	Placement.Name = AttrPlacement;
	Placement.Type = ESlaterAttrType::Enum;
	Placement.EnumValues = {FName(TEXT("above")), FName(TEXT("below")), FName(TEXT("right"))};
	Desc.Attributes = {Placement, PopupStateAttr(TEXT("open-when")), PopupStateAttr(TEXT("close-state"))};
	Desc.Children = ESlaterChildRule::Many;
	Desc.Builder = &BuildPopup;
	FString Error;
	const bool bOk = Registry.Register(MoveTemp(Desc), &Error);
	ensureMsgf(bOk, TEXT("SlaterUI stock element <popup>: %s"), *Error);
}
