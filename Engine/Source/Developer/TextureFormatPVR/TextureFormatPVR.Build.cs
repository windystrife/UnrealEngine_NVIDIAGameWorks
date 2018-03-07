// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatPVR : ModuleRules
{
	public TextureFormatPVR( ReadOnlyTargetRules Target ) : base(Target)
	{
        PrivateIncludePathModuleNames.AddRange(new string[] { "TargetPlatform", "TextureCompressor", "Engine" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "ImageCore", "ImageWrapper" });

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/ImgTec/PVRTexToolCLI.exe"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac /*|| Target.Platform == UnrealTargetPlatform.Linux*/)
		{
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/ImgTec/PVRTexToolCLI"));
		}

		//if (Target.bCompileLeanAndMeanUE == false)
		//{
		//	PrivateDependencyModuleNames.Add("nvTextureTools");
		//}
	}
}
