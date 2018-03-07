// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateFileDialogs : ModuleRules
{
    public SlateFileDialogs(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "InputCore",
                "Slate",
                "SlateCore",
                "DirectoryWatcher",
            }
        );
    
        PrivateIncludePaths.AddRange(
            new string[] {
                "Developer/SlateFileDialogs/Private",  
            }
        );

        PrivateIncludePathModuleNames.Add("TargetPlatform");
    }
}
