// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class MobilePatchingUtils : ModuleRules
    {
        public MobilePatchingUtils(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.Add("MobilePatchingUtils/Private");

            PublicDependencyModuleNames.AddRange(new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "PakFile",
                "HTTP",
                "BuildPatchServices"
            });
        }
    }
}
