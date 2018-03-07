// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class XORStreamEncryptor : ModuleRules
{
    public XORStreamEncryptor(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "StreamEncryptionHandlerComponent"
            }
        );

		PrecompileForTargets = PrecompileTargetsType.None;
    }
}
