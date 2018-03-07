// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PerforceSourceControl : ModuleRules
{
	public PerforceSourceControl(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"SourceControl",
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Perforce");
		Definitions.Add("USE_P4_API=1");

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
		}
	}
}
