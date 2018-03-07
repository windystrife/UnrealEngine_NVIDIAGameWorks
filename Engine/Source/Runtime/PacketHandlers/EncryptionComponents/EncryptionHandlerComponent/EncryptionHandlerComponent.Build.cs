// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class EncryptionHandlerComponent : ModuleRules
{
    public EncryptionHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "PacketHandler",
                "BlockEncryptionHandlerComponent",
                "StreamEncryptionHandlerComponent",
                "RSAEncryptionHandlerComponent",
            }
        );

		PrecompileForTargets = PrecompileTargetsType.None;
    }
}
