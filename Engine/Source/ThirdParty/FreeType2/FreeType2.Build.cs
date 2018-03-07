// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FreeType2 : ModuleRules
{
	public FreeType2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		Definitions.Add("WITH_FREETYPE=1");

		string FreeType2Path;
		string FreeType2LibPath;

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.HTML5)
		{
			FreeType2Path = Target.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.6/";

			PublicSystemIncludePaths.Add(FreeType2Path + "Include");
		}
		else
		{
			FreeType2Path = Target.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/";

			PublicSystemIncludePaths.Add(FreeType2Path + "include");
		}

		FreeType2LibPath = FreeType2Path + "Lib/";

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64)
		{

			FreeType2LibPath += (Target.Platform == UnrealTargetPlatform.Win64) ? "Win64/" : "Win32/";
			FreeType2LibPath += "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			PublicSystemIncludePaths.Add(FreeType2Path + "include");

			PublicLibraryPaths.Add(FreeType2LibPath);
			PublicAdditionalLibraries.Add("freetype26MT.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(FreeType2LibPath + "Mac/libfreetype2412.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			if (Target.Architecture == "-simulator")
			{
				PublicLibraryPaths.Add(FreeType2LibPath + "ios/Simulator");
				PublicAdditionalShadowFiles.Add(Target.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/Lib/ios/Simulator/libfreetype2412.a");
			}
			else
			{
				PublicLibraryPaths.Add(FreeType2LibPath + "ios/Device");
				PublicAdditionalShadowFiles.Add(Target.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/Lib/ios/Device/libfreetype2412.a");
			}

			PublicAdditionalLibraries.Add("freetype2412");
		}
		else if (Target.Platform == UnrealTargetPlatform.TVOS)
		{
			if (Target.Architecture == "-simulator")
			{
				PublicLibraryPaths.Add(FreeType2LibPath + "TVOS/Simulator");
				PublicAdditionalShadowFiles.Add(Target.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/Lib/TVOS/Simulator/libfreetype2412.a");
			}
			else
			{
				PublicLibraryPaths.Add(FreeType2LibPath + "TVOS/Device");
				PublicAdditionalShadowFiles.Add(Target.UEThirdPartySourceDirectory + "FreeType2/FreeType2-2.4.12/Lib/TVOS/Device/libfreetype2412.a");
			}

			PublicAdditionalLibraries.Add("freetype2412");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// filtered out in the toolchain
			PublicLibraryPaths.Add(FreeType2LibPath + "Android/ARMv7");
			PublicLibraryPaths.Add(FreeType2LibPath + "Android/ARM64");
			PublicLibraryPaths.Add(FreeType2LibPath + "Android/x86");
			PublicLibraryPaths.Add(FreeType2LibPath + "Android/x64");

			PublicAdditionalLibraries.Add("freetype2412");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			if (Target.Type == TargetType.Server)
			{
				string Err = string.Format("{0} dedicated server is made to depend on {1}. We want to avoid this, please correct module dependencies.", Target.Platform.ToString(), this.ToString());
				System.Console.WriteLine(Err);
				throw new BuildException(Err);
			}

			PublicSystemIncludePaths.Add(FreeType2Path + "Include");

			if (Target.LinkType == TargetLinkType.Monolithic)
			{
				PublicAdditionalLibraries.Add(FreeType2LibPath + "Linux/" + Target.Architecture + "/libfreetype.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(FreeType2LibPath + "Linux/" + Target.Architecture + "/libfreetype_fPIC.a");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			PublicLibraryPaths.Add(FreeType2Path + "Lib/HTML5");
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
			PublicAdditionalLibraries.Add(FreeType2Path + "Lib/HTML5/libfreetype260" + OpimizationSuffix + ".bc");
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PublicLibraryPaths.Add(FreeType2LibPath + "PS4");
			PublicAdditionalLibraries.Add("freetype2412");
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				PublicLibraryPaths.Add(FreeType2LibPath + "XboxOne/VS" + VersionName.ToString());
				PublicAdditionalLibraries.Add("freetype2412.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Switch)
		{
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(FreeType2LibPath, "Switch/libFreetype.a"));
		}
	}
}
