// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class nvTessLib : ModuleRules
{
	public nvTessLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// The header file also contains the source, so we don't actually need to link anything in.
		PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "nvtesslib/inc");
	}
}
