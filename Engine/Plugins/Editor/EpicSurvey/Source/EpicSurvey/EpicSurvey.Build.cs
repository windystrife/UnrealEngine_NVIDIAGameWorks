// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EpicSurvey : ModuleRules
	{
        public EpicSurvey(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"Core",
					"CoreUObject",
					"Engine",
                    "InputCore",
					"ImageWrapper",
					"LevelEditor",
					"Slate",
                    "EditorStyle",
				}
			);	// @todo Mac: for some reason CoreUObject and Engine are needed to link in debug on Mac

            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"HTTP",
					"Json",
					"OnlineSubsystem",
					"SlateCore",
					"UnrealEd",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"MainFrame",
				}
			);

            DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					"MainFrame"
				}
			);
		}
	}
}
