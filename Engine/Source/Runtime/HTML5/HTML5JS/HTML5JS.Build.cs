// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTML5JS : ModuleRules
{
	// Does not depend on any Unreal modules.
	// UBT doesn't automatically understand .js code and the fact that it needs to be linked in or not.
	public HTML5JS(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			PublicAdditionalLibraries.Add("Runtime/HTML5/HTML5JS/Private/HTML5JavaScriptFx.js");
		}
	}
}
