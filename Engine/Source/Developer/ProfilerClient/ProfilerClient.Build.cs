// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProfilerClient : ModuleRules
	{
		public ProfilerClient(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePathModuleNames.Add("ProfilerService");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"ProfilerMessages",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
                    "MessagingCommon",
                    "SessionServices",
				}
			);
		}
	}
}
