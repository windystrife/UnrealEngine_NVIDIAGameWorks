// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NetworkFileSystem : ModuleRules
	{
		public NetworkFileSystem(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/NetworkFileSystem/Private",
					"Runtime/NetworkFileSystem/Private/Simple",
					"Runtime/NetworkFileSystem/Private/Streaming",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Projects",
					"SandboxFile",
					"TargetPlatform",
				});

			PublicIncludePaths.AddRange(
				new string[] {
					"Runtime/NetworkFileSystem/Public",
					"Runtime/NetworkFileSystem/Public/Interfaces",
					"Runtime/CoreUObject/Public/Interfaces",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Sockets",
				});

			if ((Target.Platform == UnrealTargetPlatform.Win64) ||
				(Target.Platform == UnrealTargetPlatform.Mac))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL", "libWebSockets", "zlib");
				Definitions.Add("ENABLE_HTTP_FOR_NFS=1");
			}
			else
			{
				Definitions.Add("ENABLE_HTTP_FOR_NFS=0");
			}

			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
