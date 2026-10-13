// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "SlaterUmg.generated.h"

class SWidget;
class UUserWidget;
class UWorld;
class FSlaterElementRegistry;

/**
 * Owns the `UUserWidget`s that `<umg class>` elements host, so the garbage collector cannot take a widget while its
 * Slate widget is on screen. An entry is dropped once the Slate widget it was made for is gone (at the next
 * `CreateHosted` or `ReleaseUnused`).
 */
UCLASS()
class SLATERUIUMG_API USlaterUmgSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/**
	 * Makes a widget of Class in the game world (else the editor world, else the world set by `SetWorldOverride`),
	 * keeps it, and returns its Slate widget in OutSlate. Null with OutProblem filled when there is no world or the
	 * class cannot be created.
	 */
	UUserWidget* CreateHosted(TSubclassOf<UUserWidget> Class, TSharedPtr<SWidget>& OutSlate, FString& OutProblem);

	/** Drops the widgets whose Slate widget is gone. Returns how many are left. */
	int32 ReleaseUnused();

	int32 GetHostedCount() const { return Hosted.Num(); }

	/** A world to create widgets in before looking at the engine's world list. For tests and tools. Null clears it. */
	void SetWorldOverride(UWorld* World) { WorldOverride = World; }

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> Hosted;

	/** Same order as Hosted. */
	TArray<TWeakPtr<SWidget>> HostedSlate;

	TWeakObjectPtr<UWorld> WorldOverride;

	UWorld* FindWorld() const;
};

namespace Slater
{
	/**
	 * Adds `<umg class="/Script/Module.Class">` to a registry: a `UUserWidget` of that class, created and kept by
	 * `USlaterUmgSubsystem`. The module adds it to `Slater::Elements()` when it loads. A registry that never gets it
	 * (the one a mod layer builds with, T-395) refuses `<umg>` as an unknown element.
	 */
	SLATERUIUMG_API void RegisterUmgElements(FSlaterElementRegistry& Registry);
}
