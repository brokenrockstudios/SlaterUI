// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterModel.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace SlaterModelPrivate
{
	bool ScalarToString(const FJsonValue& Value, FString& OutText)
	{
		switch (Value.Type)
		{
		case EJson::String:
			OutText = Value.AsString();
			return true;
		case EJson::Number:
		{
			const double Number = Value.AsNumber();
			OutText = (Number == FMath::FloorToDouble(Number) && FMath::Abs(Number) < 1e9) ? FString::FromInt(static_cast<int32>(Number)) : FString::SanitizeFloat(Number);
			return true;
		}
		case EJson::Boolean:
			OutText = Value.AsBool() ? TEXT("true") : TEXT("false");
			return true;
		default:
			return false;
		}
	}

	/** Follows `A.B.C` through nested objects; the last segment is returned in OutLast. */
	const FJsonObject* WalkToParent(const FJsonObject& Root, const FString& Path, FString& OutLast)
	{
		const FJsonObject* Object = &Root;
		FString Rest = Path;
		FString Head;
		FString Tail;
		while (Rest.Split(TEXT("."), &Head, &Tail))
		{
			const TSharedPtr<FJsonObject>* Child = nullptr;
			if (!Object->TryGetObjectField(Head, Child))
			{
				return nullptr;
			}
			Object = Child->Get();
			Rest = Tail;
		}
		OutLast = Rest;
		return Object;
	}
}
using namespace SlaterModelPrivate;

FSlaterModel::FSlaterModel()
	: Root(MakeShared<FJsonObject>())
{
}

FSlaterModel::~FSlaterModel() = default;

bool FSlaterModel::LoadMock(const FString& Text, const FString& Label, FSlaterError& OutError)
{
	TSharedPtr<FJsonObject> Parsed;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
	{
		OutError.Path = Label;
		OutError.Line = 1;
		OutError.Column = 1;
		OutError.Message = FString::Printf(TEXT("not a JSON object: %s"), *Reader->GetErrorMessage());
		return false;
	}
	Root = Parsed;
	KeyIndexes.Reset();
	return true;
}

bool FSlaterModel::LoadMockFile(const FString& FilePath, FSlaterError& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FilePath))
	{
		OutError.Path = FilePath;
		OutError.Line = 1;
		OutError.Column = 1;
		OutError.Message = TEXT("cannot read the mock data file");
		return false;
	}
	return LoadMock(Text, FilePath, OutError);
}

bool FSlaterModel::GetString(const FString& Path, FString& OutText) const
{
	FString Last;
	const FJsonObject* Parent = WalkToParent(*Root, Path, Last);
	if (!Parent)
	{
		return false;
	}
	return ReadField(*Parent, Last, OutText);
}

bool FSlaterModel::ReadField(const FJsonObject& Item, const FString& Field, FString& OutText)
{
	const TSharedPtr<FJsonValue> Value = Item.TryGetField(Field);
	return Value.IsValid() && ScalarToString(*Value, OutText);
}

const TArray<TSharedPtr<FJsonValue>>* FSlaterModel::FindArray(const FString& ArrayPath) const
{
	FString Last;
	const FJsonObject* Parent = WalkToParent(*Root, ArrayPath, Last);
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	return (Parent && Parent->TryGetArrayField(Last, Array)) ? Array : nullptr;
}

TSharedPtr<FJsonObject> FSlaterModel::FindItem(const FString& ArrayPath, const FString& KeyField, const FString& Key) const
{
	const TArray<TSharedPtr<FJsonValue>>* Array = FindArray(ArrayPath);
	if (!Array)
	{
		return nullptr;
	}
	TMap<FString, FKeyIndex>& ForArray = KeyIndexes.FindOrAdd(ArrayPath);
	FKeyIndex* Index = ForArray.Find(KeyField);
	if (!Index)
	{
		Index = &ForArray.Add(KeyField);
		FString Text;
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			// The first item with a key wins, as a front-to-back search would find it.
			if (Value.IsValid() && Value->TryGetObject(Object) && ReadField(**Object, KeyField, Text))
			{
				if (Index->Items.Contains(Text))
				{
					Index->bDuplicates = true;
				}
				else
				{
					Index->Items.Add(Text, {*Object, Value});
				}
			}
		}
	}
	const FKeyIndex::FEntry* Entry = Index->Items.Find(Key);
	return Entry ? Entry->Object : nullptr;
}

void FSlaterModel::DropKeyIndexes(const FString& ArrayPath, const FString* KeepKeyField)
{
	TMap<FString, FKeyIndex>* ForArray = KeyIndexes.Find(ArrayPath);
	if (!ForArray)
	{
		return;
	}
	for (auto It = ForArray->CreateIterator(); It; ++It)
	{
		if (!KeepKeyField || It.Key() != *KeepKeyField)
		{
			It.RemoveCurrent();
		}
	}
}

bool FSlaterModel::SetValue(const FString& Path, const FString& Text)
{
	FString Last;
	const FJsonObject* ConstParent = WalkToParent(*Root, Path, Last);
	if (!ConstParent)
	{
		return false;
	}
	FJsonObject* Parent = const_cast<FJsonObject*>(ConstParent);
	FString Old;
	if (ReadField(*Parent, Last, Old) && Old == Text)
	{
		return true;
	}
	Parent->SetStringField(Last, Text);
	OnValueChanged.Broadcast(Path);
	return true;
}

bool FSlaterModel::SetItemValue(const FString& ArrayPath, const FString& KeyField, const FString& Key, const FString& Field, const TSharedRef<FJsonValue>& Value)
{
	const TSharedPtr<FJsonObject> Item = FindItem(ArrayPath, KeyField, Key);
	if (!Item.IsValid())
	{
		return false;
	}
	FString Old;
	FString New;
	if (ReadField(*Item, Field, Old) && ScalarToString(*Value, New) && Old == New)
	{
		return true;
	}
	Item->SetField(Field, Value);
	// A changed key field makes the index of that field stale.
	if (TMap<FString, FKeyIndex>* ForArray = KeyIndexes.Find(ArrayPath))
	{
		ForArray->Remove(Field);
	}
	OnItemFieldChanged.Broadcast(ArrayPath, Key, Field);
	return true;
}

bool FSlaterModel::SetItemString(const FString& ArrayPath, const FString& KeyField, const FString& Key, const FString& Field, const FString& Value)
{
	return SetItemValue(ArrayPath, KeyField, Key, Field, MakeShared<FJsonValueString>(Value));
}

bool FSlaterModel::SetItemNumber(const FString& ArrayPath, const FString& KeyField, const FString& Key, const FString& Field, double Value)
{
	return SetItemValue(ArrayPath, KeyField, Key, Field, MakeShared<FJsonValueNumber>(Value));
}

bool FSlaterModel::SetItemBool(const FString& ArrayPath, const FString& KeyField, const FString& Key, const FString& Field, bool Value)
{
	return SetItemValue(ArrayPath, KeyField, Key, Field, MakeShared<FJsonValueBoolean>(Value));
}

bool FSlaterModel::AddItem(const FString& ArrayPath, const FString& KeyField, TSharedRef<FJsonObject> Item)
{
	const TArray<TSharedPtr<FJsonValue>>* ConstArray = FindArray(ArrayPath);
	FString Key;
	if (!ConstArray || !ReadField(*Item, KeyField, Key) || FindItem(ArrayPath, KeyField, Key).IsValid())
	{
		return false;
	}
	const_cast<TArray<TSharedPtr<FJsonValue>>*>(ConstArray)->Add(MakeShared<FJsonValueObject>(Item));
	// Keep the index of this key field up to date; any other key field's index is rebuilt on its next lookup.
	DropKeyIndexes(ArrayPath, &KeyField);
	TMap<FString, FKeyIndex>* ForArray = KeyIndexes.Find(ArrayPath);
	if (FKeyIndex* Index = ForArray ? ForArray->Find(KeyField) : nullptr)
	{
		Index->Items.Add(Key, {Item, ConstArray->Last()});
	}
	OnItemsChanged.Broadcast(ArrayPath, Key, true);
	return true;
}

bool FSlaterModel::RemoveItem(const FString& ArrayPath, const FString& KeyField, const FString& Key)
{
	const TArray<TSharedPtr<FJsonValue>>* ConstArray = FindArray(ArrayPath);
	if (!ConstArray || !FindItem(ArrayPath, KeyField, Key).IsValid())
	{
		return false;
	}
	TArray<TSharedPtr<FJsonValue>>& Array = *const_cast<TArray<TSharedPtr<FJsonValue>>*>(ConstArray);
	FKeyIndex* KeyIndex = KeyIndexes.FindChecked(ArrayPath).Find(KeyField);
	const int32 Index = Array.IndexOfByKey(KeyIndex->Items.FindChecked(Key).Value);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Array.RemoveAt(Index);
	// Patch this key field's index unless a duplicate key could now show through; any other key field's index is rebuilt on its next lookup.
	if (KeyIndex && !KeyIndex->bDuplicates)
	{
		KeyIndex->Items.Remove(Key);
		DropKeyIndexes(ArrayPath, &KeyField);
	}
	else
	{
		DropKeyIndexes(ArrayPath);
	}
	OnItemsChanged.Broadcast(ArrayPath, Key, false);
	return true;
}
