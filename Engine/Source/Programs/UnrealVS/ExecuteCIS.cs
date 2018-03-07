// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.ComponentModel.Design;
using System.IO;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.OLE.Interop;
using EnvDTE;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using EnvDTE80;


namespace UnrealVS
{
	class ExecuteCIS
	{
		const int ExecuteCISButtonID = 0x1200;


		public ExecuteCIS()
		{
			// ExecuteCISButton
			{
				var CommandID = new CommandID(GuidList.UnrealVSCmdSet, ExecuteCISButtonID);
				ExecuteCISButtonCommand = new MenuCommand(new EventHandler(ExecuteCISButtonHandler), CommandID);
				UnrealVSPackage.Instance.MenuCommandService.AddCommand(ExecuteCISButtonCommand);
			}


			// Register to find out when the solution is closed or opened
			UnrealVSPackage.Instance.OnSolutionOpened += OnSolutionOpened;
			UnrealVSPackage.Instance.OnSolutionClosed += OnSolutionClosed;

			UpdateExecuteCISButtonState();
		}



		/// Called when 'ExecuteCIS' button is clicked
		void ExecuteCISButtonHandler(object Sender, EventArgs Args)
		{
			// Don't allow CIS while we are already running CIS
			if (ExecuteCISProcess == null)
			{
				// Get the command we should execute for CIS
				string UATBuildString = GetUATBuildString();
				string UATExecutionString = GetUATExecutionString();

				if ((!String.IsNullOrEmpty(UATBuildString)) && (!String.IsNullOrEmpty(UATExecutionString)))
				{
					// Make sure the Output window is visible
					UnrealVSPackage.Instance.DTE.ExecuteCommand( "View.Output" );

					// Activate the output pane
					var Pane = UnrealVSPackage.Instance.GetOutputPane();
					if( Pane != null )
					{
						// Clear and activate the output pane.
						Pane.Clear();

						// @todo: Activating doesn't seem to really bring the pane to front like we would expect it to.
						Pane.Activate();
					}

					ExecuteCISProcess = UnrealVSPackage.Instance.LaunchProgram(
						"cmd.exe", "/C " + UATBuildString,
						OnBuildUATProcessExit,
						OnOutputFromExecuteCISProcess );
				}

				UpdateExecuteCISButtonState();
			}
		}


		/// Called when the ExecuteCIS process terminates
		void OnBuildUATProcessExit(object Sender, EventArgs Args)
		{
			string UATExecutionString = GetUATExecutionString();

			ExecuteCISProcess = UnrealVSPackage.Instance.LaunchProgram(
				"cmd.exe", "/C " + UATExecutionString,
				OnExecuteCISProcessExit,
				OnOutputFromExecuteCISProcess);

			UpdateExecuteCISButtonState();
		}


		/// Called when the ExecuteCIS process terminates
		void OnExecuteCISProcessExit(object Sender, EventArgs Args)
		{
			ExecuteCISProcess = null;

			UpdateExecuteCISButtonState();
		}
		
		/// <summary>
		/// Updates the enabled/disabled state of the 'ExecuteCIS' button
		/// </summary>
		void UpdateExecuteCISButtonState(bool ShouldForceDisable = false)
		{
			var CanExecuteCIS = false;

			if( !ShouldForceDisable )
			{
				// Can't launch when a process is already running
				if (ExecuteCISProcess == null)
				{
					// Get the command we should execute for CIS
					string UATExecutionString = GetUATExecutionString();

					if (UATExecutionString != null)
					{
						CanExecuteCIS = true;
					}
					else
					{
						// No solution loaded, or solution doesn't look like UE4
					}
				}
			}

			ExecuteCISButtonCommand.Enabled = CanExecuteCIS;
		}


		/// <summary>
		/// Called when a solution is opened
		/// </summary>
		public void OnSolutionOpened()
		{
			UpdateExecuteCISButtonState();
		}


		/// <summary>
		/// Called when the solution is closed
		/// </summary>
		public void OnSolutionClosed()
		{
			UpdateExecuteCISButtonState(ShouldForceDisable: true);
		}


		/// <summary>
		/// Called when data is received from the ExecuteCIS process
		/// </summary>
		public void OnOutputFromExecuteCISProcess(object Sender, DataReceivedEventArgs Args)
		{
			var Pane = UnrealVSPackage.Instance.GetOutputPane();
			if( Pane != null )
			{
				Pane.OutputString( Args.Data + "\n" );
			}
		}


		private string GetUATBuildString()
		{
			string SolutionDirectory, SolutionFile, UserOptsFile;
			UnrealVSPackage.Instance.SolutionManager.GetSolutionInfo(out SolutionDirectory, out SolutionFile, out UserOptsFile);
			if (SolutionFile != null && Path.GetFileName(SolutionFile).Equals("UE4.sln", StringComparison.InvariantCultureIgnoreCase))
			{
				//TODO - replace FrameworkVersion
				// Generate string for building UAT
				string FrameworkVersion = "v4.0.30319";
				string MSBuild10Application = GetEnvironmentVariableBasedPath("FrameworkDir", "C:\\Windows\\Microsoft.NET\\Framework", Path.Combine(FrameworkVersion, "MSBuild.exe"));
				string Parameters = Path.Combine(Path.GetDirectoryName(SolutionFile), "Engine\\Source\\Programs\\UnrealAutomationTool\\UnrealAutomationTool.csproj");
				Parameters += " /verbosity:quiet /target:Rebuild /property:Configuration=\"Development\" /property:Platform=\"AnyCPU\"";

				string FullCommandline = MSBuild10Application + " " + Parameters;
				return FullCommandline;
			}
			return null;
		}

		private string GetUATExecutionString()
		{
			string SolutionDirectory, SolutionFile, UserOptsFile;
			UnrealVSPackage.Instance.SolutionManager.GetSolutionInfo( out SolutionDirectory, out SolutionFile, out UserOptsFile );
			if( SolutionFile != null && Path.GetFileName( SolutionFile ).Equals( "UE4.sln", StringComparison.InvariantCultureIgnoreCase ) )
			{
				// UAT knows all the details of what to run
				string UATExecutionString = Path.Combine(Path.GetDirectoryName(SolutionFile), "Engine\\Intermediate\\Build\\UnrealAutomationTool\\Development\\UnrealAutomationTool.exe");
				UATExecutionString += " CIS Description=\"Local CIS\"";
				return UATExecutionString;
			}
			return null;
		}

		//TODO - move into some shared C# code area
		// Helper function to acquire a file path based on the contents on an environment variable
		private string GetEnvironmentVariableBasedPath( string EnvVarName, string DefaultValue, string PathInfo )
		{
			string EnvVarValue = Environment.GetEnvironmentVariable( EnvVarName );
			if( DefaultValue != null && EnvVarValue == null )
			{
				Environment.SetEnvironmentVariable( EnvVarName, DefaultValue );
				EnvVarValue = Environment.GetEnvironmentVariable( EnvVarName );
			}

			if( EnvVarValue != null )
			{
				return Path.Combine( EnvVarValue, PathInfo );
			}

			return null;
		}

		/// Active process for the "execute CIS" command
		System.Diagnostics.Process ExecuteCISProcess;

		/// Command for 'Execute CIS' button
		MenuCommand ExecuteCISButtonCommand;
	}

}
