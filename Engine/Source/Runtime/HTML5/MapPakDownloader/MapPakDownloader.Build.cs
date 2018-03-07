// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MapPakDownloader: ModuleRules
	{
        public MapPakDownloader(ReadOnlyTargetRules Target) : base(Target)
		{
    		PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
                    "CoreUObject",
                    "Engine",
                    "Slate",
					// ... add other public dependencies that you statically link with here ...
				}
				);
    	}
	}
}
