// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UWorld;

/**
 * What `USlaterGameFeatureAction_AddLayout` asks of a game: put a layout in a named HUD slot of a world, and take it out
 * again. SlaterUI does not know how a game's HUD is built, so the game (or an adapter plugin such as SlaterUIExtension,
 * which does it over the UIExtension system) implements this and sets it with `Slater::SetSlotHost`. Game thread only.
 */
class ISlaterSlotHost
{
public:
	virtual ~ISlaterSlotHost() = default;

	/**
	 * Shows the layout in the slot of this world. Returns a token for `RemoveLayout`, or 0 when the host cannot (the action
	 * skips it). A slot nobody has built yet is the host's business: it may show the layout when the slot appears.
	 */
	virtual uint32 AddLayout(UWorld& World, FGameplayTag Slot, const FString& LayoutId, FName StyleName) = 0;

	/** Takes a layout out again. A token of a world that is gone is fine. */
	virtual void RemoveLayout(UWorld& World, uint32 Token) = 0;
};

namespace Slater
{
	/** The game's slot host; held weakly, so whoever creates it keeps it. Null clears it. */
	SLATERUIGAMEFEATURES_API void SetSlotHost(const TSharedPtr<ISlaterSlotHost>& Host);
	SLATERUIGAMEFEATURES_API TSharedPtr<ISlaterSlotHost> GetSlotHost();
}
