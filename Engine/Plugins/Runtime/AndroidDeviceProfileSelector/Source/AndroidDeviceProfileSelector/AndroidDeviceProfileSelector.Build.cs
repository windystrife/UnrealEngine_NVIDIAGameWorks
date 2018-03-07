// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class AndroidDeviceProfileSelector : ModuleRules
	{
        public AndroidDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "AndroidDPS";

			PublicIncludePaths.AddRange(
				new string[] {
					"Runtime/AndroidDeviceProfileSelector/Public",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/AndroidDeviceProfileSelector/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
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
