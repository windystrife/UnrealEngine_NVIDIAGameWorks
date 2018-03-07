// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealLaunchDaemon : ModuleRules
{
	public UnrealLaunchDaemon( ReadOnlyTargetRules Target ) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/Launch/Public"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"NetworkFile",
				"Projects",
				"StreamingFile",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"StandaloneRenderer",
				"LaunchDaemonMessages",
				"Projects",
				"Messaging",
				"UdpMessaging"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Messaging",
			}
		);

		PrivateIncludePaths.Add("Runtime/Launch/Private");
	}
}
