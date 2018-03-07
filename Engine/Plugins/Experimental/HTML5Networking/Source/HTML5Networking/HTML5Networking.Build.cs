// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class HTML5Networking  : ModuleRules
	{
		public HTML5Networking(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[] { 
                    "Core", 
                    "CoreUObject",
                    "Engine",
					"EngineSettings",
                    "ImageCore",
                    "Sockets",
					"PacketHandler",
                    "OpenSSL",
                    "libWebSockets",
                    "zlib"
                }
            );
		}
	}
}
