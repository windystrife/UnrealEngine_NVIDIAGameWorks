// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
public class NVAftermath : ModuleRules
{
    public NVAftermath(ReadOnlyTargetRules Target)
        : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            String NVAftermathPath = Target.UEThirdPartySourceDirectory + "NVIDIA/NVaftermath/";
            PublicSystemIncludePaths.Add(NVAftermathPath);
            
            String NVAftermathLibPath = NVAftermathPath + "amd64/";
            PublicLibraryPaths.Add(NVAftermathLibPath);
            PublicAdditionalLibraries.Add("GFSDK_Aftermath_Lib.lib");

            String AftermathDllName = "GFSDK_Aftermath_Lib.dll";                  
            String nvDLLPath = "$(EngineDir)/Binaries/ThirdParty/NVIDIA/NVaftermath/Win64/" + AftermathDllName;
            PublicDelayLoadDLLs.Add(AftermathDllName);
            RuntimeDependencies.Add(new RuntimeDependency(nvDLLPath));

            Definitions.Add("NV_AFTERMATH=1");
        }
		else
		{
            Definitions.Add("NV_AFTERMATH=0");
        }
	}
}

