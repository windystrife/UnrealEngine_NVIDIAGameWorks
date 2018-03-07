// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SteamVR : ModuleRules
	{
		public SteamVR(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"SteamVR/Private",
					"../../../../../Source/Runtime/Renderer/Private",
					"../../../../../Source/Runtime/VulkanRHI/Private",
					// ... add other private include paths required here ...
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
                    "UtilityShaders",
					"Renderer",
					"ShaderCore",
                    "InputCore",
					"HeadMountedDisplay",
					"Slate",
					"SlateCore",
					"ProceduralMeshComponent"
				}
				);
            
            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }

            if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
            {
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenVR");
                PrivateDependencyModuleNames.Add("D3D11RHI");     //@todo steamvr: multiplatform

                AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
                PrivateDependencyModuleNames.Add("OpenGLDrv");

                AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
                PrivateDependencyModuleNames.Add("VulkanRHI");
            }
            else if (Target.Platform == UnrealTargetPlatform.Mac)
            {
				PublicFrameworks.Add("IOSurface");
                AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenVR");
                PrivateDependencyModuleNames.AddRange(new string[] { "MetalRHI" });
            }
            else if (Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64"))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenVR");
                AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
                PrivateDependencyModuleNames.Add("OpenGLDrv");
                PrivateDependencyModuleNames.Add("VulkanRHI");
            }
		}
	}
}
