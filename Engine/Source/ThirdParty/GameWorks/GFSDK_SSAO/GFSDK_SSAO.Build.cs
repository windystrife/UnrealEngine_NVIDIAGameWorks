// NVCHANGE_BEGIN: Add HBAO+
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class GFSDK_SSAO : ModuleRules
{
	public GFSDK_SSAO(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		Definitions.Add("WITH_GFSDK_SSAO=1");

		string LibDir = Target.UEThirdPartySourceDirectory + "GameWorks/GFSDK_SSAO";
		PublicIncludePaths.Add(LibDir + "/include");
		PublicLibraryPaths.Add(LibDir + "/lib");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add("GFSDK_SSAO_D3D11.win64.lib");
			PublicAdditionalLibraries.Add("GFSDK_SSAO_D3D12.win64.lib");
			PublicDelayLoadDLLs.Add("GFSDK_SSAO_D3D11.win64.dll");
			PublicDelayLoadDLLs.Add("GFSDK_SSAO_D3D12.win64.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicAdditionalLibraries.Add("GFSDK_SSAO_D3D11.win32.lib");
			PublicAdditionalLibraries.Add("GFSDK_SSAO_D3D12.win32.lib");
			PublicDelayLoadDLLs.Add("GFSDK_SSAO_D3D11.win32.dll");
			PublicDelayLoadDLLs.Add("GFSDK_SSAO_D3D12.win32.dll");
		}
	}
}
// NVCHANGE_END: Add HBAO+
