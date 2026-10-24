// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterLayoutWidget.h"

#include "SlaterCommands.h"
#include "SlaterObjectScope.h"
#include "SlaterScope.h"
#include "SlaterView.h"

#include "Blueprint/UserWidget.h"
#include "View/MVVMView.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SlaterUIUMG"

namespace Slater
{
	bool SetViewModelContext(SSlaterView& View, UObject* ViewModel)
	{
		return View.SetContext(ViewModel ? MakeObjectScope(ViewModel) : TSharedPtr<ISlaterScope>());
	}
}

namespace SlaterLayoutWidgetPrivate
{
	TSharedRef<SSlaterView> MakeView(const FString& LayoutId, FName StyleName, bool bMock)
	{
		return SNew(SSlaterView)
			.LayoutId(LayoutId)
			.StyleName(StyleName.IsNone() ? FName(TEXT("Base")) : StyleName)
			.UseMockContext(bMock);
	}
}
using namespace SlaterLayoutWidgetPrivate;

// USlaterLayoutWidget

TSharedRef<SWidget> USlaterLayoutWidget::RebuildWidget()
{
	const TSharedRef<SSlaterView> NewView = MakeView(LayoutId, StyleName, IsDesignTime());
	View = NewView;
	RefreshViewModel();
	return NewView;
}

void USlaterLayoutWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	// The dropdown or the style changed after the view was made (the designer edits them live).
	if (const TSharedPtr<SSlaterView> Current = View.Pin())
	{
		if (Current->GetStyleName() != (StyleName.IsNone() ? FName(TEXT("Base")) : StyleName))
		{
			Current->SetStyle(StyleName.IsNone() ? FName(TEXT("Base")) : StyleName);
		}
		if (Current->GetLayoutId() != LayoutId)
		{
			Current->SetLayout(LayoutId);
		}
	}
}

void USlaterLayoutWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	View.Reset();
}

void USlaterLayoutWidget::SetLayoutId(const FString& InLayoutId)
{
	LayoutId = InLayoutId;
	if (const TSharedPtr<SSlaterView> Current = View.Pin())
	{
		Current->SetLayout(LayoutId);
	}
}

void USlaterLayoutWidget::SetViewModel(UObject* InViewModel)
{
	ViewModel = InViewModel;
	if (const TSharedPtr<SSlaterView> Current = View.Pin())
	{
		Slater::SetViewModelContext(*Current, InViewModel);
	}
}

void USlaterLayoutWidget::RefreshViewModel()
{
	if (!ViewModelName.IsNone())
	{
		// Widget -> its widget tree -> the user widget that owns the tree.
		if (const UUserWidget* Owner = GetTypedOuter<UUserWidget>())
		{
			if (const UMVVMView* MvvmView = Owner->GetExtension<UMVVMView>())
			{
				ViewModel = MvvmView->GetViewModel(ViewModelName).GetObject();
			}
		}
	}
	if (const TSharedPtr<SSlaterView> Current = View.Pin())
	{
		if (UObject* Object = ViewModel.Get())
		{
			Slater::SetViewModelContext(*Current, Object);
		}
	}
}

TArray<FString> USlaterLayoutWidget::GetLayoutOptions() const
{
	TArray<FString> Ids;
	Slater::GetLayoutIds(Ids);
	return Ids;
}

#if WITH_EDITOR
const FText USlaterLayoutWidget::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Slater");
}
#endif

// USlaterLayoutUserWidget

TSharedRef<SWidget> USlaterLayoutUserWidget::RebuildWidget()
{
	TSharedRef<FSlaterGetterScope> Scope = MakeShared<FSlaterGetterScope>();
	BuildScope(*Scope);
	GetterScope = Scope;
	// The getters first (an explicit AddValue wins a clash), then the widget's own members (T-850).
	const TSharedRef<ISlaterScope> Chain = MakeShared<FSlaterChainScope>(TArray<TSharedRef<ISlaterScope>>{Scope, Slater::MakeObjectScope(this)});
	OwnContext = Chain;
	TSharedPtr<ISlaterScope> Context = Chain;
	if (UObject* Object = ViewModel.Get())
	{
		Context = Slater::MakeObjectScope(Object);
	}
	// Commands go in before the first build, which checks every command the layout names (T-566).
	const TSharedRef<SSlaterView> NewView = SNew(SSlaterView)
		.LayoutId(LayoutId)
		.StyleName(StyleName.IsNone() ? FName(TEXT("Base")) : StyleName)
		.Context(Context)
		.CheckCommands(true)
		.RegisterCommands([this](FSlaterCommandTable& Commands) { RegisterCommands(Commands); });
	View = NewView;
	return NewView;
}

void USlaterLayoutUserWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	View.Reset();
}

void USlaterLayoutUserWidget::Configure(const FString& InLayoutId, FName InStyleName)
{
	LayoutId = InLayoutId;
	StyleName = InStyleName;
	if (const TSharedPtr<SSlaterView> Current = View.Pin())
	{
		Current->SetStyle(StyleName.IsNone() ? FName(TEXT("Base")) : StyleName);
		Current->SetLayout(LayoutId);
	}
}

void USlaterLayoutUserWidget::SetViewModel(UObject* InViewModel)
{
	ViewModel = InViewModel;
	if (const TSharedPtr<SSlaterView> Current = View.Pin())
	{
		Slater::SetViewModelContext(*Current, InViewModel);
	}
}

TSharedPtr<FSlaterGetterScope> USlaterLayoutUserWidget::LiveScope() const
{
	const TSharedPtr<SSlaterView> Current = View.Pin();
	const TSharedPtr<FSlaterGetterScope> Scope = GetterScope.Pin();
	// A view model given later replaced the context: the getters are no longer what the view reads.
	return Current.IsValid() && Scope.IsValid() && Current->GetContext() == OwnContext.Pin() ? Scope : nullptr;
}

void USlaterLayoutUserWidget::Notify(const FString& Path)
{
	if (const TSharedPtr<FSlaterGetterScope> Scope = LiveScope())
	{
		Scope->Notify(Path);
	}
}

void USlaterLayoutUserWidget::Notify(std::initializer_list<FString> Paths)
{
	if (const TSharedPtr<FSlaterGetterScope> Scope = LiveScope())
	{
		for (const FString& Path : Paths)
		{
			Scope->Notify(Path);
		}
	}
}

void USlaterLayoutUserWidget::NotifyAll()
{
	if (const TSharedPtr<FSlaterGetterScope> Scope = LiveScope())
	{
		Scope->NotifyAll();
	}
}

#undef LOCTEXT_NAMESPACE
