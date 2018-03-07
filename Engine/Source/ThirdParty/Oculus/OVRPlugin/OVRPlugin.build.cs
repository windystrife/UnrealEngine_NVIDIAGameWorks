// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OVRPlugin : ModuleRules
{
    public OVRPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

		string SourceDirectory = Target.UEThirdPartySourceDirectory + "Oculus/OVRPlugin/OVRPlugin/";

		PublicIncludePaths.Add(SourceDirectory + "Include");

        PublicIncludePaths.Add(SourceDirectory + "ExtIncludes");

        if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(SourceDirectory + "Lib/armeabi-v7a/");
            PublicAdditionalLibraries.Add("OVRPlugin");

            PublicLibraryPaths.Add(SourceDirectory + "ExtLibs/");
            PublicAdditionalLibraries.Add("vrapi");
        }
        if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(SourceDirectory + "Lib/Win64/");
			PublicAdditionalLibraries.Add("OVRPlugin.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 )
		{
			PublicLibraryPaths.Add(SourceDirectory + "Lib/Win32/");
			PublicAdditionalLibraries.Add("OVRPlugin.lib");
		}
    }
}