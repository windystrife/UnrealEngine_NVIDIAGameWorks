// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealCEFSubProcess : ModuleRules
{
	public UnrealCEFSubProcess(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Programs/UnrealCEFSubProcess/Private",
				"Runtime/Launch/Private",					// for LaunchEngineLoop.cpp include
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ApplicationCore",
				"Projects",
				"CEF3Utils",
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"CEF3"
			);
			
		bEnableShadowVariableWarnings = false;
	}
}

