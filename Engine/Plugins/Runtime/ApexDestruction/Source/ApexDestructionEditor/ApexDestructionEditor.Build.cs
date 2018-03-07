// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ApexDestructionEditor : ModuleRules
{
	public ApexDestructionEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
				"Engine",
                "ApexDestruction",
                "AssetTools",
                "PhysX",
                "APEX",
                "InputCore",
                "RenderCore",
                "RHI",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "UnrealEd",
                "DesktopPlatform",
                "AssetRegistry",
                "ContentBrowser",
                "Projects",
                "ApexDestructionLib"
            }
		);

        AddEngineThirdPartyPrivateStaticDependencies(Target,
            "FBX"
        );

        SetupModulePhysXAPEXSupport(Target);
}
}
