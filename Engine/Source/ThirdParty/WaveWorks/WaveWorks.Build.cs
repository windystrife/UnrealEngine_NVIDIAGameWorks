/*
* This code contains NVIDIA Confidential Information and is disclosed
* under the Mutual Non-Disclosure Agreement.
*
* Notice
* ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES
* NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
* THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
*
* NVIDIA Corporation assumes no responsibility for the consequences of use of such
* information or for any infringement of patents or other rights of third parties that may
* result from its use. No license is granted by implication or otherwise under any patent
* or patent rights of NVIDIA Corporation. No third party distribution is allowed unless
* expressly authorized by NVIDIA.  Details are subject to change without notice.
* This code supersedes and replaces all information previously supplied.
* NVIDIA Corporation products are not authorized for use as critical
* components in life support devices or systems without express written approval of
* NVIDIA Corporation.
*
* Copyright ?2008- 2016 NVIDIA Corporation. All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software and related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is
* strictly prohibited.
*/

using UnrealBuildTool;

public class WaveWorks : ModuleRules
{
	public WaveWorks(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		Definitions.Add("WITH_WAVEWORKS=1");
        Definitions.Add("GFSDK_WAVEWORKS_DLL");
        Definitions.Add("__GFSDK_DX11__=1");
        Definitions.Add("__GFSDK_GL__=0");

        string WaveWorksDir = Target.UEThirdPartySourceDirectory + "WaveWorks/";
		string WaveWorksBinDir = Target.UEThirdPartyBinariesDirectory + "WaveWorks/";

        string Platform = "unknown";
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            WaveWorksBinDir += "win64/";
            Platform = "win64";
        }
        else if (Target.Platform == UnrealTargetPlatform.Win32)
        {
            WaveWorksBinDir += "win32/";
            Platform = "win32";
        }

        string FileName = "gfsdk_waveworks";
        FileName += "." + Platform;

        PublicIncludePaths.Add(WaveWorksDir + "inc");
        PublicLibraryPaths.Add(WaveWorksBinDir);
        PublicAdditionalLibraries.Add(FileName + ".lib");
        PublicDelayLoadDLLs.Add(FileName + ".dll");

        RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/WaveWorks/" + Platform + "/" + FileName + ".dll"));
        RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/WaveWorks/" + Platform + "/" + FileName + ".lib"));
        RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/WaveWorks/" + Platform + "/cudart64_55.dll"));
        RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/WaveWorks/" + Platform + "/cufft64_55.dll"));
    }
}
