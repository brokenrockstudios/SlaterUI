// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterScope.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "SlaterModel.h"
#include "SlaterStyle.h"

/** Callbacks by path. A callback may drop its own subscription while it runs. A flat list: a scope has a handful of paths. */
class FSlaterCallbackTable : public TSharedFromThis<FSlaterCallbackTable>
{
public:
	int32 Add(const FString& Path, TFunction<void()> Callback)
	{
		const int32 Id = NextId++;
		Entries.Emplace(Path, Id, MoveTemp(Callback));
		return Id;
	}

	void Remove(int32 Id)
	{
		for (int32 I = 0; I < Entries.Num(); ++I)
		{
			if (Entries[I].Id == Id)
			{
				if (NotifyDepth > 0)
				{
					// Keep the slot until the running notification is over.
					Entries[I].Callback = nullptr;
					bDirty = true;
				}
				else
				{
					Entries.RemoveAtSwap(I);
				}
				return;
			}
		}
	}

	void Notify(const FString& Path)
	{
		++NotifyDepth;
		// Entries added by a callback are not called for this notification.
		const int32 Count = Entries.Num();
		for (int32 I = 0; I < Count; ++I)
		{
			if (Entries[I].Callback && Entries[I].Path == Path)
			{
				Entries[I].Callback();
			}
		}
		if (--NotifyDepth == 0 && bDirty)
		{
			bDirty = false;
			Entries.RemoveAll([](const FEntry& Entry) { return !Entry.Callback; });
		}
	}

private:
	struct FEntry
	{
		FEntry(const FString& InPath, int32 InId, TFunction<void()> InCallback) : Path(InPath), Id(InId), Callback(MoveTemp(InCallback)) {}
		FString Path;
		int32 Id;
		TFunction<void()> Callback;
	};

	TArray<FEntry> Entries;
	int32 NextId = 1;
	int32 NotifyDepth = 0;
	bool bDirty = false;
};

namespace
{
	class FTableSubscription : public ISlaterSubscription
	{
	public:
		FTableSubscription(const TSharedRef<FSlaterCallbackTable>& InTable, int32 InId) : Table(InTable), Id(InId) {}

		virtual ~FTableSubscription() override
		{
			if (const TSharedPtr<FSlaterCallbackTable> Pinned = Table.Pin())
			{
				Pinned->Remove(Id);
			}
		}

	private:
		TWeakPtr<FSlaterCallbackTable> Table;
		int32 Id;
	};

	/** One item of a mock array: a flat JSON object. */
	class FModelItemScope : public ISlaterScope
	{
	public:
		explicit FModelItemScope(TSharedPtr<FJsonObject> InData) : Data(MoveTemp(InData)), Table(MakeShared<FSlaterCallbackTable>()) {}

		const TSharedPtr<FJsonObject>& GetData() const { return Data; }
		void Notify(const FString& Field) { Table->Notify(Field); }

		virtual bool Read(const FString& Path, FString& OutText) const override
		{
			return FSlaterModel::ReadField(*Data, Path, OutText);
		}

		virtual bool Check(const FString& Path, bool bList, FString& OutWhy) const override
		{
			FString Text;
			if (!bList && FSlaterModel::ReadField(*Data, Path, Text))
			{
				return true;
			}
			OutWhy = bList ? FString::Printf(TEXT("an item has no list '%s' (lists inside items are not supported)"), *Path) : FString::Printf(TEXT("the item has no field '%s'"), *Path);
			return false;
		}

		virtual TSharedPtr<ISlaterListSource> FindList(const FString&, const FString&) override { return nullptr; }

		virtual TUniquePtr<ISlaterSubscription> Subscribe(const FString& Path, TFunction<void()> OnChange) override
		{
			return MakeUnique<FTableSubscription>(Table, Table->Add(Path, MoveTemp(OnChange)));
		}

	private:
		TSharedPtr<FJsonObject> Data;
		TSharedRef<FSlaterCallbackTable> Table;
	};

	/** An array of the mock model, one root-level change event routed to the item it names. */
	class FModelList : public ISlaterListSource
	{
	public:
		FModelList(TSharedRef<FSlaterModel> InModel, FString InPath, FString InKeyField)
			: Model(InModel), Path(MoveTemp(InPath)), KeyField(MoveTemp(InKeyField))
		{
			Model->OnItemFieldChanged.AddRaw(this, &FModelList::HandleField);
			Model->OnItemsChanged.AddRaw(this, &FModelList::HandleItems);
		}

		virtual ~FModelList() override
		{
			Model->OnItemFieldChanged.RemoveAll(this);
			Model->OnItemsChanged.RemoveAll(this);
		}

		virtual int32 Num() const override
		{
			const TArray<TSharedPtr<FJsonValue>>* Array = Model->FindArray(Path);
			return Array ? Array->Num() : 0;
		}

		virtual FString KeyAt(int32 Index) const override
		{
			FString Key;
			const TSharedPtr<FJsonObject> Object = ObjectAt(Index);
			if (Object.IsValid())
			{
				FSlaterModel::ReadField(*Object, KeyField, Key);
			}
			return Key;
		}

		virtual TSharedPtr<ISlaterScope> ItemAt(int32 Index) override
		{
			const TSharedPtr<FJsonObject> Object = ObjectAt(Index);
			if (!Object.IsValid())
			{
				return nullptr;
			}
			FString Key;
			FSlaterModel::ReadField(*Object, KeyField, Key);
			if (const TSharedRef<FModelItemScope>* Known = Items.Find(Key))
			{
				if ((*Known)->GetData() == Object)
				{
					return *Known;
				}
				// A second item with the same key (a loaded file can have one): it gets a scope that no change reaches.
				return MakeShared<FModelItemScope>(Object);
			}
			return Items.Add(Key, MakeShared<FModelItemScope>(Object));
		}

		virtual TSharedPtr<ISlaterScope> FindItem(const FString& Key) override
		{
			if (const TSharedRef<FModelItemScope>* Known = Items.Find(Key))
			{
				return *Known;
			}
			const TSharedPtr<FJsonObject> Object = Model->FindItem(Path, KeyField, Key);
			return Object.IsValid() ? TSharedPtr<ISlaterScope>(Items.Add(Key, MakeShared<FModelItemScope>(Object))) : nullptr;
		}

	private:
		TSharedRef<FSlaterModel> Model;
		FString Path;
		FString KeyField;
		TMap<FString, TSharedRef<FModelItemScope>> Items;

		TSharedPtr<FJsonObject> ObjectAt(int32 Index) const
		{
			const TArray<TSharedPtr<FJsonValue>>* Array = Model->FindArray(Path);
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Array && Array->IsValidIndex(Index) && (*Array)[Index].IsValid() && (*Array)[Index]->TryGetObject(Object))
			{
				return *Object;
			}
			return nullptr;
		}

		void HandleField(const FString& ArrayPath, const FString& Key, const FString& Field)
		{
			if (ArrayPath != Path)
			{
				return;
			}
			if (const TSharedRef<FModelItemScope>* Item = Items.Find(Key))
			{
				// Keep the scope alive through its own callbacks.
				const TSharedRef<FModelItemScope> Hold = *Item;
				Hold->Notify(Field);
			}
		}

		void HandleItems(const FString& ArrayPath, const FString& Key, bool bAdded)
		{
			if (ArrayPath != Path)
			{
				return;
			}
			if (!bAdded)
			{
				Items.Remove(Key);
			}
			OnChanged.Broadcast(Key, bAdded);
		}
	};

	class FModelScope : public ISlaterScope
	{
	public:
		explicit FModelScope(TSharedRef<FSlaterModel> InModel) : Model(InModel), Table(MakeShared<FSlaterCallbackTable>()) {}

		virtual ~FModelScope() override
		{
			if (bHooked)
			{
				Model->OnValueChanged.RemoveAll(this);
			}
		}

		virtual bool Read(const FString& Path, FString& OutText) const override
		{
			return Model->GetString(Path, OutText);
		}

		virtual bool Check(const FString& Path, bool bList, FString& OutWhy) const override
		{
			FString Text;
			if (bList ? Model->FindArray(Path) != nullptr : Model->GetString(Path, Text))
			{
				return true;
			}
			OutWhy = bList ? FString::Printf(TEXT("the data has no list '%s'"), *Path) : FString::Printf(TEXT("the data has no value '%s'"), *Path);
			return false;
		}

		virtual TSharedPtr<ISlaterListSource> FindList(const FString& Path, const FString& KeyField) override
		{
			if (!Model->FindArray(Path))
			{
				return nullptr;
			}
			return MakeShared<FModelList>(Model, Path, KeyField);
		}

		virtual TUniquePtr<ISlaterSubscription> Subscribe(const FString& Path, TFunction<void()> OnChange) override
		{
			if (!bHooked)
			{
				bHooked = true;
				Model->OnValueChanged.AddRaw(this, &FModelScope::HandleValue);
			}
			return MakeUnique<FTableSubscription>(Table, Table->Add(Path, MoveTemp(OnChange)));
		}

	private:
		TSharedRef<FSlaterModel> Model;
		TSharedRef<FSlaterCallbackTable> Table;
		bool bHooked = false;

		void HandleValue(const FString& Path)
		{
			Table->Notify(Path);
		}
	};
}

namespace Slater
{
	TSharedRef<ISlaterScope> MakeModelScope(TSharedRef<FSlaterModel> Model)
	{
		return MakeShared<FModelScope>(Model);
	}
}

FSlaterGetterScope::FSlaterGetterScope() : Table(MakeShared<FSlaterCallbackTable>())
{
}

FSlaterGetterScope::~FSlaterGetterScope() = default;

void FSlaterGetterScope::AddValue(const FString& Path, TFunction<FString()> Getter)
{
	Getters.Add(Path, MoveTemp(Getter));
}

void FSlaterGetterScope::AddList(const FString& Path, TSharedRef<ISlaterListSource> List)
{
	Lists.Add(Path, MoveTemp(List));
}

void FSlaterGetterScope::Notify(const FString& Path)
{
	Table->Notify(Path);
}

bool FSlaterGetterScope::Read(const FString& Path, FString& OutText) const
{
	const TFunction<FString()>* Getter = Getters.Find(Path);
	if (!Getter)
	{
		return false;
	}
	OutText = (*Getter)();
	return true;
}

bool FSlaterGetterScope::Check(const FString& Path, bool bList, FString& OutWhy) const
{
	if (bList ? Lists.Contains(Path) : Getters.Contains(Path))
	{
		return true;
	}
	TArray<FString> Candidates;
	if (bList)
	{
		Lists.GetKeys(Candidates);
	}
	else
	{
		Getters.GetKeys(Candidates);
	}
	const FString Nearest = Slater::SuggestNearest(Path, Candidates);
	OutWhy = FString::Printf(TEXT("no %s '%s' is registered%s"), bList ? TEXT("list") : TEXT("value"), *Path, Nearest.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; did you mean '%s'?"), *Nearest));
	return false;
}

TSharedPtr<ISlaterListSource> FSlaterGetterScope::FindList(const FString& Path, const FString&)
{
	const TSharedRef<ISlaterListSource>* List = Lists.Find(Path);
	return List ? TSharedPtr<ISlaterListSource>(*List) : nullptr;
}

TUniquePtr<ISlaterSubscription> FSlaterGetterScope::Subscribe(const FString& Path, TFunction<void()> OnChange)
{
	return MakeUnique<FTableSubscription>(Table, Table->Add(Path, MoveTemp(OnChange)));
}
