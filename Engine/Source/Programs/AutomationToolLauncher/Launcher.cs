// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;
using System.IO;

namespace AutomationToolLauncher
{
	class Launcher
	{
		static int Main()
		{
			// Create application domain setup information.
			var Domaininfo = new AppDomainSetup();
			Domaininfo.ApplicationBase = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
			Domaininfo.ShadowCopyFiles = "true";

			// Create the application domain.			
			var Domain = AppDomain.CreateDomain("AutomationTool", AppDomain.CurrentDomain.Evidence, Domaininfo);
			// Execute assembly and pass through command line
			var CommandLine = AutomationTool.SharedUtils.ParseCommandLine();
			var UATExecutable = Path.Combine(Domaininfo.ApplicationBase, "AutomationTool.exe");
			// Default exit code in case UAT does not even start, otherwise we always return UAT's exit code.
			var ExitCode = 193;

			try
			{
				ExitCode = Domain.ExecuteAssembly(UATExecutable, CommandLine);
				// Unload the application domain.
				AppDomain.Unload(Domain);
			}
			catch (Exception Ex)
			{
				Console.WriteLine(Ex.Message);
				Console.WriteLine(Ex.StackTrace);
			}
			return ExitCode;
		}
	}
}
