// Copyright Broken Rock Studios LLC. All Rights Reserved.

// T-416 spike harness, not a feature: `Slater.PopupSpike` puts a footer button with an `SMenuAnchor` menu (placement above, a "Learn more"
// row with a second anchor placed `MenuRight`) into the game viewport, and logs every open/close and key/focus event to `LogSlaterPopupSpike`
// so a hand run in PIE (under Lyra's CommonUI) can answer the questions in DevNotes "Popup spike (T-416)". `Slater.PopupSpike 0` removes it.

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Input/Events.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterPopupSpike, Log, All);

namespace
{
	TSharedPtr<SWidget> SpikeRoot;

	/** Logs every key that reaches it, to show whether Escape and gamepad keys arrive at the menu content. */
	class SSpikeKeyLogger : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSpikeKeyLogger) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_ARGUMENT(FString, Name)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Name = InArgs._Name;
			ChildSlot[InArgs._Content.Widget];
		}

		virtual bool SupportsKeyboardFocus() const override { return false; }
		/** Rows Up and Down move between by hand (`Slater.PopupSpike manual`), to see whether SetUserFocus works where Slate navigation did not. */
		TArray<TWeakPtr<SWidget>> Rows;
		bool bManualNav = false;

		virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& Event) override
		{
			const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
			UE_LOG(LogSlaterPopupSpike, Log, TEXT("%s: key down %s (focused: %s)"), *Name, *Event.GetKey().ToString(), Focused ? *Focused->GetTypeAsString() : TEXT("none"));
			if (bManualNav && (Event.GetKey() == EKeys::Up || Event.GetKey() == EKeys::Down))
			{
				int32 Index = Rows.IndexOfByPredicate([&](const TWeakPtr<SWidget>& Row) { return Row.Pin() == Focused; });
				const int32 Step = Event.GetKey() == EKeys::Down ? 1 : -1;
				Index = Rows.Num() > 0 ? (Index + Step + Rows.Num()) % Rows.Num() : INDEX_NONE;
				if (Index != INDEX_NONE)
				{
					FSlateApplication::Get().SetUserFocus(0, Rows[Index].Pin(), EFocusCause::Navigation);
					return FReply::Handled();
				}
			}
			return FReply::Unhandled();
		}
		virtual FReply OnFocusReceived(const FGeometry&, const FFocusEvent& Event) override { UE_LOG(LogSlaterPopupSpike, Log, TEXT("%s: focus received (cause %d)"), *Name, (int32)Event.GetCause()); return FReply::Unhandled(); }
		virtual void OnFocusLost(const FFocusEvent& Event) override { UE_LOG(LogSlaterPopupSpike, Log, TEXT("%s: focus lost"), *Name); }

	private:
		FString Name;
	};

	TSharedRef<SWidget> MakeRow(const FString& Label, TSharedPtr<SWidget>* OutButton = nullptr)
	{
		const TSharedRef<SButton> Button = SNew(SButton)
			.OnClicked_Lambda([Label]()
			{
				UE_LOG(LogSlaterPopupSpike, Log, TEXT("row clicked: %s"), *Label);
				return FReply::Handled();
			})
			[SNew(STextBlock).Text(FText::FromString(Label))];
		if (OutButton)
		{
			*OutButton = Button;
		}
		return Button;
	}

	/** The click rule SComboButton uses: a press outside the open menu dismisses it, so the release that follows must not reopen it. */
	void ToggleFromClick(SMenuAnchor& Anchor)
	{
		if (Anchor.IsOpen())
		{
			Anchor.SetIsOpen(false);
		}
		else if (Anchor.ShouldOpenDueToClick())
		{
			Anchor.SetIsOpen(true);
		}
	}

	TSharedRef<SWidget> BuildSpike(EHorizontalAlignment HAlign, EVerticalAlignment VAlign, EMenuPlacement Placement, bool bManualNav, bool bFit)
	{
		const TSharedRef<TSharedPtr<SWidget>> FirstRow = MakeShared<TSharedPtr<SWidget>>();
		// Heap holders so the click lambdas can reach the anchors that are assigned after them.
		const TSharedRef<TSharedPtr<SMenuAnchor>> MenuHolder = MakeShared<TSharedPtr<SMenuAnchor>>();
		const TSharedRef<TSharedPtr<SMenuAnchor>> SubHolder = MakeShared<TSharedPtr<SMenuAnchor>>();
		TSharedPtr<SMenuAnchor> Menu;
		TSharedPtr<SMenuAnchor> Sub;

		// Submenu content, opened to the right of the "Learn more" row.
		const TSharedRef<SWidget> SubContent = SNew(SSpikeKeyLogger).Name(TEXT("submenu"))
			[SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[MakeRow(TEXT("About"))]
				+ SVerticalBox::Slot().AutoHeight()[MakeRow(TEXT("Docs"))]];

		// "Learn more" is the anchor of the submenu: the row itself is the anchored content.
		SAssignNew(Sub, SMenuAnchor)
			.Placement(MenuPlacement_MenuRight)
			.OnMenuOpenChanged_Lambda([](bool bOpen) { UE_LOG(LogSlaterPopupSpike, Log, TEXT("submenu open=%d"), bOpen); })
			.MenuContent(SubContent)
			[
				SNew(SButton)
				.OnClicked_Lambda([SubHolder]() { ToggleFromClick(**SubHolder); return FReply::Handled(); })
				[SNew(STextBlock).Text(FText::FromString(TEXT("Learn more  >")))]
			];
		*SubHolder = Sub;

		TSharedPtr<SWidget> Row2, Row4;
		const TSharedRef<SWidget> SettingsRow = MakeRow(TEXT("Settings"), &*FirstRow);
		const TSharedRef<SWidget> LanguageRow = MakeRow(TEXT("Language"), &Row2);
		const TSharedRef<SWidget> LogoutRow = MakeRow(TEXT("Log out"), &Row4);
		const TSharedRef<SSpikeKeyLogger> MenuContent = SNew(SSpikeKeyLogger).Name(TEXT("menu"))
			[SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SettingsRow]
				+ SVerticalBox::Slot().AutoHeight()[LanguageRow]
				+ SVerticalBox::Slot().AutoHeight()[Sub.ToSharedRef()]
				+ SVerticalBox::Slot().AutoHeight()[LogoutRow]];
		MenuContent->bManualNav = bManualNav;
		MenuContent->Rows = { *FirstRow, Row2, Row4 };

		// The footer button, in the corner the command asked for, with the menu above or below it.
		SAssignNew(Menu, SMenuAnchor)
			.Placement(Placement)
			.FitInWindow(bFit)
			// Keyboard focus on the first row, one frame after the anchor focused the content, so Up and Down have a row to move from.
			.OnMenuOpenChanged_Lambda([FirstRow, MenuHolder](bool bOpen)
			{
				UE_LOG(LogSlaterPopupSpike, Log, TEXT("menu open=%d"), bOpen);
				if (bOpen && FirstRow->IsValid() && *MenuHolder)
				{
					(*MenuHolder)->RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([FirstRow](double, float)
					{
						FSlateApplication::Get().SetUserFocus(0, *FirstRow, EFocusCause::SetDirectly);
						return EActiveTimerReturnType::Stop;
					}));
				}
			})
			.MenuContent(MenuContent)
			[
				SNew(SButton)
				.OnClicked_Lambda([MenuHolder]() { ToggleFromClick(**MenuHolder); return FReply::Handled(); })
				[SNew(STextBlock).Text(FText::FromString(TEXT("Account  ^")))]
			];

		// The anchors own their buttons, which own the holders (a cycle on purpose for a throwaway spike: `Slater.PopupSpike 0` leaks it).
		*MenuHolder = Menu;
		return SNew(SBox).HAlign(HAlign).VAlign(VAlign).Padding(FMargin(40.f))[Menu.ToSharedRef()];
	}

	void OnSpikeCommand(const TArray<FString>& Args)
	{
		UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
		if (!Viewport)
		{
			UE_LOG(LogSlaterPopupSpike, Warning, TEXT("No game viewport: run this in PIE."));
			return;
		}
		if (SpikeRoot.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(SpikeRoot.ToSharedRef());
			SpikeRoot.Reset();
		}
		if (Args.Num() > 0 && Args[0] == TEXT("0"))
		{
			return;
		}
		// Args: a corner (bl default, br, tl, tr) then any of `below` (menu under the button), `fit` (`FitInWindow(true)`, to see whether the anchor flips itself) and `manual` (Up/Down move focus by hand): `Slater.PopupSpike tr fit manual`.
		const bool bRight = Args.Contains(TEXT("br")) || Args.Contains(TEXT("tr"));
		const bool bTop = Args.Contains(TEXT("tl")) || Args.Contains(TEXT("tr"));
		const EMenuPlacement Placement = Args.Contains(TEXT("below")) ? MenuPlacement_BelowAnchor : MenuPlacement_AboveAnchor;
		SpikeRoot = BuildSpike(bRight ? HAlign_Right : HAlign_Left, bTop ? VAlign_Top : VAlign_Bottom, Placement, Args.Contains(TEXT("manual")), Args.Contains(TEXT("fit")));
		Viewport->AddViewportWidgetContent(SpikeRoot.ToSharedRef(), 1000);
		UE_LOG(LogSlaterPopupSpike, Log, TEXT("Spike added. Click Account; see DevNotes 'Popup spike (T-416)' for what to check."));
	}

	FAutoConsoleCommand PopupSpikeCommand(
		TEXT("Slater.PopupSpike"),
		TEXT("Slater.PopupSpike [0 | bl|br|tl|tr] [below] [fit] [manual]: T-416 spike. Shows an SMenuAnchor menu with a submenu in the game viewport (0 removes it)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&OnSpikeCommand));
}
