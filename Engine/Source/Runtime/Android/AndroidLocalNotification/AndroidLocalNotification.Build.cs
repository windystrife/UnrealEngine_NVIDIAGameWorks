// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AndroidLocalNotification : ModuleRules
{
	public AndroidLocalNotification(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "Android";

        PublicIncludePaths.AddRange(new string[]
        {
            "Runtime/Android/AndroidLocalNotification/Public",
            "Runtime/Engine/Public",
            "Runtime/Launch/Android/Public",
            "/Runtime/Launch/Private"
        });


        PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
            "Launch"
		});
	}
}
