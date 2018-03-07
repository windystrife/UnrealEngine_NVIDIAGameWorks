// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UEOpenExr : ModuleRules
{
    public UEOpenExr(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
        {
            bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);
            string LibDir = Target.UEThirdPartySourceDirectory + "openexr/Deploy/lib/";
            string Platform;
            switch (Target.Platform)
            {
                case UnrealTargetPlatform.Win64:
                    Platform = "x64";
                    LibDir += "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
                    break;
                case UnrealTargetPlatform.Win32:
                    Platform = "Win32";
                    LibDir += "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
                    break;
                case UnrealTargetPlatform.Mac:
                    Platform = "Mac";
                    bDebug = false;
                    break;
                case UnrealTargetPlatform.Linux:
                    Platform = "Linux";
                    bDebug = false;
                    break;
                default:
                    return;
            }
            LibDir = LibDir + "/" + Platform;
            LibDir = LibDir + "/Static" + (bDebug ? "Debug" : "Release");
            PublicLibraryPaths.Add(LibDir);

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						"Half.lib",
						"Iex.lib",
						"IlmImf.lib",
						"IlmThread.lib",
						"Imath.lib",
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						LibDir + "/libHalf.a",
						LibDir + "/libIex.a",
						LibDir + "/libIlmImf.a",
						LibDir + "/libIlmThread.a",
						LibDir + "/libImath.a",
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				string LibArchDir = LibDir + "/" + Target.Architecture;
				PublicAdditionalLibraries.AddRange(
					new string[] {
						LibArchDir + "/libHalf.a",
						LibArchDir + "/libIex.a",
						LibArchDir + "/libIlmImf.a",
						LibArchDir + "/libIlmThread.a",
						LibArchDir + "/libImath.a",
					}
				);
			}

            PublicSystemIncludePaths.AddRange(
                new string[] {
                    Target.UEThirdPartySourceDirectory + "openexr/Deploy/include",
			    }
            );
        }
    }
}

