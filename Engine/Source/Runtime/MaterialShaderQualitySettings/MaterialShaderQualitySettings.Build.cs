// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MaterialShaderQualitySettings : ModuleRules
{
    public MaterialShaderQualitySettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "RHI",
			}
		);
        PrivateIncludePathModuleNames.Add("Engine");

        if (Target.bBuildEditor == true)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[] {            
       				"Slate",
				    "SlateCore",
				    "EditorStyle",
                    "PropertyEditor",
                    "TargetPlatform",
                    "InputCore",
                }
            );

		}

	}
}
