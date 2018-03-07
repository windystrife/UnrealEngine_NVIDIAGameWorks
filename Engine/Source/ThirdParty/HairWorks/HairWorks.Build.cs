// @third party code - BEGIN HairWorks
using UnrealBuildTool;

public class HairWorks : ModuleRules
{
	public HairWorks(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Add DLL to package
		string PlatformString = null;
		switch (Target.Platform)
		{
			case UnrealTargetPlatform.Win32:
				PlatformString = "win32";
				break;
			case UnrealTargetPlatform.Win64:
				PlatformString = "win64";
				break;
		}

		if (PlatformString != null)
		{
			// Add HairWorks DLL
			var HairWorksBinaryDir = "$(EngineDir)/Binaries/ThirdParty/HairWorks";
			var HairWorksDllPath = System.IO.Path.Combine(HairWorksBinaryDir, "NvHairWorksDx11." + PlatformString + ".dll");
			RuntimeDependencies.Add(new RuntimeDependency(HairWorksDllPath));

			// Add shader compiler DLL
			var ShaderCompilerDllName = "d3dcompiler_47.dll";
			var ShaderCompilerDllPath = System.IO.Path.Combine(HairWorksBinaryDir, ShaderCompilerDllName);
			RuntimeDependencies.Add(new RuntimeDependency(ShaderCompilerDllPath));
		}
	}
}
// @third party code - END HairWorks


