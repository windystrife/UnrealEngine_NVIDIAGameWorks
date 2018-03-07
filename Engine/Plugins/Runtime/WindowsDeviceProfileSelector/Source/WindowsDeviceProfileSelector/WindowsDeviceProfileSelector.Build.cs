// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class WindowsDeviceProfileSelector : ModuleRules
	{
        public WindowsDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
                    "Runtime/WindowsDeviceProfileSelector/Public",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/WindowsDeviceProfileSelector/Private",
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

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
				}
				);
		}
	}
}
