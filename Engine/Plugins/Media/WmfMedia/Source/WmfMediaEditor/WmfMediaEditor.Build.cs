// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WmfMediaEditor : ModuleRules
	{
		public WmfMediaEditor(ReadOnlyTargetRules Target) : base(Target)
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
					"WmfMediaEditor/Private",
				});
		}
	}
}
