// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GraphColor : ModuleRules
{
	public GraphColor(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicIncludePaths.Add("Developer/GraphColor/Public");
		//PrivateIncludePaths.Add("Developer/GraphColor/Private");

        PrivateDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
