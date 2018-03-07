// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class ApexDestructionLib : ModuleRules
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
                if (Target.bUseShippingPhysXLibraries)
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

    public ApexDestructionLib(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.bCompileAPEX == false)
        {
            return;
        }
        
        // Determine which kind of libraries to link against
        APEXLibraryMode LibraryMode = GetAPEXLibraryMode(Target.Configuration);
        string LibrarySuffix = GetAPEXLibrarySuffix(LibraryMode);

        string ApexVersion = "APEX_1.4";

        string APEXDir = Target.UEThirdPartySourceDirectory + "PhysX/" + ApexVersion + "/";

        string APEXLibDir = Target.UEThirdPartySourceDirectory + "PhysX/Lib";

        PublicSystemIncludePaths.AddRange(
            new string[] {
                APEXDir + "include/destructible",
            }
            );

        // List of default library names (unused unless LibraryFormatString is non-null)
        List<string> ApexLibraries = new List<string>();
        ApexLibraries.AddRange(
            new string[]
            {
                "APEX_Destructible{0}",
            });
        string LibraryFormatString = null;
        

        // Libraries and DLLs for windows platform
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            APEXLibDir += "/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
            PublicLibraryPaths.Add(APEXLibDir);

            PublicAdditionalLibraries.Add(String.Format("APEXFramework{0}_x64.lib", LibrarySuffix));
            PublicDelayLoadDLLs.Add(String.Format("APEXFramework{0}_x64.dll", LibrarySuffix));

            string[] RuntimeDependenciesX64 =
            {
                "APEX_Destructible{0}_x64.dll",
            };

            string ApexBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win64/VS{0}/", Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
            foreach (string RuntimeDependency in RuntimeDependenciesX64)
            {
                string FileName = ApexBinariesDir + String.Format(RuntimeDependency, LibrarySuffix);
                RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
                RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
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
                "APEX_Destructible{0}_x86.dll",
            };

            string ApexBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win32/VS{0}/", Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
            foreach (string RuntimeDependency in RuntimeDependenciesX86)
            {
                string FileName = ApexBinariesDir + String.Format(RuntimeDependency, LibrarySuffix);
                RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
                RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            APEXLibDir += "/Mac";

            string[] DynamicLibrariesMac = new string[] {
                "/libAPEX_Destructible{0}.dylib"
            };

            string PhysXBinariesDir = Target.UEThirdPartyBinariesDirectory + "PhysX/Mac";
            foreach (string Lib in DynamicLibrariesMac)
            {
                string LibraryPath = PhysXBinariesDir + String.Format(Lib, LibrarySuffix);
                PublicDelayLoadDLLs.Add(LibraryPath);
                RuntimeDependencies.Add(new RuntimeDependency(LibraryPath));
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            if (Target.Architecture.StartsWith("x86_64"))
            {
                APEXLibDir += "/Linux/" + Target.Architecture;
                ApexLibraries.Add("APEX_Destructible{0}");
                LibraryFormatString = APEXLibDir + "/lib{0}" + ".a";
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.PS4)
        {
            APEXLibDir += "/PS4";
            PublicLibraryPaths.Add(APEXLibDir);

            LibraryFormatString = "{0}";
        }
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            APEXLibDir += "/XboxOne/VS2015";
            PublicLibraryPaths.Add(APEXLibDir);

            LibraryFormatString = "{0}.lib";
        }
        else if (Target.Platform == UnrealTargetPlatform.Switch)
        {
            APEXLibDir += "/Switch";
            PublicLibraryPaths.Add(APEXLibDir);

            LibraryFormatString = "{0}";
        }

        // Add the libraries needed (used for all platforms except Windows and Mac)
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
