// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DirectShow : ModuleRules
{
	public DirectShow(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{

            string DirectShowLibPath = Target.UEThirdPartySourceDirectory
                + "DirectShow/DirectShow-1.0.0/Lib/" + ((Target.Platform == UnrealTargetPlatform.Win32) ? "Win32" : "Win64") + "/vs" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";

			PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "DirectShow/DirectShow-1.0.0/src/Public");


            PublicLibraryPaths.Add( DirectShowLibPath );

			string LibraryName = "DirectShow";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				LibraryName += "d";
			}
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryName += "_64";
			}
			LibraryName += ".lib";
			PublicAdditionalLibraries.Add(LibraryName);
		}
	}
}

