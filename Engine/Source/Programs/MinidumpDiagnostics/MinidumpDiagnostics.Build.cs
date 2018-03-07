// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MinidumpDiagnostics : ModuleRules
{
	public MinidumpDiagnostics( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateIncludePathModuleNames.Add( "Launch" );
		PrivateIncludePaths.Add( "Runtime/Launch/Private" );
	
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"ApplicationCore",
				"CrashDebugHelper",
				"PerforceSourceControl",
				"Projects"
			}
			);
	}
}
