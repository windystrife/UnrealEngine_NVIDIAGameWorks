// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealLightmass : ModuleRules
{
	public UnrealLightmass(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "zlib", "SwarmInterface", "Projects", "ApplicationCore" });

		Definitions.Add("UE_LIGHTMASS=1");

		if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX9");

			// Unreallightmass requires GetProcessMemoryInfo exported by psapi.dll. http://msdn.microsoft.com/en-us/library/windows/desktop/ms683219(v=vs.85).aspx
			PublicAdditionalLibraries.Add("psapi.lib");
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Messaging",
				}
			);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			// On Mac/Linux UnrealLightmass is executed locally and communicates with the editor using Messaging module instead of SwarmAgent
			// @todo: allow for better plug-in support in standalone Slate apps
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Networking",
					"Sockets",
					"Messaging",
					"UdpMessaging",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
				}
			);
		}

		// Lightmass ray tracing is 8% faster with buffer security checks disabled due to fixed size arrays on the stack in the kDOP ray tracing functions
		// Warning: This means buffer overwrites will not be detected
		bEnableBufferSecurityChecks = false;

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/Launch");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/ImportExport");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/CPUSolver");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/Lighting");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/LightmassCore");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/LightmassCore/Misc");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/LightmassCore/Math");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/LightmassCore/Templates");
		PrivateIncludePaths.Add("Programs/UnrealLightmass/Private/LightmassCore/Types");

        // EMBREE
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string SDKDir = Target.UEThirdPartySourceDirectory + "IntelEmbree/Embree270/Win64/";

            PublicIncludePaths.Add(SDKDir + "include");
            PublicLibraryPaths.Add(SDKDir + "lib");
            PublicAdditionalLibraries.Add("embree.lib");
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Win64/embree.dll"));
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Win64/tbb.dll"));
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Win64/tbbmalloc.dll"));
            Definitions.Add("USE_EMBREE=1");
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
            string SDKDir = Target.UEThirdPartySourceDirectory + "IntelEmbree/Embree270/MacOSX/";

            PublicIncludePaths.Add(SDKDir + "include");
            PublicAdditionalLibraries.Add(SDKDir + "lib/libembree.2.dylib");
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Mac/libembree.2.dylib"));
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Mac/libtbb.dylib"));
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Mac/libtbbmalloc.dylib"));
            Definitions.Add("USE_EMBREE=1");
		}
        else
        {
            Definitions.Add("USE_EMBREE=0");
        }
    }
}
