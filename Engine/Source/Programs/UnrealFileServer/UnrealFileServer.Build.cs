// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealFileServer : ModuleRules
{
	public UnrealFileServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Runtime/Launch/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
				"ApplicationCore",
                "DirectoryWatcher",
                "NetworkFileSystem",
                "SandboxFile",
				"Sockets",
				"Projects",
           }
		);
	}
}
