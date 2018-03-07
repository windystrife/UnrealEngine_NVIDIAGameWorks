// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class QualcommTextureConverter : ModuleRules
{
	public QualcommTextureConverter(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.Mac) ||
			(Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64")))
		{
			PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "Qualcomm/TextureConverter/Include");

			string LibraryPath = Target.UEThirdPartySourceDirectory + "Qualcomm/TextureConverter/Lib/";
			string LibraryName = "TextureConverter";
			string LibraryExtension = ".lib";

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryPath += "vs" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/x64";
				PublicDelayLoadDLLs.Add("TextureConverter.dll");

				RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Qualcomm/Win64/TextureConverter.dll"));
            }
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				LibraryPath += "osx64";
				LibraryExtension = ".dylib";
				LibraryName = LibraryPath + "/lib" + LibraryName;
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				LibraryPath += "linux_x64";	// FIXME: change to proper architecture
				LibraryExtension = ".so";
				LibraryName = LibraryPath + "/lib" + LibraryName;

				RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Qualcomm/Linux/libTextureConverter.so"));
			}

			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + LibraryExtension);
		}
	}
}
