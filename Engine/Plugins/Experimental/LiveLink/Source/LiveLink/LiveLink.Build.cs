// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class LiveLink : ModuleRules
    {
        public LiveLink(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Projects",

                "Messaging",
                "LiveLinkInterface",
                "LiveLinkMessageBusFramework",
            }
            );

            PrivateIncludePaths.Add("/LiveLink/Private");
        }
    }
}
