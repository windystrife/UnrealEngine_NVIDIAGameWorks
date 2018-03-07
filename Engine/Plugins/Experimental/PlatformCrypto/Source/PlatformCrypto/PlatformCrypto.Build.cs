// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PlatformCrypto : ModuleRules
	{
		public PlatformCrypto(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Experimental/PlatformCrypto/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
				);

			if (Target.Platform == UnrealTargetPlatform.XboxOne)
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoBCrypt",
					}
					);

				PublicIncludePathModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoBCrypt"
					}
					);
			}
			else
			{
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoOpenSSL",
					}
					);

				PublicIncludePathModuleNames.AddRange(
					new string[]
					{
						"PlatformCryptoOpenSSL"
					}
					);
			}
		}
	}
}
