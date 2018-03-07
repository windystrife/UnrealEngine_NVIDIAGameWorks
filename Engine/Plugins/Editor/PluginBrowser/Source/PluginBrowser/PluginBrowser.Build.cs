// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PluginBrowser : ModuleRules
	{
		public PluginBrowser(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",		// @todo Mac: for some reason CoreUObject and Engine are needed to link in debug on Mac
                    "InputCore",
					"Engine",
					"Slate",
					"SlateCore",
				}
			);
			
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"EditorStyle",
					"Projects",
					"UnrealEd",
					"PropertyEditor",
					"SharedSettingsWidgets",
					"DirectoryWatcher",
					"GameProjectGeneration",
					"MainFrame",
                    "UATHelper",
                    "AssetTools",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"DesktopPlatform",
					"GameProjectGeneration",
				}
			);

			// TODO: Move back to using this if we can remove dependencies on GameProjectUtils
			/*DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"GameProjectGeneration",
				}
			);*/
		}
	}
}
