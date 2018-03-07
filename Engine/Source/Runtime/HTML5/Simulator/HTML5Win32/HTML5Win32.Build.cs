// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
 
public class HTML5Win32 : ModuleRules
{
	public HTML5Win32(ReadOnlyTargetRules Target) : base(Target)
	{
        // Don't depend on UE types or modules.  
		AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
	}
}
