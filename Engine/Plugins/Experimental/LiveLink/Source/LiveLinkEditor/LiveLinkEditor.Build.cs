// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkEditor : ModuleRules
	{
		public LiveLinkEditor(ReadOnlyTargetRules Target) : base(Target)
        {
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "UnrealEd",
                    "Engine",
                    "Projects",

                    "WorkspaceMenuStructure",
                    "EditorStyle",
                    "SlateCore",
                    "Slate",
                    "InputCore",

                    //"Messaging",
                    "LiveLinkInterface",
					"LiveLinkMessageBusFramework",
                    "BlueprintGraph",
					"LiveLink",
                    "AnimGraph",
                }
			); 

			PrivateIncludePaths.Add("/LiveLinkEditor/Private");
		}
	}
}
