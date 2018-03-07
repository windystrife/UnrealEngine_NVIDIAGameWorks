// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class MikkTSpace : ModuleRules
{
	public MikkTSpace(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string MikkTSpacePath = Target.UEThirdPartySourceDirectory + "MikkTSpace/";

		PublicIncludePaths.Add(MikkTSpacePath + "inc/");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(MikkTSpacePath + "lib/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName()); 
			PublicAdditionalLibraries.Add("MikkTSpace.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(MikkTSpacePath + "lib/Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName()); 
			PublicAdditionalLibraries.Add("MikkTSpace.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(MikkTSpacePath + "/lib/Linux/" + Target.Architecture + "/libMikkTSpace.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(MikkTSpacePath + "/lib/Mac/libMikkTSpace.a");
		}
	}
}
