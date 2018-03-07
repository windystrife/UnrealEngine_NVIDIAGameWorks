// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class MTLPP : ModuleRules
{
	public MTLPP(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string MTLPPPath = Target.UEThirdPartySourceDirectory + "mtlpp/mtlpp-master-7efad47/";

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicSystemIncludePaths.Add(MTLPPPath + "Source");
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(MTLPPPath + "Mac/libmtlppd.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(MTLPPPath + "Mac/libmtlpp.a");
			}
		}
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicSystemIncludePaths.Add(MTLPPPath + "Source");
            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add(MTLPPPath + "IOS/libmtlppd.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(MTLPPPath + "IOS/libmtlpp.a");
            }
        }
    }
}
