// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Facebook : ModuleRules
{
	public Facebook(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		// Additional Frameworks and Libraries for Android found in OnlineSubsystemFacebook_UPL.xml
        if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			Definitions.Add("WITH_FACEBOOK=1");
			Definitions.Add("UE4_FACEBOOK_VER=4.18");

            // These are iOS system libraries that Facebook depends on (FBAudienceNetwork, FBNotifications)
            PublicFrameworks.AddRange(
            new string[] {
                "ImageIO"
            });

            // More dependencies for Facebook (FBAudienceNetwork, FBNotifications)
            PublicAdditionalLibraries.AddRange(
            new string[] {
                "xml2"
            });

            PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"AccountKit",
					"IOS/FacebookSDK/AccountKit.embeddedframework.zip",
					"AccountKit.framework/AccountKitAdditionalStrings.bundle"
				)
			);

			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"AccountKit",
					"IOS/FacebookSDK/AccountKit.embeddedframework.zip",
					"AccountKit.framework/AccountKitStrings.bundle"
				)
			);

			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"Bolts",
					"IOS/FacebookSDK/Bolts.embeddedframework.zip"
				)
			);

			// Add the FBAudienceNetwork framework
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBAudienceNetwork",
					"IOS/FacebookSDK/FBAudienceNetwork.embeddedframework.zip"
				)
			);

			// Access to Facebook notifications
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBNotifications",
					"IOS/FacebookSDK/FBNotifications.embeddedframework.zip"
				)
			);

			// Access to Facebook core
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBSDKCoreKit",
					"IOS/FacebookSDK/FBSDKCoreKit.embeddedframework.zip",
					"FBSDKCoreKit.framework/Resources/FacebookSDKStrings.bundle"
				)
			);

			// Access to Facebook login
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBSDKLoginKit",
					"IOS/FacebookSDK/FBSDKLoginKit.embeddedframework.zip"
				)
			);

			// Access to Facebook messenger sharing
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBSDKMessengerShareKit",
					"IOS/FacebookSDK/FBSDKMessengerShareKit.embeddedframework.zip"
				)
			);

			// Access to Facebook sharing
			PublicAdditionalFrameworks.Add(
				new UEBuildFramework(
					"FBSDKShareKit",
					"IOS/FacebookSDK/FBSDKShareKit.embeddedframework.zip"
				)
			);
		}
	}
}

