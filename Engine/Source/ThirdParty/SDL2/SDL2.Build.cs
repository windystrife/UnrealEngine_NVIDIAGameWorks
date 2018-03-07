// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class SDL2 : ModuleRules
{
	public SDL2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string SDL2Path = Target.UEThirdPartySourceDirectory + "SDL2/SDL-gui-backend/";
		string SDL2LibPath = SDL2Path + "lib/";

		// assume SDL to be built with extensions
		Definitions.Add("SDL_WITH_EPIC_EXTENSIONS=1");

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicIncludePaths.Add(SDL2Path + "include");
			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				// Debug version should be built with -fPIC and usable in all targets
				PublicAdditionalLibraries.Add(SDL2LibPath + "Linux/" + Target.Architecture + "/libSDL2_fPIC_Debug.a");
			}
			else if (Target.LinkType == TargetLinkType.Monolithic)
			{
				PublicAdditionalLibraries.Add(SDL2LibPath + "Linux/" + Target.Architecture + "/libSDL2.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(SDL2LibPath + "Linux/" + Target.Architecture + "/libSDL2_fPIC.a");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "SDL2/HTML5/SDL2-master/include/");
			SDL2LibPath = Target.UEThirdPartySourceDirectory + "SDL2/HTML5/SDL2-master/libs/";
			PublicAdditionalLibraries.Add(SDL2LibPath + "/libSDL2.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(SDL2Path + "include");

			SDL2LibPath += "Win64/";

			PublicAdditionalLibraries.Add(SDL2LibPath + "SDL2.lib");

			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/SDL2/Win64/SDL2.dll"));
			PublicDelayLoadDLLs.Add("SDL2.dll");
		}

	}
}
