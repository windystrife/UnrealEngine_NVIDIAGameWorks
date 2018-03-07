// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class FBX : ModuleRules
{
	public FBX(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string FBXSDKDir = Target.UEThirdPartySourceDirectory + "FBX/2018.1.1/";
		PublicSystemIncludePaths.AddRange(
			new string[] {
					FBXSDKDir + "include",
					FBXSDKDir + "include/fbxsdk",
				}
			);


		if ( Target.Platform == UnrealTargetPlatform.Win64 )
		{
			string FBxLibPath = FBXSDKDir + "lib/vs" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";

			FBxLibPath += "x64/release/";
			PublicLibraryPaths.Add(FBxLibPath);

			if (Target.LinkType != TargetLinkType.Monolithic)
			{
				PublicAdditionalLibraries.Add("libfbxsdk.lib");

				// We are using DLL versions of the FBX libraries
				Definitions.Add("FBXSDK_SHARED");

				RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Win64/libfbxsdk.dll"));
			}
			else
			{
				if (Target.bUseStaticCRT)
				{
					PublicAdditionalLibraries.Add("libfbxsdk-mt.lib");
				}
				else
				{
					PublicAdditionalLibraries.Add("libfbxsdk-md.lib");
				}
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibDir = FBXSDKDir + "lib/clang/release/";
			PublicAdditionalLibraries.Add(LibDir + "libfbxsdk.dylib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string LibDir = FBXSDKDir + "lib/gcc4/" + Target.Architecture + "/release/";
			if (!Directory.Exists(LibDir))
			{
				string Err = string.Format("FBX SDK not found in {0}", LibDir);
				System.Console.WriteLine(Err);
				throw new BuildException(Err);
			}

			PublicAdditionalLibraries.Add(LibDir + "/libfbxsdk.a");
			/* There is a bug in fbxarch.h where is doesn't do the check
			 * for clang under linux */
			Definitions.Add("FBXSDK_COMPILER_CLANG");

			// libfbxsdk has been built against libstdc++ and as such needs this library
			PublicAdditionalLibraries.Add("stdc++");
		}
	}
}

