// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

// Editor-only side of SlaterUI: the folder watcher behind live reload and the layout preview tab. Engine modules and SlaterUI only.
public class SlaterUIEditor : ModuleRules
{
	public SlaterUIEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DirectoryWatcher",
				"InputCore",
				"Projects",
				"Slate",
				"SlateCore",
				"SlaterUI",
				"WorkspaceMenuStructure",
			}
		);
	}
}
