// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class libcurl : ModuleRules
{
	public libcurl(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		Definitions.Add("WITH_LIBCURL=1");

		string NewLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/7_48_0/";
		string LibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/curl-7.47.1/";

		// TODO: latest recompile for consoles and mobile platforms
		string OldLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/";

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string platform = "/Linux/" + Target.Architecture;
			string IncludePath = NewLibCurlPath + "include" + platform;
			string LibraryPath = NewLibCurlPath + "lib" + platform;

			PublicIncludePaths.Add(IncludePath);
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryPath + "/libcurl.a");

			PrivateDependencyModuleNames.Add("SSL");
		}

		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// toolchain will filter properly
            PublicIncludePaths.Add(OldLibCurlPath + "include/Android/ARMv7");
            PublicLibraryPaths.Add(OldLibCurlPath + "lib/Android/ARMv7");
            PublicIncludePaths.Add(OldLibCurlPath + "include/Android/ARM64");
            PublicLibraryPaths.Add(OldLibCurlPath + "lib/Android/ARM64");
            PublicIncludePaths.Add(OldLibCurlPath + "include/Android/x86");
            PublicLibraryPaths.Add(OldLibCurlPath + "lib/Android/x86");
            PublicIncludePaths.Add(OldLibCurlPath + "include/Android/x64");
            PublicLibraryPaths.Add(OldLibCurlPath + "lib/Android/x64");

			PublicAdditionalLibraries.Add("curl");
//			PublicAdditionalLibraries.Add("crypto");
//			PublicAdditionalLibraries.Add("ssl");
//			PublicAdditionalLibraries.Add("dl");
        }

   		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string PlatformSubdir = "/Mac/";
			PublicIncludePaths.Add(LibCurlPath + "include" + PlatformSubdir);
			// OSX needs full path
			PublicAdditionalLibraries.Add(LibCurlPath + "lib" + PlatformSubdir + "libcurl.a");
		}

		else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(LibCurlPath + "include/" + Target.Platform.ToString() +  "/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			PublicLibraryPaths.Add(LibCurlPath + "lib/" + Target.Platform.ToString() +  "/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());

			PublicAdditionalLibraries.Add("libcurl_a.lib");
			Definitions.Add("CURL_STATICLIB=1");
		}
	}
}
