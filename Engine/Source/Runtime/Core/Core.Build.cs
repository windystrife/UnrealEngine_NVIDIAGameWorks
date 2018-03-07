// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Core : ModuleRules
{
	public Core(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivatePCHHeaderFile = "Private/CorePrivatePCH.h";

		SharedPCHHeaderFile = "Public/CoreSharedPCH.h";

		bAddDefaultIncludePaths = false;

		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/Core/Public",
				"Runtime/Core/Public/Internationalization",
				"Runtime/Core/Public/Async",
				"Runtime/Core/Public/Concurrency",
				"Runtime/Core/Public/Containers",
				"Runtime/Core/Public/Delegates",
				"Runtime/Core/Public/GenericPlatform",
				"Runtime/Core/Public/HAL",
				"Runtime/Core/Public/Logging",
				"Runtime/Core/Public/Math",
				"Runtime/Core/Public/Misc",
				"Runtime/Core/Public/Modules",
				"Runtime/Core/Public/Modules/Boilerplate",
				"Runtime/Core/Public/ProfilingDebugging",
				"Runtime/Core/Public/Serialization",
				"Runtime/Core/Public/Serialization/Csv",
				"Runtime/Core/Public/Stats",
				"Runtime/Core/Public/Templates",
				"Runtime/Core/Public/UObject",
			}
			);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/DerivedDataCache/Public",
				"Runtime/SynthBenchmark/Public",
				"Runtime/Core/Private",
				"Runtime/Core/Private/Misc",
				"Runtime/Core/Private/Serialization/Json",
                "Runtime/Core/Private/Internationalization",
				"Runtime/Core/Private/Internationalization/Cultures",
                "Runtime/Analytics/Public",
				"Runtime/Engine/Public",
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"DerivedDataCache",
                "InputDevice",
                "Analytics",
				"RHI"
			}
			);

		if (Target.bBuildEditor == true)
		{
			DynamicallyLoadedModuleNames.Add("SourceCodeAccess");

			PrivateIncludePathModuleNames.Add("DirectoryWatcher");
			DynamicallyLoadedModuleNames.Add("DirectoryWatcher");
		}

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			PublicIncludePaths.Add("Runtime/Core/Public/Windows");
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib");

			AddEngineThirdPartyPrivateStaticDependencies(Target, 
				"IntelTBB"
				);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicIncludePaths.AddRange(new string[] { "Runtime/Core/Public/Apple", "Runtime/Core/Public/Mac" });
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"IntelTBB",
				"zlib",
				"PLCrashReporter"
				);
			PublicFrameworks.AddRange(new string[] { "Cocoa", "Carbon", "IOKit", "Security" });
			
			if (Target.bBuildEditor == true)
			{
				PublicAdditionalLibraries.Add("/System/Library/PrivateFrameworks/MultitouchSupport.framework/Versions/Current/MultitouchSupport");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PublicIncludePaths.AddRange(new string[] {"Runtime/Core/Public/Apple", "Runtime/Core/Public/IOS"});
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib"
				);
			PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation", "AudioToolbox", "AVFoundation", "GameKit", "StoreKit", "CoreVideo", "CoreMedia", "CoreGraphics", "GameController", "SystemConfiguration" });
			if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				PublicFrameworks.AddRange(new string[] { "CoreMotion", "AdSupport" });
                AddEngineThirdPartyPrivateStaticDependencies(Target,
                    "PLCrashReporter"
                    );
			}

			PrivateIncludePathModuleNames.Add("ApplicationCore");

			bool bSupportAdvertising = Target.Platform == UnrealTargetPlatform.IOS;
			if (bSupportAdvertising)
			{
				PublicFrameworks.AddRange(new string[] { "iAD" });
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicIncludePaths.Add("Runtime/Core/Public/Android");
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"cxademangle",
				"zlib"
				);
		}
        else if ((Target.Platform == UnrealTargetPlatform.Linux))
        {
            PublicIncludePaths.Add("Runtime/Core/Public/Linux");
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"zlib",
				"jemalloc",
				"elftoolchain"
                );

			// Core uses dlopen()
			PublicAdditionalLibraries.Add("dl");
        }
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
            PrivateDependencyModuleNames.Add("HTML5JS");
            PrivateDependencyModuleNames.Add("MapPakDownloader");
        }
        else if (Target.Platform == UnrealTargetPlatform.PS4)
        {
            PublicAdditionalLibraries.Add("SceRtc_stub_weak"); //ORBIS SDK rtc.h, used in PS4Time.cpp
        }

		if ( Target.bCompileICU == true )
        {
			AddEngineThirdPartyPrivateStaticDependencies(Target, "ICU");
        }
        Definitions.Add("UE_ENABLE_ICU=" + (Target.bCompileICU ? "1" : "0")); // Enable/disable (=1/=0) ICU usage in the codebase. NOTE: This flag is for use while integrating ICU and will be removed afterward.

        // If we're compiling with the engine, then add Core's engine dependencies
		if (Target.bCompileAgainstEngine == true)
		{
			if (!Target.bBuildRequiresCookedData)
			{
				DynamicallyLoadedModuleNames.AddRange(new string[] { "DerivedDataCache" });
			}
		}

		
		// On Windows platform, VSPerfExternalProfiler.cpp needs access to "VSPerf.h".  This header is included with Visual Studio, but it's not in a standard include path.
		if( Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 )
		{
			var VisualStudioVersionNumber = "11.0";
			var SubFolderName = ( Target.Platform == UnrealTargetPlatform.Win64 ) ? "x64/PerfSDK" : "PerfSDK";

			string PerfIncludeDirectory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), String.Format("Microsoft Visual Studio {0}/Team Tools/Performance Tools/{1}", VisualStudioVersionNumber, SubFolderName));

			if (File.Exists(Path.Combine(PerfIncludeDirectory, "VSPerf.h")))
			{
				PrivateIncludePaths.Add(PerfIncludeDirectory);
				Definitions.Add("WITH_VS_PERF_PROFILER=1");
			}
			else
			{
				Definitions.Add("WITH_VS_PERF_PROFILER=0");
			}
		}

		if(Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			WhitelistRestrictedFolders.Add("Private/Windows/NoRedist");
		}

        if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            Definitions.Add("WITH_DIRECTXMATH=1");
        }
        else if ((Target.Platform == UnrealTargetPlatform.Win64) ||
                (Target.Platform == UnrealTargetPlatform.Win32))
        {
			// To enable this requires Win8 SDK
            Definitions.Add("WITH_DIRECTXMATH=0");  // Enable to test on Win64/32.

            //PublicDependencyModuleNames.AddRange(  // Enable to test on Win64/32.
			//    new string[] {
			//    "DirectXMath"
            //});
        }
        else
        {
            Definitions.Add("WITH_DIRECTXMATH=0");
        }

        // NVCHANGE_BEGIN: Add VXGI
        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
        {
            PublicDependencyModuleNames.Add("VXGI");
        }
        // NVCHANGE_END: Add VXGI
    }
}
