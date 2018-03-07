// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
public class MaterialBaking : ModuleRules
{
	public MaterialBaking(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string [] {
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
                "RHI",                
                "UnrealEd",
                "ShaderCore",
                "MainFrame",
                "SlateCore",
                "Slate",
                "InputCore",
                "PropertyEditor",
                "EditorStyle",
                "Renderer",
                "RawMesh",
            }
		);
    }
}
