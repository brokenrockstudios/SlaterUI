// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "SlaterHealthHud.generated.h"

/**
 * The view model of `Examples/HealthHud.sltr`, the engine's way: derive from `UMVVMViewModelBase` and the layout's
 * `{Health}`, `{HealthFraction}` and `{IsLow}` follow it with no Slater-specific code. The minimal field-notify pattern:
 *
 *  1. A stored value is `UPROPERTY(FieldNotify, Getter, Setter)`; UHT generates the field list (`FFieldNotificationClassDescriptor`).
 *  2. Its setter changes it through `UE_MVVM_SET_PROPERTY_VALUE`, which broadcasts only when the value really changed.
 *  3. A derived value is a `UFUNCTION(BlueprintPure, FieldNotify)` getter with no storage; whoever changes what it reads
 *     broadcasts it with `UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED`.
 *
 * `MakeObjectScope` reads the fields by name, subscribes through the descriptor and a bound widget reads again on each broadcast.
 */
UCLASS()
class SLATERUIEXAMPLES_API USlaterHealthHudViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter)
	FString PlayerName = TEXT("Rook");

	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter)
	int32 Health = 100;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Getter, Setter)
	int32 MaxHealth = 100;

	/** 0..1, what `<meter value>` reads. Derived: notified by the setters below. */
	UFUNCTION(BlueprintPure, FieldNotify)
	float GetHealthFraction() const { return MaxHealth > 0 ? FMath::Clamp(static_cast<float>(Health) / MaxHealth, 0.f, 1.f) : 0.f; }

	/** True at a quarter of the maximum or less: shows the warning line. */
	UFUNCTION(BlueprintPure, FieldNotify)
	bool GetIsLow() const { return Health * 4 <= MaxHealth; }

	const FString& GetPlayerName() const { return PlayerName; }
	void SetPlayerName(const FString& NewName) { UE_MVVM_SET_PROPERTY_VALUE(PlayerName, NewName); }

	int32 GetHealth() const { return Health; }
	void SetHealth(int32 NewHealth)
	{
		if (UE_MVVM_SET_PROPERTY_VALUE(Health, FMath::Clamp(NewHealth, 0, MaxHealth)))
		{
			NotifyDerived();
		}
	}

	int32 GetMaxHealth() const { return MaxHealth; }
	void SetMaxHealth(int32 NewMax)
	{
		if (UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, FMath::Max(NewMax, 1)))
		{
			Health = FMath::Min(Health, MaxHealth);
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Health);
			NotifyDerived();
		}
	}

	void Damage(int32 Amount) { SetHealth(Health - Amount); }
	void Heal(int32 Amount) { SetHealth(Health + Amount); }

	/** One step of the demo timer: drains while it can, heals back up from empty to full, so the warning comes and goes. */
	void Advance()
	{
		if (Health <= 0)
		{
			bHealing = true;
		}
		else if (Health >= MaxHealth)
		{
			bHealing = false;
		}
		if (bHealing)
		{
			Heal(4);
		}
		else
		{
			Damage(3);
		}
	}

private:
	void NotifyDerived()
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthFraction);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsLow);
	}

	bool bHealing = false;
};
