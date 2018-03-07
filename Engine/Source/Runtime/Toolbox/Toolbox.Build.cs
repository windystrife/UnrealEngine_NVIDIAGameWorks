// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Toolbox : ModuleRules
{
	public Toolbox(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"GammaUI",
				"MainFrame",
				"ModuleUI"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"DesktopPlatform",
				"AppFramework"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"GammaUI", 
				"MainFrame",
				"ModuleUI"
			}
		);

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);
		}

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
