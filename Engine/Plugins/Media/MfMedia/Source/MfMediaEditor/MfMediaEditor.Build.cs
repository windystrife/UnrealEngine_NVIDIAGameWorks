// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MfMediaEditor : ModuleRules
	{
		public MfMediaEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaAssets",
					"UnrealEd",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MfMediaEditor/Private",
				});
		}
	}
}
