// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UnrealEdMessages : ModuleRules
	{
		public UnrealEdMessages(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				});

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Editor/UnrealEdMessages/Private",
				});
		}
	}
}
