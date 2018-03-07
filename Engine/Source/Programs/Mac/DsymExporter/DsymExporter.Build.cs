// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DsymExporter : ModuleRules
{
	public DsymExporter( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateIncludePathModuleNames.Add( "Launch" );
		PrivateIncludePaths.Add( "Runtime/Launch/Private" );
	
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"ApplicationCore",
				"Projects"
			}
			);
	}
}
