// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class QoSReporter : ModuleRules
	{
		public QoSReporter(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.Add("Core");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"HTTP",
					"Json",
				}
			);

			if (Target.Type != TargetType.Program)
			{
				PrivateDependencyModuleNames.Add("Engine"); // for GAverageFPS - unused by programs
			}

			// servers expose QoS metrics via perfcounters
			if (Target.Type != TargetType.Client && Target.Type != TargetType.Program)
			{
				PrivateDependencyModuleNames.Add("PerfCounters");
			}

			WhitelistRestrictedFolders.Add("Private/NotForLicensees");

			Definitions.Add("WITH_QOSREPORTER=1");
		}
	}
}
