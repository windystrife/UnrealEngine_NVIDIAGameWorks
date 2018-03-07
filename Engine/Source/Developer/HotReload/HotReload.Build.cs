// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class HotReload : ModuleRules
{
	public HotReload(ReadOnlyTargetRules Target) : base(Target)
	{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"DirectoryWatcher",
					"DesktopPlatform",
					"Projects"
				}
			);

            if (Target.bCompileAgainstEngine)
            {
                PrivateDependencyModuleNames.AddRange(
				    new string[] 
				    {
					    "Engine",
					    "UnrealEd", 
				    }
			    );
            }
	}
}
