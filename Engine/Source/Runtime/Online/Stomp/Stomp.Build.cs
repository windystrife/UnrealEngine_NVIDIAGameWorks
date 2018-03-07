// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Stomp : ModuleRules
{
    public Stomp(ReadOnlyTargetRules Target) : base(Target)
    {
        Definitions.Add("STOMP_PACKAGE=1");

		bool bShouldUseModule = 
			Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.XboxOne ||
			Target.Platform == UnrealTargetPlatform.PS4;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core"
			}
		);

		if (bShouldUseModule)
		{
			Definitions.Add("WITH_STOMP=1");

			PrivateIncludePaths.AddRange(
				new string[]
				{
				"Runtime/Online/Stomp/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"WebSockets"
				}
			);
		}
		else
		{
			Definitions.Add("WITH_STOMP=0");
		}
	}
}
