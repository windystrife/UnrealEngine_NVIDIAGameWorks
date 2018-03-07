// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StandaloneRenderer : ModuleRules
{
	public StandaloneRenderer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Developer/StandaloneRenderer/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"ImageWrapper",
				"InputCore",
				"SlateCore",
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			// @todo: This should be private? Not sure!!
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.Add("QuartzCore");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PublicFrameworks.AddRange(new string[] { "OpenGLES", "GLKit" });
			// weak for IOS8 support since CAMetalLayer is in QuartzCore
			PublicWeakFrameworks.AddRange(new string[] { "QuartzCore" });
		}

		RuntimeDependencies.Add("$(EngineDir)/Shaders/StandaloneRenderer/...", StagedFileType.NonUFS);
	}
}
