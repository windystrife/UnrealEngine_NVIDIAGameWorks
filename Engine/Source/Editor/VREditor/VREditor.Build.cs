// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class VREditor : ModuleRules
	{
        public VREditor(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "AppFramework",
				    "Core",
				    "CoreUObject",
					"ApplicationCore",
				    "Engine",
                    "InputCore",
				    "Slate",
					"SlateCore",
                    "EditorStyle",
				    "UnrealEd",
					"UMG",
					"LevelEditor",
					"HeadMountedDisplay",
					"Analytics",
                    "LevelSequence",
                    "Sequencer",
                    "Projects"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"ViewportInteraction"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"PlacementMode"
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
                    "PlacementMode"
                }
			);

            PrivateIncludePaths.AddRange(
                new string[] {
                    "Editor/VREditor/Gizmo",
                    "Editor/VREditor/UI",
                    "Editor/VREditor/Teleporter",
                    "Editor/VREditor/Interactables"
                }
            );

		}
	}
}