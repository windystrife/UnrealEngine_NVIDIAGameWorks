// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MayaLiveLinkPluginTarget : TargetRules
{
	public MayaLiveLinkPluginTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;

		OverrideExecutableFileExtension = ".mll";   // Maya requires plugin binaries to have the ".mll" extension
		bShouldCompileAsDLL = true;
		LinkType = TargetLinkType.Monolithic;

		LaunchModuleName = "MayaLiveLinkPlugin";

		// We only need minimal use of the engine for this plugin
		bCompileLeanAndMeanUE = true;
		bUseMallocProfiler = false;
		bBuildEditor = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
        bCompileICU = false;
        bHasExports = true;

		bBuildInSolutionByDefault = false;
	}
}
