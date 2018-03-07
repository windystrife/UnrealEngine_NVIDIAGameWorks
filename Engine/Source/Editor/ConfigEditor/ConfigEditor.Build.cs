// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ConfigEditor : ModuleRules
{
	public ConfigEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/ConfigEditor/Private",
				"Editor/ConfigEditor/Private/PropertyVisualization",
			}
		);
		
		PublicIncludePaths.AddRange(
			new string[] {
				"Editor/ConfigEditor/Public",
				"Editor/ConfigEditor/Public/PropertyVisualization",
			}
		);
	
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorStyle",
				"InputCore",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"SourceControl",
				"TargetPlatform",
				"WorkspaceMenuStructure",
			}
		);


		CircularlyReferencedDependentModules.AddRange(
			new string[] 
			{
				"PropertyEditor",
			}
		); 


		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
			}
		);
	}
}
