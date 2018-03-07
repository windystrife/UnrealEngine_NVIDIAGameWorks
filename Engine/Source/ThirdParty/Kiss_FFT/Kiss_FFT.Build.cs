// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Kiss_FFT : ModuleRules
{
	public Kiss_FFT(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		Definitions.Add("WITH_KISSFFT=1");

		// Compile and link with kissFFT
		string Kiss_FFTPath = Target.UEThirdPartySourceDirectory + "Kiss_FFT/kiss_fft129";

		PublicIncludePaths.Add(Kiss_FFTPath);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicLibraryPaths.Add(Kiss_FFTPath + "/lib/x64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/Debug");
			}
			else
			{
				PublicLibraryPaths.Add(Kiss_FFTPath + "/lib/x64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/Release");
			}

			PublicAdditionalLibraries.Add("KissFFT.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(Kiss_FFTPath + "/Lib/Mac/Debug/libKissFFT.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(Kiss_FFTPath + "/Lib/Mac/Release/libKissFFT.a");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			if (Target.LinkType == TargetLinkType.Monolithic)
			{
				PublicAdditionalLibraries.Add(Kiss_FFTPath + "/Lib/Linux/Release/" + Target.Architecture + "/libKissFFT.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(Kiss_FFTPath + "/Lib/Linux/Release/" + Target.Architecture + "/libKissFFT_fPIC.a");
			}
		}
	}
}
