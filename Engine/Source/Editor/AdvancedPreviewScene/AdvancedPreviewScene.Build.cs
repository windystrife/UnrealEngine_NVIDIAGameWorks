// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AdvancedPreviewScene : ModuleRules
{
    public AdvancedPreviewScene(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePathModuleNames.AddRange(
            new string[] {
                "PropertyEditor",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "UnrealEd",
            }
        );
    }
}
