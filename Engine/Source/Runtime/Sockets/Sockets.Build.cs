// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sockets : ModuleRules
{
	public Sockets(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DerivedDataCache"
			});

		PrivateIncludePaths.Add("Runtime/Sockets/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"SandboxFile"
			});

		Definitions.Add("SOCKETS_PACKAGE=1");
	}
}
