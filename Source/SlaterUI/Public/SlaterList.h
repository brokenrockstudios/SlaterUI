// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterBindings.h"
#include "SlaterElements.h"
#include "SlaterModel.h"
#include "SlaterScope.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SUniformGridPanel;
struct FSlateBrush;

/**
 * `<repeat>`, `<image>` and the binder that connects a built layout to a scope (SlaterScope.h): the mock JSON model, or
 * a UObject view model. The T-351 list probe measured what a list of templated cells and items costs against
 * hand-written Slate; the numbers are in Docs/SlaterUI/DevNotes.md.
 */

/** Maps `src` names to brushes for `<image>`. The game (or a test) supplies it; layouts never name asset paths. */
class SLATERUI_API ISlaterImageSource
{
public:
	virtual ~ISlaterImageSource() = default;
	/** A brush that stays valid while the source is installed, or null. */
	virtual const FSlateBrush* FindImage(FName Name) const = 0;
};

namespace Slater
{
	/** Installs the process-wide image source (null removes it). Game thread only. */
	SLATERUI_API void SetImageSource(TSharedPtr<const ISlaterImageSource> Source);
	SLATERUI_API const FSlateBrush* FindImage(FName Name);
}

/** What `<repeat>` was written with, and what it needs to build more items later. */
struct SLATERUI_API FSlaterRepeatConfig
{
	/** The child element, checked and resolved once; each item instantiates it. */
	TSharedPtr<const FSlaterCompiledTemplate> Template;
	/** The array's path in the model (`Cells`). */
	FString SourcePath;
	/** The field of each item that names it. */
	FString KeyField;
	/** Where `source` was written, for an error about the path. */
	FSlaterPos SourcePos;
	/** Optional: the field holding the item's grid position (a cell index). Without it the item's place in the array is its position. */
	FString AtField;
	int32 Columns = 1;
	float CellWidth = 0.f;
	float CellHeight = 0.f;
	/** Both must outlive the widget. */
	const ISlaterStyleSource* Style = nullptr;
	const FSlaterElementRegistry* Registry = nullptr;
};

/**
 * Lays an array of the model out in a uniform grid, one instance of its template per item. Instances are keyed: a
 * changed field updates the bindings of that item only, an added key builds one instance, a removed key destroys one,
 * and the others (and their widget state) are never rebuilt. Items are laid out by array order, or by `AtField`.
 */
class SLATERUI_API SSlaterRepeat : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSlaterRepeat) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FSlaterRepeatConfig InConfig);
	virtual ~SSlaterRepeat() override;

	/** Builds an instance for every item of the list the scope has at `source` and follows its changes. Calling it again starts over. */
	void Attach(TSharedRef<ISlaterScope> InScope, ESlaterBindMode InMode);
	/** `Attach` over the mock JSON model. */
	void Attach(TSharedRef<FSlaterModel> InModel, ESlaterBindMode InMode);

	int32 NumItems() const { return Items.Num(); }
	TSharedPtr<SWidget> FindItemWidget(const FString& Key) const;
	const FSlaterRepeatConfig& GetConfig() const { return Config; }

private:
	struct FItem
	{
		TSharedPtr<ISlaterScope> Scope;
		FSlaterBuildResult Built;
		/** The item's own `{Path}` bindings, subscribed to its scope. */
		TUniquePtr<FSlaterBindingSet> Bindings;
		/** Follows the `at` field, when the repeat has one. */
		TUniquePtr<ISlaterSubscription> AtSubscription;
	};

	FSlaterRepeatConfig Config;
	TSharedPtr<ISlaterListSource> Source;
	/** The layout's label, for the stale-binding report of each item. */
	FName TemplateLabel;
	ESlaterBindMode Mode = ESlaterBindMode::Pushed;
	TSharedPtr<SUniformGridPanel> Grid;
	TMap<FString, FItem> Items;
	/** Keys in array order, for the ordinal layout. */
	TArray<FString> Order;

	void AddItem(const TSharedPtr<ISlaterScope>& ItemScope, const FString& Key);
	void RemoveItemByKey(const FString& Key);
	void Place(const FString& Key, int32 Ordinal);
	void Connect(const FString& Key, FItem& Item);
	void HandleListChanged(const FString& Key, bool bAdded);
};

/** Holds the connection between a built layout and its data. Drop it and the subscriptions go. The style must outlive it. */
class SLATERUI_API FSlaterBinder : public TSharedFromThis<FSlaterBinder>
{
public:
	FSlaterBinder(TSharedRef<ISlaterScope> InScope, ESlaterBindMode InMode, const ISlaterStyleSource& InStyle);
	/** Over the mock JSON model. */
	FSlaterBinder(TSharedRef<FSlaterModel> InModel, ESlaterBindMode InMode, const ISlaterStyleSource& InStyle);
	~FSlaterBinder();

	/** Applies the layout's own bindings once, subscribes to the scope, and attaches the layout's `repeat` elements. Label names the layout in the stale-binding report. */
	void Attach(const FSlaterBuildResult& Result, const FString& Label = FString());

	const TSharedRef<ISlaterScope>& GetScope() const { return Scope; }
	/** The layout's own bindings (not those of repeated items). Null before `Attach`. */
	const FSlaterBindingSet* GetBindings() const { return Bindings.Get(); }

private:
	TSharedRef<ISlaterScope> Scope;
	ESlaterBindMode Mode;
	const ISlaterStyleSource* Style;
	TUniquePtr<FSlaterBindingSet> Bindings;
};

namespace Slater
{
	/** Connects a built layout to a model. The caller keeps the result alive for as long as the layout is shown. */
	SLATERUI_API TSharedRef<FSlaterBinder> Bind(const FSlaterBuildResult& Result, TSharedRef<FSlaterModel> Model, ESlaterBindMode Mode, const ISlaterStyleSource& Style);
	/** The same over any scope (a view model, SlaterObjectScope.h). */
	SLATERUI_API TSharedRef<FSlaterBinder> Bind(const FSlaterBuildResult& Result, TSharedRef<ISlaterScope> Scope, ESlaterBindMode Mode, const ISlaterStyleSource& Style);

	/**
	 * Checks every `{Path}` of a built layout against a scope, before it is shown: the layout's own bindings, each
	 * `repeat` source, and (when the list has an item) the bindings of the repeated template against that item. One
	 * error per path that cannot resolve, at the binding's line and column, with the reason and a nearest-name hint. True when there is none.
	 */
	SLATERUI_API bool CheckBindings(const FSlaterBuildResult& Result, ISlaterScope& Scope, const FString& Label, TArray<FSlaterError>& OutErrors);

	/** Loads the mock data next to a layout (`Inventory.sltr` is paired with `Inventory.mock.json`). */
	SLATERUI_API bool LoadMockFor(const FString& LayoutFilePath, FSlaterModel& Model, FSlaterError& OutError);
}
