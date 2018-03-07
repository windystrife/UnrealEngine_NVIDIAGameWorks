// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class IOSDeviceProfileSelector : ModuleRules
	{
        public IOSDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
                    "Runtime/IOSDeviceProfileSelector/Public",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/IOSDeviceProfileSelector/Private",
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
