// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NetworkReplayStreaming : ModuleRules
	{
		public NetworkReplayStreaming( ReadOnlyTargetRules Target ) : base(Target)
		{
			PrivateIncludePaths.Add( "Runtime/NetworkReplayStreaming/NetworkReplayStreaming/Private" );

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
                    "Json",
                    // ... add other public dependencies that you statically link with here ...
				}
			);
		}
	}
}
