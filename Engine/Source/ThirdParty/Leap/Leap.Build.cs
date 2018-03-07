// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Leap : ModuleRules
{
    public Leap(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.Win64)
            || (Target.Platform == UnrealTargetPlatform.Win32))
		{
            PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "Leap/include");

            string LibraryPath = Target.UEThirdPartySourceDirectory + "Leap/lib";
			string LibraryName = "Leap";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				LibraryName += "d";
			}

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryPath += "/x64";
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				LibraryPath += "/x86";
			}

			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Leap/" + Target.Platform.ToString() + "/" + LibraryName + ".dll"));
		}
	}
}
