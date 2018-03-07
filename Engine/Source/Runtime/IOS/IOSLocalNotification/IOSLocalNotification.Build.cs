// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IOSLocalNotification : ModuleRules
{
	public IOSLocalNotification(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "IOS";

		PublicIncludePaths.AddRange(new string[]
		{
			"Runtime/IOS/IOSLocalNotification/Public",
			"Runtime/Engine/Public",
		});
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});
	}
}
