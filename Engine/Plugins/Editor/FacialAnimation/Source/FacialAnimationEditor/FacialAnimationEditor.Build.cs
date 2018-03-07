// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class FacialAnimationEditor : ModuleRules
	{
		public FacialAnimationEditor(ReadOnlyTargetRules Target) : base(Target)
		{			
			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
                    "CoreUObject",
                    "InputCore",
                    "Engine",
                    "FacialAnimation",
                    "SlateCore",
                    "Slate",
					"EditorStyle",
					"UnrealEd",
                    "AudioEditor",
                    "WorkspaceMenuStructure",
                    "DesktopPlatform",
                    "PropertyEditor",
                    "TargetPlatform",
                    "Persona",
                }
			);
		}
	}
}
