// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "SlaterExamples.h"
#include "SlaterHelloWidget.h"
#include "SlaterView.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterExamples, Log, All);

namespace SlaterUIExamplesModulePrivate
{
	const FName ExampleHello(TEXT("Hello"));
	const FName ExampleHelloWidget(TEXT("HelloWidget"));
	const FName ExampleHealthHud(TEXT("HealthHud"));

	/** The game or PIE world a widget example goes into, or null in an editor with no play session. */
	UWorld* FindPlayWorld()
	{
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
				{
					return Context.World();
				}
			}
		}
		return nullptr;
	}

	/** `Slater.Example [Name] [Style]`: no name lists the examples; a name opens it in a window of its own, in the dark `Dusk` style unless one is given. */
	void RunExampleCommand(const TArray<FString>& Args)
	{
		const FSlaterExampleRegistry& Registry = FSlaterExampleRegistry::Get();
		if (Args.IsEmpty())
		{
			TArray<FName> Names;
			Registry.GetNames(Names);
			for (const FName Name : Names)
			{
				UE_LOG(LogSlaterExamples, Display, TEXT("%s: %s"), *Name.ToString(), *Registry.Find(Name)->Summary);
			}
			return;
		}

		const FSlaterExample* Example = Registry.Find(FName(*Args[0]));
		if (!Example)
		{
			UE_LOG(LogSlaterExamples, Warning, TEXT("No example named '%s'. Slater.Example lists them."), *Args[0]);
			return;
		}
		if (Example->Show)
		{
			if (UWorld* World = FindPlayWorld())
			{
				Example->Show(*World);
			}
			else
			{
				UE_LOG(LogSlaterExamples, Warning, TEXT("'%s' is a widget: start a play session (PIE) so there is a world to add it to."), *Args[0]);
			}
			return;
		}
		if (!Example->Create || !FSlateApplication::IsInitialized())
		{
			UE_LOG(LogSlaterExamples, Warning, TEXT("Slate is not running here, so there is nowhere to show '%s'."), *Args[0]);
			return;
		}

		const TSharedRef<SSlaterView> View = Example->Create(Args.IsValidIndex(1) ? FName(*Args[1]) : FName(TEXT("Dusk")));
		FSlateApplication::Get().AddWindow(
			SNew(SWindow)
			.Title(FText::FromString(FString::Printf(TEXT("Slater example: %s"), *Example->Name.ToString())))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			[View]);
	}
}
using namespace SlaterUIExamplesModulePrivate;

class FSlaterUIExamplesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FSlaterExampleRegistry& Registry = FSlaterExampleRegistry::Get();
		Registry.Register({ ExampleHello, TEXT("Examples/Hello"), TEXT("a counter over plain getters: FSlaterGetterScope, a command, Notify"), &SlaterExamples::MakeHello });

		Registry.Register({ ExampleHelloWidget, TEXT("Examples/Hello"), TEXT("Hello as a UUserWidget subclass (USlaterHelloWidget): CreateWidget and AddToViewport, in PIE"), nullptr,
			[](UWorld& World) { CreateWidget<USlaterHelloWidget>(&World)->AddToViewport(); } });

		Registry.Register({ ExampleHealthHud, TEXT("Examples/HealthHud"), TEXT("a UMVVMViewModelBase view model: meter, texts and a warning follow a timer; commands with an argument"),
			[](FName StyleName) { return SlaterExamples::MakeHealthHud(StyleName); } });

		ExampleCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("Slater.Example"),
			TEXT("Slater.Example [Name] [Style]: opens a worked example of C++ bound to a layout; with no name, lists them."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&RunExampleCommand));
	}

	virtual void ShutdownModule() override
	{
		ExampleCommand.Reset();
		FSlaterExampleRegistry::Get().Unregister(ExampleHello);
		FSlaterExampleRegistry::Get().Unregister(ExampleHelloWidget);
		FSlaterExampleRegistry::Get().Unregister(ExampleHealthHud);
	}

private:
	TUniquePtr<FAutoConsoleCommand> ExampleCommand;
};

IMPLEMENT_MODULE(FSlaterUIExamplesModule, SlaterUIExamples)
