// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class APEX : ModuleRules
{
	enum APEXLibraryMode
	{
		Debug,
		Profile,
		Checked,
		Shipping
	}

	APEXLibraryMode GetAPEXLibraryMode(UnrealTargetConfiguration Config)
	{
		switch (Config)
		{
			case UnrealTargetConfiguration.Debug:
                if (Target.bDebugBuildsActuallyUseDebugCRT)
                {
                    return APEXLibraryMode.Debug;
                }
                else
                {
                    return APEXLibraryMode.Checked;
                }
			case UnrealTargetConfiguration.Shipping:
			case UnrealTargetConfiguration.Test:
				return APEXLibraryMode.Shipping;
			case UnrealTargetConfiguration.Development:
			case UnrealTargetConfiguration.DebugGame:
			case UnrealTargetConfiguration.Unknown:
			default:
                if(Target.bUseShippingPhysXLibraries)
                {
                    return APEXLibraryMode.Shipping;
                }
                else if (Target.bUseCheckedPhysXLibraries)
                {
                    return APEXLibraryMode.Checked;
                }
                else
                {
                    return APEXLibraryMode.Profile;
                }
		}
	}

	static string GetAPEXLibrarySuffix(APEXLibraryMode Mode)
	{
		switch (Mode)
		{
			case APEXLibraryMode.Debug:
				return "DEBUG";
			case APEXLibraryMode.Checked:
				return "CHECKED";
			case APEXLibraryMode.Profile:
				return "PROFILE";
            case APEXLibraryMode.Shipping:
            default:
				return "";	
		}
	}

	public APEX(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Determine which kind of libraries to link against
		APEXLibraryMode LibraryMode = GetAPEXLibraryMode(Target.Configuration);
		string LibrarySuffix = GetAPEXLibrarySuffix(LibraryMode);

		string ApexVersion = "APEX_1.4";

        string APEXDir = Target.UEThirdPartySourceDirectory + "PhysX/" + ApexVersion + "/";

		string APEXLibDir = Target.UEThirdPartySourceDirectory + "PhysX/Lib";

		PublicSystemIncludePaths.AddRange(
			new string[] {
                APEXDir + "include",
                APEXDir + "include/clothing",
                APEXDir + "include/nvparameterized",
                APEXDir + "include/legacy",
                APEXDir + "include/PhysX3",
                APEXDir + "common/include",
				APEXDir + "common/include/autogen",
				APEXDir + "framework/include",
				APEXDir + "framework/include/autogen",
                APEXDir + "shared/general/RenderDebug/public",
                APEXDir + "shared/general/PairFilter/include",
				APEXDir + "shared/internal/include",
                APEXDir + "externals/CUDA_6.5.19/include",
			}
			);

		// List of default library names (unused unless LibraryFormatString is non-null)
		List<string> ApexLibraries = new List<string>();
		ApexLibraries.AddRange(
			new string[]
			{
				"ApexCommon{0}",
				"ApexFramework{0}",
				"ApexShared{0}",
				"APEX_Clothing{0}",
			});
		string LibraryFormatString = null;

		bool bIsApexStaticallyLinked = false;
		bool bHasApexLegacy = true;

		// Libraries and DLLs for windows platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			APEXLibDir += "/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(APEXLibDir);

			PublicAdditionalLibraries.Add(String.Format("APEXFramework{0}_x64.lib", LibrarySuffix));
			PublicDelayLoadDLLs.Add(String.Format("APEXFramework{0}_x64.dll", LibrarySuffix));

			string[] RuntimeDependenciesX64 =
			{
				"APEX_Clothing{0}_x64.dll",
				"APEX_Legacy{0}_x64.dll",
				"ApexFramework{0}_x64.dll",
			};

			string ApexBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win64/VS{0}/", Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			foreach(string RuntimeDependency in RuntimeDependenciesX64)
			{
				string FileName = ApexBinariesDir + String.Format(RuntimeDependency, LibrarySuffix);
				RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
				RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
			}
            if(LibrarySuffix != "")
            {
                Definitions.Add("UE_APEX_SUFFIX=" + LibrarySuffix);
            }
			
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			APEXLibDir += "/Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(APEXLibDir);

			PublicAdditionalLibraries.Add(String.Format("APEXFramework{0}_x86.lib", LibrarySuffix));
			PublicDelayLoadDLLs.Add(String.Format("APEXFramework{0}_x86.dll", LibrarySuffix));

			string[] RuntimeDependenciesX86 =
			{
				"APEX_Clothing{0}_x86.dll",
				"APEX_Legacy{0}_x86.dll",
				"ApexFramework{0}_x86.dll",
			};

			string ApexBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win32/VS{0}/", Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			foreach(string RuntimeDependency in RuntimeDependenciesX86)
			{
				string FileName = ApexBinariesDir + String.Format(RuntimeDependency, LibrarySuffix);
				RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
				RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
			}
            if (LibrarySuffix != "")
            {
                Definitions.Add("UE_APEX_SUFFIX=" + LibrarySuffix);
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			APEXLibDir += "/Mac";

			ApexLibraries.Clear();
			ApexLibraries.AddRange(
				new string[]
				{
					"ApexCommon{0}",
					"ApexShared{0}",
				});

			LibraryFormatString = APEXLibDir + "/lib{0}" + ".a";

			string[] DynamicLibrariesMac = new string[] {
				"/libAPEX_Clothing{0}.dylib",
				"/libAPEX_Legacy{0}.dylib",
				"/libApexFramework{0}.dylib"
			};

			string PhysXBinariesDir = Target.UEThirdPartyBinariesDirectory + "PhysX/Mac";
			foreach (string Lib in DynamicLibrariesMac)
			{
				string LibraryPath = PhysXBinariesDir + String.Format(Lib, LibrarySuffix);
				PublicDelayLoadDLLs.Add(LibraryPath);
				RuntimeDependencies.Add(new RuntimeDependency(LibraryPath));
			}
			if (LibrarySuffix != "")
			{
				Definitions.Add("UE_APEX_SUFFIX=" + LibrarySuffix);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			if (Target.Architecture.StartsWith("x86_64"))
			{
				APEXLibDir += "/Linux/" + Target.Architecture;
				bIsApexStaticallyLinked = true;

				ApexLibraries.Add("APEX_Clothing{0}");
				ApexLibraries.Add("APEX_Legacy{0}");
				ApexLibraries.Add("ApexFramework{0}");
				LibraryFormatString = APEXLibDir + "/lib{0}" + ".a";
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			bIsApexStaticallyLinked = true;
			bHasApexLegacy = false;

			APEXLibDir += "/PS4";
			PublicLibraryPaths.Add(APEXLibDir);

            ApexLibraries.Add("NvParameterized{0}");
            ApexLibraries.Add("RenderDebug{0}");

			LibraryFormatString = "{0}";
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			bIsApexStaticallyLinked = true;
			bHasApexLegacy = false;

			Definitions.Add("_XBOX_ONE=1");

			// This MUST be defined for XboxOne!
			Definitions.Add("PX_HAS_SECURE_STRCPY=1");

			APEXLibDir += "/XboxOne/VS2015";
			PublicLibraryPaths.Add(APEXLibDir);

			ApexLibraries.Add("NvParameterized{0}");
			ApexLibraries.Add("RenderDebug{0}");

			LibraryFormatString = "{0}.lib";
		}
		else if (Target.Platform == UnrealTargetPlatform.Switch)
		{
			bIsApexStaticallyLinked = true;
			bHasApexLegacy = false;

			APEXLibDir += "/Switch";
			PublicLibraryPaths.Add(APEXLibDir);

			ApexLibraries.Add("NvParameterized{0}");
			ApexLibraries.Add("RenderDebug{0}");

			LibraryFormatString = "{0}";
		}

		Definitions.Add("APEX_UE4=1");

		Definitions.Add(string.Format("APEX_STATICALLY_LINKED={0}", bIsApexStaticallyLinked ? 1 : 0));
		Definitions.Add(string.Format("WITH_APEX_LEGACY={0}", bHasApexLegacy ? 1 : 0));

		// Add the libraries needed (used for all platforms except Windows)
		if (LibraryFormatString != null)
		{
			foreach (string Lib in ApexLibraries)
			{
				string ConfiguredLib = String.Format(Lib, LibrarySuffix);
				string FinalLib = String.Format(LibraryFormatString, ConfiguredLib);
				PublicAdditionalLibraries.Add(FinalLib);
			}
		}
	}
}
