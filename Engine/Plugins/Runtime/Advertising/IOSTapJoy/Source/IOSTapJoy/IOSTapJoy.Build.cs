// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class IOSTapJoy : ModuleRules
	{
		public IOSTapJoy( ReadOnlyTargetRules Target ) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					// ... add other private include paths required here ...
				}
				);

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
					"Advertising",
					"ApplicationCore",
					// ... add private dependencies that you statically link with here ...
				}
				);

			PublicIncludePathModuleNames.Add( "Advertising" );

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);

			// Add the TapJoy framework
			PublicAdditionalFrameworks.Add( 
				new UEBuildFramework( 
					"TapJoy",														// Framework name
					"../../ThirdPartyFrameworks/Tapjoy.embeddedframework.zip",		// Zip name
					"Resources/TapjoyResources.bundle"								// Resources we need copied and staged
				)
			); 

			PublicFrameworks.AddRange( 
				new string[] 
				{ 
					"EventKit",
					"MediaPlayer",
					"AdSupport",
					"CoreLocation",
					"SystemConfiguration",
					"MessageUI",
					"Security",
					"CoreTelephony",
					"Twitter",
					"Social"
				}
				);
		}
	}
}
