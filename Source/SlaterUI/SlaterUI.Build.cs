// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

// Standalone on purpose: engine modules only, no other Rock plugin. Keep it that way (it duplicates small things from RockUI and RockData instead of depending on them).
public class SlaterUI : ModuleRules
{
	public SlaterUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
					"CoreUObject",
				"FieldNotification",
				"InputCore",
				"Json",
				"Projects",
				"Slate",
				"SlateCore",
			}
		);
	}
}
