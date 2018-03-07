// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TargetPlatform : ModuleRules
{
	public TargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PublicDependencyModuleNames.Add("DesktopPlatform");
		PublicDependencyModuleNames.Add("LauncherPlatform");

		PrivateIncludePathModuleNames.Add("Engine");

		// no need for all these modules if the program doesn't want developer tools at all (like UnrealFileServer)
		if (!Target.bBuildRequiresCookedData && Target.bBuildDeveloperTools)
		{
			// these are needed by multiple platform specific target platforms, so we make sure they are built with the base editor
			DynamicallyLoadedModuleNames.Add("ShaderPreprocessor");
			DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
			DynamicallyLoadedModuleNames.Add("ImageWrapper");

			if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
			{
				if (Target.bCompileLeanAndMeanUE == false)
				{
					DynamicallyLoadedModuleNames.Add("TextureFormatIntelISPCTexComp");
				}
			}

			if (Target.Platform == UnrealTargetPlatform.Win32 ||
				Target.Platform == UnrealTargetPlatform.Win64)
			{

				// these are needed by multiple platform specific target platforms, so we make sure they are built with the base editor
				DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
				DynamicallyLoadedModuleNames.Add("MetalShaderFormat");

				if (Target.bCompileLeanAndMeanUE == false)
				{
					DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
					DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
					DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
				}

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (Target.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatADPCM"); // For IOS cooking
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
				}

				if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_PVRTCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ATCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_DXTTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC1TargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC2TargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ASTCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_MultiTargetPlatform");
					DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("HTML5TargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacNoEditorTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacServerTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacClientTargetPlatform");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				if (Target.bCompileLeanAndMeanUE == false)
				{
					DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
					DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
					DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
				}

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (Target.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
				}

				if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_MultiTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_PVRTCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ATCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_DXTTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC1TargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC2TargetPlatform");
					DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("HTML5TargetPlatform");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				if (Target.bCompileLeanAndMeanUE == false)
				{
					DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
					DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
					DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
				}

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (Target.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
				}

				if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_MultiTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_PVRTCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ATCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_DXTTargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC1TargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ETC2TargetPlatform");
					DynamicallyLoadedModuleNames.Add("Android_ASTCTargetPlatform");
					DynamicallyLoadedModuleNames.Add("HTML5TargetPlatform");
				}
			}
		}

		if (Target.bBuildDeveloperTools == true && Target.bBuildRequiresCookedData && Target.bCompileAgainstEngine && Target.bCompilePhysX)
		{
			DynamicallyLoadedModuleNames.Add("PhysXCooking");
		}
	}
}
