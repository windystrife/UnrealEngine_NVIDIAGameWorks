// NVCHANGE_BEGIN: Add VXGI
using UnrealBuildTool;

public class VXGI : ModuleRules
{
	public VXGI(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		Definitions.Add("WITH_GFSDK_VXGI=1");

		string VXGIDir = Target.UEThirdPartySourceDirectory + "GameWorks/VXGI";
		PublicIncludePaths.Add(VXGIDir + "/include");

		string ArchName = "<unknown>";
		string DebugSuffix = "";

		if (Target.Configuration == UnrealTargetConfiguration.Debug)
			DebugSuffix = "d";

		if (Target.Platform == UnrealTargetPlatform.Win64)
			ArchName = "x64";
		else if (Target.Platform == UnrealTargetPlatform.Win32)
			ArchName = "x86";

		PublicLibraryPaths.Add(VXGIDir + "/lib/" + ArchName);
		PublicAdditionalLibraries.Add("GFSDK_VXGI" + DebugSuffix + "_" + ArchName + ".lib");
		PublicDelayLoadDLLs.Add("GFSDK_VXGI" + DebugSuffix + "_" + ArchName + ".dll");
	}
}
// NVCHANGE_END: Add VXGI
