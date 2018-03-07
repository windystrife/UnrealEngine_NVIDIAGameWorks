// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class NvCloth : ModuleRules
{
	enum NvClothLibraryMode
	{
		Debug,
		Profile,
		Checked,
		Shipping
	}

	NvClothLibraryMode GetNvClothLibraryMode(UnrealTargetConfiguration Config)
	{
		switch (Config)
		{
			case UnrealTargetConfiguration.Debug:
                if (Target.bDebugBuildsActuallyUseDebugCRT)
                {
                    return NvClothLibraryMode.Debug;
                }
                else
                {
                    return NvClothLibraryMode.Checked;
                }
			case UnrealTargetConfiguration.Shipping:
			case UnrealTargetConfiguration.Test:
				return NvClothLibraryMode.Shipping;
			case UnrealTargetConfiguration.Development:
			case UnrealTargetConfiguration.DebugGame:
			case UnrealTargetConfiguration.Unknown:
			default:
                if(Target.bUseShippingPhysXLibraries)
                {
                    return NvClothLibraryMode.Shipping;
                }
                else if (Target.bUseCheckedPhysXLibraries)
                {
                    return NvClothLibraryMode.Checked;
                }
                else
                {
                    return NvClothLibraryMode.Profile;
                }
		}
	}

	static string GetNvClothLibrarySuffix(NvClothLibraryMode Mode)
	{
		switch (Mode)
		{
			case NvClothLibraryMode.Debug:
				return "DEBUG";
			case NvClothLibraryMode.Checked:
				return "CHECKED";
			case NvClothLibraryMode.Profile:
				return "PROFILE";
            case NvClothLibraryMode.Shipping:
            default:
				return "";	
		}
	}

	public NvCloth(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Determine which kind of libraries to link against
		NvClothLibraryMode LibraryMode = GetNvClothLibraryMode(Target.Configuration);
		string LibrarySuffix = GetNvClothLibrarySuffix(LibraryMode);

		Definitions.Add("WITH_NVCLOTH=1");

        string NvClothDir = Target.UEThirdPartySourceDirectory + "PhysX/NvCloth/";

		string NvClothLibDir = Target.UEThirdPartySourceDirectory + "PhysX/Lib";

        string PxSharedVersion = "PxShared";
        string PxSharedDir = Target.UEThirdPartySourceDirectory + "PhysX/" + PxSharedVersion + "/";
        string PxSharedIncludeDir = PxSharedDir + "include/";

        PublicSystemIncludePaths.AddRange(
			new string[] {
                NvClothDir + "include",
                NvClothDir + "extensions/include",

                PxSharedIncludeDir,
                PxSharedIncludeDir + "filebuf",
                PxSharedIncludeDir + "foundation",
                PxSharedIncludeDir + "pvd",
                PxSharedIncludeDir + "task",
                PxSharedDir + "src/foundation/include"
			}
            );

		// List of default library names (unused unless LibraryFormatString is non-null)
		List<string> NvClothLibraries = new List<string>();
		NvClothLibraries.AddRange(
			new string[]
			{
				"NvCloth{0}"
			});
		string LibraryFormatString = null;

		// Libraries and DLLs for windows platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			NvClothLibDir += "/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(NvClothLibDir);
            
            string[] StaticLibrariesX64 = new string[]
            {
                "NvCloth{0}_x64.lib"
            };

			string[] RuntimeDependenciesX64 =
			{
                "NvCloth{0}_x64.dll",
			};

            string[] DelayLoadDLLsX64 =
            {
                "NvCloth{0}_x64.dll",
            };

            foreach(string Lib in StaticLibrariesX64)
            {
                PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
            }

            foreach(string DLL in DelayLoadDLLsX64)
            {
                PublicDelayLoadDLLs.Add(String.Format(DLL, LibrarySuffix));
            }

			string NvClothBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win64/VS{0}/", Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			foreach(string RuntimeDependency in RuntimeDependenciesX64)
			{
				string FileName = NvClothBinariesDir + String.Format(RuntimeDependency, LibrarySuffix);
				RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
				RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
			}

            if(LibrarySuffix != "")
            {
                Definitions.Add("UE_NVCLOTH_SUFFIX=" + LibrarySuffix);
            }
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
            NvClothLibDir += "/Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
            PublicLibraryPaths.Add(NvClothLibDir);

            string[] StaticLibrariesX64 = new string[]
            {
                "NvCloth{0}_x86.lib"
            };

            string[] RuntimeDependenciesX64 =
            {
                "NvCloth{0}_x86.dll",
            };

            string[] DelayLoadDLLsX64 =
            {
                "NvCloth{0}_x86.dll",
            };

            foreach (string Lib in StaticLibrariesX64)
            {
                PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
            }

            foreach (string DLL in DelayLoadDLLsX64)
            {
                PublicDelayLoadDLLs.Add(String.Format(DLL, LibrarySuffix));
            }

            string NvClothBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win32/VS{0}/", Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
            foreach (string RuntimeDependency in RuntimeDependenciesX64)
            {
                string FileName = NvClothBinariesDir + String.Format(RuntimeDependency, LibrarySuffix);
                RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
                RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
            }

            if (LibrarySuffix != "")
            {
                Definitions.Add("UE_NVCLOTH_SUFFIX=" + LibrarySuffix);
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
            NvClothLibDir += "/Mac";

            LibraryFormatString = NvClothLibDir + "/lib{0}" + ".a";

            NvClothLibraries.Clear();

            string[] DynamicLibrariesMac = new string[]
            {
                "/libNvCloth{0}.dylib",
            };

            string PhysXBinDir = Target.UEThirdPartyBinariesDirectory + "PhysX/Mac";

            foreach(string Lib in DynamicLibrariesMac)
            {
                string LibraryPath = PhysXBinDir + String.Format(Lib, LibrarySuffix);
                PublicDelayLoadDLLs.Add(LibraryPath);
                RuntimeDependencies.Add(new RuntimeDependency(LibraryPath));
            }

            if(LibrarySuffix != "")
            {
                Definitions.Add("UE_NVCLOTH_SUFFIX=" + LibrarySuffix);
            }
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            if (Target.Architecture != "arm-unknown-linux-gnueabihf")
            {
                NvClothLibDir += "/Linux/" + Target.Architecture;

                NvClothLibraries.Add("NvCloth{0}");

                LibraryFormatString = NvClothLibDir + "/lib{0}" + ".a";
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			NvClothLibDir += "/PS4";
			PublicLibraryPaths.Add(NvClothLibDir);

            NvClothLibraries.Add("NvCloth{0}");

			LibraryFormatString = "{0}";
		}
        else if (Target.Platform == UnrealTargetPlatform.Switch)
        {
            NvClothLibDir += "/Switch";
            PublicLibraryPaths.Add(NvClothLibDir);

            NvClothLibraries.Add("NvCloth{0}");

            LibraryFormatString = "{0}";
        }
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			Definitions.Add("_XBOX_ONE=1");

			// This MUST be defined for XboxOne!
			Definitions.Add("PX_HAS_SECURE_STRCPY=1");

			NvClothLibDir += "/XboxOne/VS2015";
			PublicLibraryPaths.Add(NvClothLibDir);

            NvClothLibraries.Add("NvCloth{0}");

			LibraryFormatString = "{0}.lib";
		}

		// Add the libraries needed (used for all platforms except Windows)
		if (LibraryFormatString != null)
		{
			foreach (string Lib in NvClothLibraries)
			{
				string ConfiguredLib = String.Format(Lib, LibrarySuffix);
				string FinalLib = String.Format(LibraryFormatString, ConfiguredLib);
				PublicAdditionalLibraries.Add(FinalLib);
			}
		}
	}
}
