// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StringTableEditor : ModuleRules
{
    public StringTableEditor(ReadOnlyTargetRules Target)
         : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "DesktopPlatform",
                "UnrealEd",
            });

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetTools"
            });

        DynamicallyLoadedModuleNames.Add("WorkspaceMenuStructure");
    }
}
