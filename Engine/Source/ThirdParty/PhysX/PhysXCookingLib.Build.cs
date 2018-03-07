// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class PhysXCookingLib : ModuleRules
{
	enum PhysXLibraryMode
	{
		Debug,
		Profile,
		Checked,
		Shipping
	}

	PhysXLibraryMode GetPhysXLibraryMode(UnrealTargetConfiguration Config)
	{
		switch (Config)
		{
			case UnrealTargetConfiguration.Debug:
				if (Target.bDebugBuildsActuallyUseDebugCRT)
				{
					return PhysXLibraryMode.Debug;
				}
				else
				{
					return PhysXLibraryMode.Checked;
				}
			case UnrealTargetConfiguration.Shipping:
			case UnrealTargetConfiguration.Test:
				return PhysXLibraryMode.Shipping;
			case UnrealTargetConfiguration.Development:
			case UnrealTargetConfiguration.DebugGame:
			case UnrealTargetConfiguration.Unknown:
			default:
				if (Target.bUseShippingPhysXLibraries)
				{
					return PhysXLibraryMode.Shipping;
				}
				else if (Target.bUseCheckedPhysXLibraries)
				{
					return PhysXLibraryMode.Checked;
				}
				else
				{
					return PhysXLibraryMode.Profile;
				}
		}
	}

	static string GetPhysXLibrarySuffix(PhysXLibraryMode Mode)
	{
		switch (Mode)
		{
			case PhysXLibraryMode.Debug:
				return "DEBUG";
			case PhysXLibraryMode.Checked:
				return "CHECKED";
			case PhysXLibraryMode.Profile:
				return "PROFILE";
			case PhysXLibraryMode.Shipping:
			default:
				return "";
		}
	}

	public PhysXCookingLib(ReadOnlyTargetRules Target) : base(Target)
	{
        Type = ModuleType.External;

        // Determine which kind of libraries to link against
        PhysXLibraryMode LibraryMode = GetPhysXLibraryMode(Target.Configuration);
        string LibrarySuffix = GetPhysXLibrarySuffix(LibraryMode);

        string PhysXLibDir = Target.UEThirdPartySourceDirectory + "PhysX/Lib/";

        // Libraries and DLLs for windows platform
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(String.Format("PhysX3Cooking{0}_x64.lib", LibrarySuffix));
            PublicDelayLoadDLLs.Add(String.Format("PhysX3Cooking{0}_x64.dll", LibrarySuffix));

            string PhysXBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win64/VS{0}/", Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
            string FileName = PhysXBinariesDir + String.Format("PhysX3Cooking{0}_x64.dll", LibrarySuffix);
            RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
            RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
        }
        else if (Target.Platform == UnrealTargetPlatform.Win32)
        {
            PublicAdditionalLibraries.Add(String.Format("PhysX3Cooking{0}_x86.lib", LibrarySuffix));
            PublicDelayLoadDLLs.Add(String.Format("PhysX3Cooking{0}_x86.dll", LibrarySuffix));

            string PhysXBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win32/VS{0}/", Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
            string FileName = PhysXBinariesDir + String.Format("PhysX3Cooking{0}_x86.dll", LibrarySuffix);
            RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
            RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string PhysXBinariesDir = Target.UEThirdPartyBinariesDirectory + "PhysX/Mac/";
            string LibraryPath = PhysXBinariesDir + String.Format("libPhysX3Cooking{0}.dylib", LibrarySuffix);

            PublicDelayLoadDLLs.Add(LibraryPath);
            RuntimeDependencies.Add(new RuntimeDependency(LibraryPath));
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicAdditionalLibraries.Add(String.Format("PhysX3Cooking{0}", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(String.Format("PhysX3Cooking{0}", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PhysXLibDir = Path.Combine(PhysXLibDir, "IOS/");
            PublicAdditionalLibraries.Add("PhysX3Cooking" + LibrarySuffix);
            PublicAdditionalShadowFiles.Add(Path.Combine(PhysXLibDir, "libPhysX3Cooking" + LibrarySuffix + ".a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.TVOS)
        {
            PhysXLibDir = Path.Combine(PhysXLibDir, "TVOS/");
            PublicAdditionalLibraries.Add("PhysX3Cooking" + LibrarySuffix);
            PublicAdditionalShadowFiles.Add(Path.Combine(PhysXLibDir, "libPhysX3Cooking" + LibrarySuffix + ".a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            PhysXLibDir = Path.Combine(PhysXLibDir, "HTML5/");
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

            PublicAdditionalLibraries.Add(PhysXLibDir + "PhysX3Cooking" + OpimizationSuffix + ".bc");

        }
        else if (Target.Platform == UnrealTargetPlatform.PS4)
        {
            PublicAdditionalLibraries.Add(String.Format("PhysX3Cooking{0}", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            PublicAdditionalLibraries.Add(String.Format("PhysX3Cooking{0}.lib", LibrarySuffix));
        }
        else if (Target.Platform == UnrealTargetPlatform.Switch)
        {
            PublicAdditionalLibraries.Add("PhysX3Cooking" + LibrarySuffix);
        }
	}
}
