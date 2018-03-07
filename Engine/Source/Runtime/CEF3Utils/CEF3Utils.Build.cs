// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CEF3Utils : ModuleRules
{
	public CEF3Utils(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/CEF3Utils/Public");
		PrivateIncludePaths.Add("Runtime/CEF3Utils/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64
		||  Target.Platform == UnrealTargetPlatform.Win32
		||  Target.Platform == UnrealTargetPlatform.Mac
		||  Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"CEF3"
				);
		}
	}
}
