// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE4EditorServices : ModuleRules
{
	public UE4EditorServices(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");
		PublicIncludePaths.Add("Runtime/Launch/Private"); // Yuck. Required for RequiredProgramMainCPPInclude.h. (Also yuck).

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"DesktopPlatform",
				"Json",
				"Projects",
			}
		);
	}
}
