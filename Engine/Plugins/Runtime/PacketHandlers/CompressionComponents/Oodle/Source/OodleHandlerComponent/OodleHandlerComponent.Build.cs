// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OodleHandlerComponent : ModuleRules
{
    public OodleHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
    {
		BinariesSubFolder = "NotForLicensees";
		
		PrivateIncludePaths.Add("OodleHandlerComponent/Private");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"PacketHandler",
				"Core",
				"CoreUObject",
				"Engine"
			});


		bool bHaveOodleSDK = false;
		string OodleNotForLicenseesLibDir = "";

		// Check the NotForLicensees folder first
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
        {
			OodleNotForLicenseesLibDir = System.IO.Path.Combine( Target.UEThirdPartySourceDirectory, "..", "..",
				"Plugins", "Runtime", "PacketHandlers", "CompressionComponents", "Oodle", "Source", "ThirdParty", "NotForLicensees",
				"Oodle", "215", "win", "lib" );
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			OodleNotForLicenseesLibDir = System.IO.Path.Combine( Target.UEThirdPartySourceDirectory, "..", "..",
				"Plugins", "Runtime", "PacketHandlers", "CompressionComponents", "Oodle", "Source", "ThirdParty", "NotForLicensees",
				"Oodle", "215", "linux", "lib" );
		}
		else if ( Target.Platform == UnrealTargetPlatform.PS4 )
		{
			OodleNotForLicenseesLibDir = System.IO.Path.Combine( Target.UEThirdPartySourceDirectory, "..", "..",
				"Plugins", "Runtime", "PacketHandlers", "CompressionComponents", "Oodle", "Source", "ThirdParty", "NotForLicensees",
				"Oodle", "215", "ps4", "lib" );
		}
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            OodleNotForLicenseesLibDir = System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "..", "..",
                "Plugins", "Runtime", "PacketHandlers", "CompressionComponents", "Oodle", "Source", "ThirdParty", "NotForLicensees",
                "Oodle", "215", "XboxOne", "lib");
        }

        if (OodleNotForLicenseesLibDir.Length > 0)
		{
			try
			{
				bHaveOodleSDK = System.IO.Directory.Exists( OodleNotForLicenseesLibDir );
			}
			catch ( System.Exception )
			{
			}
        }

		if ( bHaveOodleSDK )
		{
	        AddEngineThirdPartyPrivateStaticDependencies(Target, "Oodle");
	        PublicIncludePathModuleNames.Add("Oodle");
			Definitions.Add( "HAS_OODLE_SDK=1" );
		}
		else
		{
			Definitions.Add( "HAS_OODLE_SDK=0" );
		}
    }
}
