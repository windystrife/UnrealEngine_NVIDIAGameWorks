// Copyright 2017 Google Inc.


using System.IO;


namespace UnrealBuildTool.Rules
{
	public class GoogleARCoreBase : ModuleRules
	{
		public GoogleARCoreBase(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[]
				{
					"GoogleARCoreBase/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"RHI",
					"RenderCore",
					"ShaderCore",
					"HeadMountedDisplay", // For IMotionController interface.
					"AndroidPermission",
					"GoogleARCoreRendering",
					"TangoSDK",
                    "AugmentedReality"
                }
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Settings" // For editor settings panel.
				}
			);

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// Additional dependencies on android...
				PrivateDependencyModuleNames.Add("Launch");

				// Register Plugin Language
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(ModuleDirectory, "GoogleARCoreBase_APL.xml")));
			}

			bFasterWithoutUnity = false;
		}
	}
}
