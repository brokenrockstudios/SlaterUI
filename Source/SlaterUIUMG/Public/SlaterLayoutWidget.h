// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "SlaterLayoutWidget.generated.h"

class SSlaterView;
class UObject;

namespace Slater
{
	/**
	 * Binds a view's `{Path}` data to a UObject view model (`MakeObjectScope`); null removes the context. Both layout
	 * widgets use it. Returns false when the layout then failed to build (its errors are on screen).
	 */
	SLATERUIUMG_API bool SetViewModelContext(SSlaterView& View, UObject* ViewModel);
}

/**
 * A layout from `Content/UI/Layouts` as a widget (T-352): sits in the UMG palette under "Slater", so a designer can put
 * a `.sltr` screen into a widget Blueprint. `LayoutId` is a dropdown of the layouts found.
 *
 * Data: `ViewModelName` takes the context from the enclosing widget's MVVM view (`UMVVMView::GetViewModel`); `SetViewModel`
 * does it from code. In the designer the layout shows its `.mock.json` data.
 */
UCLASS(meta = (DisplayName = "Slater Layout"))
class SLATERUIUMG_API USlaterLayoutWidget : public UWidget
{
	GENERATED_BODY()

public:
	/** The layout ID (`Panel`, `Hud/Health`). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slater", meta = (GetOptions = "GetLayoutOptions"))
	FString LayoutId;

	/** The style (`Base`, `Dusk`, or one from `Content/UI/Styles`). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slater")
	FName StyleName = TEXT("Base");

	/** The name of a view model in the enclosing widget's MVVM view, whose fields `{Path}` reads. Empty means no data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slater")
	FName ViewModelName;

	/** Shows another layout. Rebuilds the view when it exists. */
	void SetLayoutId(const FString& InLayoutId);

	/** Data for `{Path}` from code. Kept weakly. Null clears it. */
	void SetViewModel(UObject* InViewModel);

	/** Looks the named view model up again (when the MVVM view made it after this widget was built). */
	void RefreshViewModel();

	/** The Slate view, or null before the widget was built. */
	TSharedPtr<SSlaterView> GetView() const { return View.Pin(); }

	//~ UWidget interface
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	/** For the dropdown: every layout ID found. */
	UFUNCTION()
	TArray<FString> GetLayoutOptions() const;

	TWeakPtr<SSlaterView> View;
	TWeakObjectPtr<UObject> ViewModel;
};

/**
 * A `UUserWidget` that is only a layout. The game feature action creates these (UI extension points make UserWidgets);
 * `Configure` says which layout. It has no widget tree.
 */
UCLASS()
class SLATERUIUMG_API USlaterLayoutUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(const FString& InLayoutId, FName InStyleName);
	void SetViewModel(UObject* InViewModel);

	const FString& GetLayoutId() const { return LayoutId; }
	TSharedPtr<SSlaterView> GetView() const { return View.Pin(); }

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FString LayoutId;
	FName StyleName = TEXT("Base");
	TWeakPtr<SSlaterView> View;
	TWeakObjectPtr<UObject> ViewModel;
};
