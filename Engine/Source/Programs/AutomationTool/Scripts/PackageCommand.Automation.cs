// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

public partial class Project : CommandUtils
{
	#region Package Command

	public static void Package(ProjectParams Params, int WorkingCL=-1)
	{
		if (!Params.SkipStage || Params.Package)
		{
			Params.ValidateAndLog();
			List<DeploymentContext> DeployContextList = new List<DeploymentContext>();
			if (!Params.NoClient)
			{
				DeployContextList.AddRange(CreateDeploymentContext(Params, false, false));
			}
			if (Params.DedicatedServer)
			{
				DeployContextList.AddRange(CreateDeploymentContext(Params, true, false));
			}

			if (DeployContextList.Count > 0 )
			{
				Log("********** PACKAGE COMMAND STARTED **********");

				foreach (var SC in DeployContextList)
				{
					if (Params.Package || (SC.StageTargetPlatform.RequiresPackageToDeploy && Params.Deploy))
					{
						SC.StageTargetPlatform.Package(Params, SC, WorkingCL);
					}
				}

				Log("********** PACKAGE COMMAND COMPLETED **********");
			}
		}
	}

	#endregion
}
