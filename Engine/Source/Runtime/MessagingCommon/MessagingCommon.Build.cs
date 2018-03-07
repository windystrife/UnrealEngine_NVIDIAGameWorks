// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MessagingCommon : ModuleRules
	{
		public MessagingCommon(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				});

			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"Messaging",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/MessagingCommon/Private",
				});
		}
	}
}
