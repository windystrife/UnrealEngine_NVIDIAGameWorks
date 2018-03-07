// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IOSPlatformEditor : ModuleRules
{
	public IOSPlatformEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "IOS";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"DesktopPlatform",
				"Engine",
				"MainFrame",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"SourceControl",
				"IOSRuntimeSettings",
				"TargetPlatform",
				"MaterialShaderQualitySettings",
				"RenderCore",
                "AudioSettingsEditor"
            }
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"GameProjectGeneration",
				"Settings",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"GameProjectGeneration",
				}
		);

        // this is listed above, so it isn't really dynamically loaded, this just marks it as being platform specific.
		PlatformSpecificDynamicallyLoadedModuleNames.Add("IOSRuntimeSettings");
	}
}
