// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Launch : ModuleRules
{
	public Launch(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/Launch/Private");

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AutomationController",
				"Media",
                "MediaUtils",
				"TaskGraph",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"MoviePlayer",
				"Networking",
				"PakFile",
				"Projects",
				"RenderCore",
				"RHI",
				"SandboxFile",
				"Serialization",
				"ShaderCore",
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"Sockets",
                "Overlay",
				"UtilityShaders",
			}
		);

		// Enable the LauncherCheck module to be used for platforms that support the Launcher.
		// Projects should set Target.bUseLauncherChecks in their Target.cs to enable the functionality.
		if (Target.bUseLauncherChecks &&
			((Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Mac)))
		{
			PrivateDependencyModuleNames.Add("LauncherCheck");
			Definitions.Add("WITH_LAUNCHERCHECK=1");
		}
		else
		{
			Definitions.Add("WITH_LAUNCHERCHECK=0");
		}

		if (Target.Type != TargetType.Server)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"HeadMountedDisplay",
					"MRMesh",
				}
			);

			if ((Target.Platform == UnrealTargetPlatform.Win32) ||
				(Target.Platform == UnrealTargetPlatform.Win64))
			{
				DynamicallyLoadedModuleNames.Add("D3D12RHI");
				DynamicallyLoadedModuleNames.Add("D3D11RHI");
				DynamicallyLoadedModuleNames.Add("XAudio2");
				DynamicallyLoadedModuleNames.Add("AudioMixerXAudio2");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				DynamicallyLoadedModuleNames.Add("CoreAudio");
				DynamicallyLoadedModuleNames.Add("AudioMixerAudioUnit");
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				DynamicallyLoadedModuleNames.Add("ALAudio");
				DynamicallyLoadedModuleNames.Add("AudioMixerSDL");
				PrivateDependencyModuleNames.Add("Json");
			}

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateNullRenderer",
					"SlateRHIRenderer"
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SlateNullRenderer",
					"SlateRHIRenderer"
				}
			);
		}

		// UFS clients are not available in shipping builds
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"NetworkFile",
					"StreamingFile",
					"CookedIterativeFile",
					"AutomationWorker",
				}
			);
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"Media",
				"Renderer",
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"MessagingCommon",
					"MediaAssets"
				}
			);

			PublicDependencyModuleNames.Add("SessionServices");
			PrivateIncludePaths.Add("Developer/DerivedDataCache/Public");

			// LaunchEngineLoop.cpp will still attempt to load XMPP but not all projects require it so it will silently fail unless referenced by the project's build.cs file.
			// DynamicallyLoadedModuleNames.Add("XMPP");
			DynamicallyLoadedModuleNames.Add("HTTP");

			PrivateDependencyModuleNames.Add("ClothingSystemRuntimeInterface");
			PrivateDependencyModuleNames.Add("ClothingSystemRuntime");
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PublicIncludePathModuleNames.Add("ProfilerService");
			DynamicallyLoadedModuleNames.AddRange(new string[] { "TaskGraph", "RealtimeProfiler", "ProfilerService" });
		}

		// The engine can use AutomationController in any connfiguration besides shipping.  This module is loaded
		// dynamically in LaunchEngineLoop.cpp in non-shipping configurations
		if (Target.bCompileAgainstEngine && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			DynamicallyLoadedModuleNames.AddRange(new string[] { "AutomationController" });
		}

		if (Target.bBuildEditor == true)
		{
			PublicIncludePathModuleNames.Add("ProfilerClient");

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SourceControl",
					"UnrealEd",
					"DesktopPlatform",
					"PIEPreviewDeviceProfileSelector",
				}
			);


			// ExtraModules that are loaded when WITH_EDITOR=1 is true
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"AutomationWindow",
					"ProfilerClient",
					"Toolbox",
					"GammaUI",
					"ModuleUI",
					"OutputLog",
					"TextureCompressor",
					"MeshUtilities",
					"SourceCodeAccess"
				}
			);

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PrivateDependencyModuleNames.Add("MainFrame");
				PrivateDependencyModuleNames.Add("Settings");
			}
			else
			{
				DynamicallyLoadedModuleNames.Add("MainFrame");
			}
		}

		if (Target.Platform == UnrealTargetPlatform.IOS ||
			Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PrivateDependencyModuleNames.Add("OpenGLDrv");
			PrivateDependencyModuleNames.Add("IOSAudio");
			PrivateDependencyModuleNames.Add("AudioMixerAudioUnit");
			DynamicallyLoadedModuleNames.Add("IOSRuntimeSettings");
			DynamicallyLoadedModuleNames.Add("IOSLocalNotification");
			PublicFrameworks.Add("OpenGLES");
			// this is weak for IOS8 support for CAMetalLayer that is in QuartzCore
			PublicWeakFrameworks.Add("QuartzCore");

			PrivateDependencyModuleNames.Add("LaunchDaemonMessages");
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("OpenGLDrv");
			PrivateDependencyModuleNames.Add("AndroidAudio");
			PrivateDependencyModuleNames.Add("AudioMixerAndroid");
			DynamicallyLoadedModuleNames.Add("AndroidRuntimeSettings");
			DynamicallyLoadedModuleNames.Add("AndroidLocalNotification");
		}

		if ((Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Linux && Target.Type != TargetType.Server))
		{
			// TODO: re-enable after implementing resource tables for OpenGL.
			DynamicallyLoadedModuleNames.Add("OpenGLDrv");
		}

		if (Target.Platform == UnrealTargetPlatform.HTML5 )
		{
			PrivateDependencyModuleNames.Add("ALAudio");
            PrivateDependencyModuleNames.Add("AudioMixerSDL");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}

		// @todo ps4 clang bug: this works around a PS4/clang compiler bug (optimizations)
		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			bFasterWithoutUnity = true;
		}

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"LinuxCommonStartup"
				}
			);
		}
	}
}
