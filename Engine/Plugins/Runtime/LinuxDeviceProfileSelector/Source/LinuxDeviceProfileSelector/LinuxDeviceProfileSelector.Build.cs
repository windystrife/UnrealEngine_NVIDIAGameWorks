// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class LinuxDeviceProfileSelector : ModuleRules
	{
        public LinuxDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
                    "Runtime/LinuxDeviceProfileSelector/Public",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/LinuxDeviceProfileSelector/Private",
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
