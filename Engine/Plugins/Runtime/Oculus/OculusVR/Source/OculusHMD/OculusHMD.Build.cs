// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class OculusHMD : ModuleRules
	{
		public OculusHMD(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					// Relative to Engine\Plugins\Runtime\Oculus\OculusVR\Source
					"../../../../../Source/Runtime/Renderer/Private",
					"../../../../../Source/Runtime/OpenGLDrv/Private",
					"../../../../../Source/Runtime/VulkanRHI/Private",
					"../../../../../Source/Runtime/Engine/Classes/Components",
				});

			PublicIncludePathModuleNames.Add("Launch");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"HeadMountedDisplay",
					"Slate",
					"SlateCore",
					"ImageWrapper",
					"MediaAssets",
					"Analytics",
					"UtilityShaders",
					"OpenGLDrv",
					"VulkanRHI",
					"OVRPlugin",
				});

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");

			if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
			{
				// D3D
				{
					PrivateDependencyModuleNames.AddRange(
						new string[]
						{
							"D3D11RHI",
							"D3D12RHI",
						});

					PrivateIncludePaths.AddRange(
						new string[]
						{
                            "../../../../../Source/Runtime/Windows/D3D11RHI/Private",
							"../../../../../Source/Runtime/Windows/D3D11RHI/Private/Windows",
							"../../../../../Source/Runtime/D3D12RHI/Private",
							"../../../../../Source/Runtime/D3D12RHI/Private/Windows",
						});

					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11Audio");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectSound");
				}

				// Vulkan
				{
                    string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");
                    bool bSDKInstalled = !String.IsNullOrEmpty(VulkanSDKPath);
                    bool bUseThirdParty = true;
                    if (bSDKInstalled)
                    {
                        // Check if the installed SDK is newer or the same than the provided headers distributed with the Engine
                        int ThirdPartyVersion = GetThirdPartyVersion();
                        int SDKVersion = GetSDKVersion(VulkanSDKPath);
                        if (SDKVersion >= ThirdPartyVersion)
                        {
                            // If the user has an installed SDK, use that instead
                            PrivateIncludePaths.Add(VulkanSDKPath + "/Include");
                            // Older SDKs have an extra subfolder
                            PrivateIncludePaths.Add(VulkanSDKPath + "/Include/vulkan");

                            if (Target.Platform == UnrealTargetPlatform.Win32)
                            {
                                PublicLibraryPaths.Add(VulkanSDKPath + "/Source/lib32");
                            }
                            else
                            {
                                PublicLibraryPaths.Add(VulkanSDKPath + "/Source/lib");
                            }

                            PublicAdditionalLibraries.Add("vulkan-1.lib");
                            PublicAdditionalLibraries.Add("vkstatic.1.lib");
                            bUseThirdParty = false;
                        }
                    }
                    if (bUseThirdParty)
                    {
                        AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
                    }
				}

				// OVRPlugin
				{
					PublicDelayLoadDLLs.Add("OVRPlugin.dll");
					RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/" + Target.Platform.ToString() + "/OVRPlugin.dll"));
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// Vulkan
				{
					string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
					// Note: header is the same for all architectures so just use arch-arm
					string NDKVulkanIncludePath = NDKPath + "/platforms/android-24/arch-arm/usr/include/vulkan";

					if (File.Exists(NDKVulkanIncludePath + "/vulkan.h"))
					{
						// Use NDK Vulkan header if discovered
						PrivateIncludePaths.Add(NDKVulkanIncludePath);
					}
					else 
					{
						string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");

						if (!String.IsNullOrEmpty(VulkanSDKPath))
						{
							// If the user has an installed SDK, use that instead
							PrivateIncludePaths.Add(VulkanSDKPath + "/Include/vulkan");
						}
						else
						{
							// Fall back to the Windows Vulkan SDK (the headers are the same)
							PrivateIncludePaths.Add(Target.UEThirdPartySourceDirectory + "Vulkan/Windows/Include/vulkan");
						}
					}
				}

				// AndroidPlugin
				{
					string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
					AdditionalPropertiesForReceipt.Add(new ReceiptProperty("AndroidPlugin", Path.Combine(PluginPath, "GearVR_APL.xml")));
				}
			}
		}

        static int GetVersionFromString(string Text)
        {
            string Token = "#define VK_HEADER_VERSION ";
            Int32 FoundIndex = Text.IndexOf(Token);
            if (FoundIndex > 0)
            {
                string Version = Text.Substring(FoundIndex + Token.Length, 5);
                int Index = 0;
                while (Version[Index] >= '0' && Version[Index] <= '9')
                {
                    ++Index;
                }

                Version = Version.Substring(0, Index);

                int VersionNumber = Convert.ToInt32(Version);
                return VersionNumber;
            }

            return -1;
        }
        static int GetThirdPartyVersion()
        {
            try
            {
                // Extract current version on ThirdParty
                string Text = File.ReadAllText("ThirdParty/Vulkan/Windows/Include/vulkan/vulkan.h");
                return GetVersionFromString(Text);
            }
            catch (Exception)
            {
            }

            return -1;
        }

        static int GetSDKVersion(string VulkanSDKPath)
        {
            try
            {
                // Extract current version on the SDK folder
                string Header = Path.Combine(VulkanSDKPath, "Include/vulkan/vulkan.h");
                string Text = File.ReadAllText(Header);
                return GetVersionFromString(Text);
            }
            catch (Exception)
            {
            }

            return -1;
        }
    }
}