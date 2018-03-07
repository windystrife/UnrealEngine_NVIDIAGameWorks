// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.ComponentModel.Design;
using System.IO;
using System.Linq;
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
	internal class CommandLineEditor
	{
		/** constants */

		private const int ComboID = 0x1030;
		private const int ComboListID = 0x1040;
		private const int ComboListCountMax = 32;

		/** methods */

		public CommandLineEditor()
		{
			// Create the handlers for our commands
			{
				// CommandLineCombo
				var ComboCommandID = new CommandID(GuidList.UnrealVSCmdSet, ComboID);
				ComboCommand = new OleMenuCommand(new EventHandler(ComboHandler), ComboCommandID);
				UnrealVSPackage.Instance.MenuCommandService.AddCommand(ComboCommand);

				// CommandLineComboList
				var ComboListCommandID = new CommandID(GuidList.UnrealVSCmdSet, ComboListID);
				ComboCommandList = new OleMenuCommand(new EventHandler(ComboListHandler), ComboListCommandID);
				UnrealVSPackage.Instance.MenuCommandService.AddCommand(ComboCommandList);
			}

			// Register for events that we care about
			UnrealVSPackage.Instance.OnSolutionOpened += UpdateCommandLineCombo;
			UnrealVSPackage.Instance.OnSolutionClosed += UpdateCommandLineCombo;
			UnrealVSPackage.Instance.OnStartupProjectChanged += (p) => UpdateCommandLineCombo();
			UnrealVSPackage.Instance.OnStartupProjectPropertyChanged += OnStartupProjectPropertyChanged;
			UnrealVSPackage.Instance.OnStartupProjectConfigChanged += OnStartupProjectConfigChanged;

			UpdateCommandLineCombo();
		}

		/// <summary>
		/// Called when a startup project property has changed.  We'll refresh our interface.
		/// </summary>
		public void OnStartupProjectPropertyChanged(UInt32 itemid, Int32 propid, UInt32 flags)
		{
			IVsHierarchy ProjectHierarchy;
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject(out ProjectHierarchy);
			if (ProjectHierarchy != null)
			{
				
				// @todo: filter this so that we only respond to changes in the command line property
				UpdateCommandLineCombo();
			}
		}

		/// <summary>
		/// Reads options out of the solution file.
		/// </summary>
		public void LoadOptions(Stream Stream)
		{
			using (BinaryReader Reader = new BinaryReader(Stream))
			{
				List<string> CommandLines = new List<string>();
				int Count = Reader.ReadInt32();
				for (int CommandLineIdx = 0; CommandLineIdx < Count; CommandLineIdx++)
				{
					CommandLines.Add(Reader.ReadString());
				}
				ComboList.Clear();
				ComboList.AddRange(CommandLines.ToArray());
			}
		}

		/// <summary>
		/// Writes options to the solution file.
		/// </summary>
		public void SaveOptions(Stream Stream)
		{
			using (BinaryWriter Writer = new BinaryWriter(Stream))
			{
				string[] CommandLines = ComboList.ToArray();
				Writer.Write(CommandLines.Length);
				for (int CommandLineIdx = 0; CommandLineIdx < CommandLines.Length; CommandLineIdx++)
				{
					Writer.Write(CommandLines[CommandLineIdx]);
				}
			}
		}

		/// <summary>
		/// Called when the startup project active config has changed.
		/// </summary>
		private void OnStartupProjectConfigChanged(Project Project)
		{
			UpdateCommandLineCombo();
		}

		/// <summary>
		/// Updates the command-line combo box after the project loaded state has changed
		/// </summary>
		private void UpdateCommandLineCombo()
		{
			// Enable or disable our command-line selector
			DesiredCommandLine = null;	// clear this state var used by the combo box handler
			IVsHierarchy ProjectHierarchy;
			UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject( out ProjectHierarchy );
			if( ProjectHierarchy != null )
			{
				ComboCommand.Enabled = true;
				ComboCommandList.Enabled = true;
			}
			else
			{
				ComboCommand.Enabled = false;
				ComboCommandList.Enabled = false;
			}

			string CommandLine = MakeCommandLineComboText();
			CommitCommandLineToMRU(CommandLine);
		}


		/// <summary>
		/// Returns a string to display in the command-line combo box, based on project state
		/// </summary>
		/// <returns>String to display</returns>
		private string MakeCommandLineComboText()
		{
			string Text = "";

			IVsHierarchy ProjectHierarchy;
			if (UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject(out ProjectHierarchy) == VSConstants.S_OK && ProjectHierarchy != null)
			{
				Project SelectedStartupProject = Utils.HierarchyObjectToProject(ProjectHierarchy);
				if (SelectedStartupProject != null)
				{
					Configuration SelectedConfiguration = SelectedStartupProject.ConfigurationManager.ActiveConfiguration;
					if (SelectedConfiguration != null)
					{
						IVsBuildPropertyStorage PropertyStorage = ProjectHierarchy as IVsBuildPropertyStorage;
						if (PropertyStorage != null)
						{
							// Query the property store for the debugger arguments
							string ConfigurationName = String.Format("{0}|{1}", SelectedConfiguration.ConfigurationName, SelectedConfiguration.PlatformName);
							if (PropertyStorage.GetPropertyValue("LocalDebuggerCommandArguments", ConfigurationName, (uint)_PersistStorageType.PST_USER_FILE, out Text) != VSConstants.S_OK)
							{
								if (PropertyStorage.GetPropertyValue("StartArguments", ConfigurationName, (uint)_PersistStorageType.PST_USER_FILE, out Text) != VSConstants.S_OK)
								{
									Text = "";
								}
							}

							// for "Game" projects automatically remove the game project filename from the start of the command line
							var ActiveConfiguration = (SolutionConfiguration2)UnrealVSPackage.Instance.DTE.Solution.SolutionBuild.ActiveConfiguration;
							if (UnrealVSPackage.Instance.IsUE4Loaded && Utils.IsGameProject(SelectedStartupProject) && Utils.HasUProjectCommandLineArg(ActiveConfiguration.Name))
							{
								string AutoPrefix = Utils.GetAutoUProjectCommandLinePrefix(SelectedStartupProject);
								if (!string.IsNullOrEmpty(AutoPrefix))
								{
									if (Text.Trim().StartsWith(AutoPrefix, StringComparison.OrdinalIgnoreCase))
									{
										Text = Text.Trim().Substring(AutoPrefix.Length).Trim();
									}
								}
							}
						}
					}
				}
			}

			return Text;
		}


		/// Called by combo control to query the text to display or to apply newly-entered text
		private void ComboHandler(object Sender, EventArgs Args)
		{
			try
			{
				var OleArgs = (OleMenuCmdEventArgs)Args;

				string InString = OleArgs.InValue as string;
				if (InString != null)
				{
					// New text set on the combo - set the command line property
					DesiredCommandLine = null;
					CommitCommandLineText(InString);
				}
				else if (OleArgs.OutValue != IntPtr.Zero)
				{
					string EditingString = null;
					if (OleArgs.InValue != null)
					{
						object[] InArray = OleArgs.InValue as object[];
						if (InArray != null && 0 < InArray.Length)
						{
							EditingString = InArray.Last() as string;
						}
					}

					string TextToDisplay = string.Empty;
					if (EditingString != null)
					{
						// The control wants to put EditingString in the box
						TextToDisplay = DesiredCommandLine = EditingString;
					}
					else
					{
						// This is always hit at the end of interaction with the combo
						if (DesiredCommandLine != null)
						{
							TextToDisplay = DesiredCommandLine;
							DesiredCommandLine = null;
							CommitCommandLineText(TextToDisplay);
						}
						else
						{
							TextToDisplay = MakeCommandLineComboText();
						}
					}

					Marshal.GetNativeVariantForObject(TextToDisplay, OleArgs.OutValue);
				}
			}
			catch (Exception ex)
			{
				Exception AppEx = new ApplicationException("CommandLineEditor threw an exception in ComboHandler()", ex);
				Logging.WriteLine(AppEx.ToString());
				throw AppEx;
			}
		}

		private void CommitCommandLineText(string CommandLine)
		{
			IVsHierarchy ProjectHierarchy;
			if (UnrealVSPackage.Instance.SolutionBuildManager.get_StartupProject(out ProjectHierarchy) == VSConstants.S_OK && ProjectHierarchy != null)
			{
				Project SelectedStartupProject = Utils.HierarchyObjectToProject(ProjectHierarchy);
				if (SelectedStartupProject != null)
				{
					Configuration SelectedConfiguration = SelectedStartupProject.ConfigurationManager.ActiveConfiguration;
					if (SelectedConfiguration != null)
					{
						IVsBuildPropertyStorage PropertyStorage = ProjectHierarchy as IVsBuildPropertyStorage;
						if (PropertyStorage != null)
						{
							string FullCommandLine = CommandLine;

							// for "Game" projects automatically remove the game project filename from the start of the command line
							var ActiveConfiguration = (SolutionConfiguration2)UnrealVSPackage.Instance.DTE.Solution.SolutionBuild.ActiveConfiguration;
							if (UnrealVSPackage.Instance.IsUE4Loaded && Utils.IsGameProject(SelectedStartupProject) && Utils.HasUProjectCommandLineArg(ActiveConfiguration.Name))
							{
								string AutoPrefix = Utils.GetAutoUProjectCommandLinePrefix(SelectedStartupProject);
								if (FullCommandLine.IndexOf(Utils.UProjectExtension, StringComparison.OrdinalIgnoreCase) < 0 &&
									string.Compare(FullCommandLine, SelectedStartupProject.Name, StringComparison.OrdinalIgnoreCase) != 0 &&
									!FullCommandLine.StartsWith(SelectedStartupProject.Name + " ", StringComparison.OrdinalIgnoreCase))
								{
									// Committed command line does not specify a .uproject
									FullCommandLine = AutoPrefix + " " + FullCommandLine;
								}
							}

							// Get the project platform name.
							string ProjectPlatformName = SelectedConfiguration.PlatformName;
							if(ProjectPlatformName == "Any CPU")
							{
								ProjectPlatformName = "AnyCPU";
							}

							// Get the project kind. C++ projects store the debugger arguments differently to other project types.
							string ProjectKind = SelectedStartupProject.Kind;

							// Update the property
							string ProjectConfigurationName = String.Format("{0}|{1}", SelectedConfiguration.ConfigurationName, ProjectPlatformName);
							if (String.Equals(ProjectKind, "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}", StringComparison.InvariantCultureIgnoreCase))
							{
								if (FullCommandLine.Length == 0)
								{
									PropertyStorage.RemoveProperty("LocalDebuggerCommandArguments", ProjectConfigurationName, (uint)_PersistStorageType.PST_USER_FILE);
								}
								else
								{
									PropertyStorage.SetPropertyValue("LocalDebuggerCommandArguments", ProjectConfigurationName, (uint)_PersistStorageType.PST_USER_FILE, FullCommandLine);
								}
							}
							else
							{
								// For some reason, have to update C# projects this way, otherwise the properties page doesn't update. Conversely, SelectedConfiguration.Properties is always null for C++ projects in VS2017.
								if (SelectedConfiguration.Properties != null)
								{
									foreach (Property Property in SelectedConfiguration.Properties)
									{
										if (Property.Name.Equals("StartArguments", StringComparison.InvariantCultureIgnoreCase))
										{
											Property.Value = FullCommandLine;
											break;
										}
									}
								}
							}
						}
					}
				}
			}

			CommitCommandLineToMRU(CommandLine);
		}

		private void CommitCommandLineToMRU(string CommandLine)
		{
			if (0 < CommandLine.Length)
			{
				// Maintain the MRU history
				// Adds the entered command line to the top of the list
				// Moves it to the top if it's already in the list
				// Trims the list to the max length if it's too big
				ComboList.RemoveAll(s => 0 == string.Compare(s, CommandLine));
				ComboList.Insert(0, CommandLine);
				if (ComboList.Count > ComboListCountMax)
				{
					ComboList.RemoveAt(ComboList.Count - 1);
				}
			}
		}

		/// Called by combo control to populate the drop-down list
		private void ComboListHandler(object Sender, EventArgs Args)
		{
			var OleArgs = (OleMenuCmdEventArgs)Args;

			Marshal.GetNativeVariantForObject(ComboList.ToArray(), OleArgs.OutValue);
		}

		/// List of MRU strings to show in the combobox drop-down list
		private readonly List<string> ComboList = new List<string>();

		/// Combo control for command-line editor
		private OleMenuCommand ComboCommand;
		private OleMenuCommand ComboCommandList;

		/// Used to store the user edited commandline mid-edit, in the combo handler
		private string DesiredCommandLine;
	}
}
