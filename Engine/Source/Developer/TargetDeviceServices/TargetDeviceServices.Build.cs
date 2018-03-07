// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class TargetDeviceServices : ModuleRules
	{
		public TargetDeviceServices(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"TargetPlatform",
					"DesktopPlatform",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"MessagingCommon",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/TargetDeviceServices/Private",
					"Developer/TargetDeviceServices/Private/Proxies",
					"Developer/TargetDeviceServices/Private/Services",
				});
		}
	}
}
