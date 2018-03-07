// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTML5PlatformFeatures : ModuleRules
{
	public HTML5PlatformFeatures(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] 
			{ 
				"Core", 
				"Engine" 
			});
	}
}
