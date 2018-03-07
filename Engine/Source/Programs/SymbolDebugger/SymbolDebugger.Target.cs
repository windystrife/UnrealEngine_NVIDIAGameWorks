// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac)]
public class SymbolDebuggerTarget : TargetRules
{
	public SymbolDebuggerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;

		LaunchModuleName = "SymbolDebugger";
        ExtraModuleNames.Add("EditorStyle");
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
		OutCPPEnvironmentConfiguration.Definitions.Add("WITH_DATABASE_SUPPORT=1");

		bCompileLeanAndMeanUE = true;

		// Don't need editor
		bBuildEditor = false;

		// SymbolDebugger doesn't ever compile with the engine linked in
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;

		bIncludeADO = true;

		// SymbolDebugger.exe has no exports, so no need to verify that a .lib and .exp file was emitted by
		// the linker.
		OutLinkEnvironmentConfiguration.bHasExports = false;
	}
}
