// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealAtoS : ModuleRules
{
	public UnrealAtoS( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateIncludePathModuleNames.Add( "Launch" );
		PrivateIncludePaths.Add( "Runtime/Launch/Private" );
	
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"Projects",
                "ApplicationCore"
			}
			);
	}
}
