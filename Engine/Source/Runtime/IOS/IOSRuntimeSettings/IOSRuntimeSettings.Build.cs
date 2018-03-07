// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IOSRuntimeSettings : ModuleRules
{
	public IOSRuntimeSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "IOS";

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject"
			}
		);
	}
}
