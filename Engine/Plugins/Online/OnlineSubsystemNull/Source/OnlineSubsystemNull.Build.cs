// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemNull : ModuleRules
{
	public OnlineSubsystemNull(ReadOnlyTargetRules Target) : base(Target)
    {
		Definitions.Add("ONLINESUBSYSTEMNULL_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"Engine", 
				"Sockets", 
				"OnlineSubsystem", 
				"OnlineSubsystemUtils",
				"Json"
			}
			);
	}
}
