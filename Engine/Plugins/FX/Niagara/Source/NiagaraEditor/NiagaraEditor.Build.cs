// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraEditor : ModuleRules
{
	public NiagaraEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[] {
			"NiagaraEditor/Private",
			"NiagaraEditor/Private/Toolkits",
			"NiagaraEditor/Private/Widgets",
			"NiagaraEditor/Private/Sequencer/NiagaraSequence",
			"NiagaraEditor/Private/Sequencer/LevelSequence",
			"NiagaraEditor/Private/ViewModels",
			"NiagaraEditor/Private/TypeEditorUtilities"
		});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
                "RHI",
                "Core", 
				"CoreUObject", 
				"ApplicationCore",
                "InputCore",
				"RenderCore",
				"Slate", 
				"SlateCore",
				"Kismet",
                "EditorStyle",
				"UnrealEd", 
				"VectorVM",
                "Niagara",
                "NiagaraShader",
                "MovieScene",
				"Sequencer",
				"PropertyEditor",
				"GraphEditor",
                "ShaderFormatVectorVM",
                "TargetPlatform",
                "AppFramework",
				"MovieSceneTools",
                "MovieSceneTracks",
                "AdvancedPreviewScene",
            }
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Engine",
				"Messaging",
				"LevelEditor",
				"AssetTools",
				"ContentBrowser",
                "DerivedDataCache",
                "ShaderCore",
            }
        );

		PublicDependencyModuleNames.AddRange(
            new string[] {
                "NiagaraShader",
                "Engine",
                "Niagara",
                "UnrealEd",
            }
        );

        PublicIncludePathModuleNames.AddRange(
            new string[] {
				"Engine",
				"Niagara"
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "WorkspaceMenuStructure",
                }
            );
	}
}
