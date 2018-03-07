// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Editor)]
public class ShaderCompileWorkerTarget : TargetRules
{
	public ShaderCompileWorkerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Modular;

		LaunchModuleName = "ShaderCompileWorker";

        if (bUseXGEController && (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64))
        {
            // The interception interface in XGE requires that the parent and child processes have different filenames on disk.
            // To avoid building an entire separate worker just for this, we duplicate the ShaderCompileWorker in a post build step.
            const string SrcPath  = "$(EngineDir)\\Binaries\\$(TargetPlatform)\\ShaderCompileWorker.exe";
            const string DestPath = "$(EngineDir)\\Binaries\\$(TargetPlatform)\\XGEControlWorker.exe";

            PostBuildSteps.Add(string.Format("echo Copying {0} to {1}", SrcPath, DestPath));
            PostBuildSteps.Add(string.Format("copy /Y /B \"{0}\" /B \"{1}\"", SrcPath, DestPath));
        }
	}

	//
	// TargetRules interface.
	//

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		// Turn off various third party features we don't need

		// Currently we force Lean and Mean mode
		bCompileLeanAndMeanUE = true;

		// ShaderCompileWorker isn't localized, so doesn't need ICU
		bCompileICU = false;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;
		bBuildWithEditorOnlyData = true;
		bCompileCEF3 = false;

		if (Target.Configuration == UnrealTargetConfiguration.Debug)
		{
			bDebugBuildsActuallyUseDebugCRT = true;
		}

		// Never use malloc profiling in ShaderCompileWorker.
		bUseMallocProfiler = false;

		// Force all shader formats to be built and included.
        bForceBuildShaderFormats = true;

		// ShaderCompileWorker is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = true;

		// Disable logging, as the workers are spawned often and logging will just slow them down
		OutCPPEnvironmentConfiguration.Definitions.Add("ALLOW_LOG_FILE=0");

        // Linking against wer.lib/wer.dll causes XGE to bail when the worker is run on a Windows 8 machine, so turn this off.
        OutCPPEnvironmentConfiguration.Definitions.Add("ALLOW_WINDOWS_ERROR_REPORT_LIB=0");
	}
}
