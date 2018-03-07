// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

[Flags]
public enum ProjectBuildTargets
{
	None = 0,
	Editor = 1 << 0,
	ClientCooked = 1 << 1,
	ServerCooked = 1 << 2,
	Bootstrap = 1 << 3,
	CrashReporter = 1 << 4,
	Programs = 1 << 5,

	// All targets
	All = Editor | ClientCooked | ServerCooked | Bootstrap | CrashReporter | Programs,
}

/// <summary>
/// Helper command used for compiling.
/// </summary>
/// <remarks>
/// Command line params used by this command:
/// -cooked
/// -cookonthefly
/// -clean
/// -[platform]
/// </remarks>
public partial class Project : CommandUtils
{
	#region Build Command

	/// <summary>
	/// PlatformSupportsCrashReporter
	/// </summary>
	/// <param name="InPlatform">The platform of interest</param>
	/// <returns>True if the given platform supports a crash reporter client (i.e. it can be built for it)</returns>
	static public bool PlatformSupportsCrashReporter(UnrealTargetPlatform InPlatform)
	{
		return (
			(InPlatform == UnrealTargetPlatform.Win64) ||
			(InPlatform == UnrealTargetPlatform.Win32) ||
			(InPlatform == UnrealTargetPlatform.Linux) ||
			(InPlatform == UnrealTargetPlatform.Mac)
			);
	}

	public static void Build(BuildCommand Command, ProjectParams Params, int WorkingCL = -1, ProjectBuildTargets TargetMask = ProjectBuildTargets.All)
	{
		Params.ValidateAndLog();

		if (!Params.Build)
		{
			return;
		}

		Log("********** BUILD COMMAND STARTED **********");

		var UE4Build = new UE4Build(Command);
		var Agenda = new UE4Build.BuildAgenda();
		var CrashReportPlatforms = new HashSet<UnrealTargetPlatform>();

		// Setup editor targets
		if (Params.HasEditorTargets && (!Params.SkipBuildEditor) && !Automation.IsEngineInstalled() && (TargetMask & ProjectBuildTargets.Editor) == ProjectBuildTargets.Editor)
		{
			// @todo Mac: proper platform detection
			UnrealTargetPlatform EditorPlatform = HostPlatform.Current.HostEditorPlatform;
			const UnrealTargetConfiguration EditorConfiguration = UnrealTargetConfiguration.Development;

			CrashReportPlatforms.Add(EditorPlatform);
            Agenda.AddTargets(Params.EditorTargets.ToArray(), EditorPlatform, EditorConfiguration, Params.CodeBasedUprojectPath);
			if (Params.EditorTargets.Contains("UnrealHeaderTool") == false)
			{
				Agenda.AddTargets(new string[] { "UnrealHeaderTool" }, EditorPlatform, EditorConfiguration);
			}
			if (Params.EditorTargets.Contains("ShaderCompileWorker") == false)
			{
				Agenda.AddTargets(new string[] { "ShaderCompileWorker" }, EditorPlatform, EditorConfiguration);
			}
			if (Params.Pak && Params.EditorTargets.Contains("UnrealPak") == false)
			{
				Agenda.AddTargets(new string[] { "UnrealPak" }, EditorPlatform, EditorConfiguration);
			}
			if (Params.FileServer && Params.EditorTargets.Contains("UnrealFileServer") == false)
			{
				Agenda.AddTargets(new string[] { "UnrealFileServer" }, EditorPlatform, EditorConfiguration);
			}
		}

		// Additional compile arguments
		string AdditionalArgs = "";
		if (Params.MapFile)
		{
			AdditionalArgs += " -mapfile";
		}

		if (Params.Deploy || Params.Package)
		{
			AdditionalArgs += " -skipdeploy"; // skip deploy step in UBT if we going to do it later anyway
		}

		// Config overrides (-ini)
		foreach (string ConfigOverrideParam in Params.ConfigOverrideParams)
		{
			AdditionalArgs += " -";
			AdditionalArgs += ConfigOverrideParam;
		}

		// Setup cooked targets
		if (Params.HasClientCookedTargets && (!Params.SkipBuildClient) && (TargetMask & ProjectBuildTargets.ClientCooked) == ProjectBuildTargets.ClientCooked)
		{
            List<UnrealTargetPlatform> UniquePlatformTypes = Params.ClientTargetPlatforms.ConvertAll(x => x.Type).Distinct().ToList();

            foreach (var BuildConfig in Params.ClientConfigsToBuild)
			{
                foreach (var ClientPlatformType in UniquePlatformTypes)
				{
                    CrashReportPlatforms.Add(ClientPlatformType);
					Agenda.AddTargets(Params.ClientCookedTargets.ToArray(), ClientPlatformType, BuildConfig, Params.CodeBasedUprojectPath, InAddArgs: " -remoteini=\"" + Params.RawProjectPath.Directory.FullName + "\"" + AdditionalArgs);
				}
			}
		}
		if (Params.HasServerCookedTargets && (TargetMask & ProjectBuildTargets.ServerCooked) == ProjectBuildTargets.ServerCooked)
		{
            List<UnrealTargetPlatform> UniquePlatformTypes = Params.ServerTargetPlatforms.ConvertAll(x => x.Type).Distinct().ToList();

            foreach (var BuildConfig in Params.ServerConfigsToBuild)
			{
				foreach (var ServerPlatformType in UniquePlatformTypes)
				{
                    CrashReportPlatforms.Add(ServerPlatformType);
					Agenda.AddTargets(Params.ServerCookedTargets.ToArray(), ServerPlatformType, BuildConfig, Params.CodeBasedUprojectPath, InAddArgs: " -remoteini=\"" + Params.RawProjectPath.Directory.FullName + "\"" + AdditionalArgs);
				}
			}
		}
		if (!Params.NoBootstrapExe && !Automation.IsEngineInstalled() && (TargetMask & ProjectBuildTargets.Bootstrap) == ProjectBuildTargets.Bootstrap)
		{
			UnrealBuildTool.UnrealTargetPlatform[] BootstrapPackagedGamePlatforms = { UnrealBuildTool.UnrealTargetPlatform.Win32, UnrealBuildTool.UnrealTargetPlatform.Win64 };
			foreach(UnrealBuildTool.UnrealTargetPlatform BootstrapPackagedGamePlatformType in BootstrapPackagedGamePlatforms)
			{
				if(Params.ClientTargetPlatforms.Contains(new TargetPlatformDescriptor(BootstrapPackagedGamePlatformType)))
				{
					Agenda.AddTarget("BootstrapPackagedGame", BootstrapPackagedGamePlatformType, UnrealBuildTool.UnrealTargetConfiguration.Shipping);
				}
			}
		}
		if (Params.CrashReporter && !Automation.IsEngineInstalled() && (TargetMask & ProjectBuildTargets.CrashReporter) == ProjectBuildTargets.CrashReporter)
		{
			foreach (var CrashReportPlatform in CrashReportPlatforms)
			{
				if (PlatformSupportsCrashReporter(CrashReportPlatform))
				{
					Agenda.AddTarget("CrashReportClient", CrashReportPlatform, UnrealTargetConfiguration.Shipping, InAddArgs: " -remoteini=\"" + Params.RawProjectPath.Directory.FullName + "\"");
				}
			}
		}
		if (Params.HasProgramTargets && (TargetMask & ProjectBuildTargets.Programs) == ProjectBuildTargets.Programs)
		{
            List<UnrealTargetPlatform> UniquePlatformTypes = Params.ClientTargetPlatforms.ConvertAll(x => x.Type).Distinct().ToList();

            foreach (var BuildConfig in Params.ClientConfigsToBuild)
			{
				foreach (var ClientPlatformType in UniquePlatformTypes)
				{
					Agenda.AddTargets(Params.ProgramTargets.ToArray(), ClientPlatformType, BuildConfig, Params.CodeBasedUprojectPath);
				}
			}
		}
		UE4Build.Build(Agenda, InDeleteBuildProducts: Params.Clean, InUpdateVersionFiles: WorkingCL > 0);

		if (WorkingCL > 0) // only move UAT files if we intend to check in some build products
		{
			UE4Build.AddUATFilesToBuildProducts();
		}
		UE4Build.CheckBuildProducts(UE4Build.BuildProductFiles);

		if (WorkingCL > 0)
		{
			// Sign everything we built
			CodeSign.SignMultipleIfEXEOrDLL(Command, UE4Build.BuildProductFiles);

			// Open files for add or edit
			UE4Build.AddBuildProductsToChangelist(WorkingCL, UE4Build.BuildProductFiles);
		}

		Log("********** BUILD COMMAND COMPLETED **********");
	}

	#endregion
}
