// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NUTUnrealEngine4 : ModuleRules
	{
		public NUTUnrealEngine4(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("NUTUnrealEngine4/Private");

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"NetcodeUnitTest"
				}
			);
		}
	}
}
