// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UdpMessaging : ModuleRules
	{
		public UdpMessaging(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Messaging",
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"Json",
					"Networking",
					"Serialization",
					"Sockets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"MessagingCommon",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"UdpMessaging/Private",
					"UdpMessaging/Private/Shared",
					"UdpMessaging/Private/Transport",
					"UdpMessaging/Private/Transport/Tests",
					"UdpMessaging/Private/Tunnel",
				});

			if (Target.Type == TargetType.Editor)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
						"Settings",
					});

				PrivateIncludePathModuleNames.AddRange(
					new string[] {
						"Settings",
					});
			}
		}
	}
}
