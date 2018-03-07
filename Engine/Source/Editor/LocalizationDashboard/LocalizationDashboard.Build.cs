// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LocalizationDashboard : ModuleRules
{
	public LocalizationDashboard(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
                "PropertyEditor",
                "Localization"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
                "UnrealEd",
                "EditorStyle",
				"DesktopPlatform",
                "TranslationEditor",
                "MainFrame",
                "SourceControl",
                "SharedSettingsWidgets",
                "Localization",
				"LocalizationCommandletExecution",
				"LocalizationService",
				"InternationalizationSettings",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Editor/LocalizationDashboard/Private",
			}
		);

        PublicIncludePaths.AddRange(
			new string[]
			{
				"Editor/LocalizationDashboard/Public",
			}
		);

        CircularlyReferencedDependentModules.AddRange(
           new string[] {
                "LocalizationService",
				"MainFrame",
				"TranslationEditor"
            }
           );
	}
}
