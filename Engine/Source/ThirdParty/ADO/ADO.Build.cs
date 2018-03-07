// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ADO : ModuleRules
{
	public ADO(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			PublicIncludePaths.Add("$(CommonProgramFiles)");
		}
	}
}

