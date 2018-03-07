// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LibOVRPlatform : ModuleRules
{
	public LibOVRPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;		
		
		string OculusThirdPartyDirectory = Target.UEThirdPartySourceDirectory + "Oculus/LibOVRPlatform/LibOVRPlatform";

		bool isLibrarySupported = false;
		
		if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicAdditionalLibraries.Add(OculusThirdPartyDirectory + "/lib/LibOVRPlatform32_1.lib");
			isLibrarySupported = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(OculusThirdPartyDirectory + "/lib/LibOVRPlatform64_1.lib");
			isLibrarySupported = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicAdditionalLibraries.Add(OculusThirdPartyDirectory + "/lib/libovrplatformloader.so");
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "LibOVRPlatform_APL.xml")));
			isLibrarySupported = true;
		}
		else
		{
			System.Console.WriteLine("Oculus Platform SDK not supported for this platform");
		}
		
		if (isLibrarySupported)
		{
			PublicIncludePaths.Add(Path.Combine( OculusThirdPartyDirectory, "include" ));
		}
	}
}
