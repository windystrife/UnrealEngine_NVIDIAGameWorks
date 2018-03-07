// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class IOSApsalar : ModuleRules
	{
		public IOSApsalar( ReadOnlyTargetRules Target ) : base(Target)
		{
			BinariesSubFolder = "NotForLicensees";

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					// ... add private dependencies that you statically link with here ...
				}
				);

            PublicIncludePathModuleNames.Add("Analytics");

			PublicFrameworks.AddRange(
				new string[] {
					"CoreTelephony",
					"SystemConfiguration",
					"UIKit",
					"Foundation",
					"CoreGraphics",
					"MobileCoreServices",
					"StoreKit",
					"CFNetwork",
					"CoreData",
					"Security",
					"CoreLocation"
				});

            PublicAdditionalLibraries.AddRange(
				new string[] {
					"sqlite3",
					"z"
				});

            bool bHasApsalarSDK =
                (System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "Apsalar")) &&
                  System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "Apsalar", "IOS"))) ||
                (System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "NotForLicensees")) &&
                  System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "NotForLicensees", "Apsalar")) &&
                  System.IO.Directory.Exists(System.IO.Path.Combine(Target.UEThirdPartySourceDirectory, "NotForLicensees", "Apsalar", "IOS")));
            if (bHasApsalarSDK)
            {
                PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "NotForLicensees/Apsalar/IOS/");
                PublicAdditionalLibraries.Add(Target.UEThirdPartySourceDirectory + "NotForLicensees/Apsalar/IOS/libApsalar.a");

                Definitions.Add("WITH_APSALAR=1");
            }
            else
            {
                Definitions.Add("WITH_APSALAR=0");
            }
        }
	}
}
