// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AutomationWorker : ModuleRules
	{
		public AutomationWorker(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			); 
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AutomationMessages",
					"CoreUObject",
                    "Analytics",
    				"AnalyticsET",
					"Json",
					"JsonUtilities"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"MessagingCommon",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/AutomationWorker/Private",
				}
			);

			if (Target.bCompileAgainstEngine)
			{
				PrivateDependencyModuleNames.Add("Engine");
				PrivateDependencyModuleNames.Add("RHI");
			}
		}
	}
}
