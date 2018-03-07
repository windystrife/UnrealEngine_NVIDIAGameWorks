// Copyright 2017 Google Inc.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class GoogleVRHMD : ModuleRules
	{
		public GoogleVRHMD(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"GoogleVRHMD/Private",
					"../../../../../Source/Runtime/Renderer/Private",
					"../../../../../Source/Runtime/Core/Private",
					"../../../../../Source/Runtime/ApplicationCore/Private"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"HeadMountedDisplay",
					"InputCore"
				}
				);

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.Add("Launch");
			}

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
						"GoogleInstantPreview"
					});
			}
			else if (Target.Platform != UnrealTargetPlatform.Mac)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "OpenGLDrv" });
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
				PrivateIncludePaths.AddRange(
					new string[] {
					"../../../../../Source/Runtime/OpenGLDrv/Private",
					// ... add other private include paths required here ...
					}
				);
			}

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "GoogleVR" });

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "GoogleVRHMD_APL.xml")));
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				PrivateDependencyModuleNames.AddRange(new string[] { "GoogleVR", "ApplicationCore" });

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add(new ReceiptProperty("IOSPlugin", Path.Combine(PluginPath, "GoogleVRHMD_APL.xml")));
			}
		}
	}
}
