// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AndroidPlatformEditor : ModuleRules
{
	public AndroidPlatformEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "Android";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
				"EditorStyle",
                "EditorWidgets",
                "DesktopWidgets",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"SourceControl",
				"AndroidRuntimeSettings",
                "AndroidDeviceDetection",
                "TargetPlatform",
                "RenderCore",
                "MaterialShaderQualitySettings",
				"MainFrame",
                "AudioSettingsEditor"
            }
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

        // this is listed above, so it isn't really dynamically loaded, this just marks it as being platform specific.
		PlatformSpecificDynamicallyLoadedModuleNames.Add("AndroidRuntimeSettings");
	}
}
