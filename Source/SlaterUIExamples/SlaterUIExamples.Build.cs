// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

// Worked examples of binding C++ to .sltr layouts. DeveloperTool in the .uplugin: loads in the editor and development builds, never in a shipping target.
public class SlaterUIExamples : ModuleRules
{
	public SlaterUIExamples(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"SlaterUI",
				"SlaterUIUMG",
				"UMG",
			}
		);
	}
}
