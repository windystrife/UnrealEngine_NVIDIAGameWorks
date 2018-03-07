// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class PIEPreviewDeviceProfileSelector : ModuleRules
	{
        public PIEPreviewDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePathModuleNames.Add("Engine");

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
					"RHI",
					"Json",
					"JsonUtilities",
					"MaterialShaderQualitySettings",
					"Slate",
				}
				);
		}
	}
}
