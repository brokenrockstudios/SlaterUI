// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterElements.h"
#include "SlaterScope.h"

/** How a binding reaches its widget. */
enum class ESlaterBindMode : uint8
{
	/** The scope says what changed, and exactly that attribute of that widget is set. Idle cost is zero. */
	Pushed,
	/** The widget gets a getter that Slate evaluates every frame (`TSlateAttribute`), the way a bound lambda does. Kept to compare cost. */
	Polled,
};

/** Counters the tests read to prove what a change touched. */
struct SLATERUI_API FSlaterListStats
{
	/** Template instances built by `repeat` (one per item). */
	int32 ItemsBuilt = 0;
	/** Instances dropped because their key left the list. */
	int32 ItemsDestroyed = 0;
	/** Pushed values written to a widget. */
	int32 Applies = 0;
	/** Polled getter evaluations. */
	int32 PolledReads = 0;

	void Reset() { *this = FSlaterListStats(); }
};

namespace Slater
{
	SLATERUI_API FSlaterListStats& ListStats();

	/**
	 * Every pushed binding of every live binding set that shows another value than its data holds, one line each
	 * (`Label(Line,Column): {Path} shows 'a', the data says 'b'`). A view model that changes a field and forgets to
	 * broadcast leaves the UI stale and silent; this is how it is found. The console command `Slater.StaleBindings` prints it.
	 */
	SLATERUI_API void CollectStaleBindings(TArray<FString>& OutLines);
}

/**
 * The `{Path}` bindings of one scope: one view's own, or one list item's. Applies each value once when added, then
 * (pushed) subscribes to the path and writes again on every change, or (polled) hands the widget a getter. The
 * subscriptions belong to the set and the widgets are held weakly, so dropping the set removes every delegate. It does
 * not move once bindings were added (the subscriptions point back at it).
 */
class SLATERUI_API FSlaterBindingSet
{
public:
	FSlaterBindingSet(TSharedRef<ISlaterScope> InScope, ESlaterBindMode InMode, const ISlaterStyleSource& InStyle, int32 ExpectedBindings = 0);
	~FSlaterBindingSet();
	FSlaterBindingSet(const FSlaterBindingSet&) = delete;
	FSlaterBindingSet& operator=(const FSlaterBindingSet&) = delete;

	/** Applies the current value and starts following it. A value the scope cannot read yet leaves the widget as built. */
	void Add(const FSlaterBoundAttr& Bound);

	/** Text for the stale report: the layout the bindings came from. */
	void SetLabel(FName InLabel) { Label = InLabel; }

	int32 Num() const { return Entries.Num(); }
	const TSharedRef<ISlaterScope>& GetScope() const { return Scope; }

	/** Appends one line per pushed binding that shows another value than the scope holds now. */
	void CollectStale(TArray<FString>& OutLines) const;
	/** `CollectStale` over every live set (`Slater::CollectStaleBindings`). */
	static void CollectAllStale(TArray<FString>& OutLines);

private:
	struct FEntry
	{
		FSlaterBoundAttr Bound;
		/** The last text read from the scope for this binding. */
		FString Applied;
		bool bApplied = false;
		TUniquePtr<ISlaterSubscription> Subscription;
	};

	TSharedRef<ISlaterScope> Scope;
	ESlaterBindMode Mode;
	const ISlaterStyleSource* Style;
	FName Label;
	TArray<FEntry> Entries;
	/** Every live set is on one list (no allocation per set), for the stale-binding report. */
	FSlaterBindingSet* PrevLive = nullptr;
	FSlaterBindingSet* NextLive = nullptr;

	void Refresh(int32 Index);
	void Write(FEntry& Entry, const FString& Text);
};
