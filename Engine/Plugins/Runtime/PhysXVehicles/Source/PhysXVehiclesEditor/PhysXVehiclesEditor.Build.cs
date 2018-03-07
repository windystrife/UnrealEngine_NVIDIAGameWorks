// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PhysXVehiclesEditor : ModuleRules
	{
        public PhysXVehiclesEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("PhysXVehiclesEditor/Private");
            PublicIncludePaths.Add("PhysXVehiclesEditor/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Slate",
                    "SlateCore",
                    "Engine",
                    "UnrealEd",
                    "PropertyEditor",
                    "AnimGraphRuntime",
                    "AnimGraph",
                    "BlueprintGraph",
                    "PhysXVehicles"
                }
			);

            PrivateDependencyModuleNames.AddRange(
                new string[] 
                {
                    "EditorStyle",
                    "AssetRegistry"
                }
            );
        }
	}
}
