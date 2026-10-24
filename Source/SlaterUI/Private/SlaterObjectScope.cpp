// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterObjectScope.h"

#include "INotifyFieldValueChanged.h"
#include "SlaterStyle.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace SlaterObjectScopePrivate
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

	/**
	 * How a control writes a field (T-381), the engine's MVVM destination rules (`UE::MVVM::BindingHelper::IsValidForDestinationBinding`)
	 * minus what needs editor metadata: a setter function (`Name` or `SetName`, one parameter, no return, not const), else the
	 * property itself when it has a native `Setter` or is BlueprintReadWrite.
	 */
	struct FWriteTarget
	{
		UFunction* Function = nullptr;
		FProperty* Property = nullptr;
		/** The function's parameter, or the property: the type the text is converted to. */
		const FProperty* Value = nullptr;
	};

	bool IsSetterFunction(const UFunction* Function)
	{
		return Function && Function->NumParms == 1 && !Function->GetReturnProperty() && !Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure);
	}

	const FProperty* FirstParameter(const UFunction* Function)
	{
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Text, numbers, true/false, names and enums are what a control's value can become. Empty when the type is one of them, else its C++ type. */
	FString UnwritableType(const FProperty* Property)
	{
		if (CastField<FBoolProperty>(Property) || CastField<FStrProperty>(Property) || CastField<FNameProperty>(Property) || CastField<FTextProperty>(Property)
			|| CastField<FEnumProperty>(Property) || CastField<FNumericProperty>(Property))
		{
			return FString();
		}
		return Property ? Property->GetCPPType() : FString(TEXT("nothing"));
	}

	FString Suggestion(UObject* Object, const FString& Wanted);

	bool FindWriteTarget(UObject* Object, FName Name, FWriteTarget& Out, FString& OutWhy)
	{
		const UClass* Class = Object->GetClass();
		for (const FName Candidate : {Name, FName(*(TEXT("Set") + Name.ToString()))})
		{
			UFunction* Function = Class->FindFunctionByName(Candidate);
			if (IsSetterFunction(Function))
			{
				Out.Function = Function;
				Out.Value = FirstParameter(Function);
				break;
			}
		}
		FProperty* Property = Class->FindPropertyByName(Name);
		if (!Out.Function && Property && Property->ArrayDim == 1)
		{
			const bool bReadWrite = Property->HasAnyPropertyFlags(CPF_BlueprintVisible) && !Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly | CPF_EditConst);
			if (Property->HasSetter() || bReadWrite)
			{
				Out.Property = Property;
				Out.Value = Property;
			}
		}
		if (!Out.Value)
		{
			if (!FindMember(Class, Name).IsValid())
			{
				OutWhy = FString::Printf(TEXT("'%s' is not a field of %s%s"), *Name.ToString(), *Class->GetName(), *Suggestion(Object, Name.ToString()));
			}
			else
			{
				OutWhy = FString::Printf(TEXT("'%s' of %s has no setter: add a UFUNCTION Set%s taking the value (the view-model convention; it broadcasts the change), give the property a native Setter, or make it BlueprintReadWrite"),
					*Name.ToString(), *Class->GetName(), *Name.ToString());
			}
			return false;
		}
		const FString Type = UnwritableType(Out.Value);
		if (!Type.IsEmpty())
		{
			OutWhy = FString::Printf(TEXT("'%s' of %s is a %s; a control writes text, a number, true/false, a name or an enum"), *Name.ToString(), *Class->GetName(), *Type);
			return false;
		}
		return true;
	}

	/** Text into a value of the property's type; false when it does not convert (`abc` into a float). */
	bool ImportValue(const FProperty* Property, void* Value, const FString& Text)
	{
		if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
		{
			if (Text != TEXT("true") && Text != TEXT("false"))
			{
				return false;
			}
			Bool->SetPropertyValue(Value, Text == TEXT("true"));
			return true;
		}
		if (const FStrProperty* String = CastField<FStrProperty>(Property))
		{
			String->SetPropertyValue(Value, Text);
			return true;
		}
		if (const FNameProperty* Name = CastField<FNameProperty>(Property))
		{
			Name->SetPropertyValue(Value, FName(*Text));
			return true;
		}
		if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			TextProperty->SetPropertyValue(Value, FText::FromString(Text));
			return true;
		}
		const FNumericProperty* Numeric = CastField<FNumericProperty>(Property);
		if (CastField<FEnumProperty>(Property) || (Numeric && Numeric->IsEnum()))
		{
			// An enum takes its value name (`Hard`), as the property's own text import does.
			return Property->ImportText_Direct(*Text, Value, nullptr, PPF_None) != nullptr;
		}
		double Number = 0.0;
		if (!Numeric || !LexTryParseString(Number, *Text))
		{
			return false;
		}
		if (Numeric->IsFloatingPoint())
		{
			Numeric->SetFloatingPointPropertyValue(Value, Number);
		}
		else
		{
			Numeric->SetIntPropertyValue(Value, static_cast<int64>(FMath::RoundToDouble(Number)));
		}
		return true;
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

		virtual bool CheckWrite(const FString& Path, FString& OutWhy) const override
		{
			// The links on the way are read like any path; only the last segment needs a setter.
			FString Owner, Last;
			SplitLast(Path, Owner, Last);
			if (!Owner.IsEmpty() && !Check(Owner, false, OutWhy))
			{
				return false;
			}
			if (!Object.IsValid())
			{
				OutWhy = TEXT("the view model is gone");
				return false;
			}
			UObject* Target = Owner.IsEmpty() ? Object.Get() : ResolveObject(Owner);
			if (!Target)
			{
				// Empty right now: the setter cannot be checked until the link is set.
				return true;
			}
			FWriteTarget Unused;
			return FindWriteTarget(Target, FName(*Last), Unused, OutWhy);
		}

		virtual bool Write(const FString& Path, const FString& Text) override
		{
			FString Owner, Last;
			SplitLast(Path, Owner, Last);
			UObject* Target = Owner.IsEmpty() ? Object.Get() : ResolveObject(Owner);
			FWriteTarget Writer;
			FString Why;
			if (!Target || !FindWriteTarget(Target, FName(*Last), Writer, Why))
			{
				return false;
			}
			if (Writer.Function)
			{
				TArray<uint8, TInlineAllocator<64>> Buffer;
				Buffer.SetNumZeroed(Writer.Function->ParmsSize);
				Writer.Value->InitializeValue_InContainer(Buffer.GetData());
				const bool bOk = ImportValue(Writer.Value, Writer.Value->ContainerPtrToValuePtr<void>(Buffer.GetData()), Text);
				if (bOk)
				{
					Target->ProcessEvent(Writer.Function, Buffer.GetData());
				}
				Writer.Value->DestroyValue_InContainer(Buffer.GetData());
				return bOk;
			}
			FProperty* Property = Writer.Property;
			if (Property->HasSetter())
			{
				// The native setter takes a whole value: build it apart, then hand it over.
				void* Temp = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
				Property->InitializeValue(Temp);
				const bool bOk = ImportValue(Property, Temp, Text);
				if (bOk)
				{
					Property->SetValue_InContainer(Target, Temp);
				}
				Property->DestroyValue(Temp);
				FMemory::Free(Temp);
				return bOk;
			}
			if (!ImportValue(Property, Property->ContainerPtrToValuePtr<void>(Target), Text))
			{
				return false;
			}
			// A BlueprintReadWrite property was written directly and nothing of the view model ran, so tell the other bindings as its setter would.
			if (INotifyFieldValueChanged* Notifier = AsNotifier(Target))
			{
				const FFieldId Field = FindField(Target, Property->GetFName());
				if (Field.IsValid())
				{
					Notifier->BroadcastFieldValueChanged(Field);
				}
			}
			return true;
		}

		UObject* GetObject() const { return Object.Get(); }

	private:
		TWeakObjectPtr<UObject> Object;

		static void SplitLast(const FString& Path, FString& OutOwner, FString& OutLast)
		{
			if (!Path.Split(TEXT("."), &OutOwner, &OutLast, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				OutOwner.Reset();
				OutLast = Path;
			}
		}

		/** The object at a path of links (`Player`), or null when a link on the way is empty or not an object. */
		UObject* ResolveObject(const FString& Path) const
		{
			TArray<FString> Segments;
			SplitPath(Path, Segments);
			UObject* Current = Object.Get();
			for (int32 I = 0; Current && I < Segments.Num(); ++I)
			{
				const FMember Member = FindMember(Current->GetClass(), FName(*Segments[I]));
				UObject* Next = nullptr;
				if (Member.IsValid())
				{
					ReadMember(Current, Member, nullptr, &Next);
				}
				Current = Next;
			}
			return Current;
		}
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
using namespace SlaterObjectScopePrivate;

namespace Slater
{
	TSharedRef<ISlaterScope> MakeObjectScope(UObject* Object)
	{
		return MakeShared<FObjectScope>(Object);
	}
}
