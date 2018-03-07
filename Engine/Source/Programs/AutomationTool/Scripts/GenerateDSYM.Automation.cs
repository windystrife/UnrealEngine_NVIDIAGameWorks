// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

[Help(@"Generates IOS debug symbols for a remote project.")]
[Help("project=Name", @"Project name (required), i.e: -project=QAGame")]
[Help("config=Configuration", @"Project configuration (required), i.e: -config=Development")]
public class GenerateDSYM : BuildCommand
{
	#region BaseCommand interface

	public override void ExecuteBuild()
	{
		var ProjectName = ParseParamValue("project");
		var Config = ParseParamValue("config");

		var IPPExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/IPhonePackager.exe");

		RunAndLog(CmdEnv, IPPExe, "RPC " + ProjectName + " -config " + Config + " GenDSYM");
	}

	#endregion
}