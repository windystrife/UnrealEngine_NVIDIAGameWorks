// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OneSkyLocalizationService : ModuleRules
{
    public OneSkyLocalizationService(ReadOnlyTargetRules Target) : base(Target)
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
                "LocalizationService",
                "Json",
                "HTTP",
                "Serialization",
				"Localization",
				"LocalizationCommandletExecution",
				"MainFrame",
			}
		);
	}
}
