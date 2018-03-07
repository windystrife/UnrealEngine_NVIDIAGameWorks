// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class OpenGL : ModuleRules
{
	public OpenGL(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;


		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			PublicAdditionalLibraries.Add("opengl32.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalFrameworks.Add(new UEBuildFramework("OpenGL"));
			PublicAdditionalFrameworks.Add(new UEBuildFramework("QuartzCore"));
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalFrameworks.Add(new UEBuildFramework("OpenGLES"));
		}
	}
}
