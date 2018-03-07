// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SpeedTreeImporter : ModuleRules
	{
		public SpeedTreeImporter(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
                    "MainFrame",
					"Editor/SpeedTreeImporter/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "Core",
				    "CoreUObject",
				    "Engine",
				    "Slate",
					"SlateCore",
                    "EditorStyle",
                    "InputCore",
				    "RawMesh",
                    "UnrealEd",
                    "MainFrame"
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
				}
				);
				
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SpeedTree");
		}
	}
}
