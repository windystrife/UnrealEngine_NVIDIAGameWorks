// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DeviceManager : ModuleRules
{
	public DeviceManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"TargetDeviceServices",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"EditorStyle",
                "InputCore",
				"Slate",
				"SlateCore",
				"TargetPlatform",
				"DesktopPlatform",
                "WorkspaceMenuStructure",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/DeviceManager/Private",
				"Developer/DeviceManager/Private/Models",
				"Developer/DeviceManager/Private/Widgets",
				"Developer/DeviceManager/Private/Widgets/Apps",
				"Developer/DeviceManager/Private/Widgets/Browser",
				"Developer/DeviceManager/Private/Widgets/Details",
				"Developer/DeviceManager/Private/Widgets/Processes",
				"Developer/DeviceManager/Private/Widgets/Shared",
				"Developer/DeviceManager/Private/Widgets/Toolbar",
			}
		);
	}
}
