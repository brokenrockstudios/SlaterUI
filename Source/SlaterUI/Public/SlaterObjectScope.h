// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterScope.h"

class UObject;

namespace Slater
{
	/**
	 * A scope over a UObject view model, the engine's FieldNotification way. `{Name}` reads the UPROPERTY `Name` (or the
	 * UFUNCTION `Name` or `GetName` with no parameters and a return value) as text; `{Player.Name}` follows an object
	 * field first. A path is watched when the object implements `INotifyFieldValueChanged` and the member is one of its
	 * field-notify fields: the descriptor finds the field by name, a change event pushes the value in, and a replaced link
	 * (`Player`) moves the subscription to the new object. A member that is not a field-notify field is read when bound
	 * and never again (and `Slater.StaleBindings` shows it once it differs). A list is a `TArray` of object pointers, its
	 * items keyed by a field of each item; the owner broadcasts the array's field when it changes and the list diffs it.
	 *
	 * A control's event writes a field back (`on-change="{Volume}"`, T-381) the engine's MVVM way: a UFUNCTION `SetVolume`
	 * (or `Volume`) with one parameter, else the property when it has a native `Setter` or is BlueprintReadWrite (then the
	 * scope broadcasts its field, as a setter would). The text converts to the field's type (number, bool, string, name,
	 * text, enum by value name). A field with none of these fails the layout at load, with the attribute's line.
	 *
	 * The scope holds the object weakly: a destroyed view model reads nothing and notifies nothing. Subscriptions are
	 * owned by whoever holds the returned `ISlaterSubscription`, so a view that goes away takes its delegates with it.
	 */
	SLATERUI_API TSharedRef<ISlaterScope> MakeObjectScope(UObject* Object);
}
