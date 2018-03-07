// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenGLDrv : ModuleRules
{
	public OpenGLDrv(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/OpenGLDrv/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Engine",
				"RHI",
				"RenderCore",
				"ShaderCore",
				"UtilityShaders",
			}
			);

		PrivateIncludePathModuleNames.Add("ImageWrapper");
		DynamicallyLoadedModuleNames.Add("ImageWrapper");

        if (Target.Platform != UnrealTargetPlatform.HTML5)
		{
		    AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
		}

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string GLPath = Target.UEThirdPartySourceDirectory + "OpenGL/";
			PublicIncludePaths.Add(GLPath);
		}

		if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.HTML5)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"TaskGraph"
                }
			);
		}

		if(Target.Platform != UnrealTargetPlatform.Win32 && Target.Platform != UnrealTargetPlatform.Win64 &&
			Target.Platform != UnrealTargetPlatform.IOS && Target.Platform != UnrealTargetPlatform.Android
			&& Target.Platform != UnrealTargetPlatform.HTML5 && Target.Platform != UnrealTargetPlatform.Linux)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
