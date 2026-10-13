// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FieldNotificationDelegate.h"
#include "FieldNotificationId.h"
#include "IFieldNotificationClassDescriptor.h"
#include "INotifyFieldValueChanged.h"
#include "UObject/Object.h"

#include "SlaterTestViewModels.generated.h"

/** Every UPROPERTY of the class is a field-notify field, so the tests do not repeat the engine's descriptor macros. */
struct FSlaterTestDescriptor : UE::FieldNotification::IClassDescriptor
{
	virtual void ForEachField(const UClass* Class, TFunctionRef<bool(UE::FieldNotification::FFieldId FieldId)> Callback) const override
	{
		int32 Index = 0;
		for (TFieldIterator<FProperty> It(Class); It; ++It, ++Index)
		{
			if (!Callback(UE::FieldNotification::FFieldId(It->GetFName(), Index)))
			{
				return;
			}
		}
		// A getter function is a field too: GetLabel notifies as itself.
		for (TFieldIterator<UFunction> It(Class); It; ++It, ++Index)
		{
			if (It->GetName().StartsWith(TEXT("Get")) && !Callback(UE::FieldNotification::FFieldId(It->GetFName(), Index)))
			{
				return;
			}
		}
	}
};

/** A view model the engine's way (INotifyFieldValueChanged), with the delegate list visible to tests. */
UCLASS(Abstract)
class USlaterTestVMBase : public UObject, public INotifyFieldValueChanged
{
	GENERATED_BODY()

public:
	virtual FDelegateHandle AddFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FFieldValueChangedDelegate InNewDelegate) override
	{
		return Delegates.Add(this, InFieldId, MoveTemp(InNewDelegate));
	}

	virtual bool RemoveFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FDelegateHandle InHandle) override
	{
		return Delegates.RemoveFrom(this, InFieldId, InHandle).bRemoved;
	}

	virtual int32 RemoveAllFieldValueChangedDelegates(FDelegateUserObjectConst InUserObject) override
	{
		return Delegates.RemoveAll(this, InUserObject).RemoveCount;
	}

	virtual int32 RemoveAllFieldValueChangedDelegates(UE::FieldNotification::FFieldId InFieldId, FDelegateUserObjectConst InUserObject) override
	{
		return Delegates.RemoveAll(this, InFieldId, InUserObject).RemoveCount;
	}

	virtual const UE::FieldNotification::IClassDescriptor& GetFieldNotificationDescriptor() const override
	{
		static const FSlaterTestDescriptor Descriptor;
		return Descriptor;
	}

	virtual void BroadcastFieldValueChanged(UE::FieldNotification::FFieldId InFieldId) override
	{
		Delegates.Broadcast(this, InFieldId);
	}

	/** Broadcasts the field of that name. */
	void Notify(FName Field)
	{
		BroadcastFieldValueChanged(GetFieldNotificationDescriptor().GetField(GetClass(), Field));
	}

	/** Delegates bound to this object right now. */
	int32 NumBoundDelegates() const { return Delegates.GetView().Num(); }

private:
	UE::FieldNotification::FFieldMulticastDelegate Delegates;
};

UCLASS()
class USlaterTestPlayerVM : public USlaterTestVMBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	int32 Health = 100;

	UPROPERTY()
	float Ratio = 0.5f;

	UPROPERTY()
	bool bAlive = true;

	/** A getter field: `{Label}` reads GetLabel. */
	UFUNCTION()
	FString GetLabel() const { return FString::Printf(TEXT("%s (%d)"), *Name, Health); }

	void SetName(const FString& InName)
	{
		if (Name != InName)
		{
			Name = InName;
			Notify(TEXT("Name"));
			Notify(TEXT("GetLabel"));
		}
	}

	void SetHealth(int32 InHealth)
	{
		if (Health != InHealth)
		{
			Health = InHealth;
			Notify(TEXT("Health"));
		}
	}

	void SetAlive(bool bInAlive)
	{
		bAlive = bInAlive;
		Notify(TEXT("bAlive"));
	}
};

UCLASS()
class USlaterTestItemVM : public USlaterTestVMBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Id;

	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	bool bLocked = false;

	void SetCount(int32 InCount)
	{
		if (Count != InCount)
		{
			Count = InCount;
			Notify(TEXT("Count"));
		}
	}

	void SetLocked(bool bInLocked)
	{
		bLocked = bInLocked;
		Notify(TEXT("bLocked"));
	}
};

UCLASS()
class USlaterTestRootVM : public USlaterTestVMBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Title;

	UPROPERTY()
	TObjectPtr<USlaterTestPlayerVM> Player;

	UPROPERTY()
	TArray<TObjectPtr<USlaterTestItemVM>> Items;

	void SetTitle(const FString& InTitle)
	{
		Title = InTitle;
		Notify(TEXT("Title"));
	}

	void SetPlayer(USlaterTestPlayerVM* InPlayer)
	{
		Player = InPlayer;
		Notify(TEXT("Player"));
	}

	void AddItem(USlaterTestItemVM* Item)
	{
		Items.Add(Item);
		Notify(TEXT("Items"));
	}

	void RemoveItem(USlaterTestItemVM* Item)
	{
		Items.Remove(Item);
		Notify(TEXT("Items"));
	}
};
