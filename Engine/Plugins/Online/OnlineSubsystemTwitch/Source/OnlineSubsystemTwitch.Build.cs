// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemTwitch : ModuleRules
{
	public OnlineSubsystemTwitch(ReadOnlyTargetRules Target) : base(Target)
	{
		Definitions.Add("ONLINESUBSYSTEM_TWITCH_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseSharedPCHs;

		PrivateIncludePaths.Add("Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Sockets",
				"HTTP",
				"Json",
				"OnlineSubsystem"
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Engine",
					"OnlineSubsystemUtils"
				}
			);
		}
	}
}
