// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	enum BuildConfig
	{
		Debug,
		DebugGame,
		Development,
	}

	enum TabLabels
	{
		Stream,
		WorkspaceName,
		WorkspaceRoot,
		ProjectFile,
	}

	class UserWorkspaceSettings
	{
		// Settings for the currently synced project in this workspace. CurrentChangeNumber is only valid for this workspace if CurrentProjectPath is the current project.
		public string CurrentProjectIdentifier;
		public int CurrentChangeNumber;
		public List<int> AdditionalChangeNumbers = new List<int>();

		// Settings for the last attempted sync. These values are set to persist error messages between runs.
		public int LastSyncChangeNumber;
		public WorkspaceUpdateResult LastSyncResult;
		public string LastSyncResultMessage;
		public DateTime? LastSyncTime;
		public int LastSyncDurationSeconds;

		// The last successful build, regardless of whether a failed sync has happened in the meantime. Used to determine whether to force a clean due to entries in the project config file.
		public int LastBuiltChangeNumber;

		// Expanded archives in the workspace
		public string[] ExpandedArchiveTypes;

		// Workspace specific SyncFilters
		public string[] SyncView;
		public Guid[] SyncExcludedCategories;
	}

	class UserProjectSettings
	{
		public List<ConfigObject> BuildSteps = new List<ConfigObject>();
	}

	class UserSettings
	{
		string FileName;
		ConfigFile ConfigFile = new ConfigFile();

		// General settings
		public bool bBuildAfterSync;
		public bool bRunAfterSync;
		public bool bSyncPrecompiledEditor;
		public bool bOpenSolutionAfterSync;
		public bool bShowLogWindow;
		public bool bAutoResolveConflicts;
		public bool bUseIncrementalBuilds;
		public bool bShowLocalTimes;
		public bool bShowAllStreams;
		public bool bKeepInTray;
		public int FilterIndex;
		public string LastProjectFileName;
		public string[] OpenProjectFileNames;
		public string[] OtherProjectFileNames;
		public string[] SyncView;
		public Guid[] SyncExcludedCategories;
		public LatestChangeType SyncType;
		public BuildConfig CompiledEditorBuildConfig; // NB: This assumes not using precompiled editor. See CurrentBuildConfig.
		public TabLabels TabLabels;

		// Window settings
		public bool bHasWindowSettings;
		public Rectangle WindowRectangle;
		public Dictionary<string, int> ColumnWidths = new Dictionary<string,int>();
		public bool bWindowVisible;
		
		// Schedule settings
		public bool bScheduleEnabled;
		public TimeSpan ScheduleTime;
		public LatestChangeType ScheduleChange;

		// Run configuration
		public List<Tuple<string, bool>> EditorArguments = new List<Tuple<string,bool>>();

		// Project settings
		Dictionary<string, UserWorkspaceSettings> WorkspaceKeyToSettings = new Dictionary<string,UserWorkspaceSettings>();
		Dictionary<string, UserProjectSettings> ProjectKeyToSettings = new Dictionary<string,UserProjectSettings>();

		// Perforce settings
		public PerforceSyncOptions SyncOptions = new PerforceSyncOptions();

		public UserSettings(string InFileName)
		{
			FileName = InFileName;
			if(File.Exists(FileName))
			{
				ConfigFile.Load(FileName);
			}

			// General settings
			bBuildAfterSync = (ConfigFile.GetValue("General.BuildAfterSync", "1") != "0");
			bRunAfterSync = (ConfigFile.GetValue("General.RunAfterSync", "1") != "0");
			bSyncPrecompiledEditor = (ConfigFile.GetValue("General.SyncPrecompiledEditor", "0") != "0");
			bOpenSolutionAfterSync = (ConfigFile.GetValue("General.OpenSolutionAfterSync", "0") != "0");
			bShowLogWindow = (ConfigFile.GetValue("General.ShowLogWindow", false));
			bAutoResolveConflicts = (ConfigFile.GetValue("General.AutoResolveConflicts", "1") != "0");
			bUseIncrementalBuilds = ConfigFile.GetValue("General.IncrementalBuilds", true);
			bShowLocalTimes = ConfigFile.GetValue("General.ShowLocalTimes", false);
			bShowAllStreams = ConfigFile.GetValue("General.ShowAllStreams", false);
			bKeepInTray = ConfigFile.GetValue("General.KeepInTray", true);
			int.TryParse(ConfigFile.GetValue("General.FilterIndex", "0"), out FilterIndex);
			LastProjectFileName = ConfigFile.GetValue("General.LastProjectFileName", null);

			OpenProjectFileNames = ConfigFile.GetValues("General.OpenProjectFileNames", new string[0]);
			if(LastProjectFileName != null && !OpenProjectFileNames.Any(x => x.Equals(LastProjectFileName, StringComparison.InvariantCultureIgnoreCase)))
			{
				OpenProjectFileNames = OpenProjectFileNames.Concat(new string[]{ LastProjectFileName }).ToArray();
			}

			OtherProjectFileNames = ConfigFile.GetValues("General.OtherProjectFileNames", new string[0]);
			SyncView = ConfigFile.GetValues("General.SyncFilter", new string[0]);
			SyncExcludedCategories = ConfigFile.GetGuidValues("General.SyncExcludedCategories", new Guid[0]);
			if(!Enum.TryParse(ConfigFile.GetValue("General.SyncType", ""), out SyncType))
			{
				SyncType = LatestChangeType.Good;
			}

			// Build configuration
			string CompiledEditorBuildConfigName = ConfigFile.GetValue("General.BuildConfig", "");
			if(!Enum.TryParse(CompiledEditorBuildConfigName, true, out CompiledEditorBuildConfig))
			{
				CompiledEditorBuildConfig = BuildConfig.DebugGame;
			}

			// Tab names
			string TabNamesValue = ConfigFile.GetValue("General.TabNames", "");
			if(!Enum.TryParse(TabNamesValue, true, out TabLabels))
			{
				TabLabels = TabLabels.ProjectFile;
			}

			// Editor arguments
			string[] Arguments = ConfigFile.GetValues("General.EditorArguments", new string[]{ "0:-log", "0:-fastload" });
			foreach(string Argument in Arguments)
			{
				if(Argument.StartsWith("0:"))
				{
					EditorArguments.Add(new Tuple<string,bool>(Argument.Substring(2), false));
				}
				else if(Argument.StartsWith("1:"))
				{
					EditorArguments.Add(new Tuple<string,bool>(Argument.Substring(2), true));
				}
				else
				{
					EditorArguments.Add(new Tuple<string,bool>(Argument, true));
				}
			}

			// Window settings
			ConfigSection WindowSection = ConfigFile.FindSection("Window");
			if(WindowSection != null)
			{
				bHasWindowSettings = true;

				int X = WindowSection.GetValue("X", -1);
				int Y = WindowSection.GetValue("Y", -1);
				int Width = WindowSection.GetValue("Width", -1);
				int Height = WindowSection.GetValue("Height", -1);
				WindowRectangle = new Rectangle(X, Y, Width, Height);

				ConfigObject ColumnWidthObject = new ConfigObject(WindowSection.GetValue("ColumnWidths", ""));
				foreach(KeyValuePair<string, string> ColumnWidthPair in ColumnWidthObject.Pairs)
				{
					int Value;
					if(int.TryParse(ColumnWidthPair.Value, out Value))
					{
						ColumnWidths[ColumnWidthPair.Key] = Value;
					}
				}

				bWindowVisible = WindowSection.GetValue("Visible", true);
			}

			// Schedule settings
			bScheduleEnabled = ConfigFile.GetValue("Schedule.Enabled", false);
			if(!TimeSpan.TryParse(ConfigFile.GetValue("Schedule.Time", ""), out ScheduleTime))
			{
				ScheduleTime = new TimeSpan(6, 0, 0);
			}
			if(!Enum.TryParse(ConfigFile.GetValue("Schedule.Change", ""), out ScheduleChange))
			{
				ScheduleChange = LatestChangeType.Good;
			}

			// Perforce settings
			if(!int.TryParse(ConfigFile.GetValue("Perforce.NumRetries", "0"), out SyncOptions.NumRetries))
			{
				SyncOptions.NumRetries = 0;
			}
			if(!int.TryParse(ConfigFile.GetValue("Perforce.NumThreads", "0"), out SyncOptions.NumThreads))
			{
				SyncOptions.NumThreads = 0;
			}
			if(!int.TryParse(ConfigFile.GetValue("Perforce.TcpBufferSize", "0"), out SyncOptions.TcpBufferSize))
			{
				SyncOptions.TcpBufferSize = 0;
			}
		}

		public UserWorkspaceSettings FindOrAddWorkspace(string ClientBranchPath)
		{
			// Update the current workspace
			string CurrentWorkspaceKey = ClientBranchPath.Trim('/');

			UserWorkspaceSettings CurrentWorkspace;
			if(!WorkspaceKeyToSettings.TryGetValue(CurrentWorkspaceKey, out CurrentWorkspace))
			{
				// Create a new workspace settings object
				CurrentWorkspace = new UserWorkspaceSettings();
				WorkspaceKeyToSettings.Add(CurrentWorkspaceKey, CurrentWorkspace);

				// Read the workspace settings
				ConfigSection WorkspaceSection = ConfigFile.FindSection(CurrentWorkspaceKey);
				if(WorkspaceSection == null)
				{
					string LegacyBranchAndClientKey = ClientBranchPath.Trim('/');

					int SlashIdx = LegacyBranchAndClientKey.IndexOf('/');
					if(SlashIdx != -1)
					{
						LegacyBranchAndClientKey = LegacyBranchAndClientKey.Substring(0, SlashIdx) + "$" + LegacyBranchAndClientKey.Substring(SlashIdx + 1);
					}

					string CurrentSync = ConfigFile.GetValue("Clients." + LegacyBranchAndClientKey, null);
					if(CurrentSync != null)
					{
						int AtIdx = CurrentSync.LastIndexOf('@');
						if(AtIdx != -1)
						{
							int ChangeNumber;
							if(int.TryParse(CurrentSync.Substring(AtIdx + 1), out ChangeNumber))
							{
								CurrentWorkspace.CurrentProjectIdentifier = CurrentSync.Substring(0, AtIdx);
								CurrentWorkspace.CurrentChangeNumber = ChangeNumber;
							}
						}
					}

					string LastUpdateResultText = ConfigFile.GetValue("Clients." + LegacyBranchAndClientKey + "$LastUpdate", null);
					if(LastUpdateResultText != null)
					{
						int ColonIdx = LastUpdateResultText.LastIndexOf(':');
						if(ColonIdx != -1)
						{
							int ChangeNumber;
							if(int.TryParse(LastUpdateResultText.Substring(0, ColonIdx), out ChangeNumber))
							{
								WorkspaceUpdateResult Result;
								if(Enum.TryParse(LastUpdateResultText.Substring(ColonIdx + 1), out Result))
								{
									CurrentWorkspace.LastSyncChangeNumber = ChangeNumber;
									CurrentWorkspace.LastSyncResult = Result;
								}
							}
						}
					}

					CurrentWorkspace.SyncView = new string[0];
					CurrentWorkspace.SyncExcludedCategories = new Guid[0];
				}
				else
				{
					CurrentWorkspace.CurrentProjectIdentifier = WorkspaceSection.GetValue("CurrentProjectPath");
					CurrentWorkspace.CurrentChangeNumber = WorkspaceSection.GetValue("CurrentChangeNumber", -1);
					foreach(string AdditionalChangeNumberString in WorkspaceSection.GetValues("AdditionalChangeNumbers", new string[0]))
					{
						int AdditionalChangeNumber;
						if(int.TryParse(AdditionalChangeNumberString, out AdditionalChangeNumber))
						{
							CurrentWorkspace.AdditionalChangeNumbers.Add(AdditionalChangeNumber);
						}
					}
					Enum.TryParse(WorkspaceSection.GetValue("LastSyncResult", ""), out CurrentWorkspace.LastSyncResult);
					CurrentWorkspace.LastSyncResultMessage = UnescapeText(WorkspaceSection.GetValue("LastSyncResultMessage"));
					CurrentWorkspace.LastSyncChangeNumber = WorkspaceSection.GetValue("LastSyncChangeNumber", -1);

					DateTime LastSyncTime;
					if(DateTime.TryParse(WorkspaceSection.GetValue("LastSyncTime", ""), out LastSyncTime))
					{
						CurrentWorkspace.LastSyncTime = LastSyncTime;
					}

					CurrentWorkspace.LastSyncDurationSeconds = WorkspaceSection.GetValue("LastSyncDuration", 0);
					CurrentWorkspace.LastBuiltChangeNumber = WorkspaceSection.GetValue("LastBuiltChangeNumber", 0);
					CurrentWorkspace.ExpandedArchiveTypes = WorkspaceSection.GetValues("ExpandedArchiveName", new string[0]);

					CurrentWorkspace.SyncView = WorkspaceSection.GetValues("SyncFilter", new string[0]);
					CurrentWorkspace.SyncExcludedCategories = WorkspaceSection.GetValues("SyncExcludedCategories", new Guid[0]);
				}
			}
			return CurrentWorkspace;
		}

		public UserProjectSettings FindOrAddProject(string ClientProjectFileName)
		{
			// Read the project settings
			UserProjectSettings CurrentProject;
			if(!ProjectKeyToSettings.TryGetValue(ClientProjectFileName, out CurrentProject))
			{
				CurrentProject = new UserProjectSettings();
				ProjectKeyToSettings.Add(ClientProjectFileName, CurrentProject);
	
				ConfigSection ProjectSection = ConfigFile.FindOrAddSection(ClientProjectFileName);
				CurrentProject.BuildSteps.AddRange(ProjectSection.GetValues("BuildStep", new string[0]).Select(x => new ConfigObject(x)));
			}
			return CurrentProject;
		}

		public void Save()
		{
			// General settings
			ConfigSection GeneralSection = ConfigFile.FindOrAddSection("General");
			GeneralSection.Clear();
			GeneralSection.SetValue("BuildAfterSync", bBuildAfterSync);
			GeneralSection.SetValue("RunAfterSync", bRunAfterSync);
			GeneralSection.SetValue("SyncPrecompiledEditor", bSyncPrecompiledEditor);
			GeneralSection.SetValue("OpenSolutionAfterSync", bOpenSolutionAfterSync);
			GeneralSection.SetValue("ShowLogWindow", bShowLogWindow);
			GeneralSection.SetValue("AutoResolveConflicts", bAutoResolveConflicts);
			GeneralSection.SetValue("IncrementalBuilds", bUseIncrementalBuilds);
			GeneralSection.SetValue("ShowLocalTimes", bShowLocalTimes);
			GeneralSection.SetValue("ShowAllStreams", bShowAllStreams);
			GeneralSection.SetValue("LastProjectFileName", LastProjectFileName);
			GeneralSection.SetValues("OpenProjectFileNames", OpenProjectFileNames);
			GeneralSection.SetValue("KeepInTray", bKeepInTray);
			GeneralSection.SetValue("FilterIndex", FilterIndex);
			GeneralSection.SetValues("OtherProjectFileNames", OtherProjectFileNames);
			GeneralSection.SetValues("SyncFilter", SyncView);
			GeneralSection.SetValues("SyncExcludedCategories", SyncExcludedCategories);
			GeneralSection.SetValue("SyncType", SyncType.ToString());

			// Build configuration
			GeneralSection.SetValue("BuildConfig", CompiledEditorBuildConfig.ToString());

			// Tab names
			GeneralSection.SetValue("TabNames", TabLabels.ToString());

			// Editor arguments
			List<string> EditorArgumentList = new List<string>();
			foreach(Tuple<string, bool> EditorArgument in EditorArguments)
			{
				EditorArgumentList.Add(String.Format("{0}:{1}", EditorArgument.Item2? 1 : 0, EditorArgument.Item1));
			}
			GeneralSection.SetValues("EditorArguments", EditorArgumentList.ToArray());

			// Schedule settings
			ConfigSection ScheduleSection = ConfigFile.FindOrAddSection("Schedule");
			ScheduleSection.Clear();
			ScheduleSection.SetValue("Enabled", bScheduleEnabled);
			ScheduleSection.SetValue("Time", ScheduleTime.ToString());
			ScheduleSection.SetValue("Change", ScheduleChange.ToString());

			// Window settings
			if(bHasWindowSettings)
			{
				ConfigSection WindowSection = ConfigFile.FindOrAddSection("Window");
				WindowSection.Clear();
				WindowSection.SetValue("X", WindowRectangle.X);
				WindowSection.SetValue("Y", WindowRectangle.Y);
				WindowSection.SetValue("Width", WindowRectangle.Width);
				WindowSection.SetValue("Height", WindowRectangle.Height);

				ConfigObject ColumnWidthsObject = new ConfigObject();
				foreach(KeyValuePair<string, int> ColumnWidthPair in ColumnWidths)
				{
					ColumnWidthsObject.SetValue(ColumnWidthPair.Key, ColumnWidthPair.Value.ToString());
				}
				WindowSection.SetValue("ColumnWidths", ColumnWidthsObject.ToString());

				WindowSection.SetValue("Visible", bWindowVisible);
			}

			// Current workspace settings
			foreach(KeyValuePair<string, UserWorkspaceSettings> Pair in WorkspaceKeyToSettings)
			{
				string CurrentWorkspaceKey = Pair.Key;
				UserWorkspaceSettings CurrentWorkspace = Pair.Value;

				ConfigSection WorkspaceSection = ConfigFile.FindOrAddSection(CurrentWorkspaceKey);
				WorkspaceSection.Clear();
				WorkspaceSection.SetValue("CurrentProjectPath", CurrentWorkspace.CurrentProjectIdentifier);
				WorkspaceSection.SetValue("CurrentChangeNumber", CurrentWorkspace.CurrentChangeNumber);
				WorkspaceSection.SetValues("AdditionalChangeNumbers", CurrentWorkspace.AdditionalChangeNumbers.Select(x => x.ToString()).ToArray());
				WorkspaceSection.SetValue("LastSyncResult", CurrentWorkspace.LastSyncResult.ToString());
				WorkspaceSection.SetValue("LastSyncResultMessage", EscapeText(CurrentWorkspace.LastSyncResultMessage));
				WorkspaceSection.SetValue("LastSyncChangeNumber", CurrentWorkspace.LastSyncChangeNumber);
				if(CurrentWorkspace.LastSyncTime.HasValue)
				{
					WorkspaceSection.SetValue("LastSyncTime", CurrentWorkspace.LastSyncTime.ToString());
				}
				if(CurrentWorkspace.LastSyncDurationSeconds > 0)
				{
					WorkspaceSection.SetValue("LastSyncDuration", CurrentWorkspace.LastSyncDurationSeconds);
				}
				WorkspaceSection.SetValue("LastBuiltChangeNumber", CurrentWorkspace.LastBuiltChangeNumber);
				WorkspaceSection.SetValues("ExpandedArchiveName", CurrentWorkspace.ExpandedArchiveTypes);
				WorkspaceSection.SetValues("SyncFilter", CurrentWorkspace.SyncView);
				WorkspaceSection.SetValues("SyncExcludedCategories", CurrentWorkspace.SyncExcludedCategories);
			}

			// Current project settings
			foreach(KeyValuePair<string, UserProjectSettings> Pair in ProjectKeyToSettings)
			{
				string CurrentProjectKey = Pair.Key;
				UserProjectSettings CurrentProject = Pair.Value;

				ConfigSection ProjectSection = ConfigFile.FindOrAddSection(CurrentProjectKey);
				ProjectSection.Clear();
				ProjectSection.SetValues("BuildStep", CurrentProject.BuildSteps.Select(x => x.ToString()).ToArray());
			}

			// Perforce settings
			ConfigSection PerforceSection = ConfigFile.FindOrAddSection("Perforce");
			PerforceSection.Clear();
			if(SyncOptions.NumRetries > 0)
			{
				PerforceSection.SetValue("NumRetries", SyncOptions.NumRetries);
			}
			if(SyncOptions.NumThreads > 0)
			{
				PerforceSection.SetValue("NumThreads", SyncOptions.NumThreads);
			}
			if(SyncOptions.TcpBufferSize > 0)
			{
				PerforceSection.SetValue("TcpBufferSize", SyncOptions.TcpBufferSize);
			}

			// Save the file
			ConfigFile.Save(FileName);
		}

		public static string[] GetCombinedSyncFilter(Dictionary<Guid, WorkspaceSyncCategory> UniqueIdToFilter, string[] GlobalView, Guid[] GlobalExcludedCategories, string[] WorkspaceView, Guid[] WorkspaceExcludedCategories)
		{
			List<string> Lines = new List<string>();
			foreach(string ViewLine in Enumerable.Concat(GlobalView, WorkspaceView).Select(x => x.Trim()).Where(x => x.Length > 0 && !x.StartsWith(";")))
			{
				Lines.Add(ViewLine);
			}

			HashSet<Guid> ExcludedCategories = new HashSet<Guid>(Enumerable.Concat(GlobalExcludedCategories, WorkspaceExcludedCategories));
			foreach(WorkspaceSyncCategory Filter in UniqueIdToFilter.Values.Where(x => x.bEnable && ExcludedCategories.Contains(x.UniqueId)).OrderBy(x => x.Name))
			{
				Lines.AddRange(Filter.Paths.Select(x => "-" + x.Trim()));
			}

			return Lines.ToArray();
		}

		static string EscapeText(string Text)
		{
			if(Text == null)
			{
				return null;
			}

			StringBuilder Result = new StringBuilder();
			for(int Idx = 0; Idx < Text.Length; Idx++)
			{
				switch(Text[Idx])
				{
					case '\\':
						Result.Append("\\\\");
						break;
					case '\t':
						Result.Append("\\t");
						break;
					case '\r':
						Result.Append("\\r");
						break;
					case '\n':
						Result.Append("\\n");
						break;
					case '\'':
						Result.Append("\\\'");
						break;
					case '\"':
						Result.Append("\\\"");
						break;
					default:
						Result.Append(Text[Idx]);
						break;
				}
			}
			return Result.ToString();
		}

		static string UnescapeText(string Text)
		{
			if(Text == null)
			{
				return null;
			}

			StringBuilder Result = new StringBuilder();
			for(int Idx = 0; Idx < Text.Length; Idx++)
			{
				if(Text[Idx] == '\\' && Idx + 1 < Text.Length)
				{
					switch(Text[++Idx])
					{
						case 't':
							Result.Append('\t');
							break;
						case 'r':
							Result.Append('\r');
							break;
						case 'n':
							Result.Append('\n');
							break;
						case '\'':
							Result.Append('\'');
							break;
						case '\"':
							Result.Append('\"');
							break;
						default:
							Result.Append(Text[Idx]);
							break;
					}
				}
				else
				{
					Result.Append(Text[Idx]);
				}
			}
			return Result.ToString();
		}
	}
}
