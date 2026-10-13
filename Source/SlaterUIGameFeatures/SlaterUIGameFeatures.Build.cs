// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

// The game feature side of SlaterUI: the "Add Slater Layout" action and `ISlaterSlotHost`, the seam a game fills in to say
// what a HUD slot is. Engine modules only; the UIExtension adapter is the separate SlaterUIExtension plugin.
public class SlaterUIGameFeatures : ModuleRules
{
	public SlaterUIGameFeatures(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameFeatures",
				"GameplayTags",
				"SlaterUI",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"Slate",
				"SlateCore",
			}
		);
	}
}
