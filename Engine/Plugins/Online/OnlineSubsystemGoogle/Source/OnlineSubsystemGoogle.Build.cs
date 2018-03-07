// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OnlineSubsystemGoogle : ModuleRules
{
	public OnlineSubsystemGoogle(ReadOnlyTargetRules Target) : base(Target)
	{
		Definitions.Add("ONLINESUBSYSTEMGOOGLE_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add("Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"HTTP",
				"ImageCore",
				"Json",
				"Sockets",
				"OnlineSubsystem", 
			}
			);

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			Definitions.Add("WITH_GOOGLE=1");
			Definitions.Add("UE4_GOOGLE_VER=4.0.1");
		   	PrivateIncludePaths.Add("Private/IOS");

			// These are iOS system libraries that Google depends on
			PublicFrameworks.AddRange(
			new string[] {
				"SafariServices",
				"SystemConfiguration"
			});

			PublicAdditionalFrameworks.Add(
			new UEBuildFramework(
				"GoogleSignIn",
				"ThirdParty/IOS/GoogleSignInSDK/GoogleSignIn.embeddedframework.zip",
				"GoogleSignIn.bundle"
			)
			);

			PublicAdditionalFrameworks.Add(
			new UEBuildFramework(
				"GoogleAppUtilities",
				"ThirdParty/IOS/GoogleSignInSDK/GoogleAppUtilities.embeddedframework.zip"
			)
			);

			PublicAdditionalFrameworks.Add(
			new UEBuildFramework(
				"GoogleSignInDependencies",
				"ThirdParty/IOS/GoogleSignInSDK/GoogleSignInDependencies.embeddedframework.zip"
			)
			);

			PublicAdditionalFrameworks.Add(
			new UEBuildFramework(
				"GoogleSymbolUtilities",
				"ThirdParty/IOS/GoogleSignInSDK/GoogleSymbolUtilities.embeddedframework.zip"
			)
			);

		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Launch",
			}
			);

			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "OnlineSubsystemGoogle_UPL.xml")));

			PrivateIncludePaths.Add("Private/Android");
			
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateIncludePaths.Add("Private/Windows");
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			PrivateIncludePaths.Add("Private/XboxOne");
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PrivateIncludePaths.Add("Private/PS4");
		}
		else
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
