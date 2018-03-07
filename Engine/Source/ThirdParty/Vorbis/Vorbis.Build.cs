// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class Vorbis : ModuleRules
{
	public Vorbis(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string VorbisPath = Target.UEThirdPartySourceDirectory + "Vorbis/libvorbis-1.3.2/";

		PublicIncludePaths.Add(VorbisPath + "include");
		Definitions.Add("WITH_OGGVORBIS=1");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string VorbisLibPath = VorbisPath + "Lib/win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(VorbisLibPath);

			PublicAdditionalLibraries.Add("libvorbis_64.lib");
			PublicDelayLoadDLLs.Add("libvorbis_64.dll");

			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Vorbis/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libvorbis_64.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			string VorbisLibPath = VorbisPath + "Lib/win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(VorbisLibPath);

			PublicAdditionalLibraries.Add("libvorbis.lib");
			PublicDelayLoadDLLs.Add("libvorbis.dll");

			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Vorbis/Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/libvorbis.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.AddRange(
				new string[] {
					VorbisPath + "macosx/libvorbis.dylib",
				}
			);
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			string VorbisLibPath = VorbisPath + "lib/HTML5/";
			PublicLibraryPaths.Add(VorbisLibPath);

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
			PublicAdditionalLibraries.Add(VorbisLibPath + "libvorbis" + OpimizationSuffix + ".bc");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// toolchain will filter
			PublicLibraryPaths.Add(VorbisPath + "lib/Android/ARMv7");
			PublicLibraryPaths.Add(VorbisPath + "lib/Android/ARM64");
			PublicLibraryPaths.Add(VorbisPath + "lib/Android/x86");
			PublicLibraryPaths.Add(VorbisPath + "lib/Android/x64");

			PublicAdditionalLibraries.Add("vorbis");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(VorbisPath + "lib/Linux/" + Target.Architecture + "/libvorbis.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				PublicLibraryPaths.Add(VorbisPath + "lib/XboxOne/VS" + VersionName.ToString());
				PublicAdditionalLibraries.Add("libvorbis_static.lib");
			}
		}
	}
}

