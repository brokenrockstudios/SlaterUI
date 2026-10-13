// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterMarkup.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FJsonValue;

DECLARE_MULTICAST_DELEGATE_OneParam(FSlaterModelValueChanged, const FString& /*Path*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSlaterModelItemFieldChanged, const FString& /*ArrayPath*/, const FString& /*Key*/, const FString& /*Field*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSlaterModelItemsChanged, const FString& /*ArrayPath*/, const FString& /*Key*/, bool /*bAdded*/);

/**
 * The data a layout binds to, for now read from plain JSON (`.mock.json`, through the engine's Json module). A
 * spike-sized stand-in for T-345: top-level scalars, and arrays of objects whose items are found by a key field. Every
 * change goes through a method here, which updates the JSON and broadcasts what changed, so a pushed binding can
 * update just the widget it feeds while a polled one reads the same JSON each frame. Game thread only.
 */
class SLATERUI_API FSlaterModel : public TSharedFromThis<FSlaterModel>
{
public:
	FSlaterModel();
	~FSlaterModel();

	/** Replaces the data with the JSON object in Text. Errors are reported as `Label(Line,Column): message` (the JSON reader gives no position, so Line and Column are 1). */
	bool LoadMock(const FString& Text, const FString& Label, FSlaterError& OutError);
	bool LoadMockFile(const FString& FilePath, FSlaterError& OutError);

	/** A scalar at a dotted path from the root (`Title`, `Player.Name`), as text. Numbers that are whole print without a decimal point. */
	bool GetString(const FString& Path, FString& OutText) const;

	/** The items of an array of objects at a dotted path, or null. */
	const TArray<TSharedPtr<FJsonValue>>* FindArray(const FString& ArrayPath) const;

	/** The item whose KeyField prints as Key, or null. The first call for an array and key field reads the whole array into an index; later calls are constant time. The index follows `AddItem`, `RemoveItem` and `SetItem*` on the key field; the array is not otherwise changed behind the model's back. */
	TSharedPtr<FJsonObject> FindItem(const FString& ArrayPath, const FString& KeyField, const FString& Key) const;

	/** A field of one item as text (same rules as `GetString`). */
	static bool ReadField(const FJsonObject& Item, const FString& Field, FString& OutText);

	/** Sets a scalar at a dotted path from text; the value stays a string. Broadcasts `OnValueChanged` if it differs. */
	bool SetValue(const FString& Path, const FString& Text);

	bool SetItemString(const FString& ArrayPath, const FString& KeyField, const FString& Key, const FString& Field, const FString& Value);
	bool SetItemNumber(const FString& ArrayPath, const FString& KeyField, const FString& Key, const FString& Field, double Value);
	bool SetItemBool(const FString& ArrayPath, const FString& KeyField, const FString& Key, const FString& Field, bool Value);

	/** Appends an item; its KeyField must be new in the array. */
	bool AddItem(const FString& ArrayPath, const FString& KeyField, TSharedRef<FJsonObject> Item);
	bool RemoveItem(const FString& ArrayPath, const FString& KeyField, const FString& Key);

	FSlaterModelValueChanged OnValueChanged;
	FSlaterModelItemFieldChanged OnItemFieldChanged;
	FSlaterModelItemsChanged OnItemsChanged;

private:
	TSharedPtr<FJsonObject> Root;
	/** Items by key, per array path and key field (array path, then key field). A cache of `FindItem`; dropped whenever it could be stale. */
	struct FKeyIndex
	{
		struct FEntry
		{
			TSharedPtr<FJsonObject> Object;
			/** The array's own element, so removal finds its position by pointer. */
			TSharedPtr<FJsonValue> Value;
		};
		TMap<FString, FEntry> Items;
		/** Two items had the same key (only a loaded file can do that); removal then drops the index instead of patching it. */
		bool bDuplicates = false;
	};
	mutable TMap<FString, TMap<FString, FKeyIndex>> KeyIndexes;

	void DropKeyIndexes(const FString& ArrayPath, const FString* KeepKeyField = nullptr);

	bool SetItemValue(const FString& ArrayPath, const FString& KeyField, const FString& Key, const FString& Field, const TSharedRef<FJsonValue>& Value);
};
