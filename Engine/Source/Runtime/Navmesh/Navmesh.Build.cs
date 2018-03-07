// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Navmesh : ModuleRules
	{
		public Navmesh(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicIncludePaths.Add("Runtime/Navmesh/Public");
			PrivateIncludePaths.Add("Runtime/Navmesh/Private");

            PrivateDependencyModuleNames.AddRange(new string[] { "Core" });
		}
	}
}
