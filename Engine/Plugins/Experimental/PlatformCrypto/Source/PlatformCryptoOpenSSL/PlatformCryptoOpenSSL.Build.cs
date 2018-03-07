// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PlatformCryptoOpenSSL : ModuleRules
	{
		public PlatformCryptoOpenSSL(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"PlatformCryptoOpenSSL/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
				);

			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"PlatformCrypto"
				}
				);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}
	}
}
