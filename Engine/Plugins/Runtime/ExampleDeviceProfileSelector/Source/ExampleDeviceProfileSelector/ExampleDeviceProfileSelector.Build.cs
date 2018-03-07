// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class ExampleDeviceProfileSelector : ModuleRules
	{
        public ExampleDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
                    "Runtime/ExampleDeviceProfileSelector/Public",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/ExampleDeviceProfileSelector/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				    "Core",
				    "CoreUObject",
				    "Engine",
				}
				);
		}
	}
}
