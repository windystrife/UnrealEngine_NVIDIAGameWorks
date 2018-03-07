// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class PacketHandler : ModuleRules
{
    public PacketHandler(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateIncludePaths.Add("PacketHandler/Private");

        PublicDependencyModuleNames.AddRange
		(
            new string[]
			{
				"Core",
				"CoreUObject",
                "ReliabilityHandlerComponent",
            }
        );

        CircularlyReferencedDependentModules.Add("ReliabilityHandlerComponent");
    }
}
