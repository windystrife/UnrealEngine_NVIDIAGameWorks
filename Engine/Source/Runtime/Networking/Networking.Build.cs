// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Networking : ModuleRules
	{
		public Networking(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("Runtime/Networking/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Sockets"
				});
		}
	}
}
