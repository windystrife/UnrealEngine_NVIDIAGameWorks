// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SessionServices : ModuleRules
{
	public SessionServices(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"EngineMessages",
				"SessionMessages",
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Messaging",
				"MessagingCommon",
			});
	}
}
