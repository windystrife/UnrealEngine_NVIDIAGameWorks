// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BlueprintCompilerCppBackend : ModuleRules
	{
        public BlueprintCompilerCppBackend(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/BlueprintCompilerCppBackend/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
                    "CoreUObject",
                    "Engine",
					"KismetCompiler",
                    "UnrealEd",
                    "BlueprintGraph",
				}
				);
            PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					"UMG",
                    "SlateCore",
                    "MovieScene",
				}
                );
		}
	}
}
