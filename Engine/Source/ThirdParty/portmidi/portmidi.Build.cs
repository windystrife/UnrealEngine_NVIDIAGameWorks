// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class portmidi : ModuleRules
{
	public portmidi(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "portmidi/include");

        if (Target.Platform == UnrealTargetPlatform.Win32)
        {
            PublicLibraryPaths.Add(Target.UEThirdPartySourceDirectory + "portmidi/lib/Win32");
            PublicAdditionalLibraries.Add("portmidi.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicLibraryPaths.Add(Target.UEThirdPartySourceDirectory + "portmidi/lib/Win64");
            PublicAdditionalLibraries.Add("portmidi_64.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(Target.UEThirdPartySourceDirectory + "portmidi/lib/Mac/libportmidi.a");
			PublicAdditionalFrameworks.Add( new UEBuildFramework( "CoreAudio" ));
			PublicAdditionalFrameworks.Add( new UEBuildFramework( "CoreMIDI" ));
        }
	}
}
