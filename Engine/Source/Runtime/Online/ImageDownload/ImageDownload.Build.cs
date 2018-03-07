// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImageDownload : ModuleRules
{
	public ImageDownload(ReadOnlyTargetRules Target) : base(Target)
    {
        Definitions.Add("IMAGEDOWNLOAD_PACKAGE=1");

        PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"HTTP",
				"SlateCore",
				"ImageWrapper",
			}
			);
    }
}
