// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class libWebSockets : ModuleRules
{
	public libWebSockets(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		string WebsocketPath = Path.Combine(Target.UEThirdPartySourceDirectory, "libWebSockets", "libwebsockets");
		string PlatformSubdir = Target.Platform.ToString();

		switch (Target.Platform)
		{
		case UnrealTargetPlatform.HTML5:
			return;

		case UnrealTargetPlatform.Win64:
		case UnrealTargetPlatform.Win32:
			PlatformSubdir = Path.Combine(PlatformSubdir, Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("websockets_static_d.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("websockets_static.lib");
			}
			break;

		case UnrealTargetPlatform.Mac:
		case UnrealTargetPlatform.PS4:
			PublicAdditionalLibraries.Add(Path.Combine(WebsocketPath, "lib", Target.Platform.ToString(), "libwebsockets.a"));
			break;

		case UnrealTargetPlatform.Linux:
			PlatformSubdir += "/" + Target.Architecture;

			PublicAdditionalLibraries.Add(Path.Combine(WebsocketPath, "lib", PlatformSubdir, "libwebsockets.a"));
			break;

		default:
			return;
		}

		PublicLibraryPaths.Add(Path.Combine(WebsocketPath, "lib", PlatformSubdir));
		PublicIncludePaths.Add(Path.Combine(WebsocketPath, "include", PlatformSubdir));

		PublicDependencyModuleNames.Add("OpenSSL");
	}
}
