// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MfMedia : ModuleRules
	{
		public MfMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			bOutputPubliclyDistributable = true;

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"MediaUtils",
					"RenderCore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MfMedia/Private",
					"MfMedia/Private/Mf",
					"MfMedia/Private/Player",
				});

			if (Target.Type != TargetType.Server)
			{
				if ((Target.Platform == UnrealTargetPlatform.Win32) ||
					(Target.Platform == UnrealTargetPlatform.Win64))
				{
					PublicDelayLoadDLLs.Add("mf.dll");
					PublicDelayLoadDLLs.Add("mfplat.dll");
					PublicDelayLoadDLLs.Add("mfreadwrite.dll");
					PublicDelayLoadDLLs.Add("mfuuid.dll");
					PublicDelayLoadDLLs.Add("propsys.dll");
					PublicDelayLoadDLLs.Add("shlwapi.dll");
				}
				else
				{
					PublicAdditionalLibraries.Add("mfplat.lib");
					PublicAdditionalLibraries.Add("mfreadwrite.lib");
					PublicAdditionalLibraries.Add("mfuuid.lib");
				}
			}
		}
	}
}
