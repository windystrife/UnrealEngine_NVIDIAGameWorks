// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CollectionManager : ModuleRules
{
	public CollectionManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"DirectoryWatcher",
				"SourceControl"
			}
			);
	}
}
