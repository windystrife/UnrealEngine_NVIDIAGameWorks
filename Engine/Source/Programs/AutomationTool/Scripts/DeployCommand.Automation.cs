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
	#region Deploy Command

	public static void Deploy(ProjectParams Params)
	{
		Params.ValidateAndLog();
		if (!Params.Deploy)
		{
			return;
		}

		Log("********** DEPLOY COMMAND STARTED **********");
		
		if (!Params.NoClient)
		{
			var DeployContextList = CreateDeploymentContext(Params, false, false);
			foreach ( var SC in DeployContextList )
			{
				if (SC.StageTargetPlatform.DeployViaUFE)
				{
					string ClientCmdLine = "-run=Deploy ";
					ClientCmdLine += "-Device=" + string.Join("+", Params.Devices) + " ";
					ClientCmdLine += "-Targetplatform=" + SC.StageTargetPlatform.PlatformType.ToString() + " ";
					ClientCmdLine += "-SourceDir=\"" + CombinePaths(Params.BaseStageDirectory, SC.StageTargetPlatform.PlatformType.ToString()) + "\" ";
					string ClientApp = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealFrontend.exe");

					Log("Deploying via UFE:");
					Log("\tClientCmdLine: " + ClientCmdLine + "");

					//@todo UAT: Consolidate running of external applications like UFE (See 'RunProjectCommand' for other instances)
					PushDir(Path.GetDirectoryName(ClientApp));
					// Always start client process and don't wait for exit.
					IProcessResult ClientProcess = Run(ClientApp, ClientCmdLine, null, ERunOptions.NoWaitForExit);
					PopDir();
					if (ClientProcess != null)
					{
						do
						{
							Thread.Sleep(100);
						}
						while (ClientProcess.HasExited == false);
					}
				}
				else
				{
					SC.StageTargetPlatform.Deploy(Params, SC);
				}
			}
		}
		if (Params.DedicatedServer)
		{
			ProjectParams ServerParams = new ProjectParams(Params);
			ServerParams.Devices = new ParamList<string>(ServerParams.ServerDevice);
			var DeployContextList = CreateDeploymentContext(ServerParams, true, false);
			foreach ( var SC in DeployContextList )
			{
				SC.StageTargetPlatform.Deploy(ServerParams, SC);
			}
		}

		Log("********** DEPLOY COMMAND COMPLETED **********");
	}

	#endregion
}
