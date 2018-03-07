// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Profiler : ModuleRules
{
	public Profiler( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateIncludePaths.AddRange
		(
			new string[] {
				"Developer/Profiler/Private",
				"Developer/Profiler/Private/Widgets",
			}
		);

		PublicDependencyModuleNames.AddRange
		(
			new string[] {
				"Core",
				"ApplicationCore",
                "InputCore",
				"RHI",
				"RenderCore",
				"Slate",
                "EditorStyle",
				"ProfilerClient",
				"DesktopPlatform",
			}
		);

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
					"Engine",
				}
            );
        }

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SlateCore",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Messaging",
				"SessionServices",
			}
		);
	}
}
