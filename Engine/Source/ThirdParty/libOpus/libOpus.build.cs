// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libOpus : ModuleRules
{
	public libOpus(ReadOnlyTargetRules Target) : base(Target)
	{
		/** Mark the current version of the library */
		string OpusVersion = "1.1";
		Type = ModuleType.External;

		PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "libOpus/opus-" + OpusVersion + "/include");
		string LibraryPath = Target.UEThirdPartySourceDirectory + "libOpus/opus-" + OpusVersion + "/";

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			LibraryPath += "Windows/VS2012/";
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryPath += "x64/";
			}
			else
			{
				LibraryPath += "win32/";
			}

			LibraryPath += "Release/";

			PublicLibraryPaths.Add(LibraryPath);

 			PublicAdditionalLibraries.Add("silk_common.lib");
 			PublicAdditionalLibraries.Add("silk_float.lib");
 			PublicAdditionalLibraries.Add("celt.lib");
			PublicAdditionalLibraries.Add("opus.lib");
			PublicAdditionalLibraries.Add("speex_resampler.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string OpusPath = LibraryPath + "/Mac/libopus.a";
			string SpeexPath = LibraryPath + "/Mac/libspeex_resampler.a";

			PublicAdditionalLibraries.Add(OpusPath);
			PublicAdditionalLibraries.Add(SpeexPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            if (Target.LinkType == TargetLinkType.Monolithic)
            {
                PublicAdditionalLibraries.Add(LibraryPath + "Linux/" + Target.Architecture + "/libopus.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(LibraryPath + "Linux/" + Target.Architecture + "/libopus_fPIC.a");
            }

			if (Target.Architecture.StartsWith("x86_64"))
			{
				if (Target.LinkType == TargetLinkType.Monolithic)
				{
					PublicAdditionalLibraries.Add(LibraryPath + "Linux/" + Target.Architecture + "/libresampler.a");
				}
				else
				{
					PublicAdditionalLibraries.Add(LibraryPath + "Linux/" + Target.Architecture + "/libresampler_fPIC.a");
				}
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(LibraryPath + "Android/ARMv7/");
			PublicLibraryPaths.Add(LibraryPath + "Android/ARM64/");
			PublicLibraryPaths.Add(LibraryPath + "Android/x64/");
			
			PublicAdditionalLibraries.Add("opus");
			PublicAdditionalLibraries.Add("speex_resampler");
		}
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            LibraryPath += "XboxOne/VS2015/Release/";

            PublicLibraryPaths.Add(LibraryPath);

            PublicAdditionalLibraries.Add("silk_common.lib");
            PublicAdditionalLibraries.Add("silk_float.lib");
            PublicAdditionalLibraries.Add("celt.lib");
            PublicAdditionalLibraries.Add("opus.lib");
            PublicAdditionalLibraries.Add("speex_resampler.lib");
        }
    }
}
