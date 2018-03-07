// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class TangoSDK : ModuleRules
{
	public TangoSDK(ReadOnlyTargetRules Target) : base(Target)
	{
        Type = ModuleType.External;

		string TangoSDKSDKDir = Target.UEThirdPartySourceDirectory + "TangoSDK/";
		PublicSystemIncludePaths.AddRange(
			new string[] {
					TangoSDKSDKDir + "include/",
				}
			);

		string TangoSDKBaseLibPath = TangoSDKSDKDir + "lib/";
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string TangoSDKArmLibPath = TangoSDKBaseLibPath + "armeabi-v7a/";
			string TangoSDKArm64LibPath = TangoSDKBaseLibPath + "arm64-v8a/";

			// toolchain will filter properly
			PublicLibraryPaths.Add(TangoSDKArmLibPath);
			PublicLibraryPaths.Add(TangoSDKArm64LibPath);

			PublicAdditionalLibraries.Add("tango_support");
			PublicAdditionalLibraries.Add("tango_client_api2");
			PublicAdditionalLibraries.Add("tango_3d_reconstruction");
		}
	}
}
