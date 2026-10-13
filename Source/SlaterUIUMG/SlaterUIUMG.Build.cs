// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

// The UMG side of SlaterUI (the core module stays Slate-only): the `<umg class>` element and the subsystem that owns its widgets, and the layout widgets (`USlaterLayoutWidget`). Engine modules and SlaterUI only.
public class SlaterUIUMG : ModuleRules
{
	public SlaterUIUMG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"FieldNotification",
				"ModelViewViewModel",
				"Slate",
				"SlateCore",
				"SlaterUI",
				"UMG",
			}
		);
	}
}
