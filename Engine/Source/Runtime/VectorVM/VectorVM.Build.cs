// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VectorVM : ModuleRules
{
	public VectorVM(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "Engine"
            }
        );

		PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
				"CoreUObject", 
                "Engine"
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/Engine/Classes/Curves"
            }
        );


    }
}
