// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class XORBlockEncryptor : ModuleRules
{
    public XORBlockEncryptor(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "BlockEncryptionHandlerComponent"
            }
        );

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
