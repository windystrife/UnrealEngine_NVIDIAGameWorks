// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class HttpNetworkReplayStreaming : ModuleRules
    {
        public HttpNetworkReplayStreaming(ReadOnlyTargetRules Target) : base(Target)
        {
			PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "HTTP",
					"NetworkReplayStreaming",
					"Json",
				} );

            if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac)
            {
                AddEngineThirdPartyPrivateStaticDependencies(Target,"libWebSockets");
                AddEngineThirdPartyPrivateStaticDependencies(Target,"zlib");
            }
        }
    }
}
