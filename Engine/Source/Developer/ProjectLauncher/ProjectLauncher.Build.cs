// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectLauncher : ModuleRules
{
	public ProjectLauncher(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"TargetPlatform",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"LauncherServices",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
				"ApplicationCore",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "WorkspaceMenuStructure",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"SessionServices",
				"TargetDeviceServices",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/ProjectLauncher/Private",
				"Developer/ProjectLauncher/Private/Models",
				"Developer/ProjectLauncher/Private/Widgets",
                "Developer/ProjectLauncher/Private/Widgets/Archive",
                "Developer/ProjectLauncher/Private/Widgets/Build",
				"Developer/ProjectLauncher/Private/Widgets/Cook",
				"Developer/ProjectLauncher/Private/Widgets/Deploy",
				"Developer/ProjectLauncher/Private/Widgets/Launch",
				"Developer/ProjectLauncher/Private/Widgets/Package",
                "Developer/ProjectLauncher/Private/Widgets/Profile",
                "Developer/ProjectLauncher/Private/Widgets/Progress",
                "Developer/ProjectLauncher/Private/Widgets/Project",
                "Developer/ProjectLauncher/Private/Widgets/Settings",
                "Developer/ProjectLauncher/Private/Widgets/Shared",
				"Developer/ProjectLauncher/Private/Widgets/Toolbar",
            }
		);
	}
}
