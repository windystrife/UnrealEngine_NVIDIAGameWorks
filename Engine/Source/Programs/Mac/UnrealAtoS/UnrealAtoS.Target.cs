// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealTargetPlatform.Mac)]
public class UnrealAtoSTarget : TargetRules
{
	public UnrealAtoSTarget( TargetInfo Target ) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "UnrealAtoS";
	}

	// TargetRules interface.

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		bCompileLeanAndMeanUE = true;

		// Don't need editor
		bBuildEditor = false;

		// DsymExporter doesn't ever compile with the engine linked in
		bCompileAgainstEngine = false;

		// DsymExporter has no exports, so no need to verify that a .lib and .exp file was emitted by the linker.
		OutLinkEnvironmentConfiguration.bHasExports = false;

        bCompileAgainstCoreUObject = false;

		// Do NOT produce additional console app exe
		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = true;
	}
}
