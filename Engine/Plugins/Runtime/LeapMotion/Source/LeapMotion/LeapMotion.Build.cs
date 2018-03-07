// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class LeapMotion : ModuleRules
	{
        private string ModulePath
        {
            get { return ModuleDirectory; }
        }

        private string ThirdPartyPath
        {
            get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/")); }
        }

        private string BinariesPath
        {
            get { return Path.GetFullPath(Path.Combine(ModulePath, "../../Binaries/")); }
        }

        private string LibraryPath
        {
            get { return Path.GetFullPath(Path.Combine(ThirdPartyPath, "LeapSDK","Lib")); }
        }

		public LeapMotion(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					"LeapMotion/Public",
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"LeapMotion/Private",
                    Path.Combine(ThirdPartyPath, "LeapSDK", "Include"),
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"Core",
                    "CoreUObject",
                    "InputCore",
                    "Slate",
                    "SlateCore",
                    "HeadMountedDisplay",
                    "RHI",
                    "RenderCore",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);

            LoadLeapLib(Target);
		}

        public bool LoadLeapLib(ReadOnlyTargetRules Target)
        {
            bool isLibrarySupported = false;

            if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
            {
                isLibrarySupported = true;

                string PlatformString = (Target.Platform == UnrealTargetPlatform.Win64) ? "Win64" : "Win32";

                PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, PlatformString, "Leap.lib"));

                PublicDelayLoadDLLs.Add("Leap.dll");
                RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/LeapMotion/" + Target.Platform.ToString() + "/" + "Leap.dll"));
            }
            else if (Target.Platform == UnrealTargetPlatform.Mac){

                isLibrarySupported = true;

                string PlatformString = "Mac";
                PublicAdditionalLibraries.Add(Path.Combine(BinariesPath, PlatformString, "libLeap.dylib"));

            }

            return isLibrarySupported;
        }
	}
}
