// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Documentation : ModuleRules
	{
		public Documentation(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/Documentation/Private",
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
					// ... add private dependencies that you statically link with here ...
                    "CoreUObject",
                    "Engine",
                    "InputCore",
                    "Slate",
					"SlateCore",
                    "EditorStyle",
                    "UnrealEd",
					"Analytics",
					"SourceCodeAccess",
					"SourceControl",
                    "ContentBrowser",
					"DesktopPlatform"
				}
			);

			CircularlyReferencedDependentModules.Add("SourceControl");

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
                    "MessageLog"
				}
			);
		}
	}
}
