// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkInterface : ModuleRules
{
    public LiveLinkInterface(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject"
            }
        );

        PrivateIncludePaths.Add("LiveLinkInterface/Private");
	}
}
