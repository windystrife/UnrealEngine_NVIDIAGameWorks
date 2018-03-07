// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AESHandlerComponent : ModuleRules
{
    public AESHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"AESHandlerComponent/Private",
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"PacketHandler",
				"PlatformCrypto",
			}
			);

		PublicIncludePathModuleNames.AddRange(
			new string[]
			{
				"PlatformCrypto"
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
		}
		else
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"PlatformCryptoOpenSSL",
				}
				);
		}
	}
}
