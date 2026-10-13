// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "GameplayTagContainer.h"
#include "SlaterGameFeatureAction.generated.h"

class UWorld;

/** One layout to show in one HUD slot. */
USTRUCT()
struct FSlaterLayoutSlotEntry
{
	GENERATED_BODY()

	/** The layout ID (`MyPack/Hud`): a file `Content/UI/Layouts/MyPack/Hud.sltr` in this plugin, the project or SlaterUI. */
	UPROPERTY(EditAnywhere, Category = "Slater")
	FString LayoutId;

	/** The style to show it in. */
	UPROPERTY(EditAnywhere, Category = "Slater")
	FName StyleName = TEXT("Base");

	/** The HUD slot to put it in. What a slot is, is up to the game's `ISlaterSlotHost` (SlaterUIExtension: a UI extension point with this tag). */
	UPROPERTY(EditAnywhere, Category = "Slater")
	FGameplayTag SlotID;
};

/**
 * Shows layouts in HUD slots while a game feature is active (T-352). The layout and style files of the feature's plugin
 * (`Content/UI/Layouts`, `Content/UI/Styles`) become searchable on activation and are removed again on deactivation, so a
 * plugin with no code and no Blueprints can ship a panel: put the files in, add this action to the feature's data asset.
 * Layout IDs are shared by every root, so give a pack its own folder (`MyPack/Hud`).
 *
 * The slots are the game's: the action hands each entry to the `ISlaterSlotHost` set with `Slater::SetSlotHost` (SlaterUI
 * has none of its own; the SlaterUIExtension plugin is one over UIExtension).
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Slater Layout"))
class USlaterGameFeatureAction_AddLayout : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~ UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	//~ UObject interface
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Puts the layouts into the slots of every game world now and of each game world that starts later. Idempotent. */
	SLATERUIGAMEFEATURES_API void Activate();
	/** Takes them out of every world again and forgets the plugin's folders. */
	SLATERUIGAMEFEATURES_API void Deactivate();
	SLATERUIGAMEFEATURES_API bool IsActive() const { return bActive; }

	/** Hands the layouts to the slot host for one world. Does nothing for a world already done or a dedicated server. */
	SLATERUIGAMEFEATURES_API void AddToWorld(UWorld& World);

	/** How many layouts the host has accepted, over all worlds. */
	SLATERUIGAMEFEATURES_API int32 GetRegisteredCount() const;

	/** Sets the entries from code (tests, tools). */
	void SetLayouts(const TArray<FSlaterLayoutSlotEntry>& InLayouts) { Layouts = InLayouts; }

private:
	UPROPERTY(EditAnywhere, Category = "Slater", meta = (TitleProperty = "{SlotID} -> {LayoutId}"))
	TArray<FSlaterLayoutSlotEntry> Layouts;

	struct FWorldData
	{
		TWeakObjectPtr<UWorld> World;
		TArray<uint32> Tokens;
	};

	TArray<FWorldData> Worlds;

	TArray<FString> AddedLayoutRoots;
	TArray<FString> AddedStyleRoots;
	FDelegateHandle WorldInitHandle;
	FDelegateHandle WorldCleanupHandle;
	bool bActive = false;

	void RemoveWorld(int32 Index);
	void AddPluginRoots();
	void RemovePluginRoots();
};
