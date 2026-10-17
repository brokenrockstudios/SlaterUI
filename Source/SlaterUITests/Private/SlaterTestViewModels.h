// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FieldNotificationDelegate.h"
#include "FieldNotificationId.h"
#include "IFieldNotificationClassDescriptor.h"
#include "INotifyFieldValueChanged.h"
#include "SlaterCommands.h"
#include "SlaterLayoutWidget.h"
#include "SlaterScope.h"
#include "SlaterView.h"
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

	/** A UFUNCTION, so a control inside a repeated item can write `{Count}` (T-381). */
	UFUNCTION()
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

/** Settings the UI edits (T-381): every way a control can write back, and fields it cannot write. */
UCLASS()
class USlaterTestSettingsVM : public USlaterTestVMBase
{
	GENERATED_BODY()

public:
	/** Written through SetVolume, which clamps to MaxVolume and broadcasts only a real change (as `UE_MVVM_SET_PROPERTY_VALUE` does). */
	UPROPERTY(BlueprintReadOnly)
	float Volume = 0.5f;

	/** Written directly: BlueprintReadWrite and no setter, so the scope broadcasts it. */
	UPROPERTY(BlueprintReadWrite)
	bool bMuted = false;

	/** Written through SetNickname, which trims what it is given. */
	UPROPERTY(BlueprintReadOnly)
	FString Nickname;

	/** No setter and read-only: a control cannot write it. */
	UPROPERTY(BlueprintReadOnly)
	int32 Level = 3;

	/** An object field: no text converts to it. */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UObject> Target;

	float MaxVolume = 1.f;
	/** A view model that notifies on every set, changed or not. */
	bool bAlwaysBroadcast = false;
	int32 VolumeSets = 0;
	int32 NicknameSets = 0;

	UFUNCTION()
	void SetVolume(float InVolume)
	{
		++VolumeSets;
		const float Clamped = FMath::Clamp(InVolume, 0.f, MaxVolume);
		if (Clamped != Volume || bAlwaysBroadcast)
		{
			Volume = Clamped;
			Notify(TEXT("Volume"));
		}
	}

	UFUNCTION()
	void SetNickname(const FString& InNickname)
	{
		++NicknameSets;
		const FString Trimmed = InNickname.TrimStartAndEnd();
		if (Trimmed != Nickname)
		{
			Nickname = Trimmed;
			Notify(TEXT("Nickname"));
		}
	}
};

/**
 * The T-567 spike: a widget that is its own view model. Its members are FieldNotify like a `UMVVMViewModelBase`'s, and
 * `MakeObjectScope(this)` is asked to read and follow them. `UWidget` already implements `INotifyFieldValueChanged`.
 */
UCLASS()
class USlaterTestFieldWidget : public USlaterLayoutUserWidget
{
	GENERATED_BODY()

public:
	USlaterTestFieldWidget()
	{
		LayoutId = TEXT("FieldWidget");
		StyleName = TEXT("Dusk");
	}

	UPROPERTY(FieldNotify, Getter, Setter)
	int32 Count = 0;

	/** Derived, no storage: notified by `SetCount`. */
	UFUNCTION(BlueprintPure, FieldNotify)
	FString GetGreeting() const { return Count == 0 ? TEXT("Press the button.") : TEXT("Pressed."); }

	int32 GetCount() const { return Count; }
	void SetCount(int32 NewCount)
	{
		if (Count != NewCount)
		{
			Count = NewCount;
			BroadcastFieldValueChanged(ThisClass::FFieldNotificationClassDescriptor::Count);
			BroadcastFieldValueChanged(ThisClass::FFieldNotificationClassDescriptor::GetGreeting);
		}
	}
};

/** A code-owned layout widget (T-559): layout, scope values and commands all come from the class, no `Configure`. */
UCLASS()
class USlaterTestCodeWidget : public USlaterLayoutUserWidget
{
	GENERATED_BODY()

public:
	USlaterTestCodeWidget()
	{
		LayoutId = TEXT("CodeWidget");
		StyleName = TEXT("Dusk");
	}

	int32 Count = 0;
	int32 ScopeBuilds = 0;
	int32 CommandRegistrations = 0;

	void Add()
	{
		++Count;
		Notify(TEXT("Count"));
	}

	/** Changes data and lets the base tell every binding (T-565). */
	void SetCountQuietly(int32 NewCount, bool bAll)
	{
		Count = NewCount;
		if (bAll)
		{
			NotifyAll();
		}
		else
		{
			Notify({TEXT("Other"), TEXT("Count")});
		}
	}

protected:
	virtual void BuildScope(FSlaterGetterScope& Scope) override
	{
		++ScopeBuilds;
		Scope.AddValue(TEXT("Other"), [this]() { return FString::FromInt(Count * 10); });
		Scope.AddValue(TEXT("Count"), [this]() { return FString::FromInt(Count); });
	}

	virtual void RegisterCommands(FSlaterCommandTable& Commands) override
	{
		++CommandRegistrations;
		Commands.Register(TEXT("Code.Add"), [this](const FSlaterCommandCall&) { Add(); });
	}
};
