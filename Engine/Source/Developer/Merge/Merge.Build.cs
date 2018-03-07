// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Merge : ModuleRules
	{
        public Merge(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/Merge/Private",
				    "Kismet",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				    "AssetTools",
					"Core",
				    "CoreUObject",
				    "EditorStyle",
				    "Engine", // needed so that we can clone blueprints...
				    "GraphEditor",
				    "InputCore",
				    "Kismet",
					"PropertyEditor",
				    "Slate",
				    "SlateCore",
				    "SourceControl",
				    "UnrealEd",
				}
			);
		}
	}
}
