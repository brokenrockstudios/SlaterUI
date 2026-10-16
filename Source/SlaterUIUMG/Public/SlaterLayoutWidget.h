// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "SlaterLayoutWidget.generated.h"

class FSlaterCommandTable;
class FSlaterGetterScope;
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
 *
 * Subclass it to own a layout in code (T-559, the "text version of UMG": one class tied 1:1 to its `.sltr`): set `LayoutId`
 * and `StyleName` in the constructor, fill the scope in `BuildScope` and register the commands in `RegisterCommands`.
 * `CreateWidget` and `AddToViewport` then work like for any other widget, with no outside `Configure`.
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

	/**
	 * Called each time the view is built, before the layout is: add the values, lists and setters `{Path}` reads. The scope
	 * is used as the view's context unless `SetViewModel` gave one (a view model wins). Empty by default.
	 */
	virtual void BuildScope(FSlaterGetterScope& Scope) {}

	/** Called each time the view is built: register the commands `on-click` names in this view's own table. Empty by default. */
	virtual void RegisterCommands(FSlaterCommandTable& Commands) {}

	/**
	 * Tells the bound text that the values at these paths changed (the getters `BuildScope` registered). Does nothing before
	 * the view is built, and after `SetViewModel` (the view model announces its own changes).
	 */
	void Notify(const FString& Path);
	void Notify(std::initializer_list<FString> Paths);

	/** Every binding of the view reads again. The same no-ops as `Notify`. */
	void NotifyAll();

	/** The layout ID. A subclass sets it in its constructor; an empty one shows the normal build error. */
	FString LayoutId;
	/** The style (`Base`, `Dusk`). */
	FName StyleName = TEXT("Base");

private:
	/** The scope `BuildScope` filled, while it is still the view's context. */
	TSharedPtr<FSlaterGetterScope> LiveScope() const;

	TWeakPtr<SSlaterView> View;
	TWeakObjectPtr<UObject> ViewModel;
	TWeakPtr<FSlaterGetterScope> GetterScope;
};
