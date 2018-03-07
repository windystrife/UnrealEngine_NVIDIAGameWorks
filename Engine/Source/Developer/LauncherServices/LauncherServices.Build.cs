// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LauncherServices : ModuleRules
	{
		public LauncherServices(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
				});

			PublicIncludePathModuleNames.AddRange(
				new string[] {
					"TargetDeviceServices",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"DesktopPlatform",
					"SessionMessages",
					"SourceCodeAccess",
					"TargetPlatform",
					"UnrealEdMessages",
					"JSON",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"DesktopPlatform",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/LauncherServices/Private",
					"Developer/LauncherServices/Private/Devices",
					"Developer/LauncherServices/Private/Games",
					"Developer/LauncherServices/Private/Launcher",
					"Developer/LauncherServices/Private/Profiles",
				});
		}
	}
}
