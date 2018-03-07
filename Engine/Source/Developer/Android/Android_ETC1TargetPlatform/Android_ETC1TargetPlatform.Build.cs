// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Android_ETC1TargetPlatform : ModuleRules
{
	public Android_ETC1TargetPlatform( ReadOnlyTargetRules Target ) : base(Target)
	{
		BinariesSubFolder = "Android";

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Runtime/Core/Public/Android"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"TargetPlatform",
				"DesktopPlatform",
				"AndroidDeviceDetection",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Developer/Android/AndroidTargetPlatform/Private",				
			}
		);

        Definitions.Add("WITH_OGGVORBIS=1");

		// compile with Engine
		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");		//@todo android: AndroidTargetPlatform.Build
		}
	}
}
