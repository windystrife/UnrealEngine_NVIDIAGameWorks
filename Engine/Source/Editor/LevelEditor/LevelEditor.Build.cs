// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelEditor : ModuleRules
{
	public LevelEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"ClassViewer",
				"MainFrame",
                "PlacementMode",
				"ReferenceViewer",
				"SizeMap",
				"SlateReflector",
                "IntroTutorials",
                "AppFramework",
                "PortalServices"
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"IntroTutorials",
				"HeadMountedDisplay",
				"VREditor"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"LevelSequence",
				"Analytics",
				"Core",
				"CoreUObject",
				"LauncherPlatform",
				"InputCore",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"Engine",
				"MessageLog",
				"SourceControl",
				"SourceControlWindows",
				"StatsViewer",
				"UnrealEd", 
				"RenderCore",
				"DeviceProfileServices",
				"ContentBrowser",
				"SceneOutliner",
				"ActorPickerMode",
				"RHI",
				"Projects",
				"TargetPlatform",
				"EngineSettings",
				"PropertyEditor",
				"Persona",
				"Kismet",
				"KismetWidgets",
				"Sequencer",
				"Foliage",
				"HierarchicalLODOutliner",
				"HierarchicalLODUtilities",
				"MaterialShaderQualitySettings",
				"PixelInspectorModule",
				"FunctionalTesting"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"ClassViewer",
				"DeviceManager",
				"SettingsEditor",
				"SessionFrontend",
				"SlateReflector",
				"AutomationWindow",
				"Layers",
                "WorldBrowser",
				"EditorWidgets",
				"AssetTools",
				"WorkspaceMenuStructure",
				"NewLevelDialog",
				"DeviceProfileEditor",
                "PlacementMode",
				"ReferenceViewer",
				"SizeMap",
                "IntroTutorials",
				"HeadMountedDisplay",
				"VREditor"
			}
		);
	}
}
