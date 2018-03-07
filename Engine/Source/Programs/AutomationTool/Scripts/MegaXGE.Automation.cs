// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;

[Help("Compiles a bunch of stuff together with megaxge: Example arguments: -Target1=\"PlatformerGame win32|ios debug|development\"")]
[Help(typeof(UE4Build))]
[Help("Target1", "target1[|target2...] platform1[|platform2...] config1[|config2...]")]
[Help("Target2", "target1[|target2...] platform1[|platform2...] config1[|config2...]")]

class MegaXGE : BuildCommand
{
	public override void ExecuteBuild()
	{
		int WorkingCL = -1;
		if (P4Enabled && AllowSubmit)
		{
			string CmdLine = "";
			foreach (var Arg in Params)
			{
				CmdLine += Arg.ToString() + " ";
			}
			WorkingCL = P4.CreateChange(P4Env.Client, String.Format("MegaXGE build from changelist {0} - Params: {1}", P4Env.Changelist, CmdLine));
		}

		Log("************************* MegaXGE");

		bool Clean = ParseParam("Clean");
		string CleanToolLocation = CombinePaths(CmdEnv.LocalRoot, "Engine", "Build", "Batchfiles", "Clean.bat");

		bool ShowProgress = ParseParam("Progress");

		var UE4Build = new UE4Build(this);

		var Agenda = new UE4Build.BuildAgenda();

		// we need to always build UHT when we use mega XGE
		var ProgramTargets = new string[] 
		{
			"UnrealHeaderTool",
		};
		Agenda.AddTargets(ProgramTargets, UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development);
		if (Clean)
		{
			LogSetProgress(ShowProgress, "Cleaning previous builds...");
			foreach (var CurTarget in ProgramTargets)
			{
				string Args = String.Format("{0} {1} {2}", CurTarget, UnrealTargetPlatform.Win64.ToString(), UnrealTargetConfiguration.Development.ToString());
				RunAndLog(CmdEnv, CleanToolLocation, Args);
			}
		}

		Log("*************************");
		for (int Arg = 1; Arg < 100; Arg++)
		{
			string Parm = String.Format("Target{0}", Arg);
			string Target = ParseParamValue(Parm, "");
			if (String.IsNullOrEmpty(Target))
			{
				break;
			}
			var Parts = Target.Split(' ');

			string JustTarget = Parts[0];
			if (String.IsNullOrEmpty(JustTarget))
			{
				throw new AutomationException("BUILD FAILED target option '{0}' not parsed.", Target);
			}
			var Targets = JustTarget.Split('|');
			if (Targets.Length < 1)
			{
				throw new AutomationException("BUILD FAILED target option '{0}' not parsed.", Target);
			}

			var Platforms = new List<UnrealTargetPlatform>();
			var Configurations = new List<UnrealTargetConfiguration>();

			for (int Part = 1; Part < Parts.Length; Part++)
			{
				if (!String.IsNullOrEmpty(Parts[Part]))
				{
					var SubParts = Parts[Part].Split('|');

					foreach (var SubPart in SubParts)
					{
						UnrealTargetPlatform Platform;
						if(Enum.TryParse(SubPart, true, out Platform))
						{
							Platforms.Add(Platform);
						}
						else
						{
							switch (SubPart.ToUpperInvariant())
							{
								case "DEBUG":
									Configurations.Add(UnrealTargetConfiguration.Debug);
									break;
								case "DEBUGGAME":
									Configurations.Add(UnrealTargetConfiguration.DebugGame);
									break;
								case "DEVELOPMENT":
									Configurations.Add(UnrealTargetConfiguration.Development);
									break;
								case "SHIPPING":
									Configurations.Add(UnrealTargetConfiguration.Shipping);
									break;
								case "TEST":
									Configurations.Add(UnrealTargetConfiguration.Test);
									break;
								default:
									throw new AutomationException("BUILD FAILED target option {0} not recognized.", SubPart);
							}
						}

					}
				}
			}
			if (Platforms.Count < 1)
			{
				Platforms.Add(UnrealTargetPlatform.Win64);
			}
			if (Configurations.Count < 1)
			{
				Configurations.Add(UnrealTargetConfiguration.Development);
			}
			foreach (var Platform in Platforms)
			{
				foreach (var CurTarget in Targets)
				{
					foreach (var Configuration in Configurations)
					{
						Agenda.AddTargets(new string[] { CurTarget }, Platform, Configuration);
						Log("Target {0} {1} {2}", CurTarget, Platform.ToString(), Configuration.ToString());
						if (Clean)
						{
							string Args = String.Format("{0} {1} {2}", CurTarget, Platform.ToString(), Configuration.ToString());
							RunAndLog(CmdEnv, CleanToolLocation, Args);
						}
					}
				}
			}
		}
		Log("*************************");

		Agenda.DoRetries = ParseParam("Retry");
		UE4Build.Build(Agenda, InUpdateVersionFiles: IsBuildMachine, InUseParallelExecutor: ParseParam("useparallelexecutor"), InShowProgress: ShowProgress);

		// 		if (WorkingCL > 0) // only move UAT files if we intend to check in some build products
		// 		{
		// 			UE4Build.CopyUATFilesAndAddToBuildProducts();
		// 		}

		UE4Build.CheckBuildProducts(UE4Build.BuildProductFiles);

		if (WorkingCL > 0)
		{
			// Sign everything we built
			CodeSign.SignMultipleIfEXEOrDLL(this, UE4Build.BuildProductFiles);

			// Open files for add or edit
			UE4Build.AddBuildProductsToChangelist(WorkingCL, UE4Build.BuildProductFiles);

			int SubmittedCL;
			P4.Submit(WorkingCL, out SubmittedCL, true, true);
		}

		PrintRunTime();

	}
}
