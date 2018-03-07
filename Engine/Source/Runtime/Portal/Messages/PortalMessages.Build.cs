// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PortalMessages : ModuleRules
	{
		public PortalMessages(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"MessagingRpc",
					"PortalServices",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Portal/Messages/Private",
				});
		}
	}
}
