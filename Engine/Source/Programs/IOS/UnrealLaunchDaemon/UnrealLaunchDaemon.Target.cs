// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealTargetPlatform.IOS)]
public class UnrealLaunchDaemonTarget : TargetRules
{
	public UnrealLaunchDaemonTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		bUsesSlate = false;
		//PlatformType = TargetRules.TargetPlatformType.Mobile;
		//bRequiresUnrealHeaderGeneration = true;
		AdditionalPlugins.Add("UdpMessaging");

		LaunchModuleName = "UnrealLaunchDaemon";
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
		bBuildEditor = false;
		bCompileAgainstEngine = false;

		OutLinkEnvironmentConfiguration.bHasExports = false;
	}
}
