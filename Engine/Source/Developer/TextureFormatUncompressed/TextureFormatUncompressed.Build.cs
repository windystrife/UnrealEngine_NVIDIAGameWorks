// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatUncompressed : ModuleRules
{
	public TextureFormatUncompressed(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"TextureCompressor",
				"Engine"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ImageCore"
			}
			);
	}
}
