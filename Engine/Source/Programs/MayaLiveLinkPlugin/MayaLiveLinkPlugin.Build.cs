// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MayaLiveLinkPlugin : ModuleRules
{
	public MayaLiveLinkPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		// For LaunchEngineLoop.cpp include.  You shouldn't need to add anything else to this line.
		PrivateIncludePaths.AddRange( new string[] { "Runtime/Launch/Public", "Runtime/Launch/Private" }  );

		// Unreal dependency modules
		PrivateDependencyModuleNames.AddRange( new string[] 
		{
			"Core",
            "CoreUObject",
			"ApplicationCore",
			"Projects",
            "UdpMessaging",
            "LiveLinkInterface",
            "LiveLinkMessageBusFramework",
		} );


		//
		// Maya SDK setup
		//

		{
			string MayaVersionString = "2016";
			string MayaInstallFolder = @"C:\Program Files\Autodesk\Maya" + MayaVersionString;

			// Make sure this version of Maya is actually installed
			if( Directory.Exists( MayaInstallFolder ) )
			{
                //throw new BuildException( "Couldn't find Autodesk Maya " + MayaVersionString + " in folder '" + MayaInstallFolder + "'.  This version of Maya must be installed for us to find the Maya SDK files." );

                // These are required for Maya headers to compile properly as a DLL
                Definitions.Add("NT_PLUGIN=1");
                Definitions.Add("REQUIRE_IOSTREAM=1");

                PrivateIncludePaths.Add(Path.Combine(MayaInstallFolder, "include"));

                if (Target.Platform == UnrealTargetPlatform.Win64)  // @todo: Support other platforms?
                {
                    PublicLibraryPaths.Add(Path.Combine(MayaInstallFolder, "lib"));

                    // Maya libraries we're depending on
                    PublicAdditionalLibraries.AddRange(new string[]
                        {
                            "Foundation.lib",
                            "OpenMaya.lib",
                            "OpenMayaAnim.lib" }
                    );
                }
            }
		}
	}
}
