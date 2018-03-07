// Copyright 2017 Google Inc.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class GoogleVRController : ModuleRules
	{
		public GoogleVRController(ReadOnlyTargetRules Target) : base(Target)
		{
			bFasterWithoutUnity = true;

			string GoogleVRSDKDir = Target.UEThirdPartySourceDirectory + "GoogleVR/";
			PrivateIncludePaths.AddRange(
				new string[] {
					"GoogleVRController/Private",
					"GoogleVRController/Private/ArmModel",
					// ... add other private include paths required here ...
					GoogleVRSDKDir + "include",
					GoogleVRSDKDir + "include/vr/gvr/capi/include",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"ApplicationCore",
					"Engine",
					"InputCore",
					"InputDevice",
					"HeadMountedDisplay",
					"GoogleVRHMD",
					"UMG",
					"Slate",
					"SlateCore"
				}
				);

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.Add("Launch");
			}

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
				if(Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
				{	
					PublicIncludePaths.Add("Developer/Android/AndroidDeviceDetection/Public");
					PublicIncludePaths.Add("Developer/Android/AndroidDeviceDetection/Public/Interfaces");
					PrivateDependencyModuleNames.AddRange(
						new string[]
						{
							"GoogleVR",
							"GoogleInstantPreview",
						});
				}
			}

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "GoogleVR" });

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "GoogleVRController_APL.xml")));
			}
		}
	}
}
