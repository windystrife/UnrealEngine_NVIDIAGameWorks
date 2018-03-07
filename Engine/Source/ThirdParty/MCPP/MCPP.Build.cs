// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MCPP : ModuleRules
{
	public MCPP(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "MCPP/mcpp-2.7.2/inc");

		string LibPath = Target.UEThirdPartySourceDirectory + "MCPP/mcpp-2.7.2/lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            LibPath += ("Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			PublicLibraryPaths.Add(LibPath);
			PublicAdditionalLibraries.Add("mcpp_64.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
        {
            LibPath += ("Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			PublicLibraryPaths.Add(LibPath);
			PublicAdditionalLibraries.Add("mcpp.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(LibPath + "Mac/libmcpp.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			LibPath += "Linux/" + Target.Architecture;
			PublicAdditionalLibraries.Add(LibPath + "/libmcpp.a");
		}
	}
}

