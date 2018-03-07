// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.ComponentModel.Design;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.OLE.Interop;
using EnvDTE;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using EnvDTE80;
using System.Linq;
using System.IO;
using System.Windows.Forms;

namespace UnrealVS
{
	class CompileSingleFile : IDisposable
	{
		const int CompileSingleFileButtonID = 0x1075;

		System.Diagnostics.Process ChildProcess;

		public CompileSingleFile()
		{
			CommandID CommandID = new CommandID( GuidList.UnrealVSCmdSet, CompileSingleFileButtonID );
			MenuCommand CompileSingleFileButtonCommand = new MenuCommand( new EventHandler( CompileSingleFileButtonHandler ), CommandID );
			UnrealVSPackage.Instance.MenuCommandService.AddCommand( CompileSingleFileButtonCommand );
		}

		public void Dispose()
		{
			KillChildProcess();
		}

		void CompileSingleFileButtonHandler( object Sender, EventArgs Args )
		{
			if(!TryCompileSingleFile())
			{
				DTE DTE = UnrealVSPackage.Instance.DTE;
				DTE.ExecuteCommand("Build.Compile");
			}
		}

		void KillChildProcess()
		{
			if(ChildProcess != null)
			{
				if(!ChildProcess.HasExited)
				{
					ChildProcess.Kill();
					ChildProcess.WaitForExit();
				}
				ChildProcess.Dispose();
				ChildProcess = null;
			}
		}

		bool TryCompileSingleFile()
		{
			DTE DTE = UnrealVSPackage.Instance.DTE;

			// Activate the output window
			Window Window = DTE.Windows.Item(EnvDTE.Constants.vsWindowKindOutput);
			Window.Activate();
			OutputWindow OutputWindow = Window.Object as OutputWindow;

			// Try to find the 'Build' window
			OutputWindowPane BuildOutputPane = null;
			foreach(OutputWindowPane Pane in OutputWindow.OutputWindowPanes)
			{
				if(Pane.Guid.ToUpperInvariant() == "{1BD8A850-02D1-11D1-BEE7-00A0C913D1F8}")
				{
					BuildOutputPane = Pane;
					break;
				}
			}
			if(BuildOutputPane == null)
			{
				return false;
			}

			// If there's already a build in progress, offer to cancel it
			if(ChildProcess != null && !ChildProcess.HasExited)
			{
				if(MessageBox.Show("Cancel current compile?", "Compile in progress", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					KillChildProcess();
					BuildOutputPane.OutputString("1>  Build cancelled.\r\n");
				}
				return true;
			}

			// Check we've got a file open
			if(DTE.ActiveDocument == null)
			{
				return false;
			}

			// Grab the current startup project
			IVsHierarchy ProjectHierarchy;
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject(out ProjectHierarchy);
			if (ProjectHierarchy == null)
			{
				return false;
			}
			Project StartupProject = Utils.HierarchyObjectToProject(ProjectHierarchy);
			if (StartupProject == null)
			{
				return false;
			}
			Microsoft.VisualStudio.VCProjectEngine.VCProject VCStartupProject = StartupProject.Object as Microsoft.VisualStudio.VCProjectEngine.VCProject;
			if(VCStartupProject == null)
			{
				return false;
			}

			// Get the active configuration for the startup project
			Configuration ActiveConfiguration = StartupProject.ConfigurationManager.ActiveConfiguration;
			string ActiveConfigurationName = String.Format("{0}|{1}", ActiveConfiguration.ConfigurationName, ActiveConfiguration.PlatformName);
			Microsoft.VisualStudio.VCProjectEngine.VCConfiguration ActiveVCConfiguration = VCStartupProject.Configurations.Item(ActiveConfigurationName);
			if(ActiveVCConfiguration == null)
			{
				return false;
			}

			// Get the NMake settings for this configuration
			Microsoft.VisualStudio.VCProjectEngine.VCNMakeTool ActiveNMakeTool = ActiveVCConfiguration.Tools.Item("VCNMakeTool");
			if(ActiveNMakeTool == null)
			{
				return false;
			}

			// Save all the open documents
			DTE.ExecuteCommand("File.SaveAll");

			// Check it's a cpp file
			string FileToCompile = DTE.ActiveDocument.FullName;
			if (!FileToCompile.EndsWith(".c", StringComparison.InvariantCultureIgnoreCase) && !FileToCompile.EndsWith(".cpp", StringComparison.InvariantCultureIgnoreCase))
			{
				MessageBox.Show("Invalid file extension for single-file compile.", "Invalid Extension", MessageBoxButtons.OK);
				return true;
			}

			// If there's already a build in progress, don't let another one start
			if (DTE.Solution.SolutionBuild.BuildState == vsBuildState.vsBuildStateInProgress)
			{
				if (MessageBox.Show("Cancel current compile?", "Compile in progress", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					DTE.ExecuteCommand("Build.Cancel");
				}
				return true;
			}

			// Make sure any existing build is stopped
			KillChildProcess();

			// Set up the output pane
			BuildOutputPane.Activate();
			BuildOutputPane.Clear();
			BuildOutputPane.OutputString(String.Format("1>------ Build started: Project: {0}, Configuration: {1} {2} ------\r\n", StartupProject.Name, ActiveConfiguration.ConfigurationName, ActiveConfiguration.PlatformName));
			BuildOutputPane.OutputString(String.Format("1>  Compiling {0}\r\n", FileToCompile));

			// Set up event handlers 
			DTE.Events.BuildEvents.OnBuildBegin += BuildEvents_OnBuildBegin;

			// Create a delegate for handling output messages
			DataReceivedEventHandler OutputHandler = (Sender, Args) => { if(Args.Data != null) BuildOutputPane.OutputString("1>  " + Args.Data + "\r\n"); };

			// Get the build command line and escape any environment variables that we use
			string BuildCommandLine = ActiveNMakeTool.BuildCommandLine;
			BuildCommandLine = BuildCommandLine.Replace("$(SolutionDir)", Path.GetDirectoryName(UnrealVSPackage.Instance.SolutionFilepath).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar);
			BuildCommandLine = BuildCommandLine.Replace("$(ProjectName)", VCStartupProject.Name);

			// Spawn the new process
			ChildProcess = new System.Diagnostics.Process();
			ChildProcess.StartInfo.FileName = Path.Combine(Environment.SystemDirectory, "cmd.exe");
			ChildProcess.StartInfo.Arguments = String.Format("/C {0} -singlefile=\"{1}\"", BuildCommandLine, FileToCompile);
			ChildProcess.StartInfo.WorkingDirectory = Path.GetDirectoryName(StartupProject.FullName);
			ChildProcess.StartInfo.UseShellExecute = false;
			ChildProcess.StartInfo.RedirectStandardOutput = true;
			ChildProcess.StartInfo.RedirectStandardError = true;
			ChildProcess.StartInfo.CreateNoWindow = true;
			ChildProcess.OutputDataReceived += OutputHandler;
			ChildProcess.ErrorDataReceived += OutputHandler;
			ChildProcess.Start();
			ChildProcess.BeginOutputReadLine();
			ChildProcess.BeginErrorReadLine();
			return true;
		}

		private void BuildEvents_OnBuildBegin(vsBuildScope Scope, vsBuildAction Action)
		{
			KillChildProcess();
		}
	}
}
