// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Simplygon : ModuleRules
{
	public Simplygon(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bOutputPubliclyDistributable = true;

        Definitions.Add("SGDEPRECATED_OFF=1");

        //@third party code BEGIN SIMPLYGON
        //Change the path to make it easier to update Simplygon
        string SimplygonPath = Target.UEThirdPartySourceDirectory + "NotForLicensees/Simplygon/Simplygon-latest/";
        //@third party code END SIMPLYGON
		PublicIncludePaths.Add(SimplygonPath + "Inc");

		// Simplygon depends on D3DX9.
		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicLibraryPaths.Add( Target.UEThirdPartySourceDirectory + "Windows/DirectX/Lib/x64" );
			}
			else
			{
				PublicLibraryPaths.Add( Target.UEThirdPartySourceDirectory + "Windows/DirectX/Lib/x86" );
			}
			PublicAdditionalLibraries.Add(
				(Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d3dx9d.lib" : "d3dx9.lib"
				);

			// Simplygon requires GetProcessMemoryInfo exported by psapi.dll. http://msdn.microsoft.com/en-us/library/windows/desktop/ms683219(v=vs.85).aspx
			PublicAdditionalLibraries.Add("psapi.lib");

            PublicDelayLoadDLLs.Add("d3dcompiler_47.dll");
            string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
            if (Target.Platform == UnrealTargetPlatform.Win32)
            {
                RuntimeDependencies.Add(new RuntimeDependency(EngineDir + "Binaries/ThirdParty/Windows/DirectX/x86/d3dcompiler_47.dll"));
            }
            else if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                RuntimeDependencies.Add(new RuntimeDependency(EngineDir + "Binaries/ThirdParty/Windows/DirectX/x64/d3dcompiler_47.dll"));
            }

        }
	}
}

