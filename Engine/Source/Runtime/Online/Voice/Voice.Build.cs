// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Voice : ModuleRules
{
	public Voice(ReadOnlyTargetRules Target) : base(Target)
	{
		Definitions.Add("VOICE_PACKAGE=1");

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"AndroidPermission"
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
			}
			);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Online/Voice/Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core"
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectSound");
		}
		else if(Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.AddRange(new string[] { "CoreAudio", "AudioUnit", "AudioToolbox" });
		}

		AddEngineThirdPartyPrivateStaticDependencies(Target, "libOpus");

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string ModulePath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(ModulePath, "AndroidVoiceImpl_UPL.xml")));
		}
	}
}


		



		
