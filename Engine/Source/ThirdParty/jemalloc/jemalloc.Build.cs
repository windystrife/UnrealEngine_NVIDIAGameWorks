// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class jemalloc : ModuleRules
{
	public jemalloc(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
		    // includes may differ depending on target platform
		    PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "jemalloc/include/Linux/" + Target.Architecture);
            if (Target.LinkType == TargetLinkType.Monolithic)
            {
                PublicAdditionalLibraries.Add(Target.UEThirdPartySourceDirectory + "jemalloc/lib/Linux/" + Target.Architecture + "/libjemalloc.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(Target.UEThirdPartySourceDirectory + "jemalloc/lib/Linux/" + Target.Architecture + "/libjemalloc_pic.a");
            }
        }
	}
}
