// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterUmg.h"

#include "SlaterElements.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWidget.h"

void USlaterUmgSubsystem::Deinitialize()
{
	Hosted.Reset();
	HostedSlate.Reset();
	Super::Deinitialize();
}

UWorld* USlaterUmgSubsystem::FindWorld() const
{
	if (UWorld* World = WorldOverride.Get())
	{
		return World;
	}
	if (!GEngine)
	{
		return nullptr;
	}
	UWorld* Editor = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (!Context.World())
		{
			continue;
		}
		if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
		{
			return Context.World();
		}
		if (Context.WorldType == EWorldType::Editor)
		{
			Editor = Context.World();
		}
	}
	return Editor;
}

int32 USlaterUmgSubsystem::ReleaseUnused()
{
	for (int32 I = Hosted.Num() - 1; I >= 0; --I)
	{
		if (!HostedSlate[I].IsValid())
		{
			Hosted.RemoveAtSwap(I);
			HostedSlate.RemoveAtSwap(I);
		}
	}
	return Hosted.Num();
}

UUserWidget* USlaterUmgSubsystem::CreateHosted(TSubclassOf<UUserWidget> Class, TSharedPtr<SWidget>& OutSlate, FString& OutProblem)
{
	ReleaseUnused();
	UWorld* World = FindWorld();
	if (!World)
	{
		OutProblem = TEXT("there is no game or editor world to create the widget in");
		return nullptr;
	}
	UUserWidget* Widget = UUserWidget::CreateWidgetInstance(*World, Class, NAME_None);
	if (!Widget)
	{
		OutProblem = FString::Printf(TEXT("the engine could not create a %s"), *GetNameSafe(Class.Get()));
		return nullptr;
	}
	OutSlate = Widget->TakeWidget();
	Hosted.Add(Widget);
	HostedSlate.Add(OutSlate);
	return Widget;
}

namespace
{
	TSharedPtr<SWidget> BuildUmg(FSlaterBuildContext& Context)
	{
		const FString ClassPath = Context.GetText(TEXT("class"));
		// Quiet on purpose: a wrong path is a layout error with a line, not a log warning.
		UClass* Class = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet));
		if (!Class || !Class->IsChildOf(UUserWidget::StaticClass()))
		{
			Context.Failure = FString::Printf(TEXT("<umg> class '%s' was not found, or is not a UserWidget; write the full path, for example /Script/MyModule.MyWidget or /Game/UI/WBP_Thing.WBP_Thing_C"), *ClassPath);
			return nullptr;
		}
		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			Context.Failure = FString::Printf(TEXT("<umg> class '%s' is abstract and cannot be created"), *ClassPath);
			return nullptr;
		}
		USlaterUmgSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<USlaterUmgSubsystem>() : nullptr;
		if (!Subsystem)
		{
			Context.Failure = TEXT("<umg> needs the engine, which is not running");
			return nullptr;
		}
		TSharedPtr<SWidget> Slate;
		FString Problem;
		if (!Subsystem->CreateHosted(Class, Slate, Problem))
		{
			Context.Failure = FString::Printf(TEXT("<umg class=\"%s\">: %s"), *ClassPath, *Problem);
			return nullptr;
		}
		return Slate;
	}
}

namespace Slater
{
	void RegisterUmgElements(FSlaterElementRegistry& Registry)
	{
		FSlaterElementDesc Desc;
		Desc.Tag = TEXT("umg");
		Desc.Purpose = TEXT("a UMG widget (code and design assets only; not for mod layers)");
		FSlaterAttrDesc Class;
		Class.Name = TEXT("class");
		Class.Type = ESlaterAttrType::Text;
		Class.bRequired = true;
		Class.CheckText = [](const FString& Value) -> FString
		{
			return Value.StartsWith(TEXT("/")) ? FString() : FString::Printf(TEXT("'%s' is not a class path: write the full path, for example /Script/MyModule.MyWidget"), *Value);
		};
		Desc.Attributes = {Class};
		Desc.Builder = &BuildUmg;
		FString Error;
		const bool bRegistered = Registry.Register(MoveTemp(Desc), &Error);
		ensureMsgf(bRegistered, TEXT("SlaterUI element <umg>: %s"), *Error);
	}
}
