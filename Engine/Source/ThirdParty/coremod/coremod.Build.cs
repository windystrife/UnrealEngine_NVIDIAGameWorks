// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class coremod: ModuleRules
{
	public coremod(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string CoreModVersion = "4.2.6";
		string LibraryPath = Target.UEThirdPartySourceDirectory + "coremod/coremod-" + CoreModVersion + "/";

		PublicIncludePaths.Add(LibraryPath + "include/coremod");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(LibraryPath + "/lib/Win64/VS2013");
			PublicAdditionalLibraries.Add("coremod.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(LibraryPath + "/lib/Win32/VS2013");
			PublicAdditionalLibraries.Add("coremod.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(LibraryPath + "/lib/Mac/libcoremodMac.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicLibraryPaths.Add(LibraryPath + "/lib/IOS");
			PublicAdditionalShadowFiles.Add(LibraryPath + "/lib/IOS/libcoremod.a");
		}
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(LibraryPath + "/lib/Linux/" + Target.Architecture + "/" + "libcoremodLinux.a");
        }
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string AndroidPath = LibraryPath + "/lib/Android/";
			// No 64bit support for Android yet
			PublicLibraryPaths.Add(AndroidPath + "armeabi-v7a");
			PublicLibraryPaths.Add(AndroidPath + "x86");
			PublicLibraryPaths.Add(AndroidPath + "x64");
			PublicAdditionalLibraries.Add("xmp-coremod");			
		}
	}
}
