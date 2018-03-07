// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SSL : ModuleRules
{
    public SSL(ReadOnlyTargetRules Target) : base(Target)
    {
        Definitions.Add("SSL_PACKAGE=1");

		bool bShouldUseModule =
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.PS4;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		if (bShouldUseModule)
		{
			Definitions.Add("WITH_SSL=1");

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Online/SSL/Private",
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}
		else
		{
			Definitions.Add("WITH_SSL=0");
		}
    }
}
