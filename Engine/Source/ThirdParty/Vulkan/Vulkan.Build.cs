// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Vulkan : ModuleRules
{
	public Vulkan(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			string RootPath = Target.UEThirdPartySourceDirectory + "Vulkan/Windows";
			string LibPath = RootPath + "/Bin";
			if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				LibPath += "32";
			}

			PublicLibraryPaths.Add(LibPath);
			PublicAdditionalLibraries.Add("vulkan-1.lib");
			PublicAdditionalLibraries.Add("VKstatic.1.lib");

			PublicSystemIncludePaths.Add(RootPath + "/Include");
			PublicSystemIncludePaths.Add(RootPath + "/Include/vulkan");

			// For now let's always delay load the vulkan dll as not everyone has it installed
			PublicDelayLoadDLLs.Add("vulkan-1.dll");
		}
		else if(Target.Platform == UnrealTargetPlatform.Linux)
		{
			// no need to add the library, should be loaded via SDL
			string RootPath = Target.UEThirdPartySourceDirectory + "Vulkan/Linux";

			PublicSystemIncludePaths.Add(RootPath + "/include");
			PublicSystemIncludePaths.Add(RootPath + "/include/vulkan");
		}
	}
}
