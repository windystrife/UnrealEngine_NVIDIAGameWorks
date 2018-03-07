// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UE4ClientTarget : TargetRules
{

    public UE4ClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		BuildEnvironment = TargetBuildEnvironment.Shared;
        bOutputToEngineBinaries = true;
		ExtraModuleNames.Add("UE4Game");
	}

	//
	// TargetRules interface.
	//

	public override void SetupGlobalEnvironment(
        TargetInfo Target,
        ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
        ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
        )
    {
    }
}
