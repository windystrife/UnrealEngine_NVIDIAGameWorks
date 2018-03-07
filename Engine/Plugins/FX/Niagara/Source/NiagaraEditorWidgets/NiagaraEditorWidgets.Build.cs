// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraEditorWidgets : ModuleRules
{
	public NiagaraEditorWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[] {
			"NiagaraEditorWidgets/Private",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Niagara",
			"NiagaraEditor",
			"Engine",
			"Core",
			"CoreUObject",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"GraphEditor",
			"EditorStyle",
			"PropertyEditor",
			"AppFramework",
		});

		PrivateIncludePathModuleNames.AddRange(new string[] {
		});

		PublicDependencyModuleNames.AddRange(new string[] {
		});

		PublicIncludePathModuleNames.AddRange(new string[] {
		});
	}
}
