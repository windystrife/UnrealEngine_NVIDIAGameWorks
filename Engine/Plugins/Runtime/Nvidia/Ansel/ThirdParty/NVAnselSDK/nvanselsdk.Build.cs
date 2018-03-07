// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class NvAnselSDK : ModuleRules
{
	public NvAnselSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string NvCameraSDKSourcePath = UEBuildConfiguration.UEThirdPartySourceDirectory + "NVAnselSDK/";

		string NvCameraSDKIncPath = NvCameraSDKSourcePath + "include/";
		PublicSystemIncludePaths.Add(NvCameraSDKIncPath);

		string NvCameraSDKLibPath = NvCameraSDKSourcePath + "lib/";
		PublicLibraryPaths.Add(NvCameraSDKLibPath);

		bool FoundAnselDirs = true;
		if (!Directory.Exists(NvCameraSDKSourcePath))
		{
			System.Console.WriteLine(string.Format("Ansel SDK source path not found: {0}", NvCameraSDKSourcePath));
			FoundAnselDirs = false;
		}

		string LibName;
		if ((Target.Platform == UnrealTargetPlatform.Win64 ||
			 Target.Platform == UnrealTargetPlatform.Win32)
			 && FoundAnselDirs)
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibName = "AnselSDK64";
			}
			else
			{
				LibName = "AnselSDK32";
			}

			bool HaveDebugLib = File.Exists(UEBuildConfiguration.UEThirdPartyBinariesDirectory + "NVAnselSDK/redist/" + LibName + "d" + ".dll");

			if (HaveDebugLib &&
				Target.Configuration == UnrealTargetConfiguration.Debug &&
				BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
					LibName += "d";
			}
			
			PublicAdditionalLibraries.Add(LibName + ".lib");
			string DLLName = LibName + ".dll";
			PublicDelayLoadDLLs.Add(DLLName);
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/NVAnselSDK/redist/" + DLLName
				));
			Definitions.Add("WITH_ANSEL=1");
			Definitions.Add("ANSEL_DLL=" + DLLName);
		}
		else
		{
			Definitions.Add("WITH_ANSEL=0");
			Definitions.Add("ANSEL_DLL=");
		}
	}
}

