// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Data.SqlClient;
using System.Deployment.Application;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;
using Microsoft.Win32;

using EventWaitHandle = System.Threading.EventWaitHandle;

namespace UnrealGameSync
{
	public enum LatestChangeType
	{
		Any,
		Good,
		Starred,
	}

	interface IWorkspaceControlOwner
	{
		void BrowseForProject(WorkspaceControl Workspace);
		void RequestProjectChange(WorkspaceControl Workspace, string ProjectFileName);
		void ShowAndActivate();
		void StreamChanged(WorkspaceControl Workspace);
		void SetTabNames(TabLabels TabNames);
		void SetupScheduledSync();
		void UpdateProgress();
	}

	partial class WorkspaceControl : UserControl
	{
		static Rectangle GoodBuildIcon = new Rectangle(0, 0, 16, 16);
		static Rectangle MixedBuildIcon = new Rectangle(16, 0, 16, 16);
		static Rectangle BadBuildIcon = new Rectangle(32, 0, 16, 16);
		static Rectangle DefaultBuildIcon = new Rectangle(48, 0, 16, 16);
		static Rectangle PromotedBuildIcon = new Rectangle(64, 0, 16, 16);
		static Rectangle DetailsIcon = new Rectangle(80, 0, 16, 16);
		static Rectangle InfoIcon = new Rectangle(96, 0, 16, 16);
		static Rectangle CancelIcon = new Rectangle(112, 0, 16, 16);
		static Rectangle SyncIcon = new Rectangle(128, 0, 32, 16);
		static Rectangle HappyIcon = new Rectangle(160, 0, 16, 16);
		static Rectangle DisabledHappyIcon = new Rectangle(176, 0, 16, 16);
		static Rectangle FrownIcon = new Rectangle(192, 0, 16, 16);
		static Rectangle DisabledFrownIcon = new Rectangle(208, 0, 16, 16);
		static Rectangle PreviousSyncIcon = new Rectangle(224, 0, 16, 16);
		static Rectangle AdditionalSyncIcon = new Rectangle(240, 0, 16, 16);

		[DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
		static extern int SetWindowTheme(IntPtr hWnd, string pszSubAppName, string pszSubIdList);

		const string EditorArchiveType = "Editor";

		IWorkspaceControlOwner Owner;
		string SqlConnectionString;
		string DataFolder;
		BoundedLogWriter Log;

		UserSettings Settings;
		UserWorkspaceSettings WorkspaceSettings;
		UserProjectSettings ProjectSettings;

		public string SelectedFileName
		{
			get;
			private set;
		}

		string SelectedProjectIdentifier;

		public string BranchDirectoryName
		{
			get;
			private set;
		}

		public string StreamName
		{
			get;
			private set;
		}

		public string ClientName
		{
			get;
			private set;
		}

		string EditorTargetName;
		PerforceMonitor PerforceMonitor;
		Workspace Workspace;
		EventMonitor EventMonitor;
		Timer UpdateTimer;
		HashSet<int> PromotedChangeNumbers = new HashSet<int>();
		List<int> ListIndexToChangeIndex = new List<int>();
		List<int> SortedChangeNumbers = new List<int>();
		Dictionary<int, string> ChangeNumberToArchivePath = new Dictionary<int,string>();
		List<ToolStripMenuItem> CustomToolMenuItems = new List<ToolStripMenuItem>();
		int NumChanges;
		int PendingSelectedChangeNumber = -1;
		bool bHasBuildSteps = false;

		Dictionary<string, int> NotifiedBuildTypeToChangeNumber = new Dictionary<string,int>();

		TimeSpan ServerTimeZone;

		int HoverItem = -1;
		BuildData HoverBadge;
		bool bHoverSync;
		PerforceChangeSummary ContextMenuChange;
		VisualStyleRenderer SelectedItemRenderer;
		VisualStyleRenderer TrackedItemRenderer;
		Font BuildFont;
		Font SelectedBuildFont;
		Font BadgeFont;
		bool bUnstable;

		bool bRestoreStateOnLoad;

		string OriginalExecutableFileName;

		NotificationWindow NotificationWindow;

		public Tuple<TaskbarState, float> DesiredTaskbarState
		{
			get;
			private set;
		}

		public WorkspaceControl(IWorkspaceControlOwner InOwner, string InSqlConnectionString, string InDataFolder, bool bInRestoreStateOnLoad, string InOriginalExecutableFileName, string InProjectFileName, bool bInUnstable, DetectProjectSettingsTask DetectSettings, BoundedLogWriter InLog, UserSettings InSettings)
		{
			InitializeComponent();

            if (Application.RenderWithVisualStyles) 
            { 
				SelectedItemRenderer = new VisualStyleRenderer("Explorer::ListView", 1, 3);
				TrackedItemRenderer = new VisualStyleRenderer("Explorer::ListView", 1, 2); 
			}

			Owner = InOwner;
			SqlConnectionString = InSqlConnectionString;
			DataFolder = InDataFolder;
			bRestoreStateOnLoad = bInRestoreStateOnLoad;
			OriginalExecutableFileName = InOriginalExecutableFileName;
			bUnstable = bInUnstable;
			Log = InLog;
			Settings = InSettings;
			WorkspaceSettings = InSettings.FindOrAddWorkspace(DetectSettings.BranchClientPath);
			ProjectSettings = InSettings.FindOrAddProject(DetectSettings.NewSelectedClientFileName);

			DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
			
			System.Reflection.PropertyInfo DoubleBufferedProperty = typeof(Control).GetProperty("DoubleBuffered", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
			DoubleBufferedProperty.SetValue(BuildList, true, null); 

			// force the height of the rows
			BuildList.SmallImageList = new ImageList(){ ImageSize = new Size(1, 20) };
			BuildList_FontChanged(null, null);
			BuildList.OnScroll += BuildList_OnScroll;

			Splitter.OnVisibilityChanged += Splitter_OnVisibilityChanged;

			UpdateTimer = new Timer();
			UpdateTimer.Interval = 30;
			UpdateTimer.Tick += TimerCallback;

			UpdateCheckedBuildConfig();

			UpdateSyncActionCheckboxes();

			if(Settings.bHasWindowSettings)
			{
				for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
				{
					string ColumnName = BuildList.Columns[Idx].Text;
					if(String.IsNullOrEmpty(ColumnName))
					{
						ColumnName = String.Format("Column{0}", Idx);
					}

					int ColumnWidth;
					if(Settings.ColumnWidths.TryGetValue(ColumnName, out ColumnWidth))
					{
						BuildList.Columns[Idx].Width = ColumnWidth;
					}
				}
			}

			// Set the project logo on the status panel and notification window
			NotificationWindow = new NotificationWindow(Properties.Resources.DefaultNotificationLogo);
			StatusPanel.SetProjectLogo(DetectSettings.ProjectLogo);
			DetectSettings.ProjectLogo = null;

			// Commit all the new project info
			PerforceConnection PerforceClient = DetectSettings.PerforceClient;
			ClientName = PerforceClient.ClientName;
			SelectedFileName = InProjectFileName;
			SelectedProjectIdentifier = DetectSettings.NewSelectedProjectIdentifier;
			EditorTargetName = DetectSettings.NewProjectEditorTarget;
			StreamName = DetectSettings.StreamName;
			ServerTimeZone = DetectSettings.ServerTimeZone;

			// Update the branch directory
			BranchDirectoryName = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(DetectSettings.BaseEditorTargetPath), "..", ".."));

			// Check if we've the project we've got open in this workspace is the one we're actually synced to
			int CurrentChangeNumber = -1;
			if(String.Compare(WorkspaceSettings.CurrentProjectIdentifier, SelectedProjectIdentifier, true) == 0)
			{
				CurrentChangeNumber = WorkspaceSettings.CurrentChangeNumber;
			}

			string ProjectLogBaseName = Path.Combine(DataFolder, String.Format("{0}@{1}", PerforceClient.ClientName, DetectSettings.BranchClientPath.Replace("//" + PerforceClient.ClientName + "/", "").Trim('/').Replace("/", "$")));

			string TelemetryProjectIdentifier = PerforceUtils.GetClientOrDepotDirectoryName(DetectSettings.NewSelectedProjectIdentifier);

			Workspace = new Workspace(PerforceClient, BranchDirectoryName, SelectedFileName, DetectSettings.BranchClientPath, DetectSettings.NewSelectedClientFileName, CurrentChangeNumber, WorkspaceSettings.LastBuiltChangeNumber, TelemetryProjectIdentifier, new LogControlTextWriter(SyncLog));
			Workspace.OnUpdateComplete += UpdateCompleteCallback;

			PerforceMonitor = new PerforceMonitor(PerforceClient, DetectSettings.BranchClientPath, DetectSettings.NewSelectedClientFileName, SelectedProjectIdentifier, ProjectLogBaseName + ".p4.log");
			PerforceMonitor.OnUpdate += UpdateBuildListCallback;
			PerforceMonitor.OnUpdateMetadata += UpdateBuildMetadataCallback;
			PerforceMonitor.OnStreamChange += StreamChangedCallbackAsync;

			EventMonitor = new EventMonitor(SqlConnectionString, PerforceUtils.GetClientOrDepotDirectoryName(SelectedProjectIdentifier), DetectSettings.PerforceClient.UserName, ProjectLogBaseName + ".review.log");
			EventMonitor.OnUpdatesReady += UpdateReviewsCallback;

			string LogFileName = Path.Combine(DataFolder, ProjectLogBaseName + ".sync.log");
			SyncLog.OpenFile(LogFileName);

			Splitter.SetLogVisibility(Settings.bShowLogWindow);

			BuildList.Items.Clear();
			UpdateBuildList();
			UpdateBuildSteps();
			UpdateSyncActionCheckboxes();
			UpdateStatusPanel();

			if(CurrentChangeNumber != -1)
			{
				SelectChange(CurrentChangeNumber);
			}
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);

			PerforceMonitor.Start();
			EventMonitor.Start();
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}

			CloseProject();

			if(BuildFont != null)
			{
				BuildFont.Dispose();
				BuildFont = null;
			}
			if(SelectedBuildFont != null)
			{
				SelectedBuildFont.Dispose();
				SelectedBuildFont = null;
			}
			if(BadgeFont != null)
			{
				BadgeFont.Dispose();
				BadgeFont = null;
			}

			base.Dispose(disposing);
		}

		public bool IsBusy()
		{
			return Workspace.IsBusy();
		}

		public bool CanSyncNow()
		{
			return Workspace != null && !Workspace.IsBusy();
		}

		public bool CanLaunchEditor()
		{
			return Workspace != null && !Workspace.IsBusy() && Workspace.CurrentChangeNumber != -1;
		}

		private void MainWindow_Load(object sender, EventArgs e)
		{
			UpdateStatusPanel();
		}

		private void MainWindow_FormClosed(object sender, FormClosedEventArgs e)
		{
			CloseProject();
		}

		private void ShowErrorDialog(string Format, params object[] Args)
		{
			string Message = String.Format(Format, Args);
			Log.WriteLine(Message);
			MessageBox.Show(Message);
		}

		public bool CanClose()
		{
			CancelWorkspaceUpdate();
			return !Workspace.IsBusy();
		}

		private void StreamChangedCallback()
		{
			Owner.StreamChanged(this);
/*
			StatusPanel.SuspendDisplay();

			string PrevSelectedFileName = SelectedFileName;
			if(TryCloseProject())
			{
				OpenProject(PrevSelectedFileName);
			}

			StatusPanel.ResumeDisplay();*/
		}

		private void StreamChangedCallbackAsync()
		{
			BeginInvoke(new MethodInvoker(StreamChangedCallback));
		}

		private void CloseProject()
		{
			UpdateTimer.Stop();

			BuildList.Items.Clear();
			BuildList.Groups.Clear();

			SelectedFileName = null;
			SelectedProjectIdentifier = null;
			BranchDirectoryName = null;
			EditorTargetName = null;

			if(NotificationWindow != null)
			{
				NotificationWindow.Dispose();
				NotificationWindow = null;
			}
			if(PerforceMonitor != null)
			{
				PerforceMonitor.Dispose();
				PerforceMonitor = null;
			}
			if(Workspace != null)
			{
				Workspace.Dispose();
				Workspace = null;
			}
			if(EventMonitor != null)
			{
				EventMonitor.Dispose();
				EventMonitor = null;
			}

			ListIndexToChangeIndex = new List<int>();
			SortedChangeNumbers = new List<int>();
			NumChanges = 0;
			ContextMenuChange = null;
			HoverItem = -1;
			PendingSelectedChangeNumber = -1;
			NotifiedBuildTypeToChangeNumber = new Dictionary<string,int>();

			SyncLog.CloseFile();
			SyncLog.Clear();

			UpdateBuildSteps();

			StatusPanel.SetProjectLogo(null);

			DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
		}

		private void UpdateSyncConfig(int ChangeNumber)
		{
			WorkspaceSettings.CurrentProjectIdentifier = SelectedProjectIdentifier;
			WorkspaceSettings.CurrentChangeNumber = ChangeNumber;
			if(ChangeNumber == -1 || ChangeNumber != WorkspaceSettings.CurrentChangeNumber)
			{ 
				WorkspaceSettings.AdditionalChangeNumbers.Clear();
			}
			Settings.Save();
		}

		private void BuildList_OnScroll()
		{
			PendingSelectedChangeNumber = -1;
			UpdateNumRequestedBuilds(false);
		}

		void UpdateNumRequestedBuilds(bool bAllowShrink)
		{
			if(PerforceMonitor != null)
			{
				if(OnlyShowReviewedCheckBox.Checked)
				{
					PerforceMonitor.PendingMaxChanges = 1000;
				}
				else
				{
					int NumItemsPerPage = Math.Max(BuildList.GetVisibleItemsPerPage(), 10);

					// Find the number of visible items using a (slightly wasteful) binary search
					int VisibleItemCount = 1;
					for(int StepSize = BuildList.Items.Count / 2; StepSize >= 1; )
					{
						int TestIndex = VisibleItemCount + StepSize;
						if(TestIndex < BuildList.Items.Count && BuildList.GetItemRect(TestIndex).Top < BuildList.Height)
						{
							VisibleItemCount += StepSize;
						}
						else
						{
							StepSize /= 2;
						}
					}

					// Increase or decrease the number of builds we want, with a bit of rubber-banding
					const int IncreaseStep = 50;
					if(VisibleItemCount > BuildList.Items.Count - 20)
					{
						PerforceMonitor.PendingMaxChanges = NumChanges + IncreaseStep;
					}
					else if(bAllowShrink)
					{
						int NewNumChanges = ListIndexToChangeIndex[VisibleItemCount - 1] + IncreaseStep;
						if(NewNumChanges < NumChanges)
						{
							PerforceMonitor.PendingMaxChanges = NewNumChanges;
						}
					}
				}
			}
		}

		void StartSync(int ChangeNumber)
		{
			WorkspaceUpdateOptions Options = WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.GenerateProjectFiles;
			if(Settings.bAutoResolveConflicts)
			{
				Options |= WorkspaceUpdateOptions.AutoResolveChanges;
			}
			if(Settings.bUseIncrementalBuilds)
			{
				Options |= WorkspaceUpdateOptions.UseIncrementalBuilds;
			}
			if(Settings.bBuildAfterSync)
			{
				Options |= WorkspaceUpdateOptions.Build;
			}
			if(Settings.bBuildAfterSync && Settings.bRunAfterSync)
			{
				Options |= WorkspaceUpdateOptions.RunAfterSync;
			}
			if(Settings.bOpenSolutionAfterSync)
			{
				Options |= WorkspaceUpdateOptions.OpenSolutionAfterSync;
			}
			StartWorkspaceUpdate(ChangeNumber, Options);
		}

		void StartWorkspaceUpdate(int ChangeNumber, WorkspaceUpdateOptions Options)
		{
			if((Options & (WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.Build)) != 0 && IsEditorRunning(GetEditorBuildConfig()))
			{
				MessageBox.Show("Please close the editor before trying to sync/build.", "Editor Open");
				return;
			}

			string[] CombinedSyncFilter = UserSettings.GetCombinedSyncFilter(Workspace.GetSyncCategories(), Settings.SyncView, Settings.SyncExcludedCategories, WorkspaceSettings.SyncView, WorkspaceSettings.SyncExcludedCategories);

			WorkspaceUpdateContext Context = new WorkspaceUpdateContext(ChangeNumber, Options, CombinedSyncFilter, GetDefaultBuildStepObjects(), ProjectSettings.BuildSteps, null, GetWorkspaceVariables());
			if(Options.HasFlag(WorkspaceUpdateOptions.SyncArchives))
			{
				string EditorArchivePath = null;
				if(ShouldSyncPrecompiledEditor)
				{
					EditorArchivePath = GetArchivePathForChangeNumber(ChangeNumber);
					if(EditorArchivePath == null)
					{
						MessageBox.Show("There are no compiled editor binaries for this change. To sync it, you must disable syncing of precompiled editor binaries.");
						return;
					}
					Context.Options &= ~WorkspaceUpdateOptions.GenerateProjectFiles;
				}
				Context.ArchiveTypeToDepotPath.Add(EditorArchiveType, EditorArchivePath);
			}
			StartWorkspaceUpdate(Context);
		}

		void StartWorkspaceUpdate(WorkspaceUpdateContext Context)
		{
			Context.StartTime = DateTime.UtcNow;
			Context.PerforceSyncOptions = (PerforceSyncOptions)Settings.SyncOptions.Clone();

			Log.WriteLine();
			Log.WriteLine("Updating workspace at {0}...", Context.StartTime.ToLocalTime().ToString());
			Log.WriteLine("  ChangeNumber={0}", Context.ChangeNumber);
			Log.WriteLine("  Options={0}", Context.Options.ToString());
			Log.WriteLine("  Clobbering {0} files", Context.ClobberFiles.Count);

			if(Context.Options.HasFlag(WorkspaceUpdateOptions.Sync))
			{
				UpdateSyncConfig(-1);
				EventMonitor.PostEvent(Context.ChangeNumber, EventType.Syncing);
			}

			if(Context.Options.HasFlag(WorkspaceUpdateOptions.Sync) || Context.Options.HasFlag(WorkspaceUpdateOptions.Build))
			{
				if(!Context.Options.HasFlag(WorkspaceUpdateOptions.ContentOnly) && (Context.CustomBuildSteps == null || Context.CustomBuildSteps.Count == 0))
				{
					foreach(BuildConfig Config in Enum.GetValues(typeof(BuildConfig)))
					{
						List<string> EditorReceiptPaths = GetEditorReceiptPaths(Config);
						foreach(string EditorReceiptPath in EditorReceiptPaths)
						{
							if(File.Exists(EditorReceiptPath))
							{
								try { File.Delete(EditorReceiptPath); } catch(Exception){ }
							}
						}
					}
				}
			}

			SyncLog.Clear();
			Workspace.Update(Context);
			UpdateSyncActionCheckboxes();
			Refresh();
			UpdateTimer.Start();
		}

		void CancelWorkspaceUpdate()
		{
			if(Workspace.IsBusy() && MessageBox.Show("Are you sure you want to cancel the current operation?", "Cancel operation", MessageBoxButtons.YesNo) == DialogResult.Yes)
			{
				WorkspaceSettings.LastSyncChangeNumber = Workspace.PendingChangeNumber;
				WorkspaceSettings.LastSyncResult = WorkspaceUpdateResult.Canceled;
				WorkspaceSettings.LastSyncResultMessage = null;
				WorkspaceSettings.LastSyncTime = null;
				WorkspaceSettings.LastSyncDurationSeconds = 0;
				Settings.Save();

				Workspace.CancelUpdate();

				UpdateTimer.Stop();

				UpdateSyncActionCheckboxes();
				Refresh();
				UpdateSyncConfig(Workspace.CurrentChangeNumber);
				UpdateStatusPanel();
				DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
				Owner.UpdateProgress();
			}
		}

		void UpdateCompleteCallback(WorkspaceUpdateContext Context, WorkspaceUpdateResult Result, string ResultMessage)
		{
			Invoke(new MethodInvoker(() => UpdateComplete(Context, Result, ResultMessage)));
		}

		void UpdateComplete(WorkspaceUpdateContext Context, WorkspaceUpdateResult Result, string ResultMessage)
		{
			UpdateTimer.Stop();

			UpdateSyncConfig(Workspace.CurrentChangeNumber);

			if(Result == WorkspaceUpdateResult.Success && Context.Options.HasFlag(WorkspaceUpdateOptions.SyncSingleChange))
			{
				WorkspaceSettings.AdditionalChangeNumbers.Add(Context.ChangeNumber);
				Settings.Save();
			}

			if(Result == WorkspaceUpdateResult.Success && WorkspaceSettings.ExpandedArchiveTypes != null)
			{
				WorkspaceSettings.ExpandedArchiveTypes = WorkspaceSettings.ExpandedArchiveTypes.Except(Context.ArchiveTypeToDepotPath.Where(x => x.Value == null).Select(x => x.Key)).ToArray();
			}

			WorkspaceSettings.LastSyncChangeNumber = Context.ChangeNumber;
			WorkspaceSettings.LastSyncResult = Result;
			WorkspaceSettings.LastSyncResultMessage = ResultMessage;
			WorkspaceSettings.LastSyncTime = DateTime.UtcNow;
			WorkspaceSettings.LastSyncDurationSeconds = (int)(WorkspaceSettings.LastSyncTime.Value - Context.StartTime).TotalSeconds;
			WorkspaceSettings.LastBuiltChangeNumber = Workspace.LastBuiltChangeNumber;
			Settings.Save();
			
			if(Result == WorkspaceUpdateResult.FilesToResolve)
			{
				MessageBox.Show("You have files to resolve after syncing your workspace. Please check P4.");
			}
			else if(Result == WorkspaceUpdateResult.FilesToClobber)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.Paused, 0.0f);
				Owner.UpdateProgress();

				ClobberWindow Window = new ClobberWindow(Context.ClobberFiles);
				if(Window.ShowDialog(this) == DialogResult.OK)
				{
					StartWorkspaceUpdate(Context);
					return;
				}
			}
			else if(Result == WorkspaceUpdateResult.FailedToCompileWithCleanWorkspace)
			{
				EventMonitor.PostEvent(Context.ChangeNumber, EventType.DoesNotCompile);
			}
			else if(Result == WorkspaceUpdateResult.Success)
			{
				if(Context.Options.HasFlag(WorkspaceUpdateOptions.Build))
				{
					EventMonitor.PostEvent(Context.ChangeNumber, EventType.Compiles);
				}
				if(Context.Options.HasFlag(WorkspaceUpdateOptions.RunAfterSync))
				{
					LaunchEditor();
				}
				if(Context.Options.HasFlag(WorkspaceUpdateOptions.OpenSolutionAfterSync))
				{
					OpenSolution();
				}
			}

			DesiredTaskbarState = Tuple.Create((Result == WorkspaceUpdateResult.Success)? TaskbarState.NoProgress : TaskbarState.Error, 0.0f);
			Owner.UpdateProgress();

			BuildList.Invalidate();
			Refresh();
			UpdateStatusPanel();
			UpdateSyncActionCheckboxes();
		}

		void UpdateBuildListCallback()
		{
			Invoke(new MethodInvoker(UpdateBuildList));
		}

		void UpdateBuildList()
		{
			if(SelectedFileName != null)
			{
				int SelectedChange = (BuildList.SelectedItems.Count > 0)? ((PerforceChangeSummary)BuildList.SelectedItems[0].Tag).Number : -1;

				ChangeNumberToArchivePath.Clear();

				BuildList.BeginUpdate();

				foreach(ListViewGroup Group in BuildList.Groups)
				{
					Group.Name = "xxx " + Group.Name;
				}

				int RemoveItems = BuildList.Items.Count;
				int RemoveGroups = BuildList.Groups.Count;

				List<PerforceChangeSummary> Changes = PerforceMonitor.GetChanges();
				EventMonitor.FilterChanges(Changes.Select(x => x.Number));

				PromotedChangeNumbers = PerforceMonitor.GetPromotedChangeNumbers();

				string[] ExcludeChanges = new string[0];
				if(Workspace != null)
				{
					ConfigFile ProjectConfigFile = Workspace.ProjectConfigFile;
					if(ProjectConfigFile != null)
					{
						ExcludeChanges = Workspace.ProjectConfigFile.GetValues("Options.ExcludeChanges", ExcludeChanges);
					}
				}

				bool bFirstChange = true;
				bool bOnlyShowReviewed = OnlyShowReviewedCheckBox.Checked;

				NumChanges = Changes.Count;
				ListIndexToChangeIndex = new List<int>();
				SortedChangeNumbers = new List<int>();
				
				for(int ChangeIdx = 0; ChangeIdx < Changes.Count; ChangeIdx++)
				{
					PerforceChangeSummary Change = Changes[ChangeIdx];
					if(ShouldShowChange(Change, ExcludeChanges) || PromotedChangeNumbers.Contains(Change.Number))
					{
						SortedChangeNumbers.Add(Change.Number);

						if(!bOnlyShowReviewed || (!EventMonitor.IsUnderInvestigation(Change.Number) && (ShouldIncludeInReviewedList(Change.Number) || bFirstChange)))
						{
							bFirstChange = false;

							ListViewGroup Group;

							string GroupName = Change.Date.ToString("D");//"dddd\\,\\ h\\.mmtt");
							for(int Idx = 0;;Idx++)
							{
								if(Idx == BuildList.Groups.Count)
								{
									Group = new ListViewGroup(GroupName);
									Group.Name = GroupName;
									BuildList.Groups.Add(Group);
									break;
								}
								else if(BuildList.Groups[Idx].Name == GroupName)
								{
									Group = BuildList.Groups[Idx];
									break;
								}
							}

							DateTime DisplayTime = Change.Date;
							if(Settings.bShowLocalTimes)
							{
								DisplayTime = (DisplayTime - ServerTimeZone).ToLocalTime();
							}

							ListViewItem Item = new ListViewItem(Group);
							Item.Tag = Change;
							Item.Selected = (Change.Number == SelectedChange);
							Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, String.Format("{0}", Change.Number)));
							Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, DisplayTime.ToString("h\\.mmtt")));
							Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, FormatUserName(Change.User)));
							Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, Change.Description.Replace('\n', ' ')));
							Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, ""));
							Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, ""));

							// Insert it at the right position within the group
							int GroupInsertIdx = 0;
							while(GroupInsertIdx < Group.Items.Count && Change.Number < ((PerforceChangeSummary)Group.Items[GroupInsertIdx].Tag).Number)
							{
								GroupInsertIdx++;
							}
							Group.Items.Insert(GroupInsertIdx, Item);

							// Insert it at the right place in the list
							BuildList.Items.Add(Item);

							// Store off the list index for this change
							ListIndexToChangeIndex.Add(ChangeIdx);
						}
					}
				}

				SortedChangeNumbers.Sort();

				for(int Idx = 0; Idx < RemoveItems; Idx++)
				{
					BuildList.Items.RemoveAt(0);
				}
				for(int Idx = 0; Idx < RemoveGroups; Idx++)
				{
					BuildList.Groups.RemoveAt(0);
				}

				BuildList.EndUpdate();

				if(PendingSelectedChangeNumber != -1)
				{
					SelectChange(PendingSelectedChangeNumber);
				}
			}

			if(HoverItem > BuildList.Items.Count)
			{
				HoverItem = -1;
			}

			UpdateBuildFailureNotification();

			UpdateBuildSteps();
			UpdateSyncActionCheckboxes();
		}

		bool ShouldShowChange(PerforceChangeSummary Change, string[] ExcludeChanges)
		{
			foreach(string ExcludeChange in ExcludeChanges)
			{
				if(Regex.IsMatch(Change.Description, ExcludeChange, RegexOptions.IgnoreCase))
				{
					return false;
				}
			}

			if(String.Compare(Change.User, "buildmachine", true) == 0 && Change.Description.IndexOf("lightmaps", StringComparison.InvariantCultureIgnoreCase) == -1)
			{
				return false;
			}
			return true;
		}

		void UpdateBuildMetadataCallback()
		{
			Invoke(new MethodInvoker(UpdateBuildMetadata));
		}

		void UpdateBuildMetadata()
		{
			ChangeNumberToArchivePath.Clear();
			BuildList.Invalidate();
			UpdateStatusPanel();
			UpdateBuildFailureNotification();
		}

		bool ShouldIncludeInReviewedList(int ChangeNumber)
		{
			if(PromotedChangeNumbers.Contains(ChangeNumber))
			{
				return true;
			}

			EventSummary Review = EventMonitor.GetSummaryForChange(ChangeNumber);
			if(Review != null)
			{
				if(Review.LastStarReview != null && Review.LastStarReview.Type == EventType.Starred)
				{
					return true;
				}
				if(Review.Verdict == ReviewVerdict.Good || Review.Verdict == ReviewVerdict.Mixed)
				{
					return true;
				}
			}
			return false;
		}

		void UpdateReviewsCallback()
		{
			Invoke(new MethodInvoker(UpdateReviews));
		}

		void UpdateReviews()
		{
			ChangeNumberToArchivePath.Clear();
			EventMonitor.ApplyUpdates();
			Refresh();
			UpdateBuildFailureNotification();
		}

		void UpdateBuildFailureNotification()
		{
			int LastChangeByCurrentUser = PerforceMonitor.LastChangeByCurrentUser;
			int LastCodeChangeByCurrentUser = PerforceMonitor.LastCodeChangeByCurrentUser;

			// Find all the badges which should notify users due to content changes
			HashSet<string> ContentBadges = new HashSet<string>();
			if(Workspace != null && Workspace.ProjectConfigFile != null)
			{
				ContentBadges.UnionWith(Workspace.ProjectConfigFile.GetValues("Notifications.ContentBadges", new string[0]));
			}

			// Find the most recent build of each type, and the last time it succeeded
			Dictionary<string, BuildData> TypeToLastBuild = new Dictionary<string,BuildData>();
			Dictionary<string, BuildData> TypeToLastSucceededBuild = new Dictionary<string,BuildData>();
			for(int Idx = SortedChangeNumbers.Count - 1; Idx >= 0; Idx--)
			{
				EventSummary Summary = EventMonitor.GetSummaryForChange(SortedChangeNumbers[Idx]);
				if(Summary != null)
				{
					foreach(BuildData Build in Summary.Builds)
					{
						if(!TypeToLastBuild.ContainsKey(Build.BuildType) && (Build.Result == BuildDataResult.Success || Build.Result == BuildDataResult.Warning || Build.Result == BuildDataResult.Failure))
						{
							TypeToLastBuild.Add(Build.BuildType, Build);
						}
						if(!TypeToLastSucceededBuild.ContainsKey(Build.BuildType) && Build.Result == BuildDataResult.Success)
						{
							TypeToLastSucceededBuild.Add(Build.BuildType, Build);
						}
					}
				}
			}

			// Find all the build types that the user needs to be notified about.
			int RequireNotificationForChange = -1;
			List<BuildData> NotifyBuilds = new List<BuildData>();
			foreach(BuildData LastBuild in TypeToLastBuild.Values.OrderBy(x => x.BuildType))
			{
				if(LastBuild.Result == BuildDataResult.Failure || LastBuild.Result == BuildDataResult.Warning)
				{
					// Get the last submitted changelist by this user of the correct type
					int LastChangeByCurrentUserOfType;
					if(ContentBadges.Contains(LastBuild.BuildType))
					{
						LastChangeByCurrentUserOfType = LastChangeByCurrentUser;
					}
					else
					{
						LastChangeByCurrentUserOfType = LastCodeChangeByCurrentUser;
					}

					// Check if the failed build was after we submitted
					if(LastChangeByCurrentUserOfType > 0 && LastBuild.ChangeNumber >= LastChangeByCurrentUserOfType)
					{
						// And check that there wasn't a successful build after we submitted (if there was, we're in the clear)
						BuildData LastSuccessfulBuild;
						if(!TypeToLastSucceededBuild.TryGetValue(LastBuild.BuildType, out LastSuccessfulBuild) || LastSuccessfulBuild.ChangeNumber < LastChangeByCurrentUserOfType)
						{
							// Add it to the list of notifications
							NotifyBuilds.Add(LastBuild);

							// Check if this is a new notification, rather than one we've already dismissed
							int NotifiedChangeNumber;
							if(!NotifiedBuildTypeToChangeNumber.TryGetValue(LastBuild.BuildType, out NotifiedChangeNumber) || NotifiedChangeNumber < LastChangeByCurrentUserOfType)
							{
								RequireNotificationForChange = Math.Max(RequireNotificationForChange, LastChangeByCurrentUserOfType);
							}
						}
					}
				}
			}

			// If there's anything we haven't already notified the user about, do so now
			if(RequireNotificationForChange != -1)
			{
				// Format the platform list
				StringBuilder PlatformList = new StringBuilder(NotifyBuilds[0].BuildType);
				for(int Idx = 1; Idx < NotifyBuilds.Count - 1; Idx++)
				{
					PlatformList.AppendFormat(", {0}", NotifyBuilds[Idx].BuildType);
				}
				if(NotifyBuilds.Count > 1)
				{
					PlatformList.AppendFormat(" and {0}", NotifyBuilds[NotifyBuilds.Count - 1].BuildType);
				}

				// Show the balloon tooltip
				if(NotifyBuilds.Any(x => x.Result == BuildDataResult.Failure))
				{
					string Title = String.Format("{0} Errors", PlatformList.ToString());
					string Message = String.Format("CIS failed after your last submitted changelist ({0}).", RequireNotificationForChange);
					NotificationWindow.Show(NotificationType.Error, Title, Message);
				}
				else
				{
					string Title = String.Format("{0} Warnings", PlatformList.ToString());
					string Message = String.Format("CIS completed with warnings after your last submitted changelist ({0}).", RequireNotificationForChange);
					NotificationWindow.Show(NotificationType.Warning, Title, Message);
				}

				// Set the link to open the right build pages
				int HighlightChange = NotifyBuilds.Max(x => x.ChangeNumber);
				NotificationWindow.OnMoreInformation = () => { Owner.ShowAndActivate(); SelectChange(HighlightChange); };
				
				// Don't show messages for this change again
				foreach(BuildData NotifyBuild in NotifyBuilds)
				{
					NotifiedBuildTypeToChangeNumber[NotifyBuild.BuildType] = RequireNotificationForChange;
				}
			}
		}

		private void BuildList_DrawColumnHeader(object sender, DrawListViewColumnHeaderEventArgs e)
		{
			e.DrawDefault = true;
		}

		private void BuildList_DrawItem(object sender, DrawListViewItemEventArgs e)
		{
			if(Application.RenderWithVisualStyles)
			{
				if(e.State.HasFlag(ListViewItemStates.Selected))
				{
					SelectedItemRenderer.DrawBackground(e.Graphics, e.Bounds);
				}
				else if(e.ItemIndex == HoverItem)
				{
					TrackedItemRenderer.DrawBackground(e.Graphics, e.Bounds);
				}
				else if(((PerforceChangeSummary)e.Item.Tag).Number == Workspace.PendingChangeNumber)
				{
					TrackedItemRenderer.DrawBackground(e.Graphics, e.Bounds);
				}
			}
			else
			{
				if(e.State.HasFlag(ListViewItemStates.Selected))
				{
					e.Graphics.FillRectangle(SystemBrushes.ButtonFace, e.Bounds);
				}
				else
				{
					e.Graphics.FillRectangle(SystemBrushes.Window, e.Bounds);
				}
			}
		}

		private string GetArchivePathForChangeNumber(int ChangeNumber)
		{
			string ArchivePath;
			if(!ChangeNumberToArchivePath.TryGetValue(ChangeNumber, out ArchivePath))
			{
				PerforceChangeType Type;
				if(PerforceMonitor.TryGetChangeType(ChangeNumber, out Type))
				{
					// Try to get the archive for this CL
					if(!PerforceMonitor.TryGetArchivePathForChangeNumber(ChangeNumber, out ArchivePath) && Type == PerforceChangeType.Content)
					{
						// Otherwise if it's a content change, find the previous build any use the archive path from that
						int Index = SortedChangeNumbers.BinarySearch(ChangeNumber);
						if(Index > 0)
						{
							ArchivePath = GetArchivePathForChangeNumber(SortedChangeNumbers[Index - 1]);
						}
					}
				}
				ChangeNumberToArchivePath.Add(ChangeNumber, ArchivePath);
			}
			return ArchivePath;
		}

		private Color Blend(Color First, Color Second, float T)
		{
			return Color.FromArgb((int)(First.R + (Second.R - First.R) * T), (int)(First.G + (Second.G - First.G) * T), (int)(First.B + (Second.B - First.B) * T));
		}

		private bool CanSyncChange(int ChangeNumber)
		{
			return !ShouldSyncPrecompiledEditor || GetArchivePathForChangeNumber(ChangeNumber) != null;
		}

		private void BuildList_DrawSubItem(object sender, DrawListViewSubItemEventArgs e)
		{
			PerforceChangeSummary Change = (PerforceChangeSummary)e.Item.Tag;

			int IconY = e.Bounds.Top + (e.Bounds.Height - 16) / 2;

			StringFormat Format = new StringFormat();
			Format.LineAlignment = StringAlignment.Center;
			Format.FormatFlags = StringFormatFlags.NoWrap;
			Format.Trimming = StringTrimming.EllipsisCharacter;

			Font CurrentFont = (Change.Number == Workspace.PendingChangeNumber || Change.Number == Workspace.CurrentChangeNumber)? SelectedBuildFont : BuildFont;

			bool bAllowSync = CanSyncChange(Change.Number);
			Color TextColor = (bAllowSync || Change.Number == Workspace.PendingChangeNumber || Change.Number == Workspace.CurrentChangeNumber || (WorkspaceSettings != null && WorkspaceSettings.AdditionalChangeNumbers.Contains(Change.Number)))? SystemColors.WindowText : Blend(SystemColors.Window, SystemColors.WindowText, 0.25f);

			if(e.ColumnIndex == IconColumn.Index)
			{
				EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);

				int MinX = 4;
				if((Summary != null && EventMonitor.WasSyncedByCurrentUser(Summary.ChangeNumber)) || (Workspace != null && Workspace.CurrentChangeNumber == Change.Number))
				{
					e.Graphics.DrawImage(Properties.Resources.Icons, MinX, IconY, PreviousSyncIcon, GraphicsUnit.Pixel);
				}
				else if(WorkspaceSettings != null && WorkspaceSettings.AdditionalChangeNumbers.Contains(Change.Number))
				{
					e.Graphics.DrawImage(Properties.Resources.Icons, MinX, IconY, AdditionalSyncIcon, GraphicsUnit.Pixel);
				}
				else if(bAllowSync && ((Summary != null && Summary.LastStarReview != null && Summary.LastStarReview.Type == EventType.Starred) || PromotedChangeNumbers.Contains(Change.Number)))
				{
					e.Graphics.DrawImage(Properties.Resources.Icons, MinX, IconY, PromotedBuildIcon, GraphicsUnit.Pixel);
				}
				MinX += PromotedBuildIcon.Width;

				if(bAllowSync)
				{
					Rectangle QualityIcon = DefaultBuildIcon;
					if(EventMonitor.IsUnderInvestigation(Change.Number))
					{
						QualityIcon = BadBuildIcon;
					}
					else if(Summary != null)
					{
						if(Summary.Verdict == ReviewVerdict.Good)
						{
							QualityIcon = GoodBuildIcon;
						}
						else if(Summary.Verdict == ReviewVerdict.Bad)
						{
							QualityIcon = BadBuildIcon;
						}
						else if(Summary.Verdict == ReviewVerdict.Mixed)
						{
							QualityIcon = MixedBuildIcon;
						}
					}

					e.Graphics.DrawImage(Properties.Resources.Icons, MinX, IconY, QualityIcon, GraphicsUnit.Pixel);
					MinX += QualityIcon.Width;
				}
			}
			else if(e.ColumnIndex == ChangeColumn.Index || e.ColumnIndex == TimeColumn.Index || e.ColumnIndex == AuthorColumn.Index || e.ColumnIndex == DescriptionColumn.Index)
			{
				TextRenderer.DrawText(e.Graphics, e.SubItem.Text, CurrentFont, e.Bounds, TextColor, TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
			}
			else if(e.ColumnIndex == CISColumn.Index)
			{
				e.Graphics.IntersectClip(e.Bounds);
				e.Graphics.SmoothingMode = SmoothingMode.HighQuality;

				EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
				if(Summary == null || Summary.Builds.Count == 0)
				{
					PerforceChangeType Type;
					if(PerforceMonitor.TryGetChangeType(Change.Number, out Type))
					{
						if(Type == PerforceChangeType.Code)
						{
							DrawSingleBadge(e.Graphics, e.Bounds, "Code", Color.FromArgb(192, 192, 192));
						}
						else if(Type == PerforceChangeType.Content)
						{
							DrawSingleBadge(e.Graphics, e.Bounds, "Content", bAllowSync? Color.FromArgb(128, 128, 192) : Color.FromArgb(192, 192, 192));
						}
					}
				}
				else
				{
					Tuple<BuildData, Rectangle>[] Badges = LayoutBadges(Summary.Builds, e.Bounds);
					foreach(Tuple<BuildData, Rectangle> Badge in Badges)
					{
						Color BadgeColor = Color.FromArgb(128, 192, 64);
						if(Badge.Item1.Result == BuildDataResult.Starting)
						{
							BadgeColor = Color.FromArgb(128, 192, 255);
						}
						else if(Badge.Item1.Result == BuildDataResult.Warning)
						{
							BadgeColor = Color.FromArgb(255, 192, 0);
						}
						else if(Badge.Item1.Result == BuildDataResult.Failure)
						{
							BadgeColor = Color.FromArgb(192, 64, 0);
						}

						if(Badge.Item1 == HoverBadge)
						{
							BadgeColor = Color.FromArgb(Math.Min(BadgeColor.R + 32, 255), Math.Min(BadgeColor.G + 32, 255), Math.Min(BadgeColor.B + 32, 255));
						}

						Size BadgeSize = GetBadgeSize(Badge.Item1.BuildType);
						DrawBadge(e.Graphics, Badge.Item2, Badge.Item1.BuildType, BadgeColor);
					}
				}
			}
			else if(e.ColumnIndex == StatusColumn.Index)
			{
				int MaxX = e.SubItem.Bounds.Right;

				if(Change.Number == Workspace.PendingChangeNumber && Workspace.IsBusy())
				{
					Tuple<string, float> Progress = Workspace.CurrentProgress;

					MaxX -= CancelIcon.Width;
					e.Graphics.DrawImage(Properties.Resources.Icons, MaxX, IconY, CancelIcon, GraphicsUnit.Pixel);

					if(!Splitter.IsLogVisible())
					{
						MaxX -= InfoIcon.Width; 
						e.Graphics.DrawImage(Properties.Resources.Icons, MaxX, IconY, InfoIcon, GraphicsUnit.Pixel);
					}

					int MinX = e.Bounds.Left;

					TextRenderer.DrawText(e.Graphics, Progress.Item1, CurrentFont, new Rectangle(MinX, e.Bounds.Top, MaxX - MinX, e.Bounds.Height), TextColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
				}
				else
				{
					e.Graphics.IntersectClip(e.Bounds);
					e.Graphics.SmoothingMode = SmoothingMode.HighQuality;

					if(Change.Number == Workspace.CurrentChangeNumber)
					{
						EventData Review = EventMonitor.GetReviewByCurrentUser(Change.Number);

						MaxX -= FrownIcon.Width;
						e.Graphics.DrawImage(Properties.Resources.Icons, MaxX, IconY, (Review == null || !EventMonitor.IsPositiveReview(Review.Type))? FrownIcon : DisabledFrownIcon, GraphicsUnit.Pixel);

						MaxX -= HappyIcon.Width;
						e.Graphics.DrawImage(Properties.Resources.Icons, MaxX, IconY, (Review == null || !EventMonitor.IsNegativeReview(Review.Type))? HappyIcon : DisabledHappyIcon, GraphicsUnit.Pixel);
					}
					else if(e.ItemIndex == HoverItem && bAllowSync)
					{
						Rectangle SyncBadgeRectangle = GetSyncBadgeRectangle(e.SubItem.Bounds);
						DrawBadge(e.Graphics, SyncBadgeRectangle, "Sync", bHoverSync? Color.FromArgb(140, 180, 230) : Color.FromArgb(112, 146, 190));
						MaxX = SyncBadgeRectangle.Left - 2;
					}

					string SummaryText;
					if(WorkspaceSettings.LastSyncChangeNumber == -1 || WorkspaceSettings.LastSyncChangeNumber != Change.Number || !GetLastUpdateMessage(WorkspaceSettings.LastSyncResult, WorkspaceSettings.LastSyncResultMessage, out SummaryText))
					{
						StringBuilder SummaryTextBuilder = new StringBuilder();

						EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);

						AppendItemList(SummaryTextBuilder, " ", "Under investigation by {0}.", EventMonitor.GetInvestigatingUsers(Change.Number).Select(x => FormatUserName(x)));

						if(Summary != null)
						{
							string Comments = String.Join(", ", Summary.Comments.Where(x => !String.IsNullOrWhiteSpace(x.Text)).Select(x => String.Format("{0}: \"{1}\"", FormatUserName(x.UserName), x.Text)));
							if(Comments.Length > 0)
							{
								SummaryTextBuilder.Append(((SummaryTextBuilder.Length == 0)? ""  : " ") + Comments);
							}
							else
							{
								AppendItemList(SummaryTextBuilder, " ", "Used by {0}.", Summary.CurrentUsers.Select(x => FormatUserName(x)));
							}
						}

						SummaryText = (SummaryTextBuilder.Length == 0)? "No current users." : SummaryTextBuilder.ToString();
					}

					if(SummaryText != null && SummaryText.Contains('\n'))
					{
						SummaryText = SummaryText.Substring(0, SummaryText.IndexOf('\n')).TrimEnd() + "...";
					}

					TextRenderer.DrawText(e.Graphics, SummaryText, CurrentFont, new Rectangle(e.Bounds.Left, e.Bounds.Top, MaxX - e.Bounds.Left, e.Bounds.Height), TextColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
				}
			}
			else
			{
				throw new NotImplementedException();
			}
		}

		private Tuple<BuildData, Rectangle>[] LayoutBadges(IEnumerable<BuildData> Builds, Rectangle DisplayRectangle)
		{
			int NextX = 0;
			List<Tuple<BuildData, Rectangle>> Badges = new List<Tuple<BuildData,Rectangle>>();
			foreach(BuildData Build in Builds.OrderBy(x => x.BuildType))
			{
				Size BadgeSize = GetBadgeSize(Build.BuildType);
				Badges.Add(new Tuple<BuildData, Rectangle>(Build, new Rectangle(NextX, DisplayRectangle.Top + (DisplayRectangle.Height - BadgeSize.Height) / 2, BadgeSize.Width, BadgeSize.Height)));
				NextX += BadgeSize.Width + 2;
			}

			int Offset = (Badges.Count > 0)? DisplayRectangle.Left + Math.Max(DisplayRectangle.Width - Badges.Last().Item2.Right, 0) / 2 : 0;
			return Badges.Select(x => new Tuple<BuildData, Rectangle>(x.Item1, new Rectangle(x.Item2.Left + Offset, x.Item2.Top, x.Item2.Width, x.Item2.Height))).ToArray();
		}

		private Rectangle GetSyncBadgeRectangle(Rectangle Bounds)
		{
			Size BadgeSize = GetBadgeSize("Sync");
			return new Rectangle(Bounds.Right - BadgeSize.Width - 2, Bounds.Top + (Bounds.Height - BadgeSize.Height) / 2, BadgeSize.Width, BadgeSize.Height);
		}

		private void DrawSingleBadge(Graphics Graphics, Rectangle DisplayRectangle, string BadgeText, Color BadgeColor)
		{
			Size BadgeSize = GetBadgeSize(BadgeText);

			int X = DisplayRectangle.Left + (DisplayRectangle.Width - BadgeSize.Width) / 2;
			int Y = DisplayRectangle.Top + (DisplayRectangle.Height - BadgeSize.Height) / 2;

			DrawBadge(Graphics, new Rectangle(X, Y, BadgeSize.Width, BadgeSize.Height), BadgeText, BadgeColor);
		}

		private void DrawBadge(Graphics Graphics, Rectangle BadgeRect, string BadgeText, Color BadgeColor)
		{
			GraphicsPath Path = new GraphicsPath();
			Path.StartFigure();
			Path.AddArc((float)BadgeRect.Left, (float)BadgeRect.Top, BadgeRect.Height, BadgeRect.Height, 90, 180);
			Path.AddArc((float)BadgeRect.Right - BadgeRect.Height - 1, (float)BadgeRect.Top, BadgeRect.Height, BadgeRect.Height, 270, 180);
			Path.CloseFigure();

			using(SolidBrush Brush = new SolidBrush(BadgeColor))
			{
				Graphics.FillPath(Brush, Path);
			}

			TextRenderer.DrawText(Graphics, BadgeText, BadgeFont, BadgeRect, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix);
		}

		private Size GetBadgeSize(string BadgeText)
		{
			Size LabelSize = TextRenderer.MeasureText(BadgeText, BadgeFont);
			int BadgeHeight = BadgeFont.Height - 1;
			return new Size(LabelSize.Width + BadgeHeight - 4, BadgeHeight);
		}

		private static bool GetLastUpdateMessage(WorkspaceUpdateResult Result, string ResultMessage, out string Message)
		{
			if(Result != WorkspaceUpdateResult.Success && ResultMessage != null)
			{
				Message = ResultMessage;
				return true;
			}
			return GetGenericLastUpdateMessage(Result, out Message);
		}

		private static bool GetGenericLastUpdateMessage(WorkspaceUpdateResult Result, out string Message)
		{
			switch(Result)
			{
				case WorkspaceUpdateResult.Canceled:
					Message = "Sync canceled.";
					return true;
				case WorkspaceUpdateResult.FailedToSync:
					Message = "Failed to sync files.";
					return true;
				case WorkspaceUpdateResult.FilesToResolve:
					Message = "Sync finished with unresolved files.";
					return true;
				case WorkspaceUpdateResult.FilesToClobber:
					Message = "Sync failed due to modified files in workspace.";
					return true;
				case WorkspaceUpdateResult.FailedToCompile:
				case WorkspaceUpdateResult.FailedToCompileWithCleanWorkspace:
					Message = "Compile failed.";
					return true;
				default:
					Message = null;
					return false;
			}
		}

		private static string FormatUserName(string UserName)
		{
			StringBuilder NormalUserName = new StringBuilder();
			for(int Idx = 0; Idx < UserName.Length; Idx++)
			{
				if(Idx == 0 || UserName[Idx - 1] == '.')
				{
					NormalUserName.Append(Char.ToUpper(UserName[Idx]));
				}
				else if(UserName[Idx] == '.')
				{
					NormalUserName.Append(' ');
				}
				else
				{
					NormalUserName.Append(UserName[Idx]);
				}
			}
			return NormalUserName.ToString();
		}

		private static void AppendUserList(StringBuilder Builder, string Separator, string Format, IEnumerable<EventData> Reviews)
		{
			AppendItemList(Builder, Separator, Format, Reviews.Select(x => FormatUserName(x.UserName)));
		}

		private static void AppendItemList(StringBuilder Builder, string Separator, string Format, IEnumerable<string> Items)
		{
			string ItemList = FormatItemList(Format, Items);
			if(ItemList != null)
			{
				if(Builder.Length > 0)
				{
					Builder.Append(Separator);
				}
				Builder.Append(ItemList);
			}
		}

		private static string FormatItemList(string Format, IEnumerable<string> Items)
		{
			string[] ItemsArray = Items.Distinct().ToArray();
			if(ItemsArray.Length == 0)
			{
				return null;
			}

			StringBuilder Builder = new StringBuilder(ItemsArray[0]);
			if(ItemsArray.Length > 1)
			{
				for(int Idx = 1; Idx < ItemsArray.Length - 1; Idx++)
				{
					Builder.Append(", ");
					Builder.Append(ItemsArray[Idx]);
				}
				Builder.Append(" and ");
				Builder.Append(ItemsArray.Last());
			}
			return String.Format(Format, Builder.ToString());
		}

		private void BuildList_MouseDoubleClick(object Sender, MouseEventArgs Args)
		{
			if(Args.Button == MouseButtons.Left)
			{
				ListViewHitTestInfo HitTest = BuildList.HitTest(Args.Location);
				if(HitTest.Item != null)
				{
					PerforceChangeSummary Change = (PerforceChangeSummary)HitTest.Item.Tag;
					if(Change.Number == Workspace.CurrentChangeNumber)
					{
						LaunchEditor();
					}
					else
					{
						StartSync(Change.Number);
					}
				}
			}
		}

		public void LaunchEditor()
		{
			if(!Workspace.IsBusy() && Workspace.CurrentChangeNumber != -1)
			{
				BuildConfig EditorBuildConfig = GetEditorBuildConfig();

				List<string> ReceiptPaths = GetEditorReceiptPaths(EditorBuildConfig);

				string EditorExe = GetEditorExePath(EditorBuildConfig);
				if(ReceiptPaths.Any(x => File.Exists(x)) && File.Exists(EditorExe))
				{
					StringBuilder LaunchArguments = new StringBuilder();
					if(SelectedFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
					{
						LaunchArguments.AppendFormat("\"{0}\"", SelectedFileName);
					}
					foreach(Tuple<string, bool> EditorArgument in Settings.EditorArguments)
					{
						if(EditorArgument.Item2)
						{
							LaunchArguments.AppendFormat(" {0}", EditorArgument.Item1);
						}
					}
					if(EditorBuildConfig == BuildConfig.Debug || EditorBuildConfig == BuildConfig.DebugGame)
					{
						LaunchArguments.Append(" -debug");
					}
					
					if(!Utility.SpawnProcess(EditorExe, LaunchArguments.ToString()))
					{
						ShowErrorDialog("Unable to spawn {0} {1}", EditorExe, LaunchArguments.ToString());
					}
				}
				else
				{
					if(MessageBox.Show("The editor needs to be built before you can run it. Build it now?", "Editor out of date", MessageBoxButtons.YesNo) == System.Windows.Forms.DialogResult.Yes)
					{
						Owner.ShowAndActivate();

						WorkspaceUpdateOptions Options = WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.RunAfterSync;
						if(Settings.bUseIncrementalBuilds)
						{
							Options |= WorkspaceUpdateOptions.UseIncrementalBuilds;
						}
		
						WorkspaceUpdateContext Context = new WorkspaceUpdateContext(Workspace.CurrentChangeNumber, Options, null, GetDefaultBuildStepObjects(), ProjectSettings.BuildSteps, null, GetWorkspaceVariables());
						StartWorkspaceUpdate(Context);
					}
				}
			}
		}

		private string GetEditorExePath(BuildConfig Config)
		{
			string ExeFileName = "UE4Editor.exe";
			if(Config != BuildConfig.DebugGame && Config != BuildConfig.Development)
			{
				ExeFileName	= String.Format("UE4Editor-Win64-{0}.exe", Config.ToString());
			}
			return Path.Combine(BranchDirectoryName, "Engine", "Binaries", "Win64", ExeFileName);
		}

		private bool IsEditorRunning(BuildConfig Config)
		{
			string EditorFileName = Path.GetFullPath(GetEditorExePath(GetEditorBuildConfig()));
			try
			{
				foreach(Process ProcessInstance in Process.GetProcessesByName(Path.GetFileNameWithoutExtension(EditorFileName)))
				{
					try
					{
						string ProcessFileName = Path.GetFullPath(Path.GetFullPath(ProcessInstance.MainModule.FileName));
						if(String.Compare(EditorFileName, ProcessFileName, StringComparison.InvariantCultureIgnoreCase) == 0)
						{
							return true;
						}
					}
					catch
					{
					}
				}
			}
			catch
			{
			}
			return false;
		}

		private List<string> GetEditorReceiptPaths(BuildConfig Config)
		{
			string ConfigSuffix = (Config == BuildConfig.Development)? "" : String.Format("-Win64-{0}", Config.ToString());

			List<string> PossiblePaths = new List<string>();
			if(EditorTargetName == null)
			{
				PossiblePaths.Add(Path.Combine(BranchDirectoryName, "Engine", "Binaries", "Win64", String.Format("UE4Editor{0}.target", ConfigSuffix)));
				PossiblePaths.Add(Path.Combine(BranchDirectoryName, "Engine", "Build", "Receipts", String.Format("UE4Editor-Win64-{0}.target.xml", Config.ToString())));
			}
			else
			{
				PossiblePaths.Add(Path.Combine(Path.GetDirectoryName(SelectedFileName), "Binaries", "Win64", String.Format("{0}{1}.target", EditorTargetName, ConfigSuffix)));
				PossiblePaths.Add(Path.Combine(Path.GetDirectoryName(SelectedFileName), "Build", "Receipts", String.Format("{0}-Win64-{1}.target.xml", EditorTargetName, Config.ToString())));
			}
			return PossiblePaths;
		}

		private void TimerCallback(object Sender, EventArgs Args)
		{
			Tuple<string, float> Progress = Workspace.CurrentProgress;
			if(Progress != null && Progress.Item2 > 0.0f)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.Normal, Progress.Item2);
			}
			else
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
			}

			Owner.UpdateProgress();

			UpdateStatusPanel();
			BuildList.Refresh();
		}

		private void OpenPerforce()
		{
			StringBuilder CommandLine = new StringBuilder();
			if(Workspace != null && Workspace.Perforce != null)
			{
				CommandLine.AppendFormat("-p \"{0}\" -c \"{1}\" -u \"{2}\"", Workspace.Perforce.ServerAndPort ?? "perforce:1666", Workspace.Perforce.ClientName, Workspace.Perforce.UserName);
			}
			Process.Start("p4v.exe", CommandLine.ToString());
		}

		private void UpdateStatusPanel()
		{
			int NewContentWidth = Math.Max(TextRenderer.MeasureText(String.Format("Last synced to {0} at changelist 123456789. 12 users are on a newer build.        | Sync Now | Sync To...", StreamName ?? ""), StatusPanel.Font).Width, 400);
			StatusPanel.SetContentWidth(NewContentWidth);

			List<StatusLine> Lines = new List<StatusLine>();
			if(Workspace.IsBusy())
			{
				// Sync in progress
				Tuple<string, float> Progress = Workspace.CurrentProgress;

				StatusLine SummaryLine = new StatusLine();
				if(Workspace.PendingChangeNumber == -1)
				{
					SummaryLine.AddText("Working... | ");
				}
				else
				{
					SummaryLine.AddText("Updating to changelist ");
					SummaryLine.AddLink(Workspace.PendingChangeNumber.ToString(), FontStyle.Regular, () => { SelectChange(Workspace.PendingChangeNumber); });
					SummaryLine.AddText("... | ");
				}
				SummaryLine.AddLink(Splitter.IsLogVisible()? "Hide Log" : "Show Log", FontStyle.Bold | FontStyle.Underline, () => { ToggleLogVisibility(); });
				SummaryLine.AddText(" | ");
				SummaryLine.AddLink("Cancel", FontStyle.Bold | FontStyle.Underline, () => { CancelWorkspaceUpdate(); });
				Lines.Add(SummaryLine);

				StatusLine ProgressLine = new StatusLine();
				ProgressLine.AddText(String.Format("{0}  ", Progress.Item1));
				if(Progress.Item2 > 0.0f) ProgressLine.AddProgressBar(Progress.Item2);
				Lines.Add(ProgressLine);
			}
			else
			{
				// Project
				StatusLine ProjectLine = new StatusLine();
				ProjectLine.AddText(String.Format("Opened "));
				ProjectLine.AddLink(SelectedFileName + " \u25BE", FontStyle.Regular, (P, R) => { SelectRecentProject(R); });
				ProjectLine.AddText("  |  ");
				ProjectLine.AddLink("Browse...", FontStyle.Regular, (P, R) => { Owner.BrowseForProject(this); });
				Lines.Add(ProjectLine);

				// Spacer
				Lines.Add(new StatusLine(){ LineHeight = 0.5f });

				// Sync status
				StatusLine SummaryLine = new StatusLine();
				if(Workspace.CurrentChangeNumber != -1)
				{
					if(StreamName == null)
					{
						SummaryLine.AddText("Last synced to changelist ");
					}
					else
					{
						SummaryLine.AddText("Last synced to ");
						SummaryLine.AddLink(StreamName + "\u25BE", FontStyle.Regular, (P, R) => { SelectOtherStream(R); });
						SummaryLine.AddText(" at changelist ");
					}
					SummaryLine.AddLink(String.Format("{0}.", Workspace.CurrentChangeNumber), FontStyle.Regular, () => { SelectChange(Workspace.CurrentChangeNumber); });
					int NumUsersOnNewerChange = SortedChangeNumbers.Where(x => (x > Workspace.CurrentChangeNumber)).OrderByDescending(x => x).Select(x => EventMonitor.GetSummaryForChange(x)).Where(x => x != null && (!ShouldSyncPrecompiledEditor || GetArchivePathForChangeNumber(x.ChangeNumber) != null)).Sum(x => x.CurrentUsers.Count);
					if(NumUsersOnNewerChange > 0)
					{
						SummaryLine.AddText((NumUsersOnNewerChange == 1)? " 1 user is on a newer build." : String.Format(" {0} users are on a newer build.", NumUsersOnNewerChange));
					}
				}
				else
				{
					SummaryLine.AddText("You are not currently synced to ");
					if(StreamName == null)
					{
						SummaryLine.AddText("this branch.");
					}
					else
					{
						SummaryLine.AddLink(StreamName + " \u25BE", FontStyle.Regular, (P, R) => { SelectOtherStream(R); });
					}
				}
				SummaryLine.AddText("  |  ");
				SummaryLine.AddLink("Sync Now", FontStyle.Bold | FontStyle.Underline, () => { SyncLatestChange(); });
				SummaryLine.AddText(" - ");
				SummaryLine.AddLink(" To... \u25BE", 0, (P, R) => { ShowSyncMenu(R); });
				Lines.Add(SummaryLine);

				// Programs
				StatusLine ProgramsLine = new StatusLine();
				if(Workspace.CurrentChangeNumber != -1)
				{
					ProgramsLine.AddLink("Unreal Editor", FontStyle.Regular, () => { LaunchEditor(); });
					ProgramsLine.AddText("  |  ");
				}
				ProgramsLine.AddLink("Perforce", FontStyle.Regular, () => { OpenPerforce(); });
				ProgramsLine.AddText("  |  ");
				ProgramsLine.AddLink("Visual Studio", FontStyle.Regular, () => { OpenSolution(); });
				ProgramsLine.AddText("  |  ");
				ProgramsLine.AddLink("Windows Explorer", FontStyle.Regular, () => { Process.Start("explorer.exe", String.Format("\"{0}\"", Path.GetDirectoryName(SelectedFileName))); });
				ProgramsLine.AddText("  |  ");
				ProgramsLine.AddLink("More... \u25BE", FontStyle.Regular, (P, R) => { ShowActionsMenu(R); });
				Lines.Add(ProgramsLine);

				// Get the summary of the last sync
				if(WorkspaceSettings.LastSyncChangeNumber > 0)
				{
					string SummaryText;
					if(WorkspaceSettings.LastSyncChangeNumber == Workspace.CurrentChangeNumber && WorkspaceSettings.LastSyncResult == WorkspaceUpdateResult.Success && WorkspaceSettings.LastSyncTime.HasValue)
					{
						Lines.Add(new StatusLine(){ LineHeight = 0.5f });

						StatusLine SuccessLine = new StatusLine();
						SuccessLine.AddIcon(Properties.Resources.StatusIcons, new Size(16, 16), 0);
						SuccessLine.AddText(String.Format("  Sync took {0}{1}s, completed at {2}.", (WorkspaceSettings.LastSyncDurationSeconds >= 60)? String.Format("{0}m ", WorkspaceSettings.LastSyncDurationSeconds / 60) : "", WorkspaceSettings.LastSyncDurationSeconds % 60, WorkspaceSettings.LastSyncTime.Value.ToLocalTime().ToString("h\\:mmtt").ToLowerInvariant()));
						Lines.Add(SuccessLine);
					}
					else if(GetLastUpdateMessage(WorkspaceSettings.LastSyncResult, WorkspaceSettings.LastSyncResultMessage, out SummaryText))
					{
						Lines.Add(new StatusLine(){ LineHeight = 0.5f });

						int SummaryTextLength = SummaryText.IndexOf('\n');
						if(SummaryTextLength == -1)
						{
							SummaryTextLength = SummaryText.Length;
						}
						SummaryTextLength = Math.Min(SummaryTextLength, 80);

						StatusLine FailLine = new StatusLine();
						FailLine.AddIcon(Properties.Resources.StatusIcons, new Size(16, 16), 1);

						if(SummaryTextLength == SummaryText.Length)
						{
							FailLine.AddText(String.Format("  {0}  ", SummaryText));
						}
						else 
						{
							FailLine.AddText(String.Format("  {0}...  ", SummaryText.Substring(0, SummaryTextLength).TrimEnd()));
							FailLine.AddLink("More...", FontStyle.Bold | FontStyle.Underline, () => { ViewLastSyncStatus(); });
							FailLine.AddText("  |  ");
						}
						FailLine.AddLink("Show Log", FontStyle.Bold | FontStyle.Underline, () => { ShowErrorInLog(); });
						Lines.Add(FailLine);
					}
				}
			}
			StatusPanel.Set(Lines);
		}

		private void SelectOtherStream(Rectangle Bounds)
		{
			if(StreamName != null)
			{
				IReadOnlyList<string> OtherStreamNames = PerforceMonitor.OtherStreamNames;
				IReadOnlyList<string> OtherStreamFilter = Workspace.ProjectStreamFilter;

				StreamContextMenu.Items.Clear();

				if(OtherStreamFilter != null && !Settings.bShowAllStreams)
				{
					OtherStreamNames = OtherStreamNames.Where(x => OtherStreamFilter.Any(y => y.Equals(x, StringComparison.InvariantCultureIgnoreCase)) || x.Equals(StreamName, StringComparison.InvariantCultureIgnoreCase)).ToList().AsReadOnly();
				}

				foreach(string OtherStreamName in OtherStreamNames.OrderBy(x => x).Where(x => !x.EndsWith("/Dev-Binaries")))
				{
					string ThisStreamName = OtherStreamName; // Local for lambda capture

					ToolStripMenuItem Item = new ToolStripMenuItem(ThisStreamName, null, new EventHandler((S, E) => SelectStream(ThisStreamName)));
					if(String.Compare(StreamName, OtherStreamName, StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						Item.Checked = true;
					}
					StreamContextMenu.Items.Add(Item);
				}

				if(OtherStreamFilter != null)
				{
					StreamContextMenu.Items.Add(new ToolStripSeparator());
					if(Settings.bShowAllStreams)
					{
						StreamContextMenu.Items.Add(new ToolStripMenuItem("Show Filtered Stream List...", null, new EventHandler((S, E) => SetShowAllStreamsOption(false, Bounds))));
					}
					else
					{
						StreamContextMenu.Items.Add(new ToolStripMenuItem("Show All Streams...", null, new EventHandler((S, E) => SetShowAllStreamsOption(true, Bounds))));
					}
				}

				StreamContextMenu.Show(StatusPanel, new Point(Bounds.Left, Bounds.Bottom), ToolStripDropDownDirection.BelowRight);
			}
		}

		private void SetShowAllStreamsOption(bool bValue, Rectangle Bounds)
		{
			Settings.bShowAllStreams = bValue;
			Settings.Save();

			// Showing the new context menu before the current one has closed seems to fail to display it if the new context menu is shorter (ie. the current cursor position is out of bounds).
			// BeginInvoke() it so it won't happen until the menu has closed.
			BeginInvoke(new MethodInvoker(() => SelectOtherStream(Bounds)));
		}

		private void SelectStream(string StreamName)
		{
			if(Workspace.IsBusy())
			{
				MessageBox.Show("Please retry after the current sync has finished.", "Sync in Progress");
			}
			else if(Workspace.Perforce != null)
			{
				if(!Workspace.Perforce.HasOpenFiles(Log) || MessageBox.Show("You have files open for edit in this workspace. If you continue, you will not be able to submit them until you switch back.\n\nContinue switching workspaces?", "Files checked out", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					if(Workspace.Perforce.SwitchStream(StreamName, Log))
					{
						StatusPanel.SuspendLayout();
						StreamChangedCallback();
						StatusPanel.ResumeLayout();
					}
				}
			}
		}

		private void ViewLastSyncStatus()
		{
			string SummaryText;
			if(GetLastUpdateMessage(WorkspaceSettings.LastSyncResult, WorkspaceSettings.LastSyncResultMessage, out SummaryText))
			{
				string CaptionText;
				if(!GetGenericLastUpdateMessage(WorkspaceSettings.LastSyncResult, out CaptionText))
				{
					CaptionText = "Sync error";
				}
				MessageBox.Show(SummaryText, CaptionText);
			}
		}

		private void ShowErrorInLog()
		{
			if(!Splitter.IsLogVisible())
			{
				ToggleLogVisibility();
			}
			SyncLog.ScrollToEnd();
		}

		private void ShowActionsMenu(Rectangle Bounds)
		{
			MoreToolsContextMenu.Show(StatusPanel, new Point(Bounds.Left, Bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void SelectChange(int ChangeNumber)
		{
			PendingSelectedChangeNumber = -1;

			foreach(ListViewItem Item in BuildList.Items)
			{
				PerforceChangeSummary Summary = (PerforceChangeSummary)Item.Tag;
				if(Summary != null && Summary.Number <= ChangeNumber)
				{
					Item.Selected = true;
					Item.EnsureVisible();
					return;
				}
			}

			PendingSelectedChangeNumber = ChangeNumber;

			if(BuildList.Items.Count > 0)
			{
				BuildList.Items[BuildList.Items.Count - 1].EnsureVisible();
			}

			UpdateNumRequestedBuilds(false);
		}

		private void BuildList_MouseClick(object Sender, MouseEventArgs Args)
		{
			if(Args.Button == MouseButtons.Left)
			{
				ListViewHitTestInfo HitTest = BuildList.HitTest(Args.Location);
				if(HitTest.Item != null)
				{
					PerforceChangeSummary Change = (PerforceChangeSummary)HitTest.Item.Tag;
					if(Workspace.PendingChangeNumber == Change.Number)
					{
						Rectangle SubItemRect = HitTest.Item.SubItems[StatusColumn.Index].Bounds;

						if(Workspace.IsBusy())
						{
							Rectangle CancelRect = new Rectangle(SubItemRect.Right - 16, SubItemRect.Top, 16, SubItemRect.Height);
							Rectangle InfoRect = new Rectangle(SubItemRect.Right - 32, SubItemRect.Top, 16, SubItemRect.Height);
							if(CancelRect.Contains(Args.Location))
							{
								CancelWorkspaceUpdate();
							}
							else if(InfoRect.Contains(Args.Location) && !Splitter.IsLogVisible())
							{
								ToggleLogVisibility();
							}
						}
						else
						{
							Rectangle HappyRect = new Rectangle(SubItemRect.Right - 32, SubItemRect.Top, 16, SubItemRect.Height);
							Rectangle FrownRect = new Rectangle(SubItemRect.Right - 16, SubItemRect.Top, 16, SubItemRect.Height);
							if(HappyRect.Contains(Args.Location))
							{
								EventMonitor.PostEvent(Change.Number, EventType.Good);
								BuildList.Invalidate();
							}
							else if(FrownRect.Contains(Args.Location))
							{
								EventMonitor.PostEvent(Change.Number, EventType.Bad);
								BuildList.Invalidate();
							}
						}
					}
					else
					{
						Rectangle SyncBadgeRectangle = GetSyncBadgeRectangle(HitTest.Item.SubItems[StatusColumn.Index].Bounds);
						if(SyncBadgeRectangle.Contains(Args.Location) && CanSyncChange(Change.Number))
						{
							StartSync(Change.Number);
						}
					}

					if(CISColumn.Index < HitTest.Item.SubItems.Count && HitTest.Item.SubItems[CISColumn.Index] == HitTest.SubItem)
					{
						EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
						if(Summary != null)
						{
							Tuple<BuildData, Rectangle>[] Badges = LayoutBadges(Summary.Builds, HitTest.SubItem.Bounds);
							for(int Idx = 0; Idx < Badges.Length; Idx++)
							{
								if(Badges[Idx].Item2.Contains(Args.Location))
								{
									Process.Start(Badges[Idx].Item1.Url);
									break;
								}
							}
						}
					}
				}
			}
			else if(Args.Button == MouseButtons.Right)
			{
				ListViewHitTestInfo HitTest = BuildList.HitTest(Args.Location);
				if(HitTest.Item != null && HitTest.Item.Tag != null)
				{
					ContextMenuChange = (PerforceChangeSummary)HitTest.Item.Tag;

					BuildListContextMenu_WithdrawReview.Visible = (EventMonitor.GetReviewByCurrentUser(ContextMenuChange.Number) != null);
					BuildListContextMenu_StartInvestigating.Visible = !EventMonitor.IsUnderInvestigationByCurrentUser(ContextMenuChange.Number);
					BuildListContextMenu_FinishInvestigating.Visible = EventMonitor.IsUnderInvestigation(ContextMenuChange.Number);

					string CommentText;
					bool bHasExistingComment = EventMonitor.GetCommentByCurrentUser(ContextMenuChange.Number, out CommentText);
					BuildListContextMenu_LeaveComment.Visible = !bHasExistingComment;
					BuildListContextMenu_EditComment.Visible = bHasExistingComment;

					bool bIsBusy = Workspace.IsBusy();
					bool bIsCurrentChange = (ContextMenuChange.Number == Workspace.CurrentChangeNumber);
					BuildListContextMenu_Sync.Visible = !bIsBusy;
					BuildListContextMenu_Sync.Font = new Font(SystemFonts.MenuFont, bIsCurrentChange? FontStyle.Regular : FontStyle.Bold);
					BuildListContextMenu_SyncContentOnly.Visible = !bIsBusy && ShouldSyncPrecompiledEditor;
					BuildListContextMenu_SyncOnlyThisChange.Visible = !bIsBusy && !bIsCurrentChange && ContextMenuChange.Number > Workspace.CurrentChangeNumber && Workspace.CurrentChangeNumber != -1;
					BuildListContextMenu_Build.Visible = !bIsBusy && bIsCurrentChange && !ShouldSyncPrecompiledEditor && Settings.bUseIncrementalBuilds;
					BuildListContextMenu_Rebuild.Visible = !bIsBusy && bIsCurrentChange && !ShouldSyncPrecompiledEditor;
					BuildListContextMenu_GenerateProjectFiles.Visible = !bIsBusy && bIsCurrentChange;
					BuildListContextMenu_LaunchEditor.Visible = !bIsBusy && ContextMenuChange.Number == Workspace.CurrentChangeNumber;
					BuildListContextMenu_LaunchEditor.Font = new Font(SystemFonts.MenuFont, FontStyle.Bold);
					BuildListContextMenu_OpenVisualStudio.Visible = !bIsBusy && bIsCurrentChange;
					BuildListContextMenu_Cancel.Visible = bIsBusy;

					EventSummary Summary = EventMonitor.GetSummaryForChange(ContextMenuChange.Number);
					bool bStarred = (Summary != null && Summary.LastStarReview != null && Summary.LastStarReview.Type == EventType.Starred);
					BuildListContextMenu_AddStar.Visible = !bStarred;
					BuildListContextMenu_RemoveStar.Visible = bStarred;

					bool bIsTimeColumn = (HitTest.Item.SubItems.IndexOf(HitTest.SubItem) == TimeColumn.Index);
					BuildListContextMenu_TimeZoneSeparator.Visible = bIsTimeColumn;
					BuildListContextMenu_ShowLocalTimes.Visible = bIsTimeColumn;
					BuildListContextMenu_ShowLocalTimes.Checked = Settings.bShowLocalTimes;
					BuildListContextMenu_ShowServerTimes.Visible = bIsTimeColumn;
					BuildListContextMenu_ShowServerTimes.Checked = !Settings.bShowLocalTimes;

					int CustomToolStart = BuildListContextMenu.Items.IndexOf(BuildListContextMenu_CustomTool_Start) + 1;
					int CustomToolEnd = BuildListContextMenu.Items.IndexOf(BuildListContextMenu_CustomTool_End);
					while(CustomToolEnd > CustomToolStart)
					{
						BuildListContextMenu.Items.RemoveAt(CustomToolEnd - 1);
						CustomToolEnd--;
					}

					ConfigFile ProjectConfigFile = Workspace.ProjectConfigFile;
					if(ProjectConfigFile != null)
					{
						Dictionary<string, string> Variables = GetWorkspaceVariables();
						Variables.Add("Change", String.Format("{0}", ContextMenuChange.Number));

						string[] ChangeContextMenuEntries = ProjectConfigFile.GetValues("Options.ContextMenu", new string[0]);
						foreach(string ChangeContextMenuEntry in ChangeContextMenuEntries)
						{
							ConfigObject Object = new ConfigObject(ChangeContextMenuEntry);

							string Label = Object.GetValue("Label");
							string Execute = Object.GetValue("Execute");
							string Arguments = Object.GetValue("Arguments");

							if(Label != null && Execute != null)
							{
								Label = Utility.ExpandVariables(Label, Variables);
								Execute = Utility.ExpandVariables(Execute, Variables);
								Arguments = Utility.ExpandVariables(Arguments ?? "", Variables);

								ToolStripMenuItem Item = new ToolStripMenuItem(Label, null, new EventHandler((o, a) => Process.Start(Execute, Arguments)));
	
								BuildListContextMenu.Items.Insert(CustomToolEnd, Item);
								CustomToolEnd++;
							}
						}
					}

					BuildListContextMenu_CustomTool_End.Visible = (CustomToolEnd > CustomToolStart);

					BuildListContextMenu.Show(BuildList, Args.Location);
				}
			}
		}

		private void BuildList_MouseMove(object sender, MouseEventArgs e)
		{
			ListViewHitTestInfo HitTest = BuildList.HitTest(e.Location);
			int HitTestIndex = (HitTest.Item == null)? -1 : HitTest.Item.Index;
			if(HitTestIndex != HoverItem)
			{
				if(HoverItem != -1)
				{
					BuildList.RedrawItems(HoverItem, HoverItem, true);
				}
				HoverItem = HitTestIndex;
				if(HoverItem != -1)
				{
					BuildList.RedrawItems(HoverItem, HoverItem, true);
				}
			}

			BuildData NewHoverBadge = null;
			if(HitTest.Item != null && HitTest.Item.SubItems.IndexOf(HitTest.SubItem) == CISColumn.Index)
			{
				EventSummary Summary = EventMonitor.GetSummaryForChange(((PerforceChangeSummary)HitTest.Item.Tag).Number);
				if(Summary != null)
				{
					foreach(Tuple<BuildData, Rectangle> Badge in LayoutBadges(Summary.Builds, HitTest.SubItem.Bounds))
					{
						if(Badge.Item2.Contains(e.Location))
						{
							NewHoverBadge = Badge.Item1;
							break;
						}
					}
				}
			}
			if(NewHoverBadge != HoverBadge)
			{
				HoverBadge = NewHoverBadge;
				BuildList.Invalidate();
			}

			bool bNewHoverSync = false;
			if(HitTest.Item != null)
			{
				bNewHoverSync = GetSyncBadgeRectangle(HitTest.Item.SubItems[StatusColumn.Index].Bounds).Contains(e.Location);
			}
			if(bNewHoverSync != bHoverSync)
			{
				bHoverSync = bNewHoverSync;
				BuildList.Invalidate();
			}
		}

		private void OptionsButton_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_AutoResolveConflicts.Checked = Settings.bAutoResolveConflicts;
			OptionsContextMenu_SyncPrecompiledEditor.Visible = PerforceMonitor != null && PerforceMonitor.HasZippedBinaries;
			OptionsContextMenu_SyncPrecompiledEditor.Checked = Settings.bSyncPrecompiledEditor;
			OptionsContextMenu_EditorBuildConfiguration.Enabled = !ShouldSyncPrecompiledEditor;
			UpdateCheckedBuildConfig();
			OptionsContextMenu_UseIncrementalBuilds.Enabled = !ShouldSyncPrecompiledEditor;
			OptionsContextMenu_UseIncrementalBuilds.Checked = Settings.bUseIncrementalBuilds;
			OptionsContextMenu_CustomizeBuildSteps.Enabled = (Workspace != null);
			OptionsContextMenu_EditorArguments.Checked = Settings.EditorArguments.Any(x => x.Item2);
			OptionsContextMenu_ScheduledSync.Checked = Settings.bScheduleEnabled;
			OptionsContextMenu_TimeZone_Local.Checked = Settings.bShowLocalTimes;
			OptionsContextMenu_TimeZone_PerforceServer.Checked = !Settings.bShowLocalTimes;
			OptionsContextMenu_AutomaticallyRunAtStartup.Checked = IsAutomaticallyRunAtStartup();
			OptionsContextMenu_KeepInTray.Checked = Settings.bKeepInTray;
			OptionsContextMenu_TabNames_Stream.Checked = Settings.TabLabels == TabLabels.Stream;
			OptionsContextMenu_TabNames_WorkspaceName.Checked = Settings.TabLabels == TabLabels.WorkspaceName;
			OptionsContextMenu_TabNames_WorkspaceRoot.Checked = Settings.TabLabels == TabLabels.WorkspaceRoot;
			OptionsContextMenu_TabNames_ProjectFile.Checked = Settings.TabLabels == TabLabels.ProjectFile;
			OptionsContextMenu.Show(OptionsButton, new Point(OptionsButton.Width - OptionsContextMenu.Size.Width, OptionsButton.Height));
		}

		private void BuildAfterSyncCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			Settings.bBuildAfterSync = BuildAfterSyncCheckBox.Checked;
			Settings.Save();

			UpdateSyncActionCheckboxes();
		}

		private void RunAfterSyncCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			Settings.bRunAfterSync = RunAfterSyncCheckBox.Checked;
			Settings.Save();

			UpdateSyncActionCheckboxes();
		}

		private void OpenSolutionAfterSyncCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			Settings.bOpenSolutionAfterSync = OpenSolutionAfterSyncCheckBox.Checked;
			Settings.Save();

			UpdateSyncActionCheckboxes();
		}

		private void UpdateSyncActionCheckboxes()
		{
			BuildAfterSyncCheckBox.Checked = Settings.bBuildAfterSync;
			RunAfterSyncCheckBox.Checked = Settings.bRunAfterSync;
			OpenSolutionAfterSyncCheckBox.Checked = Settings.bOpenSolutionAfterSync;

			if(Workspace == null || Workspace.IsBusy())
			{
				AfterSyncingLabel.Enabled = false;
				BuildAfterSyncCheckBox.Enabled = false;
				RunAfterSyncCheckBox.Enabled = false;
				OpenSolutionAfterSyncCheckBox.Enabled = false;
			}
			else
			{
				AfterSyncingLabel.Enabled = true;
				BuildAfterSyncCheckBox.Enabled = bHasBuildSteps;
				RunAfterSyncCheckBox.Enabled = BuildAfterSyncCheckBox.Checked;
				OpenSolutionAfterSyncCheckBox.Enabled = true;
			}
		}

		private void BuildList_FontChanged(object sender, EventArgs e)
		{
			if(BuildFont != null)
			{
				BuildFont.Dispose();
			}
			BuildFont = BuildList.Font;

			if(SelectedBuildFont != null)
			{
				SelectedBuildFont.Dispose();
			}
			SelectedBuildFont = new Font(BuildFont, FontStyle.Bold);

			if(BadgeFont != null)
			{
				BadgeFont.Dispose();
			}
			BadgeFont = new Font(BuildFont.FontFamily, BuildFont.SizeInPoints - 2, FontStyle.Bold);
		}

		public void SyncLatestChange()
		{
			if(Workspace != null)
			{
				int ChangeNumber;
				if(!FindChangeToSync(Settings.SyncType, out ChangeNumber))
				{
					ShowErrorDialog(String.Format("Couldn't find any {0}changelist. Double-click on the change you want to sync manually.", (Settings.SyncType == LatestChangeType.Starred)? "starred " : (Settings.SyncType == LatestChangeType.Good)? "good " : ""));
				}
				else if(ChangeNumber < Workspace.CurrentChangeNumber)
				{
					ShowErrorDialog("Workspace is already synced to a later change ({0} vs {1}).", Workspace.CurrentChangeNumber, ChangeNumber);
				}
				else if(ChangeNumber >= PerforceMonitor.LastChangeByCurrentUser || MessageBox.Show(String.Format("The changelist that would be synced is before the last change you submitted.\n\nIf you continue, your changes submitted after CL {0} will be locally removed from your workspace until you can sync past them again.\n\nAre you sure you want to continue?", ChangeNumber), "Local changes will be removed", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					Owner.ShowAndActivate();
					SelectChange(ChangeNumber);
					StartSync(ChangeNumber);
				}
			}
		}

		private void BuildListContextMenu_MarkGood_Click(object sender, EventArgs e)
		{
			EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Good);
		}

		private void BuildListContextMenu_MarkBad_Click(object sender, EventArgs e)
		{
			EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Bad);
		}

		private void BuildListContextMenu_WithdrawReview_Click(object sender, EventArgs e)
		{
			EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Unknown);
		}

		private void BuildListContextMenu_Sync_Click(object sender, EventArgs e)
		{
			StartSync(ContextMenuChange.Number);
		}

		private void BuildListContextMenu_SyncContentOnly_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(ContextMenuChange.Number, WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.ContentOnly);
		}

		private void BuildListContextMenu_SyncOnlyThisChange_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(ContextMenuChange.Number, WorkspaceUpdateOptions.SyncSingleChange);
		}

		private void BuildListContextMenu_Build_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(Workspace.CurrentChangeNumber, WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.UseIncrementalBuilds);
		}

		private void BuildListContextMenu_Rebuild_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(Workspace.CurrentChangeNumber, WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.Build);
		}

		private void BuildListContextMenu_GenerateProjectFiles_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(Workspace.CurrentChangeNumber, WorkspaceUpdateOptions.GenerateProjectFiles);
		}

		private void BuildListContextMenu_CancelSync_Click(object sender, EventArgs e)
		{
			CancelWorkspaceUpdate();
		}

		private void BuildListContextMenu_MoreInfo_Click(object sender, EventArgs e)
		{
			if(!Utility.SpawnHiddenProcess("p4vc.exe", String.Format("-c{0} change {1}", Workspace.ClientName, ContextMenuChange.Number)))
			{
				MessageBox.Show("Unable to spawn p4vc. Check you have P4V installed.");
			}
		}

		private void BuildListContextMenu_AddStar_Click(object sender, EventArgs e)
		{
			if(MessageBox.Show("Starred builds are meant to convey a stable, verified build to the rest of the team. Do not star a build unless it has been fully tested.\n\nAre you sure you want to star this build?", "Confirm star", MessageBoxButtons.YesNo) == DialogResult.Yes)
			{
				EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Starred);
			}
		}

		private void BuildListContextMenu_RemoveStar_Click(object sender, EventArgs e)
		{
			EventSummary Summary = EventMonitor.GetSummaryForChange(ContextMenuChange.Number);
			if(Summary != null && Summary.LastStarReview != null && Summary.LastStarReview.Type == EventType.Starred)
			{
				string Message = String.Format("This change was starred by {0}. Are you sure you want to remove it?", FormatUserName(Summary.LastStarReview.UserName));
				if(MessageBox.Show(Message, "Confirm removing star", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Unstarred);
				}
			}
		}

		private void BuildListContextMenu_LaunchEditor_Click(object sender, EventArgs e)
		{
			LaunchEditor();
		}

		private void BuildListContextMenu_StartInvestigating_Click(object sender, EventArgs e)
		{
			string Message = String.Format("All changes from {0} onwards will be marked as bad while you are investigating an issue.\n\nAre you sure you want to continue?", ContextMenuChange.Number);
			if(MessageBox.Show(Message, "Confirm investigating", MessageBoxButtons.YesNo) == DialogResult.Yes)
			{
				EventMonitor.StartInvestigating(ContextMenuChange.Number);
			}
		}

		private void BuildListContextMenu_FinishInvestigating_Click(object sender, EventArgs e)
		{
			int StartChangeNumber = EventMonitor.GetInvestigationStartChangeNumber(ContextMenuChange.Number);
			if(StartChangeNumber != -1)
			{
				if(ContextMenuChange.Number > StartChangeNumber)
				{
					string Message = String.Format("Mark all changes between {0} and {1} as bad?", StartChangeNumber, ContextMenuChange.Number);
					if(MessageBox.Show(Message, "Finish investigating", MessageBoxButtons.YesNo) == DialogResult.Yes)
					{
						foreach(PerforceChangeSummary Change in PerforceMonitor.GetChanges())
						{
							if (Change.Number >= StartChangeNumber && Change.Number < ContextMenuChange.Number)
							{
								EventMonitor.PostEvent(Change.Number, EventType.Bad);
							}
						}
					}
				}
				EventMonitor.FinishInvestigating(ContextMenuChange.Number);
			}
		}

		private void OnlyShowReviewedCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateBuildList();
			UpdateNumRequestedBuilds(true);
		}

		public void Activate()
		{
			UpdateSyncActionCheckboxes();

			if(PerforceMonitor != null)
			{
				PerforceMonitor.Refresh();
			}

			if(DesiredTaskbarState.Item1 == TaskbarState.Error)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
				Owner.UpdateProgress();
			}
		}

		public void Deactivate()
		{
			UpdateNumRequestedBuilds(true);
		}

		private void BuildList_ItemMouseHover(object sender, ListViewItemMouseHoverEventArgs Args)
		{
			Point ClientPoint = BuildList.PointToClient(Cursor.Position);
			if(Args.Item.SubItems.Count >= 6)
			{
				if(Args.Item.Bounds.Contains(ClientPoint))
				{
					PerforceChangeSummary Change = (PerforceChangeSummary)Args.Item.Tag;

					Rectangle DescriptionBounds = Args.Item.SubItems[DescriptionColumn.Index].Bounds;
					if(DescriptionBounds.Contains(ClientPoint))
					{
						BuildListToolTip.Show(Change.Description, BuildList, new Point(DescriptionBounds.Left, DescriptionBounds.Bottom + 2));
						return;
					}

					EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
					if(Summary != null)
					{
						StringBuilder SummaryText = new StringBuilder();
						if(Summary.Comments.Count > 0)
						{
							foreach(CommentData Comment in Summary.Comments)
							{
								if(!String.IsNullOrWhiteSpace(Comment.Text))
								{
									SummaryText.AppendFormat("{0}: \"{1}\"\n", FormatUserName(Comment.UserName), Comment.Text);
								}
							}
							if(SummaryText.Length > 0)
							{
								SummaryText.Append("\n");
							}
						}
						AppendUserList(SummaryText, "\n", "Compiled by {0}.", Summary.Reviews.Where(x => x.Type == EventType.Compiles));
						AppendUserList(SummaryText, "\n", "Failed to compile for {0}.", Summary.Reviews.Where(x => x.Type == EventType.DoesNotCompile));
						AppendUserList(SummaryText, "\n", "Marked good by {0}.", Summary.Reviews.Where(x => x.Type == EventType.Good));
						AppendUserList(SummaryText, "\n", "Marked bad by {0}.", Summary.Reviews.Where(x => x.Type == EventType.Bad));
						if(Summary.LastStarReview != null)
						{
							AppendUserList(SummaryText, "\n", "Starred by {0}.", new EventData[]{ Summary.LastStarReview });
						}
						if(SummaryText.Length > 0)
						{
							Rectangle SummaryBounds = Args.Item.SubItems[StatusColumn.Index].Bounds;
							BuildListToolTip.Show(SummaryText.ToString(), BuildList, new Point(SummaryBounds.Left, SummaryBounds.Bottom));
							return;
						}
					}
				}
			}

			BuildListToolTip.Hide(BuildList);
		}

		private void BuildList_MouseLeave(object sender, EventArgs e)
		{
			BuildListToolTip.Hide(BuildList);

			if(HoverItem != -1)
			{
				HoverItem = -1;
				BuildList.Invalidate();
			}

			if(HoverBadge != null)
			{
				HoverBadge = null;
				BuildList.Invalidate();
			}
		}

		private void OptionsContextMenu_ShowLog_Click(object sender, EventArgs e)
		{
			ToggleLogVisibility();
		}

		private void ToggleLogVisibility()
		{
			Splitter.SetLogVisibility(!Splitter.IsLogVisible());
		}

		private void Splitter_OnVisibilityChanged(bool bVisible)
		{
			Settings.bShowLogWindow = bVisible;
			Settings.Save();

			UpdateStatusPanel();
		}

		private void OptionsContextMenu_AutoResolveConflicts_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_AutoResolveConflicts.Checked ^= true;
			Settings.bAutoResolveConflicts = OptionsContextMenu_AutoResolveConflicts.Checked;
			Settings.Save();
		}

		private void OptionsContextMenu_EditorArguments_Click(object sender, EventArgs e)
		{
			ArgumentsWindow Arguments = new ArgumentsWindow(Settings.EditorArguments);
			if(Arguments.ShowDialog(this) == DialogResult.OK)
			{
				Settings.EditorArguments = Arguments.GetItems();
				Settings.Save();
			}
		}

		private void BuildListContextMenu_OpenVisualStudio_Click(object sender, EventArgs e)
		{
			OpenSolution();
		}

		private void OpenSolution()
		{
			string SolutionFileName = Path.Combine(BranchDirectoryName, "UE4.sln");
			if(!File.Exists(SolutionFileName))
			{
				MessageBox.Show(String.Format("Couldn't find solution at {0}", SolutionFileName));
			}
			else
			{
				ProcessStartInfo StartInfo = new ProcessStartInfo(SolutionFileName);
				StartInfo.WorkingDirectory = BranchDirectoryName;
				Process.Start(StartInfo);
			}
		}

		private void OptionsContextMenu_BuildConfig_Debug_Click(object sender, EventArgs e)
		{
			UpdateBuildConfig(BuildConfig.Debug);
		}

		private void OptionsContextMenu_BuildConfig_DebugGame_Click(object sender, EventArgs e)
		{
			UpdateBuildConfig(BuildConfig.DebugGame);
		}

		private void OptionsContextMenu_BuildConfig_Development_Click(object sender, EventArgs e)
		{
			UpdateBuildConfig(BuildConfig.Development);
		}

		void UpdateBuildConfig(BuildConfig NewConfig)
		{
			Settings.CompiledEditorBuildConfig = NewConfig;
			Settings.Save();
			UpdateCheckedBuildConfig();
		}

		void UpdateCheckedBuildConfig()
		{
			BuildConfig EditorBuildConfig = GetEditorBuildConfig();

			OptionsContextMenu_BuildConfig_Debug.Checked = (EditorBuildConfig == BuildConfig.Debug);
			OptionsContextMenu_BuildConfig_Debug.Enabled = (!ShouldSyncPrecompiledEditor || EditorBuildConfig == BuildConfig.Debug);

			OptionsContextMenu_BuildConfig_DebugGame.Checked = (EditorBuildConfig == BuildConfig.DebugGame);
			OptionsContextMenu_BuildConfig_DebugGame.Enabled = (!ShouldSyncPrecompiledEditor || EditorBuildConfig == BuildConfig.DebugGame);

			OptionsContextMenu_BuildConfig_Development.Checked = (EditorBuildConfig == BuildConfig.Development);
			OptionsContextMenu_BuildConfig_Development.Enabled = (!ShouldSyncPrecompiledEditor || EditorBuildConfig == BuildConfig.Development);
		}

		private void OptionsContextMenu_UseIncrementalBuilds_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_UseIncrementalBuilds.Checked ^= true;
			Settings.bUseIncrementalBuilds = OptionsContextMenu_UseIncrementalBuilds.Checked;
			Settings.Save();
		}

		private void OptionsContextMenu_ScheduleSync_Click(object sender, EventArgs e)
		{
			Owner.SetupScheduledSync();
		}

		public void ScheduleTimerElapsed()
		{
			if(Settings.bScheduleEnabled)
			{
				Log.WriteLine("Scheduled sync at {0} for {1}.", DateTime.Now, Settings.ScheduleChange);

				int ChangeNumber;
				if(!FindChangeToSync(Settings.ScheduleChange, out ChangeNumber))
				{
					Log.WriteLine("Couldn't find any matching change");
				}
				else if(Workspace.CurrentChangeNumber >= ChangeNumber)
				{
					Log.WriteLine("Sync ignored; already at or ahead of CL ({0} >= {1})", Workspace.CurrentChangeNumber, ChangeNumber);
				}
				else
				{
					WorkspaceUpdateOptions Options = WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.GenerateProjectFiles | WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.ScheduledBuild;
					if(Settings.bAutoResolveConflicts)
					{
						Options |= WorkspaceUpdateOptions.AutoResolveChanges;
					}
					if(Settings.bBuildAfterSync && Settings.bRunAfterSync)
					{
						Options |= WorkspaceUpdateOptions.RunAfterSync;
					}
					StartWorkspaceUpdate(ChangeNumber, Options);
				}
			}
		}

		bool FindChangeToSync(LatestChangeType ChangeType, out int ChangeNumber)
		{
			for(int Idx = 0; Idx < BuildList.Items.Count; Idx++)
			{
				PerforceChangeSummary Change = (PerforceChangeSummary)BuildList.Items[Idx].Tag;
				if(ChangeType == LatestChangeType.Any)
				{
					if(CanSyncChange(Change.Number))
					{
						ChangeNumber = FindNewestGoodContentChange(Change.Number);
						return true;
					}
				}
				else if(ChangeType == LatestChangeType.Good)
				{
					EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
					if(Summary != null && Summary.Verdict == ReviewVerdict.Good && CanSyncChange(Change.Number))
					{
						ChangeNumber = FindNewestGoodContentChange(Change.Number);
						return true;
					}
				}
				else if(ChangeType == LatestChangeType.Starred)
				{
					EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
					if(((Summary != null && Summary.LastStarReview != null) || PromotedChangeNumbers.Contains(Change.Number)) && CanSyncChange(Change.Number))
					{
						ChangeNumber = FindNewestGoodContentChange(Change.Number);
						return true;
					}
				}
			}

			ChangeNumber = -1;
			return false;
		}
		
		int FindNewestGoodContentChange(int ChangeNumber)
		{
			int Index = SortedChangeNumbers.BinarySearch(ChangeNumber);
			if(Index <= 0)
			{
				return ChangeNumber;
			}

			for(int NextIndex = Index + 1; NextIndex < SortedChangeNumbers.Count; NextIndex++)
			{
				int NextChangeNumber = SortedChangeNumbers[NextIndex];

				PerforceChangeType Type;
				if(!PerforceMonitor.TryGetChangeType(NextChangeNumber, out Type) || Type != PerforceChangeType.Content)
				{
					break;
				}

				EventSummary Summary = EventMonitor.GetSummaryForChange(NextChangeNumber);
				if(Summary != null && Summary.Verdict == ReviewVerdict.Bad)
				{
					break;
				}

				Index = NextIndex;
			}

			return SortedChangeNumbers[Index];
		}

		private void BuildListContextMenu_LeaveOrEditComment_Click(object sender, EventArgs e)
		{
			if(ContextMenuChange != null)
			{
				LeaveCommentWindow LeaveComment = new LeaveCommentWindow();

				string CommentText;
				if(EventMonitor.GetCommentByCurrentUser(ContextMenuChange.Number, out CommentText))
				{
					LeaveComment.CommentTextBox.Text = CommentText;
				}

				if(LeaveComment.ShowDialog() == System.Windows.Forms.DialogResult.OK)
				{
					EventMonitor.PostComment(ContextMenuChange.Number, LeaveComment.CommentTextBox.Text);
				}
			}
		}

		private void BuildListContextMenu_ShowServerTimes_Click(object sender, EventArgs e)
		{
			Settings.bShowLocalTimes = false;
			Settings.Save();
			UpdateBuildList();
		}

		private void BuildListContextMenu_ShowLocalTimes_Click(object sender, EventArgs e)
		{
			Settings.bShowLocalTimes = true;
			Settings.Save();
			UpdateBuildList();
		}

		private void MoreToolsContextMenu_CleanWorkspace_Click(object sender, EventArgs e)
		{
			CleanWorkspaceWindow.DoClean(ParentForm, Workspace.Perforce, BranchDirectoryName, Workspace.ClientRootPath, Workspace.SyncPaths, Log);
		}

		private void UpdateBuildSteps()
		{
			bHasBuildSteps = false;

			foreach(ToolStripMenuItem CustomToolMenuItem in CustomToolMenuItems)
			{
				MoreToolsContextMenu.Items.Remove(CustomToolMenuItem);
			}

			CustomToolMenuItems.Clear();

			if(Workspace != null)
			{
				ConfigFile ProjectConfigFile = Workspace.ProjectConfigFile;
				if(ProjectConfigFile != null)
				{
					Dictionary<Guid, ConfigObject> ProjectBuildStepObjects = GetProjectBuildStepObjects(ProjectConfigFile);

					int InsertIdx = 0;

					List<BuildStep> UserSteps = GetUserBuildSteps(ProjectBuildStepObjects);
					foreach(BuildStep Step in UserSteps)
					{
						if(Step.bShowAsTool)
						{
							ToolStripMenuItem NewMenuItem = new ToolStripMenuItem(Step.Description.Replace("&", "&&"));
							NewMenuItem.Click += new EventHandler((sender, e) => { RunCustomTool(Step.UniqueId); });
							CustomToolMenuItems.Add(NewMenuItem);
							MoreToolsContextMenu.Items.Insert(InsertIdx++, NewMenuItem);
						}
						bHasBuildSteps |= Step.bNormalSync;
					}
				}
			}

			MoreActionsContextMenu_CustomToolSeparator.Visible = (CustomToolMenuItems.Count > 0);
		}

		private void RunCustomTool(Guid UniqueId)
		{
			if(Workspace != null)
			{
				if(Workspace.IsBusy())
				{
					MessageBox.Show("Please retry after the current sync has finished.", "Sync in Progress");
				}
				else
				{
					WorkspaceUpdateContext Context = new WorkspaceUpdateContext(Workspace.CurrentChangeNumber, WorkspaceUpdateOptions.Build, null, GetDefaultBuildStepObjects(), ProjectSettings.BuildSteps, new HashSet<Guid>{ UniqueId }, GetWorkspaceVariables());
					StartWorkspaceUpdate(Context);
				}
			}
		}

		private Dictionary<string, string> GetWorkspaceVariables()
		{
			BuildConfig EditorBuildConfig = GetEditorBuildConfig();

			Dictionary<string, string> Variables = new Dictionary<string,string>();
			Variables.Add("Stream", StreamName);
			Variables.Add("ClientName", ClientName);
			Variables.Add("BranchDir", BranchDirectoryName);
			Variables.Add("ProjectDir", Path.GetDirectoryName(SelectedFileName));
			Variables.Add("ProjectFile", SelectedFileName);
			Variables.Add("UE4EditorExe", GetEditorExePath(EditorBuildConfig));
			Variables.Add("UE4EditorCmdExe", GetEditorExePath(EditorBuildConfig).Replace(".exe", "-Cmd.exe"));
			Variables.Add("UE4EditorConfig", EditorBuildConfig.ToString());
			Variables.Add("UE4EditorDebugArg", (EditorBuildConfig == BuildConfig.Debug || EditorBuildConfig == BuildConfig.DebugGame)? " -debug" : "");
			return Variables;
		}

		private void OptionsContextMenu_EditBuildSteps_Click(object sender, EventArgs e)
		{
			ConfigFile ProjectConfigFile = Workspace.ProjectConfigFile;
			if(Workspace != null && ProjectConfigFile != null)
			{
				// Find all the target names for this project
				List<string> TargetNames = new List<string>();
				if(!String.IsNullOrEmpty(SelectedFileName) && SelectedFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
				{
					DirectoryInfo SourceDirectory = new DirectoryInfo(Path.Combine(Path.GetDirectoryName(SelectedFileName), "Source"));
					if(SourceDirectory.Exists)
					{
						foreach(FileInfo TargetFile in SourceDirectory.EnumerateFiles("*.target.cs", SearchOption.TopDirectoryOnly))
						{
							TargetNames.Add(TargetFile.Name.Substring(0, TargetFile.Name.IndexOf('.')));
						}
					}
				}

				// Get all the task objects
				Dictionary<Guid, ConfigObject> ProjectBuildStepObjects = GetProjectBuildStepObjects(ProjectConfigFile);
				List<BuildStep> UserSteps = GetUserBuildSteps(ProjectBuildStepObjects);

				// Show the dialog
				ModifyBuildStepsWindow EditStepsWindow = new ModifyBuildStepsWindow(TargetNames, UserSteps, new HashSet<Guid>(ProjectBuildStepObjects.Keys), BranchDirectoryName, GetWorkspaceVariables());
				EditStepsWindow.ShowDialog();

				// Update the user settings
				List<ConfigObject> ModifiedBuildSteps = new List<ConfigObject>();
				foreach(BuildStep Step in UserSteps)
				{
					ConfigObject DefaultObject;
					ProjectBuildStepObjects.TryGetValue(Step.UniqueId, out DefaultObject);

					ConfigObject UserConfigObject = Step.ToConfigObject(DefaultObject);
					if(UserConfigObject != null)
					{
						ModifiedBuildSteps.Add(UserConfigObject);
					}
				}

				// Save the settings
				ProjectSettings.BuildSteps = ModifiedBuildSteps;
				Settings.Save();

				// Update the custom tools menu, because we might have changed it
				UpdateBuildSteps();
				UpdateSyncActionCheckboxes();
			}
		}

		private void AddOrUpdateBuildStep(Dictionary<Guid, ConfigObject> Steps, ConfigObject Object)
		{
			Guid UniqueId;
			if(Guid.TryParse(Object.GetValue(BuildStep.UniqueIdKey, ""), out UniqueId))
			{
				ConfigObject DefaultObject;
				if(Steps.TryGetValue(UniqueId, out DefaultObject))
				{
					Object.SetDefaults(DefaultObject);
				}
				Steps[UniqueId] = Object;
			}
		}

		private bool ShouldSyncPrecompiledEditor
		{
			get { return Settings.bSyncPrecompiledEditor && PerforceMonitor != null && PerforceMonitor.HasZippedBinaries; }
		}

		public BuildConfig GetEditorBuildConfig()
		{
			return ShouldSyncPrecompiledEditor? BuildConfig.Development : Settings.CompiledEditorBuildConfig;
		}

		private Dictionary<Guid,ConfigObject> GetDefaultBuildStepObjects()
		{
			List<BuildStep> DefaultBuildSteps = new List<BuildStep>();
			DefaultBuildSteps.Add(new BuildStep(new Guid("{01F66060-73FA-4CC8-9CB3-E217FBBA954E}"), 0, "Compile UnrealHeaderTool", "Compiling UnrealHeaderTool...", 1, "UnrealHeaderTool", "Win64", "Development", "", !ShouldSyncPrecompiledEditor));
			string ActualEditorTargetName = EditorTargetName ?? "UE4Editor";
			DefaultBuildSteps.Add(new BuildStep(new Guid("{F097FF61-C916-4058-8391-35B46C3173D5}"), 1, String.Format("Compile {0}", ActualEditorTargetName), String.Format("Compiling {0}...", ActualEditorTargetName), 10, ActualEditorTargetName, "Win64", Settings.CompiledEditorBuildConfig.ToString(), "", !ShouldSyncPrecompiledEditor));
			DefaultBuildSteps.Add(new BuildStep(new Guid("{C6E633A1-956F-4AD3-BC95-6D06D131E7B4}"), 2, "Compile ShaderCompileWorker", "Compiling ShaderCompileWorker...", 1, "ShaderCompileWorker", "Win64", "Development", "", !ShouldSyncPrecompiledEditor));
			DefaultBuildSteps.Add(new BuildStep(new Guid("{24FFD88C-7901-4899-9696-AE1066B4B6E8}"), 3, "Compile UnrealLightmass", "Compiling UnrealLightmass...", 1, "UnrealLightmass", "Win64", "Development", "", !ShouldSyncPrecompiledEditor));
			DefaultBuildSteps.Add(new BuildStep(new Guid("{FFF20379-06BF-4205-8A3E-C53427736688}"), 4, "Compile CrashReportClient", "Compiling CrashReportClient...", 1, "CrashReportClient", "Win64", "Shipping", "", !ShouldSyncPrecompiledEditor));
			return DefaultBuildSteps.ToDictionary(x => x.UniqueId, x => x.ToConfigObject());
		}

		private Dictionary<Guid, ConfigObject> GetProjectBuildStepObjects(ConfigFile ProjectConfigFile)
		{
			Dictionary<Guid, ConfigObject> ProjectBuildSteps = GetDefaultBuildStepObjects();
			foreach(string Line in ProjectConfigFile.GetValues("Build.Step", new string[0]))
			{
				AddOrUpdateBuildStep(ProjectBuildSteps, new ConfigObject(Line));
			}
			return ProjectBuildSteps;
		}
		
		private List<BuildStep> GetUserBuildSteps(Dictionary<Guid, ConfigObject> ProjectBuildStepObjects)
		{
			// Read all the user-defined build tasks and modifications to the default list
			Dictionary<Guid, ConfigObject> UserBuildStepObjects = ProjectBuildStepObjects.ToDictionary(x => x.Key, y => new ConfigObject(y.Value));
			foreach(ConfigObject UserBuildStep in ProjectSettings.BuildSteps)
			{
				AddOrUpdateBuildStep(UserBuildStepObjects, UserBuildStep);
			}

			// Create the expanded task objects
			return UserBuildStepObjects.Values.Select(x => new BuildStep(x)).OrderBy(x => x.OrderIndex).ToList();
		}

		private void OptionsContextMenu_AutomaticallyRunAtStartup_Click(object sender, EventArgs e)
		{
			RegistryKey Key = Registry.CurrentUser.CreateSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
			if(IsAutomaticallyRunAtStartup())
			{
	            Key.DeleteValue("UnrealGameSync", false);
			}
			else
			{
				Key.SetValue("UnrealGameSync", OriginalExecutableFileName);
			}
		}

		private bool IsAutomaticallyRunAtStartup()
		{
			RegistryKey Key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
			return (Key.GetValue("UnrealGameSync") != null);
		}

		private void OptionsContextMenu_SyncPrecompiledEditor_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_SyncPrecompiledEditor.Checked ^= true;

			Settings.bSyncPrecompiledEditor = OptionsContextMenu_SyncPrecompiledEditor.Checked;
			Settings.Save();

			UpdateBuildSteps();
			UpdateSyncActionCheckboxes();

			BuildList.Invalidate();
		}

		private void BuildList_SelectedIndexChanged(object sender, EventArgs e)
		{
			PendingSelectedChangeNumber = -1;
		}

		private void OptionsContextMenu_Diagnostics_Click(object sender, EventArgs e)
		{
			StringBuilder DiagnosticsText = new StringBuilder();
			DiagnosticsText.AppendFormat("Application version: {0}\n", Assembly.GetExecutingAssembly().GetName().Version);
			DiagnosticsText.AppendFormat("Synced from: {0}\n", Program.SyncVersion ?? "(unknown)");
			DiagnosticsText.AppendFormat("Selected file: {0}\n", (SelectedFileName == null)? "(none)" : SelectedFileName);
			if(Workspace != null)
			{
				DiagnosticsText.AppendFormat("P4 server: {0}\n", Workspace.Perforce.ServerAndPort ?? "(default)");
				DiagnosticsText.AppendFormat("P4 user: {0}\n", Workspace.Perforce.UserName);
				DiagnosticsText.AppendFormat("P4 workspace: {0}\n", Workspace.Perforce.ClientName);
			}
			DiagnosticsText.AppendFormat("Perforce monitor: {0}\n", (PerforceMonitor == null)? "(inactive)" : PerforceMonitor.LastStatusMessage);
			DiagnosticsText.AppendFormat("Event monitor: {0}\n", (EventMonitor == null)? "(inactive)" : EventMonitor.LastStatusMessage);

			DiagnosticsWindow Diagnostics = new DiagnosticsWindow(DataFolder, DiagnosticsText.ToString());
			Diagnostics.ShowDialog(this);
		}

		private void OptionsContextMenu_SyncFilter_Click(object sender, EventArgs e)
		{
			SyncFilter Filter = new SyncFilter(Workspace.GetSyncCategories(), Settings.SyncView, Settings.SyncExcludedCategories, WorkspaceSettings.SyncView, WorkspaceSettings.SyncExcludedCategories);
			if(Filter.ShowDialog() == DialogResult.OK)
			{
				Settings.SyncExcludedCategories = Filter.GlobalExcludedCategories;
				Settings.SyncView = Filter.GlobalView;
				WorkspaceSettings.SyncExcludedCategories = Filter.WorkspaceExcludedCategories;
				WorkspaceSettings.SyncView = Filter.WorkspaceView;
                Settings.Save();
			}
		}

		private void ShowSyncMenu(Rectangle Bounds)
		{
			SyncContextMenu_LatestChange.Checked = (Settings.SyncType == LatestChangeType.Any);
			SyncContextMenu_LatestGoodChange.Checked = (Settings.SyncType == LatestChangeType.Good);
			SyncContextMenu_LatestStarredChange.Checked = (Settings.SyncType == LatestChangeType.Starred);
			SyncContextMenu.Show(StatusPanel, new Point(Bounds.Left, Bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void SyncContextMenu_LatestChange_Click(object sender, EventArgs e)
		{
			Settings.SyncType = LatestChangeType.Any;
			Settings.Save();
		}

		private void SyncContextMenu_LatestGoodChange_Click(object sender, EventArgs e)
		{
			Settings.SyncType = LatestChangeType.Good;
			Settings.Save();
		}

		private void SyncContextMenu_LatestStarredChange_Click(object sender, EventArgs e)
		{
			Settings.SyncType = LatestChangeType.Starred;
			Settings.Save();
		}

		private void SyncContextMenu_EnterChangelist_Click(object sender, EventArgs e)
		{
			ChangelistWindow ChildWindow = new ChangelistWindow((Workspace == null)? -1 : Workspace.CurrentChangeNumber);
			if(ChildWindow.ShowDialog() == DialogResult.OK)
			{
				StartSync(ChildWindow.ChangeNumber);
			}
		}

		private void OptionsContextMenu_KeepInTray_Click(object sender, EventArgs e)
		{
			Settings.bKeepInTray ^= true;
			Settings.Save();
		}

		private void BuildList_KeyDown(object Sender, KeyEventArgs Args)
		{
			if(Args.Control && Args.KeyCode == Keys.C && BuildList.SelectedItems.Count > 0)
			{
				int SelectedChange = ((PerforceChangeSummary)BuildList.SelectedItems[0].Tag).Number;
				Clipboard.SetText(String.Format("{0}", SelectedChange));
			}
		}

		private void OptionsContextMenu_PerforceSettings_Click(object sender, EventArgs e)
		{
			PerforceSettingsWindow Perforce = new PerforceSettingsWindow(Settings);
			Perforce.ShowDialog();
		}

		private void OptionsContextMenu_TabNames_Stream_Click(object sender, EventArgs e)
		{
			Owner.SetTabNames(TabLabels.Stream);
		}

		private void OptionsContextMenu_TabNames_WorkspaceName_Click(object sender, EventArgs e)
		{
			Owner.SetTabNames(TabLabels.WorkspaceName);
		}

		private void OptionsContextMenu_TabNames_WorkspaceRoot_Click(object sender, EventArgs e)
		{
			Owner.SetTabNames(TabLabels.WorkspaceRoot);
		}

		private void OptionsContextMenu_TabNames_ProjectFile_Click(object sender, EventArgs e)
		{
			Owner.SetTabNames(TabLabels.ProjectFile);
		}

		private void SelectRecentProject(Rectangle Bounds)
		{
			while(RecentMenu.Items[2] != RecentMenu_Separator)
			{
				RecentMenu.Items.RemoveAt(2);
			}

			HashSet<string> ProjectList = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
			foreach(string ProjectFileName in Settings.OtherProjectFileNames)
			{
				if(!String.IsNullOrEmpty(ProjectFileName))
				{
					string FullProjectFileName = Path.GetFullPath(ProjectFileName);
					if(ProjectList.Add(FullProjectFileName))
					{
						ToolStripMenuItem Item = new ToolStripMenuItem(FullProjectFileName, null, new EventHandler((o, e) => Owner.RequestProjectChange(this, FullProjectFileName)));
						RecentMenu.Items.Insert(RecentMenu.Items.Count - 2, Item);
					}
				}
			}

			RecentMenu_Separator.Visible = (RecentMenu.Items.Count >= 2);
			RecentMenu.Show(StatusPanel, new Point(Bounds.Left, Bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void RecentMenu_Browse_Click(object sender, EventArgs e)
		{
			Owner.BrowseForProject(this);
		}

		private void RecentMenu_ClearList_Click(object sender, EventArgs e)
		{
			Settings.OtherProjectFileNames = new string[0];
			Settings.Save();
		}
	}
}
