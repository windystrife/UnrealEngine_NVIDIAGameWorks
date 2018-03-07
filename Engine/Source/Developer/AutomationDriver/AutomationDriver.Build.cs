// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AutomationDriver : ModuleRules
{
    public AutomationDriver(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePaths.AddRange(
            new string[] {
				"Developer/AutomationDriver/Public",
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                "Developer/AutomationDriver/Private",
                "Developer/AutomationDriver/Private/Locators",
                "Developer/AutomationDriver/Private/Specs",
                "Developer/AutomationDriver/Private/MetaData",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
				"ApplicationCore",
                "InputCore",
                "Json",
                "Slate",
                "SlateCore",
            }
        );
    }
}
