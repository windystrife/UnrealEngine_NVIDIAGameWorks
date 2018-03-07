// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class PLCrashReporter : ModuleRules
{
	public PLCrashReporter(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string PLCrashReporterPath = Target.UEThirdPartySourceDirectory + "PLCrashReporter/plcrashreporter-master-5ae3b0a/";

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicSystemIncludePaths.Add(PLCrashReporterPath + "Source");
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(PLCrashReporterPath + "Mac/Debug/libCrashReporter-MacOSX-Static.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(PLCrashReporterPath + "Mac/Release/libCrashReporter-MacOSX-Static.a");
			}
		}
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicSystemIncludePaths.Add(PLCrashReporterPath + "Source");
            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add(PLCrashReporterPath + "IOS/Debug/libCrashReporter-iphoneos.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(PLCrashReporterPath + "IOS/Release/libCrashReporter-iphoneos.a");
            }
        }
    }
}
