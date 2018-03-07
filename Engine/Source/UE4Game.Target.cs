// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class UE4GameTarget : TargetRules
{
	public UE4GameTarget( TargetInfo Target ) : base(Target)
	{
		Type = TargetType.Game;
		BuildEnvironment = TargetBuildEnvironment.Shared;

		// Output to Engine/Binaries/<PLATFORM> even if built as monolithic
		bOutputToEngineBinaries = true;

		ExtraModuleNames.Add("UE4Game");
	}

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
        if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			// to make World Explorers as small as possible we excluded some items from the engine.
			// uncomment below to make a smaller iOS build
			/*bCompileRecast = false;
			bCompileSpeedTree = false;
			bCompileAPEX = false;
			bCompileLeanAndMeanUE = true;
			bCompilePhysXVehicle = false;
			bCompileFreeType = false;
			bCompileForSize = true;*/
		}
	}
}
