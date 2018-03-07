// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SlateRemote : ModuleRules
	{
		public SlateRemote(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Networking",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"InputCore",
					"Slate",
					"SlateCore",
					"Sockets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"Settings",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"SlateRemote/Private",
					"SlateRemote/Private/Server",
					"SlateRemote/Private/Shared",
				});
		}
	}
}
