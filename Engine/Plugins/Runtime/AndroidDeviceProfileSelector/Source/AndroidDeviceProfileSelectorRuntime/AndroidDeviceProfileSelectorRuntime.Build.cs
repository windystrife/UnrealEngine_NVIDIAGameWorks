// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class AndroidDeviceProfileSelectorRuntime : ModuleRules
	{
        public AndroidDeviceProfileSelectorRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "AndroidDPSRT";

			PublicIncludePaths.AddRange(
				new string[] {
					"Runtime/AndroidDeviceProfileSelectorRuntime/Public",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/AndroidDeviceProfileSelectorRuntime/Private",
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
					"AndroidDeviceProfileSelector",
				}
				);
		}
	}
}
