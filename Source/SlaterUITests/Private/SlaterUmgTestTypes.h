// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "SlaterUmgTestTypes.generated.h"

/** A concrete UserWidget (the engine's is abstract) for the `<umg class>` tests: `/Script/SlaterUITests.SlaterTestUserWidget`. */
UCLASS()
class USlaterTestUserWidget : public UUserWidget
{
	GENERATED_BODY()
};

/** Not creatable: `<umg>` must refuse it with a reason. */
UCLASS(Abstract)
class USlaterTestAbstractUserWidget : public UUserWidget
{
	GENERATED_BODY()
};
