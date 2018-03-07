// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PortalRpc : ModuleRules
	{
		public PortalRpc(ReadOnlyTargetRules Target) : base(Target)
		{
            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
                }
            );

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "MessagingRpc",
				}
			);

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
                    "Messaging",
                    "MessagingCommon",
                    "MessagingRpc",
                    "PortalServices",
                }
            );

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Portal/Rpc/PortalRpc/Private",
				}
			);
		}
	}
}
