// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DeviceProfileEditor : ModuleRules
{
	public DeviceProfileEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Editor/UnrealEd/Public"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/DeviceProfileEditor/Private",
				"Editor/DeviceProfileEditor/Private/DetailsPanel"
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
				"LevelEditor",
				"UnrealEd",
				"WorkspaceMenuStructure",
				"PropertyEditor",
				"SourceControl",
                "TargetPlatform",
				"DesktopPlatform",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
			}
		);
	}
}
