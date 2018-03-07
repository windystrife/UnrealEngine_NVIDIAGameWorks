// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class ControlRigEditor : ModuleRules
    {
        public ControlRigEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.Add("ControlRigEditor/Private");
            PrivateIncludePaths.Add("ControlRigEditor/Private/InputOutput");
            PrivateIncludePaths.Add("ControlRigEditor/Private/Sequencer");
            PrivateIncludePaths.Add("ControlRigEditor/Private/EditMode");

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Slate",
                    "SlateCore",
                    "InputCore",
                    "Engine",
                    "UnrealEd",
                    "KismetCompiler",
                    "BlueprintGraph",
                    "ControlRig",
                    "Kismet",
                    "EditorStyle",
                    "AnimationCore",
                    "PropertyEditor",
                    "AnimGraph",
                    "AnimGraphRuntime",
                    "MovieScene",
                    "MovieSceneTracks",
                    "MovieSceneTools",
                    "Sequencer",
                    "ClassViewer",
                    "AssetTools",
                    "ContentBrowser",
                    "LevelEditor",
                    "SceneOutliner",
                    "LevelSequence",
                }
            );
        }
    }
}
