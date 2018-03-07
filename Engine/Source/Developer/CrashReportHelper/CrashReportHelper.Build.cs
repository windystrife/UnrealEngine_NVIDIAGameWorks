// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrashReportHelper : ModuleRules
{
	public CrashReportHelper( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateIncludePaths.AddRange(
		new string[] {
				"Developer/CrashReportHelper/Private/",
                "Developer/CrashReportHelper/Private/Linux",
                "Developer/CrashReportHelper/Private/Mac",
                "Developer/CrashReportHelper/Private/Windows",
                "Developer/CrashReportHelper/Private/IOS",
            }
        );

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
                "CrashDebugHelper",
                "XmlParser",
                "Analytics",
                "AnalyticsET",
                 "HTTP",
                "Json",
           }
        );

        if (Target.Type == TargetType.Game)
        {
            IsRedistributableOverride = true;
        }
        else
        {
            PrecompileForTargets = PrecompileTargetsType.None;
            PublicDependencyModuleNames.Add("SourceControl");
        }
    }
}
