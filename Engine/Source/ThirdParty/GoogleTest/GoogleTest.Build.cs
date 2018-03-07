// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;

public class GoogleTest : ModuleRules
{
    public GoogleTest(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string RootPath = Target.UEThirdPartySourceDirectory + "GoogleTest/";
        string DefaultConfiguration = "MinSizeRel";

		// Includes
        PublicSystemIncludePaths.Add(RootPath + "include/");

        // Libraries
        string PartialLibraryPath = "lib/" + Target.Platform.ToString() + "/";
        string LibraryPath = RootPath;
       
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
        {
            PartialLibraryPath += "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                PartialLibraryPath += "/Debug";
            }
            else 
            {
                PartialLibraryPath += "/" + DefaultConfiguration;
            }

            //if (!Target.IsMonolithic)
            //{
            //    PartialLibraryPath += "_Shared";
            //}

            PartialLibraryPath += "/";
            LibraryPath += PartialLibraryPath;
            //i.e. Engine\Source\ThirdParty\GoogleTest\lib\Win64\VS2013\MinSizeRel_Shared\ 

            // I was unable to get non-monolithic windows builds working without crashing within the 
            // time box I was given for the integration. The workaround is to ensure that all tests
            // are included in the same dll when building monolithic, otherwise error messages
            // will not be routed properly.

            // We should re-investigate this integration problem in the future, as more teams want to  
            // adopt usage of the library

            //if (!Target.IsMonolithic)
            //{
            //    PublicAdditionalLibraries.Add("gmock_main.lib");
            //    RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/GoogleTest/" + PartialLibraryPath + "gmock_main.dll"));
            //}
            //else
            //{
                PublicAdditionalLibraries.Add("gtest.lib");
                PublicAdditionalLibraries.Add("gmock.lib");
                PublicAdditionalLibraries.Add("gmock_main.lib");
            //}
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            if (Target.LinkType == TargetLinkType.Monolithic)
            {
                PartialLibraryPath += DefaultConfiguration + "/";
                LibraryPath += PartialLibraryPath;

                PublicAdditionalLibraries.Add(LibraryPath + "libgtest.a");
                PublicAdditionalLibraries.Add(LibraryPath + "libgmock.a");
                PublicAdditionalLibraries.Add(LibraryPath + "libgmock_main.a");
            }
            else
            {
                PartialLibraryPath += DefaultConfiguration + "_Shared/";
                LibraryPath += PartialLibraryPath;

                PublicAdditionalLibraries.Add(LibraryPath + "libgmock_main.dylib");
            }
        }

        PublicLibraryPaths.Add(LibraryPath);

        // The including module will also need these enabled
        Definitions.Add("WITH_GOOGLE_MOCK=1");
		Definitions.Add("WITH_GOOGLE_TEST=1");

        Definitions.Add("GTEST_HAS_POSIX_RE=0");

        if (Target.LinkType != TargetLinkType.Monolithic)
        {
            //Definitions.Add("GTEST_LINKED_AS_SHARED_LIBRARY=1");
        }
    }
}
