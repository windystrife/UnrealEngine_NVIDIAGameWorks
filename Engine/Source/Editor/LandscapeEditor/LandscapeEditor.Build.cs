// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LandscapeEditor : ModuleRules
{
	public LandscapeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"Engine",
				"Landscape",
				"RenderCore",
                "RHI",
                "InputCore",
				"UnrealEd",
				"PropertyEditor",
				"ImageWrapper",
                "EditorWidgets",
                "Foliage",
				"ViewportInteraction",
				"VREditor",
			}
			);

		CircularlyReferencedDependentModules.AddRange(
			new string[]
			{
				"ViewportInteraction",
				"VREditor"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"DesktopPlatform",
				"ContentBrowser",
                "AssetTools",
				"LevelEditor"
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"DesktopPlatform",
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			// VS2015 updated some of the CRT definitions but not all of the Windows SDK has been updated to match.
			// Microsoft provides this shim library to enable building with VS2015 until they fix everything up.
			//@todo: remove when no longer neeeded (no other code changes should be necessary).
			if (Target.WindowsPlatform.bNeedsLegacyStdioDefinitionsLib)
			{
				PublicAdditionalLibraries.Add("legacy_stdio_definitions.lib");
			}
		}

		// KissFFT is used by the smooth tool.
		if (Target.bCompileLeanAndMeanUE == false &&
			(Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Kiss_FFT");
		}
		else
		{
			Definitions.Add("WITH_KISSFFT=0");
		}
	}
}
