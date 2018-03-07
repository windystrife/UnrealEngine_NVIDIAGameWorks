// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibOVRAudio : ModuleRules
{
	public LibOVRAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		/** Mark the current version of the Oculus SDK */
		Type = ModuleType.External;

		string OculusThirdPartyDirectory = Target.UEThirdPartySourceDirectory + "Oculus/LibOVRAudio/LibOVRAudio";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(OculusThirdPartyDirectory + "/include");

			string LibraryPath = OculusThirdPartyDirectory + "/lib/win64";
			string LibraryName = "ovraudio.link64";

			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");
		}
	}
}
