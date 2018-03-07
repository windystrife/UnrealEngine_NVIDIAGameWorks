// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrashDebugHelper : ModuleRules
{
	public CrashDebugHelper( ReadOnlyTargetRules Target ) : base(Target)
	{
        PrivateIncludePaths.AddRange(
		new string[] {
				"Developer/CrashDebugHelper/Private/",
				"Developer/CrashDebugHelper/Private/Linux",
				"Developer/CrashDebugHelper/Private/Mac",
				"Developer/CrashDebugHelper/Private/Windows",
                "Developer/CrashDebugHelper/Private/IOS",
            }
        );
		PrivateIncludePaths.Add( "ThirdParty/PLCrashReporter/plcrashreporter-master-5ae3b0a/Source" );

        if (Target.Type != TargetType.Game)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
                "Core",
                "SourceControl"
                }
            );
        }
        else
        {
            IsRedistributableOverride = true;
            PublicDependencyModuleNames.AddRange(
                new string[] {
                "Core",
                }
            );
        }
    }
}
