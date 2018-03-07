// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UElibPNG : ModuleRules
{
	public UElibPNG(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string libPNGPath = Target.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = libPNGPath + "/lib/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(LibPath);

			string LibFileName = "libpng" + (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT ? "d" : "") + "_64.lib";
			PublicAdditionalLibraries.Add(LibFileName);
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			libPNGPath = libPNGPath + "/lib/Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(libPNGPath);

			string LibFileName = "libpng" + (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT ? "d" : "") + ".lib";
			PublicAdditionalLibraries.Add(LibFileName);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(libPNGPath + "/lib/Mac/libpng.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			if (Target.Architecture == "-simulator")
			{
				PublicLibraryPaths.Add(libPNGPath + "/lib/ios/Simulator");
				PublicAdditionalShadowFiles.Add(Target.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2/lib/ios/Simulator/libpng152.a");
			}
			else
			{
				PublicLibraryPaths.Add(libPNGPath + "/lib/ios/Device");
				PublicAdditionalShadowFiles.Add(Target.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2/lib/ios/Device/libpng152.a");
			}

			PublicAdditionalLibraries.Add("png152");
		}
		else if (Target.Platform == UnrealTargetPlatform.TVOS)
		{
			if (Target.Architecture == "-simulator")
			{
				PublicLibraryPaths.Add(libPNGPath + "/lib/TVOS/Simulator");
				PublicAdditionalShadowFiles.Add(Target.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2/lib/TVOS/Simulator/libpng152.a");
			}
			else
			{
				PublicLibraryPaths.Add(libPNGPath + "/lib/TVOS/Device");
				PublicAdditionalShadowFiles.Add(Target.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2/lib/TVOS/Device/libpng152.a");
			}

			PublicAdditionalLibraries.Add("png152");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			libPNGPath = Target.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.27";

			PublicLibraryPaths.Add(libPNGPath + "/lib/Android/ARMv7");
			PublicLibraryPaths.Add(libPNGPath + "/lib/Android/ARM64");
			PublicLibraryPaths.Add(libPNGPath + "/lib/Android/x86");
			PublicLibraryPaths.Add(libPNGPath + "/lib/Android/x64");

			PublicAdditionalLibraries.Add("png");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			// migrate all architectures to the newer binary
			if (Target.Architecture.StartsWith("aarch64") || Target.Architecture.StartsWith("i686"))
			{
				libPNGPath = Target.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.27";
			}

			PublicAdditionalLibraries.Add(libPNGPath + "/lib/Linux/" + Target.Architecture + "/libpng.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			PublicLibraryPaths.Add(libPNGPath + "/lib/HTML5");
			string OpimizationSuffix = "";
			if (Target.bCompileForSize)
			{
				OpimizationSuffix = "_Oz";
			}
			else
			{
				if (Target.Configuration == UnrealTargetConfiguration.Development)
				{
					OpimizationSuffix = "_O2";
				}
				else if (Target.Configuration == UnrealTargetConfiguration.Shipping)
				{
					OpimizationSuffix = "_O3";
				}
			}
			PublicAdditionalLibraries.Add(libPNGPath + "/lib/HTML5/libpng" + OpimizationSuffix + ".bc");
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PublicLibraryPaths.Add(libPNGPath + "/lib/PS4");
			PublicAdditionalLibraries.Add("png152");
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				PublicLibraryPaths.Add(libPNGPath + "/lib/XboxOne/VS" + VersionName.ToString());
				PublicAdditionalLibraries.Add("libpng125_XboxOne.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Switch)
		{
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(libPNGPath, "lib/Switch/libPNG.a"));
		}

		PublicIncludePaths.Add(libPNGPath);
	}
}
