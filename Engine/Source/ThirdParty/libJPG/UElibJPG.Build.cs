// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UElibJPG : ModuleRules
{
	public UElibJPG(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

		string libJPGPath = Target.UEThirdPartySourceDirectory + "libJPG";
		PublicIncludePaths.Add(libJPGPath);

        // cpp files being used like header files in implementation
        PublicAdditionalShadowFiles.Add(libJPGPath + "/jpgd.cpp");
        PublicAdditionalShadowFiles.Add(libJPGPath + "/jpge.cpp");

        bEnableShadowVariableWarnings = false;
    }
}

