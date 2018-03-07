// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class VulkanRHI : ModuleRules
{
	public VulkanRHI(ReadOnlyTargetRules Target) : base(Target)
	{
		bOutputPubliclyDistributable = true;

		PrivateIncludePaths.Add("Runtime/Vulkan/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"CoreUObject", 
				"Engine", 
				"RHI", 
				"RenderCore", 
				"ShaderCore",
				"UtilityShaders",
				"HeadMountedDisplay",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
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
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");

			string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");
			bool bSDKInstalled = !String.IsNullOrEmpty(VulkanSDKPath);
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux || !bSDKInstalled)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			}
			else
			{
				PrivateIncludePaths.Add(VulkanSDKPath + "/include");
				PrivateIncludePaths.Add(VulkanSDKPath + "/include/vulkan");
				PublicLibraryPaths.Add(VulkanSDKPath + "/lib");
				PublicAdditionalLibraries.Add("vulkan");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Android || Target.Platform == UnrealTargetPlatform.Mac)
		{
			string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");

			bool bHaveVulkan = false;
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// Note: header is the same for all architectures so just use arch-arm
				string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
				string NDKVulkanIncludePath = NDKPath + "/platforms/android-24/arch-arm/usr/include/vulkan";

				// Use NDK Vulkan header if discovered, or VulkanSDK if available
				if (File.Exists(NDKVulkanIncludePath + "/vulkan.h"))
				{
					bHaveVulkan = true;
					PrivateIncludePaths.Add(NDKVulkanIncludePath);
				}
				else
				if (!String.IsNullOrEmpty(VulkanSDKPath))
				{
					// If the user has an installed SDK, use that instead
					bHaveVulkan = true;
					PrivateIncludePaths.Add(VulkanSDKPath + "/Include/vulkan");
				}
				else
				{
					// Fall back to the Windows Vulkan SDK (the headers are the same)
					bHaveVulkan = true;
					PrivateIncludePaths.Add(Target.UEThirdPartySourceDirectory + "Vulkan/Windows/Include/vulkan");
				}
			}
			else if (!String.IsNullOrEmpty(VulkanSDKPath))
			{
				bHaveVulkan = true;
				PrivateIncludePaths.Add(VulkanSDKPath + "/Include");

				if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicLibraryPaths.Add(VulkanSDKPath + "/Bin32");
				}
				else
				{
					PublicLibraryPaths.Add(VulkanSDKPath + "/Bin");
				}

				PublicAdditionalLibraries.Add("vulkan-1.lib");
				PublicAdditionalLibraries.Add("vkstatic.1.lib");
			}

			if (bHaveVulkan)
			{
				if (Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					PrivateIncludePathModuleNames.AddRange(
						new string[]
						{
							"TaskGraph",
						}
					);
				}
			}
			else
			{
				PrecompileForTargets = PrecompileTargetsType.None;
			}
		}
		else
		{
			PrecompileForTargets = PrecompileTargetsType.None;
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
		catch(Exception)
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

