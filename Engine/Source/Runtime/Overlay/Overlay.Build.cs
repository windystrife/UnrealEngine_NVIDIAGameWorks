// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Overlay : ModuleRules
	{
		public Overlay(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
                new string[] {
				    "Core",
                    "CoreUObject",
                    "Engine"
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Slate",
                    "SlateCore",
                    "UMG",
                }
            );

            PublicIncludePaths.AddRange(
                new string[]
                {
                    "Runtime/Overlay/Public",
                }
            );

            PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Overlay/Private",
					"Runtime/Overlay/Private/Assets",
					"Runtime/Overlay/Private/Factories",
				}
			);
        }
	}
}