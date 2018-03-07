// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Serialization : ModuleRules
	{
		public Serialization(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Json",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Serialization/Private",
				});
		}
	}
}
