// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MfMediaFactory : ModuleRules
	{
		public MfMediaFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			// this is for Xbox and Windows, so it's using public APIs, so we can distribute it in binary
			bOutputPubliclyDistributable = true;

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaAssets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
					"MfMedia",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MfMediaFactory/Private",
				});

			if (Target.Platform == UnrealTargetPlatform.XboxOne)
			{
				DynamicallyLoadedModuleNames.Add("MfMedia");
			}
		}
	}
}
