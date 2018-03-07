// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class VHACD : ModuleRules
{
	public VHACD(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string VHACDDirectory = Target.UEThirdPartySourceDirectory + "VHACD/";
		string VHACDLibPath = VHACDDirectory;
		PublicIncludePaths.Add(VHACDDirectory + "public");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			VHACDLibPath = VHACDLibPath + "lib/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(VHACDLibPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("VHACDd.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("VHACD.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibPath = VHACDDirectory + "Lib/Mac/";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(LibPath + "libVHACD_LIBd.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(LibPath + "libVHACD_LIB.a");
			}
			PublicFrameworks.AddRange(new string[] { "OpenCL" });
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			if (Target.LinkType == TargetLinkType.Monolithic)
			{
				PublicAdditionalLibraries.Add(VHACDDirectory + "Lib/Linux/" + Target.Architecture + "/libVHACD.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(VHACDDirectory + "Lib/Linux/" + Target.Architecture + "/libVHACD_fPIC.a");
			}
		}
	}
}

