// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiveLinkMessageBusFramework : ModuleRules
{
    public LiveLinkMessageBusFramework(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateDependencyModuleNames.Add("MessagingCommon");
        PrivateIncludePathModuleNames.Add("Messaging");
		
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
				"CoreUObject",
                "LiveLinkInterface"
            }
        );

        PrivateIncludePaths.Add("LiveLinkMessageBusFramework/Private");
	}
}
