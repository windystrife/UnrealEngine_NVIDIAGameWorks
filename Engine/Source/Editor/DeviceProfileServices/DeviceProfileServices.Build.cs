// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DeviceProfileServices : ModuleRules
{

	public DeviceProfileServices(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Editor/DeviceProfileServices/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
				"UnrealEd",
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetDeviceServices",
			});
	}
}

