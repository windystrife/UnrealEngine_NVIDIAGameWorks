// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealTargetPlatform.Win32, UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac, UnrealTargetPlatform.Linux)]
[SupportedConfigurations(UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.Development, UnrealTargetConfiguration.Shipping)]
public class CrashReportClientTarget : TargetRules
{
	public CrashReportClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		UndecoratedConfiguration = UnrealTargetConfiguration.Shipping;

		LaunchModuleName = "CrashReportClient";

		if (Target.Platform != UnrealTargetPlatform.Linux)
		{
			ExtraModuleNames.Add("EditorStyle");
		}

        bOutputPubliclyDistributable = true;
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
		bCompileLeanAndMeanUE = true;

		// Don't need editor
		bBuildEditor = false;

		// CrashReportClient doesn't ever compile with the engine linked in
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		bUseLoggingInShipping = true;

		bIncludeADO = false;
		
		// Do not include ICU for Linux (this is a temporary workaround, separate headless CrashReportClient target should be created, see UECORE-14 for details).
		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			bCompileICU = false;
		}

		// CrashReportClient.exe has no exports, so no need to verify that a .lib and .exp file was emitted by
		// the linker.
		OutLinkEnvironmentConfiguration.bHasExports = false;

		bUseChecksInShipping = true;

		// Epic Games Launcher needs to run on OS X 10.9, so CrashReportClient needs this as well
		OutCPPEnvironmentConfiguration.bEnableOSX109Support = true;

		OutCPPEnvironmentConfiguration.Definitions.Add("NOINITCRASHREPORTER=1");
	}
}
