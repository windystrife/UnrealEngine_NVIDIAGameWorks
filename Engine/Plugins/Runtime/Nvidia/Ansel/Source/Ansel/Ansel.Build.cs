// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Ansel : ModuleRules
{
    private string ModulePath
    {
        get { return ModuleDirectory; }
    }

    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/")); }
    }

    private string LibraryPath
    {
        get { return Path.GetFullPath(Path.Combine(ThirdPartyPath, "NVAnselSDK", "lib")); }
    }

    private string BinariesPath
    {
        get { return Path.GetFullPath(Path.Combine(ModulePath, "../../Binaries/ThirdParty/")); }
    }

    public Ansel(ReadOnlyTargetRules Target) : base(Target)
    {
        string NvCameraSDKSourcePath = ThirdPartyPath + "NVAnselSDK/";

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

            bool HaveDebugLib = File.Exists(BinariesPath + LibName + "d" + ".dll");

            if (HaveDebugLib &&
                Target.Configuration == UnrealTargetConfiguration.Debug &&
                Target.bDebugBuildsActuallyUseDebugCRT)
            {
                LibName += "d";
            }

            PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, LibName + ".lib"));

            string DLLName = LibName + ".dll";
            PublicDelayLoadDLLs.Add(DLLName);
            RuntimeDependencies.Add(new RuntimeDependency(BinariesPath + DLLName));

            Definitions.Add("WITH_ANSEL=1");
            Definitions.Add("ANSEL_DLL=" + DLLName);
        }
        else
        {
            Definitions.Add("WITH_ANSEL=0");
            Definitions.Add("ANSEL_DLL=");
        }        

        PrivateDependencyModuleNames.AddRange(new string[]
        {
			"Core",
			"CoreUObject",
            "SlateCore",
			"Engine",
		});

        PublicDependencyModuleNames.Add("Engine");
    }
}
