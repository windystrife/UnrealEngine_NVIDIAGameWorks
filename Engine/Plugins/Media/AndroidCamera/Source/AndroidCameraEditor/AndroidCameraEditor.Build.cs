// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AndroidCameraEditor : ModuleRules
	{
		public AndroidCameraEditor(ReadOnlyTargetRules Target) : base(Target)
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
					"AndroidCameraEditor/Private",
				});
		}
	}
}
