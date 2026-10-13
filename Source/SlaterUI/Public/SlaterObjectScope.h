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
	 * The scope holds the object weakly: a destroyed view model reads nothing and notifies nothing. Subscriptions are
	 * owned by whoever holds the returned `ISlaterSubscription`, so a view that goes away takes its delegates with it.
	 */
	SLATERUI_API TSharedRef<ISlaterScope> MakeObjectScope(UObject* Object);
}
