// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MobileLauncherProfileWizard : ModuleRules
	{
		public MobileLauncherProfileWizard(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
                    "Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "LauncherServices",
                    "CoreUObject",
                    "TargetPlatform",
                    "DesktopPlatform",
                    "JSON",
                    "Slate",
                    "SlateCore",
                    "InputCore",
                    "EditorStyle",
                    "AppFramework",
                    "Projects",
                    // ... add private dependencies that you statically link with here ...
				}
                );

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);
		}
	}
}
