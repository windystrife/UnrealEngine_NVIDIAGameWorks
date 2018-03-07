// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	using System.IO;

	public class RenderDocPlugin : ModuleRules
	{
		public RenderDocPlugin(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { });            
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core"
				, "CoreUObject"
				, "Engine"
				, "InputCore"
				, "DesktopPlatform"
				, "Projects"
				, "RenderCore"
				, "InputDevice"
                , "MainFrame"
				, "RHI"				// RHI module: required for accessing the UE4 flag GUsingNullRHI.
			});

			if (Target.bBuildEditor == true)
			{
				DynamicallyLoadedModuleNames.AddRange(new string[] { "LevelEditor" });
				PublicDependencyModuleNames.AddRange(new string[]
				{
					"Slate"
					, "SlateCore"
					, "EditorStyle"
					, "UnrealEd"
					, "MainFrame"
					, "GameProjectGeneration"
				});
			}

            AddEngineThirdPartyPrivateStaticDependencies(Target, "RenderDoc");
        }
	}
}
