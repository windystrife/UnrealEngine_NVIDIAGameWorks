// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ReferenceViewer : ModuleRules
	{
        public ReferenceViewer(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
                new string[] {
				    "Core",
				    "CoreUObject",
					"ApplicationCore",
				    "Engine",
                    "InputCore",
				    "Slate",
					"SlateCore",
                    "EditorStyle",
				    "RenderCore",
                    "GraphEditor",
				    "UnrealEd",
			    }
			);

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
				    "AssetRegistry",
					"EditorWidgets",
					"CollectionManager",
					"SizeMap",
			    }
            );

            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
				    "AssetRegistry",
					"CollectionManager",
					"EditorWidgets",
					"SizeMap",
			    }
            );
		}
	}
}
