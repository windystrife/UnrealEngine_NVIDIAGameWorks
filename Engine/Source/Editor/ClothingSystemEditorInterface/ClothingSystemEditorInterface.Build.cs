// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ClothingSystemEditorInterface : ModuleRules
{
	public ClothingSystemEditorInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/ClothingSystemEditorInterface/Private");

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
            }
        );
    }
}
