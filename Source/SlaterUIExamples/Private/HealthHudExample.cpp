// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "Containers/Ticker.h"
#include "SlaterCommands.h"
#include "SlaterExamples.h"
#include "SlaterHealthHud.h"
#include "SlaterObjectScope.h"
#include "SlaterView.h"
#include "UObject/StrongObjectPtr.h"

namespace HealthHudExamplePrivate
{
	/** Keeps the view model alive for as long as the view's commands exist, and the demo timer with it. */
	struct FHealthHudKeepAlive
	{
		TStrongObjectPtr<USlaterHealthHudViewModel> ViewModel;
		FTSTicker::FDelegateHandle Timer;

		~FHealthHudKeepAlive()
		{
			FTSTicker::GetCoreTicker().RemoveTicker(Timer);
		}
	};
}
using namespace HealthHudExamplePrivate;

/**
 * HealthHud, binding a UObject. The layout (`Examples/HealthHud.sltr`) reads fields of `USlaterHealthHudViewModel`
 * (SlaterHealthHud.h shows how a field notifies) and names two commands. Everything C++ does here:
 *
 *  1. Make the view model. The view's scope holds it weakly, so something must own it: the handlers below do (`KeepAlive`).
 *  2. `Slater::MakeObjectScope` turns it into a scope; the layout's paths are checked against its fields at `SetContext`.
 *  3. Commands change the view model through its own functions. They never touch a widget: the setters broadcast and the
 *     bound meter and texts read again. A command's argument is the word after the colon (`HealthHud.Hurt:10`).
 *  4. A timer (here a core ticker) drains and heals the view model, to show the widgets following it.
 */
TSharedRef<SSlaterView> SlaterExamples::MakeHealthHud(FName StyleName, USlaterHealthHudViewModel* ViewModel)
{
	const TSharedRef<FHealthHudKeepAlive> KeepAlive = MakeShared<FHealthHudKeepAlive>();
	const bool bOwnsViewModel = ViewModel == nullptr;
	if (bOwnsViewModel)
	{
		ViewModel = NewObject<USlaterHealthHudViewModel>(GetTransientPackage());
		ViewModel->SetHealth(60);
	}
	KeepAlive->ViewModel.Reset(ViewModel);

	const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Examples/HealthHud")).StyleName(StyleName);

	View->GetCommands().Register(TEXT("HealthHud.Hurt"), [KeepAlive](const FSlaterCommandCall& Call) { KeepAlive->ViewModel->Damage(FCString::Atoi(*Call.Argument)); });
	View->GetCommands().Register(TEXT("HealthHud.Heal"), [KeepAlive](const FSlaterCommandCall& Call) { KeepAlive->ViewModel->Heal(FCString::Atoi(*Call.Argument)); });

	if (bOwnsViewModel)
	{
		// The ticker holds the keep-alive weakly, so it stops with the view; the keep-alive removes it in its destructor too.
		const TWeakPtr<FHealthHudKeepAlive> Weak = KeepAlive;
		KeepAlive->Timer = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
		{
			const TSharedPtr<FHealthHudKeepAlive> Alive = Weak.Pin();
			if (Alive)
			{
				Alive->ViewModel->Advance();
			}
			return Alive.IsValid();
		}), 0.25f);
	}

	View->SetContext(Slater::MakeObjectScope(ViewModel));
	return View;
}
