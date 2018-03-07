// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac)]
public class MinidumpDiagnosticsTarget : TargetRules
{
	public MinidumpDiagnosticsTarget( TargetInfo Target ) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "MinidumpDiagnostics";
	}

	// TargetRules interface.

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		bCompileLeanAndMeanUE = true;
		bCompileICU = false;

		// Don't need editor
		bBuildEditor = false;

		// MinidumpDiagnostics doesn't ever compile with the engine linked in
		bCompileAgainstEngine = false;

		bIncludeADO = false;
		//bCompileICU = false;

		// MinidumpDiagnostics.exe has no exports, so no need to verify that a .lib and .exp file was emitted by the linker.
		OutLinkEnvironmentConfiguration.bHasExports = false;

		// Do NOT produce additional console app exe
		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = true;

		OutCPPEnvironmentConfiguration.Definitions.Add("MINIDUMPDIAGNOSTICS=1");
        OutCPPEnvironmentConfiguration.Definitions.Add("NOINITCRASHREPORTER=1");
    }
}
