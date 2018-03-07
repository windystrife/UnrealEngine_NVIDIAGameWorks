// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;
using System.Diagnostics;
public class OpenAL : ModuleRules
{
	public OpenAL(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		string version = "1.15.1";

		string OpenALPath = Target.UEThirdPartySourceDirectory + "OpenAL/" + version + "/";
		PublicIncludePaths.Add(OpenALPath + "include");

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicLibraryPaths.Add(OpenALPath + "lib/Linux/" + Target.Architecture);
			PublicAdditionalLibraries.Add("openal");

			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/OpenAL/Linux/" + Target.Architecture + "/libopenal.so.1"));
		}
	}
}
