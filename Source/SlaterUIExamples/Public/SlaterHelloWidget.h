// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterCommands.h"
#include "SlaterLayoutWidget.h"
#include "SlaterScope.h"
#include "SlaterHelloWidget.generated.h"

/**
 * Hello again, as a widget: one class tied 1:1 to `Examples/Hello.sltr`, the text version of a UMG widget Blueprint.
 * `CreateWidget<USlaterHelloWidget>(World)` and `AddToViewport()` work like for any other widget. The whole class:
 */
UCLASS()
class SLATERUIEXAMPLES_API USlaterHelloWidget : public USlaterLayoutUserWidget
{
	GENERATED_BODY()

public:
	USlaterHelloWidget()
	{
		LayoutId = TEXT("Examples/Hello");
		StyleName = TEXT("Dusk");
	}

	int32 GetCount() const { return Count; }

	void Add() { Set(Count + 1); }
	void Reset() { Set(0); }

protected:
	// What `{Count}` and `{Greeting}` read, filled each time the view is built.
	virtual void BuildScope(FSlaterGetterScope& Scope) override
	{
		Scope.AddValue(TEXT("Count"), [this]() { return FString::FromInt(Count); });
		Scope.AddValue(TEXT("Greeting"), [this]() { return Count == 0 ? FString(TEXT("Press the button.")) : Count < 10 ? FString(TEXT("Keep going.")) : FString(TEXT("That is plenty.")); });
	}

	// The commands the layout names are member functions.
	virtual void RegisterCommands(FSlaterCommandTable& Commands) override
	{
		Commands.Register(TEXT("Hello.Add"), [this](const FSlaterCommandCall&) { Add(); });
		Commands.Register(TEXT("Hello.Reset"), [this](const FSlaterCommandCall&) { Reset(); });
	}

private:
	// Change the data only here, so the bound text is told.
	void Set(int32 NewCount)
	{
		Count = NewCount;
		Notify({TEXT("Count"), TEXT("Greeting")});
	}

	int32 Count = 0; // The data is a plain member.
};
