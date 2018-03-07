// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AddContentDialog : ModuleRules
{
	public AddContentDialog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"ContentBrowser"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"InputCore",
				"Json",
				"EditorStyle",
				"DirectoryWatcher",
				"DesktopPlatform",
				"PakFile",
				"ImageWrapper",
				"UnrealEd",
				"CoreUObject",				
				"WidgetCarousel",				
			
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/AddContentDialog/Private",
				"Editor/AddContentDialog/Private/ViewModels",
				"Editor/AddContentDialog/Private/ContentSourceProviders/AssetPack",
				"Editor/AddContentDialog/Private/ContentSourceProviders/FeaturePack",
			}
		);
	}
}
