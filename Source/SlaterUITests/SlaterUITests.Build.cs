// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

// Automation tests for SlaterUI. UncookedOnly in the .uplugin: never linked into a staged game target.
public class SlaterUITests : ModuleRules
{
	public SlaterUITests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"FieldNotification",
				"InputCore",
				"Json",
				"Projects",
				"CQTest",
				"SlaterUI",
				"SlaterUIUMG",
				"SlaterUIGameFeatures",
				"GameFeatures",
				"GameplayTags",
				"Slate",
				"SlateCore",
					"UMG",
					"RHI",
					"RenderCore",
					"ImageWrapper",
			}
		);
	}
}
