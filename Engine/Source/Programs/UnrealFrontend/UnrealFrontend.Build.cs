// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealFrontend : ModuleRules
{
	public UnrealFrontend( ReadOnlyTargetRules Target ) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Programs/UnrealFrontend/Private",
				"Programs/UnrealFrontend/Private/Commands",
				"Runtime/Launch/Private",					// for LaunchEngineLoop.cpp include
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AutomationController",
				"Core",
				"ApplicationCore",
				"CoreUObject",
				"DeviceManager",
				"LauncherServices",
				"Messaging",
				"Profiler",
				"ProfilerClient",
                "ProjectLauncher",
				"Projects",
				"SessionFrontend",
				"SessionServices",
				"Slate",
				"SlateCore",
				"SourceCodeAccess",
				"StandaloneRenderer",
				"TargetDeviceServices",
				"TargetPlatform",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"SlateReflector",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"SlateReflector",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PrivateDependencyModuleNames.Add("XCodeSourceCodeAccess");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("VisualStudioSourceCodeAccess");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"LinuxCommonStartup"
				}
			);
		}

		// @todo: allow for better plug-in support in standalone Slate apps
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Networking",
				"Sockets",
				"UdpMessaging",
                "TcpMessaging",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Messaging",
			}
		);
	}
}
