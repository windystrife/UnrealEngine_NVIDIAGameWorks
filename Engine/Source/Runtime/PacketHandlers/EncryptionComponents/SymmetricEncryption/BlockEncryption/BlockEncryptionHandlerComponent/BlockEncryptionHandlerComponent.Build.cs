// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BlockEncryptionHandlerComponent : ModuleRules
{
    public BlockEncryptionHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "PacketHandler",
                "XORBlockEncryptor",
            }
        );

        CircularlyReferencedDependentModules.Add("XORBlockEncryptor");

        if ((Target.Platform == UnrealTargetPlatform.Win64) ||
        (Target.Platform == UnrealTargetPlatform.Win32))
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target,
                "CryptoPP"
                );
        }

        PublicIncludePathModuleNames.Add("CryptoPP");

		PrecompileForTargets = PrecompileTargetsType.None;
    }
}
