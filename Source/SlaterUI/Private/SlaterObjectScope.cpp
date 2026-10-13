// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterObjectScope.h"

#include "INotifyFieldValueChanged.h"
#include "SlaterStyle.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace
{
	using UE::FieldNotification::FFieldId;

	/** A readable member of a view model: a property, or a function with no parameters and a return value. */
	struct FMember
	{
		FProperty* Property = nullptr;
		UFunction* Function = nullptr;
		/** The name it was found under (`GetHealth` when `Health` was asked for). */
		FName Name;

		bool IsValid() const { return Property || Function; }
	};

	FMember FindMember(const UClass* Class, FName Name)
	{
		auto Try = [Class](FName Candidate) -> FMember
		{
			FMember Member;
			Member.Name = Candidate;
			Member.Property = Class->FindPropertyByName(Candidate);
			if (!Member.Property)
			{
				UFunction* Function = Class->FindFunctionByName(Candidate);
				if (Function && Function->NumParms == 1 && Function->GetReturnProperty())
				{
					Member.Function = Function;
				}
			}
			return Member;
		};
		FMember Found = Try(Name);
		if (!Found.IsValid())
		{
			Found = Try(FName(*(TEXT("Get") + Name.ToString())));
		}
		return Found;
	}

	/** The text of one value, the way the mock model prints its scalars: whole numbers without a point, bool as true/false. */
	bool ReadValue(const FProperty* Property, const void* Value, FString* OutText, UObject** OutObject)
	{
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);
			if (OutObject)
			{
				*OutObject = Object;
			}
			if (OutText)
			{
				*OutText = Object ? Object->GetName() : FString();
			}
			return true;
		}
		if (!OutText)
		{
			return true;
		}
		if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
		{
			*OutText = Bool->GetPropertyValue(Value) ? TEXT("true") : TEXT("false");
		}
		else if (const FFloatProperty* Float = CastField<FFloatProperty>(Property))
		{
			const double Number = Float->GetPropertyValue(Value);
			*OutText = (Number == FMath::FloorToDouble(Number) && FMath::Abs(Number) < 1e9) ? FString::FromInt(static_cast<int32>(Number)) : FString::SanitizeFloat(Number);
		}
		else if (const FDoubleProperty* Double = CastField<FDoubleProperty>(Property))
		{
			const double Number = Double->GetPropertyValue(Value);
			*OutText = (Number == FMath::FloorToDouble(Number) && FMath::Abs(Number) < 1e9) ? FString::FromInt(static_cast<int32>(Number)) : FString::SanitizeFloat(Number);
		}
		else if (const FStrProperty* String = CastField<FStrProperty>(Property))
		{
			*OutText = String->GetPropertyValue(Value);
		}
		else if (const FNameProperty* Name = CastField<FNameProperty>(Property))
		{
			*OutText = Name->GetPropertyValue(Value).ToString();
		}
		else if (const FTextProperty* Text = CastField<FTextProperty>(Property))
		{
			*OutText = Text->GetPropertyValue(Value).ToString();
		}
		else
		{
			Property->ExportText_Direct(*OutText, Value, Value, nullptr, PPF_None);
		}
		return true;
	}

	bool ReadMember(UObject* Object, const FMember& Member, FString* OutText, UObject** OutObject)
	{
		if (Member.Property)
		{
			return ReadValue(Member.Property, Member.Property->ContainerPtrToValuePtr<void>(Object), OutText, OutObject);
		}
		if (!Member.Function)
		{
			return false;
		}
		TArray<uint8, TInlineAllocator<64>> Buffer;
		Buffer.SetNumZeroed(Member.Function->ParmsSize);
		FProperty* Return = Member.Function->GetReturnProperty();
		Return->InitializeValue_InContainer(Buffer.GetData());
		Object->ProcessEvent(Member.Function, Buffer.GetData());
		const bool bOk = ReadValue(Return, Return->ContainerPtrToValuePtr<void>(Buffer.GetData()), OutText, OutObject);
		Return->DestroyValue_InContainer(Buffer.GetData());
		return bOk;
	}

	/** The array property and its element property when the member is a `TArray` of object pointers. */
	const FObjectPropertyBase* ObjectArrayElement(const FMember& Member, const FArrayProperty** OutArray = nullptr)
	{
		const FArrayProperty* Array = CastField<FArrayProperty>(Member.Property);
		if (!Array)
		{
			return nullptr;
		}
		if (OutArray)
		{
			*OutArray = Array;
		}
		return CastField<FObjectPropertyBase>(Array->Inner);
	}

	void SplitPath(const FString& Path, TArray<FString>& OutSegments)
	{
		Path.ParseIntoArray(OutSegments, TEXT("."), false);
	}

	INotifyFieldValueChanged* AsNotifier(UObject* Object)
	{
		return Object ? Cast<INotifyFieldValueChanged>(Object) : nullptr;
	}

	FFieldId FindField(UObject* Object, FName Member)
	{
		INotifyFieldValueChanged* Notifier = AsNotifier(Object);
		return Notifier ? Notifier->GetFieldNotificationDescriptor().GetField(Object->GetClass(), Member) : FFieldId();
	}

	/** Names a "did you mean" can offer: the field-notify fields first, then the other properties. */
	FString Suggestion(UObject* Object, const FString& Wanted)
	{
		TArray<FString> Candidates;
		if (INotifyFieldValueChanged* Notifier = AsNotifier(Object))
		{
			Notifier->GetFieldNotificationDescriptor().ForEachField(Object->GetClass(), [&Candidates](FFieldId Id)
			{
				Candidates.AddUnique(Id.GetName().ToString());
				return true;
			});
		}
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			Candidates.AddUnique(It->GetName());
		}
		const FString Nearest = Slater::SuggestNearest(Wanted, Candidates);
		return Nearest.IsEmpty() ? FString() : FString::Printf(TEXT("; did you mean '%s'?"), *Nearest);
	}

	/** A view model's reading side. */
	class FObjectScope : public ISlaterScope
	{
	public:
		explicit FObjectScope(UObject* InObject) : Object(InObject) {}

		virtual bool Read(const FString& Path, FString& OutText) const override
		{
			TArray<FString> Segments;
			SplitPath(Path, Segments);
			UObject* Current = Object.Get();
			for (int32 I = 0; Current && I < Segments.Num(); ++I)
			{
				const FMember Member = FindMember(Current->GetClass(), FName(*Segments[I]));
				if (!Member.IsValid())
				{
					return false;
				}
				if (I == Segments.Num() - 1)
				{
					return ReadMember(Current, Member, &OutText, nullptr);
				}
				UObject* Next = nullptr;
				ReadMember(Current, Member, nullptr, &Next);
				Current = Next;
			}
			return false;
		}

		virtual bool Check(const FString& Path, bool bList, FString& OutWhy) const override
		{
			UObject* Current = Object.Get();
			if (!Current)
			{
				OutWhy = TEXT("the view model is gone");
				return false;
			}
			TArray<FString> Segments;
			SplitPath(Path, Segments);
			for (int32 I = 0; I < Segments.Num(); ++I)
			{
				const FMember Member = FindMember(Current->GetClass(), FName(*Segments[I]));
				if (!Member.IsValid())
				{
					OutWhy = FString::Printf(TEXT("'%s' is not a field of %s%s"), *Segments[I], *Current->GetClass()->GetName(), *Suggestion(Current, Segments[I]));
					return false;
				}
				if (I == Segments.Num() - 1)
				{
					if (bList && !ObjectArrayElement(Member))
					{
						OutWhy = FString::Printf(TEXT("'%s' of %s is not a list of objects (a TArray of view models)"), *Segments[I], *Current->GetClass()->GetName());
						return false;
					}
					return true;
				}
				if (!CastField<FObjectPropertyBase>(Member.Property) && !(Member.Function && CastField<FObjectPropertyBase>(Member.Function->GetReturnProperty())))
				{
					OutWhy = FString::Printf(TEXT("'%s' of %s is not an object, so '%s' cannot be read from it"), *Segments[I], *Current->GetClass()->GetName(), *Segments[I + 1]);
					return false;
				}
				UObject* Next = nullptr;
				ReadMember(Current, Member, nullptr, &Next);
				if (!Next)
				{
					// Empty right now: what comes after cannot be checked until it is set.
					return true;
				}
				Current = Next;
			}
			return true;
		}

		virtual TSharedPtr<ISlaterListSource> FindList(const FString& Path, const FString& KeyField) override;
		virtual TUniquePtr<ISlaterSubscription> Subscribe(const FString& Path, TFunction<void()> OnChange) override;

		UObject* GetObject() const { return Object.Get(); }

	private:
		TWeakObjectPtr<UObject> Object;
	};

	/** Follows `A.B.C`: a delegate on each object along the way, moved when a link is replaced. */
	class FObjectPathSubscription : public ISlaterSubscription
	{
	public:
		FObjectPathSubscription(UObject* Root, TArray<FString> InSegments, TFunction<void()> InOnChange)
			: Segments(MoveTemp(InSegments)), OnChange(MoveTemp(InOnChange))
		{
			Hops.SetNum(Segments.Num());
			Connect(0, Root);
		}

		virtual ~FObjectPathSubscription() override
		{
			Disconnect(0);
		}

	private:
		struct FHop
		{
			TWeakObjectPtr<UObject> Object;
			FFieldId Field;
			FDelegateHandle Handle;
		};

		TArray<FString> Segments;
		TFunction<void()> OnChange;
		TArray<FHop> Hops;

		/** Subscribes hop From on Object, then follows the link to the next hop's object. */
		void Connect(int32 From, UObject* Object)
		{
			for (int32 I = From; I < Hops.Num(); ++I)
			{
				Hops[I] = FHop();
				if (!Object)
				{
					return;
				}
				const FMember Member = FindMember(Object->GetClass(), FName(*Segments[I]));
				if (!Member.IsValid())
				{
					return;
				}
				Hops[I].Object = Object;
				if (INotifyFieldValueChanged* Notifier = AsNotifier(Object))
				{
					const FFieldId Field = FindField(Object, Member.Name);
					if (Field.IsValid())
					{
						Hops[I].Field = Field;
						Hops[I].Handle = Notifier->AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateRaw(this, &FObjectPathSubscription::HandleChanged, I));
					}
				}
				UObject* Next = nullptr;
				if (I + 1 < Hops.Num())
				{
					ReadMember(Object, Member, nullptr, &Next);
				}
				Object = Next;
			}
		}

		void Disconnect(int32 From)
		{
			for (int32 I = From; I < Hops.Num(); ++I)
			{
				FHop& Hop = Hops[I];
				if (Hop.Handle.IsValid())
				{
					if (UObject* Object = Hop.Object.Get())
					{
						if (INotifyFieldValueChanged* Notifier = AsNotifier(Object))
						{
							Notifier->RemoveFieldValueChangedDelegate(Hop.Field, Hop.Handle);
						}
					}
				}
				Hop = FHop();
			}
		}

		void HandleChanged(UObject* Source, FFieldId, int32 HopIndex)
		{
			if (HopIndex + 1 < Hops.Num())
			{
				// A link on the way changed: unhook everything behind it and follow the new object.
				UObject* Next = nullptr;
				const FMember Member = FindMember(Source->GetClass(), FName(*Segments[HopIndex]));
				if (Member.IsValid())
				{
					ReadMember(Source, Member, nullptr, &Next);
				}
				Disconnect(HopIndex + 1);
				Connect(HopIndex + 1, Next);
			}
			OnChange();
		}
	};

	/** A `TArray` of view models, diffed by key whenever the owner says the array's field changed. */
	class FObjectList : public ISlaterListSource
	{
	public:
		FObjectList(UObject* InOwner, FMember InMember, FString InKeyField)
			: Owner(InOwner), Member(InMember), KeyField(MoveTemp(InKeyField))
		{
			Entries = Snapshot(TArray<FEntry>());
			if (INotifyFieldValueChanged* Notifier = AsNotifier(InOwner))
			{
				Field = FindField(InOwner, Member.Name);
				if (Field.IsValid())
				{
					Handle = Notifier->AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateRaw(this, &FObjectList::HandleChanged));
				}
			}
		}

		virtual ~FObjectList() override
		{
			if (Handle.IsValid())
			{
				if (INotifyFieldValueChanged* Notifier = AsNotifier(Owner.Get()))
				{
					Notifier->RemoveFieldValueChangedDelegate(Field, Handle);
				}
			}
		}

		virtual int32 Num() const override { return Entries.Num(); }
		virtual FString KeyAt(int32 Index) const override { return Entries.IsValidIndex(Index) ? Entries[Index].Key : FString(); }
		virtual TSharedPtr<ISlaterScope> ItemAt(int32 Index) override { return Entries.IsValidIndex(Index) ? Entries[Index].Scope : nullptr; }

		virtual TSharedPtr<ISlaterScope> FindItem(const FString& Key) override
		{
			const FEntry* Found = Entries.FindByPredicate([&Key](const FEntry& Entry) { return Entry.Key == Key; });
			return Found ? Found->Scope : nullptr;
		}

	private:
		struct FEntry
		{
			FString Key;
			TWeakObjectPtr<UObject> Object;
			TSharedPtr<ISlaterScope> Scope;
		};

		TWeakObjectPtr<UObject> Owner;
		FMember Member;
		FString KeyField;
		FFieldId Field;
		FDelegateHandle Handle;
		TArray<FEntry> Entries;

		/** The array as it is now. Items that were there before keep their scope (and its subscriptions). */
		TArray<FEntry> Snapshot(const TArray<FEntry>& Previous) const
		{
			TArray<FEntry> Result;
			UObject* OwnerObject = Owner.Get();
			const FArrayProperty* ArrayProperty = nullptr;
			const FObjectPropertyBase* Element = ObjectArrayElement(Member, &ArrayProperty);
			if (!OwnerObject || !Element)
			{
				return Result;
			}
			FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(OwnerObject));
			for (int32 I = 0; I < Helper.Num(); ++I)
			{
				UObject* Item = Element->GetObjectPropertyValue(Helper.GetRawPtr(I));
				if (!Item)
				{
					continue;
				}
				FEntry Entry;
				Entry.Object = Item;
				const FEntry* Known = Previous.FindByPredicate([Item](const FEntry& Old) { return Old.Object.Get() == Item; });
				Entry.Scope = Known ? Known->Scope : Slater::MakeObjectScope(Item).ToSharedPtr();
				Entry.Scope->Read(KeyField, Entry.Key);
				Result.Add(MoveTemp(Entry));
			}
			return Result;
		}

		void HandleChanged(UObject*, FFieldId)
		{
			TArray<FEntry> Old = MoveTemp(Entries);
			Entries = Snapshot(Old);
			auto Has = [](const TArray<FEntry>& List, const FEntry& Wanted)
			{
				return List.ContainsByPredicate([&Wanted](const FEntry& Other) { return Other.Key == Wanted.Key && Other.Object == Wanted.Object; });
			};
			TArray<FString> Removed;
			TArray<FString> Added;
			for (const FEntry& Entry : Old)
			{
				if (!Has(Entries, Entry))
				{
					Removed.Add(Entry.Key);
				}
			}
			for (const FEntry& Entry : Entries)
			{
				if (!Has(Old, Entry))
				{
					Added.Add(Entry.Key);
				}
			}
			for (const FString& Key : Removed)
			{
				OnChanged.Broadcast(Key, false);
			}
			for (const FString& Key : Added)
			{
				OnChanged.Broadcast(Key, true);
			}
		}
	};

	TSharedPtr<ISlaterListSource> FObjectScope::FindList(const FString& Path, const FString& KeyField)
	{
		TArray<FString> Segments;
		SplitPath(Path, Segments);
		UObject* Current = Object.Get();
		for (int32 I = 0; Current && I < Segments.Num(); ++I)
		{
			const FMember Member = FindMember(Current->GetClass(), FName(*Segments[I]));
			if (!Member.IsValid())
			{
				return nullptr;
			}
			if (I == Segments.Num() - 1)
			{
				return ObjectArrayElement(Member) ? TSharedPtr<ISlaterListSource>(MakeShared<FObjectList>(Current, Member, KeyField)) : nullptr;
			}
			UObject* Next = nullptr;
			ReadMember(Current, Member, nullptr, &Next);
			Current = Next;
		}
		return nullptr;
	}

	TUniquePtr<ISlaterSubscription> FObjectScope::Subscribe(const FString& Path, TFunction<void()> OnChange)
	{
		TArray<FString> Segments;
		SplitPath(Path, Segments);
		return MakeUnique<FObjectPathSubscription>(Object.Get(), MoveTemp(Segments), MoveTemp(OnChange));
	}
}

namespace Slater
{
	TSharedRef<ISlaterScope> MakeObjectScope(UObject* Object)
	{
		return MakeShared<FObjectScope>(Object);
	}
}
