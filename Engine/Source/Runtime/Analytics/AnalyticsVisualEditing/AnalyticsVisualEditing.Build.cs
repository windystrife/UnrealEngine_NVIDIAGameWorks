// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnalyticsVisualEditing : ModuleRules
	{
        public AnalyticsVisualEditing(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
					"Analytics",
    				"Core",
	    			"CoreUObject",
                    "Engine"
				}
			);
		}
	}
}
