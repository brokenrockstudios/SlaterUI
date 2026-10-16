// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

class FSlaterCallbackTable;
class FSlaterModel;
class ISlaterListSource;

/** A live subscription to a value. Dropping it unsubscribes, so whatever owns the bindings owns the delegates. */
class SLATERUI_API ISlaterSubscription
{
public:
	virtual ~ISlaterSubscription() = default;
};

/**
 * The data a layout binds to: one record (the view's context, or one item of a list). A `{Path}` is read from it as
 * text, and a subscription tells the binding when the value at a path may have changed. Implementations: a mock JSON
 * model (`Slater::MakeModelScope`) and a UObject view model (`Slater::MakeObjectScope`, SlaterObjectScope.h). Game thread only.
 */
class SLATERUI_API ISlaterScope : public TSharedFromThis<ISlaterScope>
{
public:
	virtual ~ISlaterScope() = default;

	/** The value at a dotted path as text (bool as true/false, whole numbers without a decimal point). False when the path does not resolve now. */
	virtual bool Read(const FString& Path, FString& OutText) const = 0;

	/**
	 * Whether a binding to Path can ever resolve against this scope (a list when bList, a value otherwise). False says why
	 * in OutWhy, with a nearest-name suggestion where there is one. A path through a link that is empty right now is accepted.
	 */
	virtual bool Check(const FString& Path, bool bList, FString& OutWhy) const = 0;

	/** The list at Path, its items keyed by the text of their KeyField. Null when the path is not a list. */
	virtual TSharedPtr<ISlaterListSource> FindList(const FString& Path, const FString& KeyField) = 0;

	/**
	 * Calls OnChange whenever the value at Path may have changed, also when a link on the way (`Player` in
	 * `Player.Name`) is replaced: the subscription follows the new source. Never null; a path nothing can watch gives a subscription that does nothing.
	 */
	virtual TUniquePtr<ISlaterSubscription> Subscribe(const FString& Path, TFunction<void()> OnChange) = 0;

	/**
	 * Whether a control's event can write to Path (`on-change="{Volume}"`, T-381): the data has a setter for it, or a field
	 * it lets the UI write. False says why in OutWhy (checked at load, so the error carries the attribute's line). The
	 * default is a read-only scope.
	 */
	virtual bool CheckWrite(const FString& Path, FString& OutWhy) const;

	/**
	 * Writes a control's value (as text: `0.25`, `true`, a typed name) to Path through its setter, converted to the field's
	 * type. The data broadcasts the change itself, as for any other change. False when there is no setter or the text does
	 * not convert. The default writes nothing.
	 */
	virtual bool Write(const FString& Path, const FString& Text);
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FSlaterListChanged, const FString& /*Key*/, bool /*bAdded*/);

/**
 * The small list interface `<repeat>` reads: count, item by index, a stable key and a change event with the diff (which
 * key came or went). An item that only changes a field does not touch the list: that is the item scope's own subscription.
 */
class SLATERUI_API ISlaterListSource : public TSharedFromThis<ISlaterListSource>
{
public:
	virtual ~ISlaterListSource() = default;

	virtual int32 Num() const = 0;
	/** The key of the item at Index, empty if it has none. */
	virtual FString KeyAt(int32 Index) const = 0;
	virtual TSharedPtr<ISlaterScope> ItemAt(int32 Index) = 0;
	virtual TSharedPtr<ISlaterScope> FindItem(const FString& Key) = 0;

	/** Fires after an item with this key was added or removed. A replaced item is a removal and an addition. */
	FSlaterListChanged OnChanged;
};

/**
 * A scope for plain C++ with no view-model class: values are getters registered by path, lists are list sources, and
 * the owner calls `Notify(Path)` when a value changed. Paths are used as written (`Health`, `Player.Name`): there is no
 * walking, so a getter can compute anything. Items of a list are scopes of their own (any `ISlaterScope`).
 */
class SLATERUI_API FSlaterGetterScope : public ISlaterScope
{
public:
	FSlaterGetterScope();
	virtual ~FSlaterGetterScope() override;

	/** Registers or replaces the getter for a path. */
	void AddValue(const FString& Path, TFunction<FString()> Getter);
	/** Registers the list at a path. The source decides its keys (the `key` of the `<repeat>` is not used). */
	void AddList(const FString& Path, TSharedRef<ISlaterListSource> List);
	/** Lets controls write Path (`on-change="{Path}"`): the setter returns false for a value it refuses. The owner calls `Notify` when the value changes, as for any change. */
	void AddSetter(const FString& Path, TFunction<bool(const FString&)> Setter);
	/** Tells the subscriptions on Path that its value changed. */
	void Notify(const FString& Path);

	/** Plain-literal forms (`AddValue("Count", ...)`): convert and forward, nothing else. A `TEXT()` literal or an `FString` still takes the versions above. */
	void AddValue(const ANSICHAR* Path, TFunction<FString()> Getter) { AddValue(FString(ANSI_TO_TCHAR(Path)), MoveTemp(Getter)); }
	void AddList(const ANSICHAR* Path, TSharedRef<ISlaterListSource> List) { AddList(FString(ANSI_TO_TCHAR(Path)), MoveTemp(List)); }
	void AddSetter(const ANSICHAR* Path, TFunction<bool(const FString&)> Setter) { AddSetter(FString(ANSI_TO_TCHAR(Path)), MoveTemp(Setter)); }
	void Notify(const ANSICHAR* Path) { Notify(FString(ANSI_TO_TCHAR(Path))); }

	virtual bool Read(const FString& Path, FString& OutText) const override;
	virtual bool Check(const FString& Path, bool bList, FString& OutWhy) const override;
	virtual TSharedPtr<ISlaterListSource> FindList(const FString& Path, const FString& KeyField) override;
	virtual TUniquePtr<ISlaterSubscription> Subscribe(const FString& Path, TFunction<void()> OnChange) override;
	virtual bool CheckWrite(const FString& Path, FString& OutWhy) const override;
	virtual bool Write(const FString& Path, const FString& Text) override;

private:
	TMap<FString, TFunction<FString()>> Getters;
	TMap<FString, TFunction<bool(const FString&)>> Setters;
	TMap<FString, TSharedRef<ISlaterListSource>> Lists;
	TSharedRef<FSlaterCallbackTable> Table;
};

namespace Slater
{
	/** A scope over the mock JSON model: top-level and dotted scalars, arrays of objects keyed by a field. Pushes what the model's events say. */
	SLATERUI_API TSharedRef<ISlaterScope> MakeModelScope(TSharedRef<FSlaterModel> Model);
}
