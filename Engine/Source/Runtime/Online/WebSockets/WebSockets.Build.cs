// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
		
public class WebSockets : ModuleRules
{
  public WebSockets(ReadOnlyTargetRules Target) : base(Target)
	{
			Definitions.Add("WEBSOCKETS_PACKAGE=1");

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
				}
			);

			bool bPlatformSupportsLibWebsockets =
					Target.Platform == UnrealTargetPlatform.Win32 ||
					Target.Platform == UnrealTargetPlatform.Win64 ||
					Target.Platform == UnrealTargetPlatform.Mac ||
					Target.Platform == UnrealTargetPlatform.Linux ||
					Target.Platform == UnrealTargetPlatform.PS4;

			bool bPlatformSupportsXboxWebsockets = Target.Platform == UnrealTargetPlatform.XboxOne;

			bool bShouldUseModule = 
					bPlatformSupportsLibWebsockets || 
					bPlatformSupportsXboxWebsockets;

		if (bShouldUseModule)
		{
			Definitions.Add("WITH_WEBSOCKETS=1");

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Online/WebSockets/Private",
				}
			);

			if (bPlatformSupportsLibWebsockets)
			{
				Definitions.Add("WITH_LIBWEBSOCKETS=1");
 				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL", "libWebSockets", "zlib");
				PrivateDependencyModuleNames.Add("SSL");
			}
		}
		else
		{
			Definitions.Add("WITH_WEBSOCKETS=0");
		}
	}
}
