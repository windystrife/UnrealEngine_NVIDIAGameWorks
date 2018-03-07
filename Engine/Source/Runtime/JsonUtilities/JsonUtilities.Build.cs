// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JsonUtilities : ModuleRules
{
	public JsonUtilities( ReadOnlyTargetRules Target ) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Json",
			}
		);
	}
}
