// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ClothingSystemEditor : ModuleRules
{
	public ClothingSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/ClothingSystemEditor/Private");

        PublicIncludePathModuleNames.Add("UnrealEd");
        PublicIncludePathModuleNames.Add("ClothingSystemRuntime");
        PublicIncludePathModuleNames.Add("ClothingSystemEditorInterface");

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Engine",
                "RenderCore"
            }
        );

        PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
                "ClothingSystemRuntime",
                "ContentBrowser",
                "UnrealEd",
                "SlateCore",
                "Slate",
                "Persona",
                "ClothingSystemEditorInterface"
            }
		);
        
        SetupModulePhysXAPEXSupport(Target);
	}
}
