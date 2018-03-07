// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class OnlineSubsystemFacebook : ModuleRules
{
	public OnlineSubsystemFacebook(ReadOnlyTargetRules Target) : base(Target)
	{
		Definitions.Add("ONLINESUBSYSTEMFACEBOOK_PACKAGE=1");
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
				"OnlineSubsystem", 
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Facebook");

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			Definitions.Add("WITH_FACEBOOK=1");
			PrivateIncludePaths.Add("Private/IOS");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			bool bHasFacebookSDK = false;
			string FacebookNFLDir = "";
			try
			{
				FacebookNFLDir = System.IO.Path.Combine(ModuleDirectory, "ThirdParty", "Android", "NotForLicensees", "FacebookSDK");
				bHasFacebookSDK = System.IO.Directory.Exists(FacebookNFLDir);
			}
			catch (System.Exception)
			{
			}

			PrivateIncludePaths.Add("Private/Android");

			if (bHasFacebookSDK)
			{
				string Err = string.Format("Facebook SDK found in {0}", FacebookNFLDir);
				System.Console.WriteLine(Err);

				Definitions.Add("WITH_FACEBOOK=1");
				Definitions.Add("UE4_FACEBOOK_VER=4.19.0");

				PrivateDependencyModuleNames.AddRange(
				new string[] {
				"Launch",
				}
				);

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "OnlineSubsystemFacebook_UPL.xml")));
			}
			else
			{
				string Err = string.Format("Facebook SDK not found in {0}", FacebookNFLDir);
				System.Console.WriteLine(Err);
				Definitions.Add("WITH_FACEBOOK=0");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			Definitions.Add("WITH_FACEBOOK=1");
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
			Definitions.Add("WITH_FACEBOOK=0");
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
