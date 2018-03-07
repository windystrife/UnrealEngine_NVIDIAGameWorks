// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows.Forms;
using System.Xml;
using System.Xml.Serialization;
using System.Linq;

using EnvDTE;
using EpicCommonUtils;
using UnrealControls;
using System.Drawing.Drawing2D;
using System.Windows.Forms.DataVisualization.Charting;

namespace MemoryProfiler2
{
	public partial class MainWindow : Form
	{
		public const int DEFAULT_MINOR_TICK_WIDTH = 12;
		public const int DEFAULT_MAJOR_TICK_WIDTH = 30;

		/// <summary> A class that is serialised to disk with settings info. </summary>
		public SettableOptions Options = null;

		/// <summary> List of file offsets to create custom snapshots for. </summary>
		private List<int> CustomSnapshots = new List<int>();

		/// <summary> Current active snapshot. Either a diff if start is != start of actual snapshot if start == start. </summary>
		public FStreamSnapshot CurrentSnapshot;

		/// <summary> Current file name associated with open file. </summary>
		public string CurrentFilename;

		/// <summary> Progress indicator window. </summary>
		public SlowProgressDialog ProgressDialog = new SlowProgressDialog();

		/// <summary> Tags filter dialog. </summary>
		public FilterTagsDialog FilterTagsDialog = new FilterTagsDialog();

		/// <summary> Selected memory pool filter. </summary>
		public EMemoryPool SelectedMemoryPool = EMemoryPool.MEMPOOL_None;

		/// <summary> Font used to draw numbers on an axis. </summary>
		public Font AxisFont = new Font( FontFamily.GenericSansSerif, 13, GraphicsUnit.Pixel );

        /// <summary> Copy of original window title so we rebuild it when the loaded file changes </summary>
        public string OriginalWindowTitle;

        /// <summary> Initializes common controls and setups all needed properties. </summary>
        private void CommonInit()
		{
			OriginalWindowTitle = Text;

            Options = UnrealControls.XmlHandler.ReadXml<SettableOptions>( Path.Combine( Application.StartupPath, "MemoryProfiler2.ClassGroups.xml" ) );
			Options.ApplyDefaults();

			AppDomain.CurrentDomain.AssemblyResolve += new ResolveEventHandler( CurrentDomain_AssemblyResolve );

			ExclusiveListView.ListViewItemSorter = new FColumnSorter();

			PopulateClassGroups();

			InvertCallgraphViewMenuItem.Checked = Options.InvertCallStacksInCallGraphView;
			FilterObjectVMFunctionsMenuItem.Checked = Options.FilterOutObjectVMFunctions;

			SelectedMemoryPool = ( EMemoryPool )Options.MemoryPoolFilterState;
			UpdatePoolFilterFromSelectedPool();

			FCallGraphTreeViewParser.SetProfilerWindow( this );
			FCallStackHistoryView.SetProfilerWindow( this );
			FExclusiveListViewParser.SetProfilerWindow( this );
			FTimeLineChartView.SetProfilerWindow( this );
			FHistogramParser.SetProfilerWindow( this );
			FMemoryBitmapParser.SetProfilerWindow( this );
			FShortLivedAllocationView.SetProfilerWindow( this );

			SetupFilteringControls();
			ResetFilteringState();
			LastResizedSize = Size;

			ToolTip DetailsViewTips = new ToolTip();
			DetailsViewTips.SetToolTip( DetailsViewStartLabel, "Shows start snapshot detailed information" );
			DetailsViewTips.SetToolTip( DetailsViewDiffLabel, "Shows detailed information as a difference between end and start snapshot" );
			DetailsViewTips.SetToolTip( DetailsViewEndLabel, "Shows end snapshot detailed information" );
		}

		/// <summary> Default constructor. </summary>
		public MainWindow()
		{
			InitializeComponent();
			CommonInit();
		}

		/// <summary> Filename based constructor. </summary>
		/// <param name="Filename"> Name of the filename that we want to be loaded on the application start </param>
		public MainWindow( string Filename )
		{
			InitializeComponent();
			CommonInit();
			ParseFile( Filename );
		}

		/// <summary> Saves options, occurs before the main window is closed. </summary>
		private void MainWindowFormClosing( object sender, FormClosingEventArgs e )
		{
			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, Path.Combine( Application.StartupPath, "MemoryProfiler2.ClassGroups.xml" ), "" );

			if( FStreamInfo.GlobalInstance != null )
			{
				FStreamInfo.GlobalInstance.Shutdown();
			}
		}

		/// <summary> Set the wait cursor to the hourglass for long operations. </summary>
		public void SetWaitCursor( bool bOn )
		{
			UseWaitCursor = bOn;
			if( UseWaitCursor )
			{
				Cursor = Cursors.WaitCursor;
			}
			else
			{
				Cursor = Cursors.Default;
			}
		}

		/// <summary> Updates the status string label with the passed in string. </summary>
		/// <param name="Status"> New status to set. </param>
		public void UpdateStatus( string Status )
		{
			StatusStripLabel.Text = Status;
			Refresh();
		}

		#region "Classes to filter" menu
		/*-----------------------------------------------------------------------------
			"Classes to filter" menu 
		-----------------------------------------------------------------------------*/

		/// <summary> Tag used to determine that "All" history menu was chosen. </summary>
		const string AllHistoryTag = "AllHistory";

		/// <summary> Function used to compare class group name. </summary>
		public int ClassGroupSortFunction( ClassGroup Left, ClassGroup Right )
		{
			return ( string.Compare( Left.Name, Right.Name, StringComparison.InvariantCultureIgnoreCase ) );
		}

		/// <summary> Populates class groups into "Classes to filter" menu. </summary>
		private void PopulateClassGroups()
		{
			// Clear out the old list (if any)
			FilterClassesDropDownButton.DropDownItems.Clear();

			// Add in each classgroup alphabetically
			Options.ClassGroups.Sort( new Comparison<ClassGroup>( ClassGroupSortFunction ) );
			foreach( ClassGroup FilterClass in Options.ClassGroups )
			{
				ToolStripMenuItemEx MenuItem = new ToolStripMenuItemEx( FilterClass.Name );
				MenuItem.CheckOnClick = true;
				MenuItem.Checked = true;
				MenuItem.Tag = FilterClass;
				MenuItem.CheckState = FilterClass.bFilter ? CheckState.Checked : CheckState.Unchecked;
				MenuItem.MouseUp += ClassesToFilterMouseUp;

				MenuItem.FilteringColor = FilterClass.Color;
				MenuItem.bDrawExclamationMark = false;

				MenuItem.Image = Properties.Resources.ExclamationMark;
				MenuItem.DisplayStyle = ToolStripItemDisplayStyle.Text;

				FilterClassesDropDownButton.DropDownItems.Add( MenuItem );
			}

			// Add in the all and none options
			ToolStripSeparator Separator = new ToolStripSeparator();
			FilterClassesDropDownButton.DropDownItems.Add( Separator );

			ToolStripMenuItem AllMenuItem = new ToolStripMenuItem( "All" );
			AllMenuItem.Click += new System.EventHandler( FilterAllClassGroupsClick );
			AllMenuItem.MouseUp += ClassesToFilterMouseUp;
			AllMenuItem.Tag = AllHistoryTag;

			FilterClassesDropDownButton.DropDownItems.Add( AllMenuItem );

			ToolStripMenuItem NoneMenuItem = new ToolStripMenuItem( "None" );
			NoneMenuItem.Click += new System.EventHandler( FilterNoneClassGroupsClick );
			FilterClassesDropDownButton.DropDownItems.Add( NoneMenuItem );
		}
		#endregion

		private void FilterTagsButton_Click(object sender, EventArgs e)
		{
			FilterTagsDialog.PopulateTagFilterHierarchy();
			FilterTagsDialog.ShowDialog();
			UpdateFilteringState();
		}

		/// <summary> Assembly resolve method to pick correct StandaloneSymbolParser DLL. </summary>
		private Assembly CurrentDomain_AssemblyResolve( Object sender, ResolveEventArgs args )
		{
			// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
			string[] AssemblyInfo = args.Name.Split( ",".ToCharArray() );
			string AssemblyName = AssemblyInfo[0];

			int ToolsIndex = AssemblyName.ToLower().IndexOf( "tools" );
			if( ToolsIndex > 0 )
			{
				string ToolsName = AssemblyName.Substring( 0, ToolsIndex );
				AssemblyName = Application.StartupPath + "\\" + ToolsName + "\\";
				if( Environment.Is64BitProcess )
				{
					AssemblyName += ToolsName + "Tools_x64.dll";
				}
				else
				{
					AssemblyName += ToolsName + "Tools.dll";
				}

				Debug.WriteLineIf( System.Diagnostics.Debugger.IsAttached, "Loading assembly: " + AssemblyName );

				Assembly ToolsAssembly = Assembly.LoadFile( AssemblyName );
				return ToolsAssembly;
			}

			return ( null );
		}

		#region Snapshot helper functions 
		/*-----------------------------------------------------------------------------
			Snapshot helper functions
		-----------------------------------------------------------------------------*/

		/// <summary> Returns true if the " Filter In" option is selected (otherwise the option is assumed to be " Filter Out"). </summary>
		public bool IsFilteringIn()
		{
			return FilterTypeSplitButton.Text == " Filter In";
		}

		/// <summary> 
		/// Returns index of selected start snapshot. 
		/// NOTE: 'Start' snapshot is -1
		/// </summary>
		public int GetStartSnapshotIndex()
		{
			return DiffStartComboBox.SelectedIndex - 1;
		}

		/// <summary> 
		/// Returns index of selected end snapshot. 
		/// NOTE: 'Start' snapshot is -1
		/// </summary>
		public int GetEndSnapshotIndex()
		{
			return DiffEndComboBox.SelectedIndex - 1;
		}

		/// <summary> Returns stream index of the selected start snapshot. </summary>
		public ulong GetStartSnapshotStreamIndex()
		{
			return DiffStartComboBox.SelectedIndex == 0 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ DiffStartComboBox.SelectedIndex - 1 ].StreamIndex;
		}

		/// <summary> Returns stream index of the selected end snapshot. </summary>
		public ulong GetEndSnapshotStreamIndex()
		{
			return DiffEndComboBox.SelectedIndex == 0 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ DiffEndComboBox.SelectedIndex - 1 ].StreamIndex;
		}

		/// <summary> Returns the snapshot index based on start snapshot index and stream index. </summary>
		/// <param name="StartSnapshot"> Snapshot index we start searching, may be 0 if we want to look through all snapshots. </param>
		/// <param name="StreamIndex"> Stream index of the snapshot that we are looking for. </param>
		public int GetSnapshotIndexFromStreamIndex( int StartSnapshot, ulong StreamIndex )
		{
			for( int SnapshotIndex = StartSnapshot; SnapshotIndex < FStreamInfo.GlobalInstance.SnapshotList.Count; SnapshotIndex++ )
			{
				if( FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex > StreamIndex )
				{
					return SnapshotIndex;
				}
			}

			return FStreamInfo.GlobalInstance.SnapshotList.Count;
		}

		/// <summary> Returns the allocation count of the far left point on the timeline chart. </summary>
		private long GetStartingAllocationCount()
		{
			if( DiffStartComboBox.SelectedIndex <= DiffEndComboBox.SelectedIndex )
			{
				if( DiffStartComboBox.SelectedIndex == 0 )
				{
					return 0;
				}
				return FStreamInfo.GlobalInstance.SnapshotList[ DiffStartComboBox.SelectedIndex - 1 ].AllocationCount;
			}
			else
			{
				if( DiffEndComboBox.SelectedIndex == 0 )
				{
					return 0;
				}
				return FStreamInfo.GlobalInstance.SnapshotList[ DiffEndComboBox.SelectedIndex - 1 ].AllocationCount;
			}
		}

		/// <summary> Updates current snapshot based on selected values in diff start/ end combo boxes. </summary>
		private void UpdateCurrentSnapshot()
		{
			if( FStreamInfo.GlobalInstance.SnapshotList == null )
			{
				return;
			}

			// Start == "Start"
			if( DiffStartComboBox.SelectedIndex == 0 )
			{
				// End == Start == "Start"
				if( DiffEndComboBox.SelectedIndex == 0 )
				{
					// "Start" is a pseudo snapshot so there is nothing to compare here.
					CurrentSnapshot = null;
				}
				else
				{
					// Comparing to "Start" means we can simply use the current index (-1 as Start doesn't have index associated).
					CurrentSnapshot = FStreamInfo.GlobalInstance.SnapshotList[ DiffEndComboBox.SelectedIndex - 1 ];
				}
			}
			// Start != "Start"
			else
			{
				// End == "Start"
				if( DiffEndComboBox.SelectedIndex == 0 )
				{
					// Comparing to "Start" means we can simply use the current index (-1 as Start doesn't have index associated).
					CurrentSnapshot = FStreamInfo.GlobalInstance.SnapshotList[ DiffStartComboBox.SelectedIndex - 1 ];
				}
				// Start != End and both are != "Start"
				else
				{
					// Do a real diff in this case and create a new snapshot.
					CurrentSnapshot = FStreamSnapshot.DiffSnapshots( FStreamInfo.GlobalInstance.SnapshotList[ DiffStartComboBox.SelectedIndex - 1 ], FStreamInfo.GlobalInstance.SnapshotList[ DiffEndComboBox.SelectedIndex - 1 ] );
				}
			}

			UpdateDetailsTab();
		}

		/// <summary> Parses the data stream into the current view. </summary>
		private void ParseCurrentView()
		{
			// Update the current snapshot based on combo box settings.
			UpdateCurrentSnapshot();	

			// If valid, parse it into the selected tab view.
			if( CurrentSnapshot != null )
			{
				bool bShouldSortBySize = ( SortCriteriaSplitButton.Text == " Sort by Size" );

				List<FCallStackAllocationInfo> CallStackList = null;
				if( AllocationsTypeSplitButton.Text == " Active Allocations" )
				{
					CallStackList = CurrentSnapshot.ActiveCallStackList;
				}
				else
				{
					CallStackList = CurrentSnapshot.LifetimeCallStackList;
				}

				// make sure the SelectedMemoryPool isn't masking out anything for Non-PS3 profiles
				if (!MemoryPoolFilterButton.Enabled)
				{
					SelectedMemoryPool = IsFilteringIn() ? EMemoryPool.MEMPOOL_All : EMemoryPool.MEMPOOL_None;
				}

				// Parse snapshots into views.
				// updating the call graph view is WAY SLOW

				if( MainTabControl.SelectedTab == CallGraphTabPage )
				{
					FCallGraphTreeViewParser.ParseSnapshot( CallGraphTreeView, CallStackList, bShouldSortBySize, FilterTextBox.Text.ToUpperInvariant(), Options.InvertCallStacksInCallGraphView );
				}
				else if( MainTabControl.SelectedTab == ExclusiveTabPage )
				{
					FExclusiveListViewParser.ParseSnapshot( ExclusiveListView, CallStackList, bShouldSortBySize, FilterTextBox.Text.ToUpperInvariant() );
				}
				else if( MainTabControl.SelectedTab == TimeLineTabPage )
				{
					FTimeLineChartView.ParseSnapshot( TimeLineChart, CurrentSnapshot );
				}
				else if( MainTabControl.SelectedTab == HistogramTabPage )
				{
					FHistogramParser.ParseSnapshot( CallStackList, FilterTextBox.Text.ToUpperInvariant() );
				}
				else if( MainTabControl.SelectedTab == CallstackHistoryTabPage )
				{
					// Refresh tab with existing history callstacks, in case any options have changed.
					FCallStackHistoryView.RefreshCallStackHistoryGraph();
				}
				else if( MainTabControl.SelectedTab == MemoryBitmapTabPage )
				{
					FMemoryBitmapParser.ResetZoom();
				}
				else if( MainTabControl.SelectedTab == ShortLivedAllocationsTabPage )
				{
					FShortLivedAllocationView.ParseSnapshot( FilterTextBox.Text.ToUpperInvariant() );
				}

				ResetFilteringState();
			}
			else
			{
				// Clear the views to signal error.
				ResetViews();
			}

			UpdateStatus( "Displaying " + CurrentFilename );
		}

		public ISet<FAllocationTag> GetTagsFilter()
		{
			return CurrentState.AllocTags;
		}

		private void GoButton_Click( object sender, EventArgs e )
		{
			SetWaitCursor( true );

#if !DEBUG
            try
#endif // !DEBUG
			{
				ParseCurrentView();
			}
#if !DEBUG
			catch( Exception ex )
			{
				UpdateStatus( "Error updating view: " + ex.Message );

				Trace.WriteLine("-----------------------------------");
				Trace.WriteLine(ex.ToString());
				Trace.WriteLine("-----------------------------------");
			}
#endif // !DEBUG

			SetWaitCursor( false );
		}

		/// <summary> Resets combobox items and various views into data. </summary>
		private void ResetComboBoxAndViews()
		{
			// Reset combobox items.
			DiffStartComboBox.Items.Clear();
			DiffEndComboBox.Items.Clear();

			// Clear text filter.
			FilterTextBox.Text = "";

			// Reset views.
			ResetViews();
		}

		/// <summary> Resets the various views into the data. </summary>
		private void ResetViews()
		{
			// Clear callgraph tree.
			CallGraphTreeView.Nodes.Clear();
			FCallGraphTreeViewParser.ParentFunctionIndex = -1;
			ParentLabel.Text = "(not set)";

			// Clear exclusive list view.
			FExclusiveListViewParser.ClearView();

			// Clear timeline view.
			FTimeLineChartView.ClearView();

			// Clear callstack history view.
			FCallStackHistoryView.ClearView();

			// Clear histogram view.
			FHistogramParser.ClearView();

			// Clear memory bitmap view.
			FMemoryBitmapParser.ClearView();

			// Clear short lived allocation view.
			FShortLivedAllocationView.ClearView();
		}

		/// <summary> Parses the memory profiler snapshot file. </summary>
		private void ParseFile( string InCurrentFilename )
		{
			CurrentFilename = InCurrentFilename;

            Text = OriginalWindowTitle + " - " + CurrentFilename;

			MemoryPoolFilterButton.Enabled = false;

			// Only parse if we have a valid file.
			if( CurrentFilename != "" )
			{
				// Disable all menus.
				MainToolStrip.Enabled = false;
				MainTabControl.Enabled = false;

#if !DEBUG
				try
#endif // !DEBUG
				{
					if( FStreamInfo.GlobalInstance != null )
					{
						// clean up old stream info
						// this includes terminating the addr2line process, if there is one
						FStreamInfo.GlobalInstance.Shutdown();
					}

					// Create a new stream info from the opened file.
					FStreamInfo.GlobalInstance = new FStreamInfo( CurrentFilename, new ProfilingOptions() );

					CustomSnapshots.Sort();
					// Parse the token stream and metadata.

#if !DEBUG
					ProgressDialog.OnBeginBackgroundWork = delegate( BackgroundWorker BGWorker, DoWorkEventArgs EventArgs )
					{
						FStreamParser.Parse( this, BGWorker, null, CustomSnapshots, EventArgs );
					};

					if( ProgressDialog.ShowDialog() != DialogResult.OK )
					{
						UpdateStatus( "Failed to parse '" + CurrentFilename + "', due to '" + ProgressDialog.ExceptionResult + "'" );
						return;
					}

#else

					FStreamParser.Parse( this, ProgressDialog.SlowWork, null, CustomSnapshots, null );

#endif // !DEBUG

					// Add Start, End values and user generated snapshots in between.
					DiffStartComboBox.Items.Add( "Start" );
					DiffEndComboBox.Items.Add( "Start" );
					// Count-1 as End is implicit last snapshot in list.
					for( int SnapshotIndex = 0; SnapshotIndex < FStreamInfo.GlobalInstance.SnapshotList.Count - 1; SnapshotIndex++ )
					{
						string SnapshotDescription = "#" + SnapshotIndex;
						if( !String.IsNullOrEmpty( FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].Description ) )
						{
							SnapshotDescription += ": " + FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].Description;
						}
						DiffStartComboBox.Items.Add( SnapshotDescription );
						DiffEndComboBox.Items.Add( SnapshotDescription );
					}
					DiffStartComboBox.Items.Add( "End" );
					DiffEndComboBox.Items.Add( "End" );

					// Start defaults to "Start" and End defaults to "End" being selected.
					DiffStartComboBox.SelectedIndex = 0;
					DiffEndComboBox.SelectedIndex = DiffEndComboBox.Items.Count - 1;

					UpdateStatus( "Parsed " + CurrentFilename );

					// Re-enable all required menus
					MainToolStrip.Enabled = true;
					MainTabControl.Enabled = true;

					ResetFilteringState();
				}
#if !DEBUG
				catch( Exception ex )
				{
					UpdateStatus( "Failed to parse '" + CurrentFilename + "', due to '" + ex.Message + "'" );
					// Reset combobox items and various views into data
					ResetComboBoxAndViews();

					// Clean up global FStreamInfo so that we can test whether
					// FStreamInfo.GlobalInstance is null to see if there's a valid
					// mprof loaded.
					if( FStreamInfo.GlobalInstance != null )
					{
						FStreamInfo.GlobalInstance.Shutdown();
					}

					FStreamInfo.GlobalInstance = null;
				}
#endif // !DEBUG
			}
		}
#endregion

#region Open file in Visual Studio methods
		/*-----------------------------------------------------------------------------
			Open file in Visual Studio methods
		-----------------------------------------------------------------------------*/

		protected EnvDTE.DTE FindOrCreateDTEInstance( string ObjectName )
		{
			// Try to find an open copy, and if that fails, create one
			EnvDTE.DTE result;
			try
			{
				result = System.Runtime.InteropServices.Marshal.GetActiveObject( ObjectName ) as EnvDTE.DTE;
			}
			catch
			{
				System.Type t = System.Type.GetTypeFromProgID( ObjectName );
				result = System.Activator.CreateInstance( t ) as EnvDTE.DTE;
			}

			return result;
		}

		/// <summary> Opens specified file in the Visual Studio. </summary>
		/// <param name="Filename"> File we want to be opened in the Visual Studio. </param>
		/// <param name="LineNumber"> Linenumber we want that the Visual Studio will jump to. </param>
		protected void OpenFileInVisualStudio( string Filename, int LineNumber )
		{
			// Get a copy of VS running
			EnvDTE.DTE devenv = FindOrCreateDTEInstance( "VisualStudio.DTE" );
			if( devenv == null )
			{
				// Couldn't get VS running at all!
				return;
			}

			// Make sure the window is visible and prevent it from disappearing
			devenv.MainWindow.Visible = true;
			devenv.UserControl = true;

			// Open the file
			Window win = devenv.ItemOperations.OpenFile( Filename, EnvDTE.Constants.vsViewKindTextView );

			// Go to the desired line number
			if( LineNumber >= 0 )
			{
				TextSelection selection = devenv.ActiveDocument.Selection as TextSelection;
				selection.GotoLine( LineNumber, true );
			}
		}

		// 0 == deepest part of callstack
		void OpenCallstackEntryInVS( FCallStackAllocationInfo AllocationInfo, int Index )
		{
			FCallStack CallStack = FStreamInfo.GlobalInstance.CallStackArray[ AllocationInfo.CallStackIndex ];
			FCallStackAddress Address = FStreamInfo.GlobalInstance.CallStackAddressArray[ CallStack.AddressIndices[ CallStack.AddressIndices.Count - 1 - Index ] ];
			string Filename = FStreamInfo.GlobalInstance.NameArray[ Address.FilenameIndex ];

			try
			{
				OpenFileInVisualStudio( Filename, Address.LineNumber );
			}
			catch( Exception )
			{
			}
		}
		#endregion

#region Main toolstrip, menu, tab etc events 

		/*-----------------------------------------------------------------------------
			Main toolstrip, menu, tab etc events
		-----------------------------------------------------------------------------*/

		/// <summary> Occurs when a tab is selected. </summary>
		private void MainTabControl_Selected( object sender, TabControlEventArgs e )
		{
			SortCriteriaSplitButton.Enabled = e.TabPage == CallGraphTabPage || e.TabPage == ExclusiveTabPage;
			AllocationsTypeSplitButton.Enabled = e.TabPage == CallGraphTabPage || e.TabPage == ExclusiveTabPage || e.TabPage == HistogramTabPage;
			ContainersSplitButton.Enabled = e.TabPage == ExclusiveTabPage;

			FilterTypeSplitButton.Enabled = e.TabPage == TimeLineTabPage ? false : true;
			FilterClassesDropDownButton.Enabled = e.TabPage == TimeLineTabPage ? false : true;
			FilterTagsButton.Enabled = e.TabPage == CallGraphTabPage || e.TabPage == ExclusiveTabPage || e.TabPage == HistogramTabPage;
			FilterTypeSplitButton.Enabled = e.TabPage == TimeLineTabPage ? false : true;

			FilterLabel.Enabled = e.TabPage == TimeLineTabPage ? false : true;
			FilterTextBox.Enabled = e.TabPage == TimeLineTabPage ? false : true;

			if( e.TabPage != ExclusiveTabPage )
			{
				SelectionSizeStatusLabel.Text = "";
			}

			if( e.TabPage == DetailsTabPage )
			{
				UpdateDetailsTab();
			}
		}

		private void QuitMenuItem_Click( object sender, EventArgs e )
		{
			Application.Exit();
		}

		/// <summary> Occurs when a key is pressed in "Text filter" text box. </summary>
		private void FilterTextBox_KeyPress( object sender, KeyPressEventArgs e )
		{
			if( e.KeyChar == ( char )Keys.Enter )
			{
				GoButton_Click( sender, new EventArgs() );
				e.Handled = true;
			}
		}

		private void FilterObjectVMFunctionsMenuItem_CheckedChanged( object sender, EventArgs e )
		{
			Options.FilterOutObjectVMFunctions = FilterObjectVMFunctionsMenuItem.Checked;
		}

		/// <summary> Processes a command key. </summary>
		protected override bool ProcessCmdKey( ref Message Msg, Keys KeyData )
		{
			if( MainTabControl.SelectedTab == HistogramTabPage && FHistogramParser.ProcessKeys( KeyData ) )
			{
				return true;
			}

			return base.ProcessCmdKey( ref Msg, KeyData );
		}

		/// <summary> Creates a file open dialog for selecting the .mprof file. </summary>
		private void OpenToolStripMenuItem_Click( object sender, EventArgs e )
		{
			OpenFileDialog OpenMProfFileDialog = new OpenFileDialog();
			OpenMProfFileDialog.Title = "Open the profile data file from the game's 'Profiling' folder";
			OpenMProfFileDialog.Filter = "Profiling Data (*.mprof)|*.mprof";
			OpenMProfFileDialog.RestoreDirectory = false;
			OpenMProfFileDialog.SupportMultiDottedExtensions = true;
			if( OpenMProfFileDialog.ShowDialog() == DialogResult.OK )
			{
				// Reset combobox items and various views into data
				ResetComboBoxAndViews();

				ParseFile( OpenMProfFileDialog.FileName );
			}
		}

		/// <summary> Brings up dialog for user to pick filename to export data to. </summary>
		private void ExportToCSVToolStripMenuItem_Click( object sender, EventArgs e )
		{
			try
			{
				SaveFileDialog ExportToCSVFileDialog = new SaveFileDialog();
				ExportToCSVFileDialog.Title = "Export memory profiling data to a .csv file";
				ExportToCSVFileDialog.Filter = "CallGraph in CSV (*.csv)|*.csv";
				ExportToCSVFileDialog.RestoreDirectory = false;
				ExportToCSVFileDialog.SupportMultiDottedExtensions = false;
				if( ExportToCSVFileDialog.ShowDialog() == DialogResult.OK )
				{
					// Determine whether to export lifetime or active allocations.
					bool bShouldExportActiveAllocations = ( AllocationsTypeSplitButton.Text == " Active Allocations" );

					// Export call graph from current snapshot to CSV.
					CurrentSnapshot.ExportToCSV( ExportToCSVFileDialog.FileName, bShouldExportActiveAllocations );
				}
			}
			catch( Exception ex )
			{
				MessageBox.Show( ex.Message, "Failed to export to CSV", MessageBoxButtons.OK, MessageBoxIcon.Error );
			}
		}

		/// <summary> Opens an UDN Memory Profiler page. </summary>
		private void QuickStartMenuClick( object sender, EventArgs e )
		{
			//MessageBox.Show( "Define USE_MALLOC_PROFILER 1 in UMemoryDefines.h\n\nWhile running, type SNAPSHOTMEMORY at suitable intervals. At the end of profiling, type DUMPALLOCSTOFILE to save the .mprof file.\n\nLoad in the .mprof file from <Name>Game/Profiling/<timestamp>/<Name>-<timestamp>.mprof", "Memory Profiler Quick Start", MessageBoxButtons.OK, MessageBoxIcon.Information );
			System.Diagnostics.Process.Start( "https://udn.unrealengine.com/docs/ue3/INT/Engine/Subsystems/Memory/Profiling/Tools/Profiler/index.html" );
		}

		/// <summary> Opens the options dialog. </summary>
		private void OptionsToolStripMenuItem_Click( object sender, EventArgs e )
		{
			OptionsDialog DisplayOptions = new OptionsDialog( this, Options );
			DisplayOptions.ShowDialog();

			PopulateClassGroups();
			ResetFilteringState();
		}

		/// <summary> Handles clicking on the one of the split buttons ( sort, allocations, containers and filter ). </summary>
		private void SplitButtonClick( object sender, EventArgs e )
		{
			ToolStripSplitButton Button = ( ToolStripSplitButton )sender;
			if( Button.DropDownItems.Count > 1 )
			{
				// Update the button to the next entry in the split button drop down item collection
				bool bFoundEntry = false;
				foreach( ToolStripMenuItem MenuItem in Button.DropDownItems )
				{
					if( bFoundEntry )
					{
						Button.Text = MenuItem.Text;
						bFoundEntry = false;
					}
					else if( Button.Text == MenuItem.Text )
					{
						bFoundEntry = true;
					}
				}

				// Handle case of looping around to the beginning
				if( bFoundEntry )
				{
					Button.Text = Button.DropDownItems[ 0 ].Text;
				}
			}

			UpdateFilteringState();
		}

		/// <summary> Handles clicking on the one of the sort by menu items. </summary>
		private void SortByClick( object sender, EventArgs e )
		{
			ToolStripMenuItem Item = ( ToolStripMenuItem )sender;
			SortCriteriaSplitButton.Text = Item.Text;

			UpdateFilteringState();
		}

		/// <summary> Handles clicking on the one of the allocations type menu items. </summary>
		private void AllocationsTypeClick( object sender, EventArgs e )
		{
			ToolStripMenuItem Item = ( ToolStripMenuItem )sender;
			AllocationsTypeSplitButton.Text = Item.Text;

			UpdateFilteringState();
		}

		/// <summary> Handles clicking on the one of the containers visibility menu items. </summary>
		private void ContainersSplitClick( object sender, EventArgs e )
		{
			ToolStripMenuItem Item = ( ToolStripMenuItem )sender;
			ContainersSplitButton.Text = Item.Text;

			UpdateFilteringState();
		}

		/// <summary> Handles clicking on the one of the filtering type menu items. </summary>
		private void FilterTypeClick( object sender, EventArgs e )
		{
			ToolStripMenuItem Item = ( ToolStripMenuItem )sender;
			FilterTypeSplitButton.Text = Item.Text;

			UpdateFilteringState();
		}

		/// <summary> Changes the check state of all class groups. </summary>
		/// <param name="bState"> Boolean check state that we want to set </param>
		private void SetAllClasses( bool bState )
		{
			foreach( ToolStripItem Item in FilterClassesDropDownButton.DropDownItems )
			{
				ToolStripMenuItem MenuItem = Item as ToolStripMenuItem;
				if( MenuItem != null && Item.Tag is ClassGroup )
				{
					ClassGroup FilterClass = ( ClassGroup )Item.Tag;
					if( FilterClass != null )
					{
						MenuItem.Checked = bState;
						FilterClass.bFilter = bState;
					}
				}
			}
		}

		/// <summary> Checks all class group in the class filter menu. </summary>
		private void FilterAllClassGroupsClick( object sender, EventArgs e )
		{
			SetAllClasses( true );
			FilterClassesDropDownButton.ShowDropDown();

			UpdateFilteringState();
		}

		/// <summary> Unchecks all class group in the class filter menu. </summary>
		private void FilterNoneClassGroupsClick( object sender, EventArgs e )
		{
			SetAllClasses( false );
			FilterClassesDropDownButton.ShowDropDown();

			UpdateFilteringState();
		}

		/// <summary> Handles clicking on the one of the class groups. </summary>
		private void ClassesToFilterMouseUp( object sender, MouseEventArgs e )
		{
			ToolStripMenuItem Item = ( ToolStripMenuItem )sender;
			if( Item.Tag is ClassGroup )
			{
				ClassGroup FilterClass = ( ClassGroup )Item.Tag;
				if( e.Button == MouseButtons.Left )
				{
					FilterClass.bFilter = Item.Checked;
				}
				else if( e.Button == MouseButtons.Right )
				{
					ViewHistoryContextMenu.Tag = FilterClass;
					ViewHistoryContextMenu.Show( Item.Owner, new Point( e.Location.X + Item.Bounds.Location.X, e.Location.Y + Item.Bounds.Location.Y ) );
				}
			}
			else if( Item.Tag is string )
			{
				if( e.Button == MouseButtons.Right )
				{
					ViewHistoryContextMenu.Tag = Item.Tag;
					ViewHistoryContextMenu.Show( Item.Owner, new Point( e.Location.X + Item.Bounds.Location.X, e.Location.Y + Item.Bounds.Location.Y ) );
				}
			}

			if( e.Button == MouseButtons.Left )
			{
				// Make the drop down list not go away when clicked (DismissWhenClicked is unsettable)
				FilterClassesDropDownButton.ShowDropDown();
			}

			UpdateFilteringState();
		}
#endregion

#region XML options methods 

		/*-----------------------------------------------------------------------------
			XML options methods
		-----------------------------------------------------------------------------*/

		protected void XmlSerializer_UnknownAttribute( object sender, XmlAttributeEventArgs e )
		{
		}

		protected void XmlSerializer_UnknownNode( object sender, XmlNodeEventArgs e )
		{
		}

#endregion

#region Timeline chart panel events 

		/*-----------------------------------------------------------------------------
			Timeline chart panel events
		-----------------------------------------------------------------------------*/

		private void TimeLineChart_SelectionRangeChanged( object sender, System.Windows.Forms.DataVisualization.Charting.CursorEventArgs Event )
		{
			if( Event.Axis.AxisName == System.Windows.Forms.DataVisualization.Charting.AxisName.X )
			{
				// Disregard event when zooming or invalid (NaN) selection.
				if( Event.NewSelectionStart != Event.NewSelectionEnd )
				{
				}
				// Single frame is selected, parse it!
				else
				{
					int SelectedFrame = ( int )( Event.NewSelectionStart + ( GetStartingAllocationCount() / 5000 ) );
					bool bAdded = FTimeLineChartView.AddCustomSnapshot( TimeLineChart, Event );
					if( bAdded )
					{
						MessageBox.Show( "Custom mark point added. Reload file for new blank mark point." );
					}
					
					if( !CustomSnapshots.Contains( SelectedFrame ) )
					{
						CustomSnapshots.Add( SelectedFrame );
					}
				}
			}
		}
#endregion

#region Memory pool menu events 

		/*-----------------------------------------------------------------------------
			Memory pool menu events
		-----------------------------------------------------------------------------*/

		/// <summary> Updates memory pool filter menu items based on the selected memory pool. </summary>
		private void UpdatePoolFilterFromSelectedPool()
		{
			PoolMainItem.Checked = ( SelectedMemoryPool & EMemoryPool.MEMPOOL_Main ) != EMemoryPool.MEMPOOL_None;
			PoolLocalItem.Checked = ( SelectedMemoryPool & EMemoryPool.MEMPOOL_Local ) != EMemoryPool.MEMPOOL_None;
			PoolHostDefaultItem.Checked = ( SelectedMemoryPool & EMemoryPool.MEMPOOL_HostDefault ) != EMemoryPool.MEMPOOL_None;
			PoolHostMoviesItem.Checked = ( SelectedMemoryPool & EMemoryPool.MEMPOOL_HostMovies ) != EMemoryPool.MEMPOOL_None;
		}

		/// <summary> Updates the selected memory pool based on memory pool filter menu items. </summary>
		private void RefreshSelectedMemoryPool()
		{
			if (MemoryPoolFilterButton.Enabled)
			{
				SelectedMemoryPool = EMemoryPool.MEMPOOL_None;

				if (PoolMainItem.Checked)
				{
					SelectedMemoryPool |= EMemoryPool.MEMPOOL_Main;
				}

				if (PoolLocalItem.Checked)
				{
					SelectedMemoryPool |= EMemoryPool.MEMPOOL_Local;
				}

				if (PoolHostDefaultItem.Checked)
				{
					SelectedMemoryPool |= EMemoryPool.MEMPOOL_HostDefault;
				}

				if (PoolHostMoviesItem.Checked)
				{
					SelectedMemoryPool |= EMemoryPool.MEMPOOL_HostMovies;
				}
			}
			else
			{
				SelectedMemoryPool = EMemoryPool.MEMPOOL_All;
			}

			Options.MemoryPoolFilterState = ( uint )SelectedMemoryPool;

			UpdateFilteringState();
		}

		/// <summary> Handles clicking on the one of the memory pools. </summary>
		private void MemoryPoolFilterClick( object sender, EventArgs e )
		{
			RefreshSelectedMemoryPool();
		}

		/// <summary> Unchecks all memory pools in the memory pool filter menu. </summary>
		private void PoolAllItem_Click( object sender, EventArgs e )
		{
			PoolMainItem.Checked = true;
			PoolLocalItem.Checked = true;
			PoolHostDefaultItem.Checked = true;
			PoolHostMoviesItem.Checked = true;

			RefreshSelectedMemoryPool();
		}

		/// <summary> Unchecks all memory pools in the memory pool filter menu. </summary>
		private void PoolNoneItem_Click( object sender, EventArgs e )
		{
			PoolMainItem.Checked = false;
			PoolLocalItem.Checked = false;
			PoolHostDefaultItem.Checked = false;
			PoolHostMoviesItem.Checked = false;

			RefreshSelectedMemoryPool();
		}

		/// <summary> Prevents from hiding the drop down list after selecting one of the menu items. </summary>
		private void PoolFilterButtonItem_MouseUp( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Left )
			{
				// Make the drop down list not go away when clicked (DismissWhenClicked is unsettable)
				MemoryPoolFilterButton.ShowDropDown();
			}
		}
#endregion

#region Callstack history view helper functions 

		/*-----------------------------------------------------------------------------
			Callstack history view helper functions
		-----------------------------------------------------------------------------*/

		/// <summary> Set callstacks for the callstack history view based on selected histogram bar and starts processing. </summary>
		public void SetHistoryCallStacks( FHistogramBar Bar )
		{
			// Collect callstack indices in a sorted list to make it easier
			// to filter out duplicate callstacks.
			List<int> CallstackIndices = new List<int>();
			foreach( FCallStackAllocationInfo AllocInfo in Bar.CallStackList )
			{
				bool bDone = false;
				for( int Index = 0; Index < CallstackIndices.Count; Index++ )
				{
					if( CallstackIndices[ Index ] == AllocInfo.CallStackIndex )
					{
						// callstack is already in the list
						bDone = true;
						break;
					}
					else if( CallstackIndices[ Index ] > AllocInfo.CallStackIndex )
					{
						CallstackIndices.Insert( Index, AllocInfo.CallStackIndex );
						bDone = true;
						break;
					}
				}

				if( !bDone )
				{
					CallstackIndices.Add( AllocInfo.CallStackIndex );
				}
			}

			List<FCallStack> Callstacks = new List<FCallStack>( CallstackIndices.Count );
			foreach( int Index in CallstackIndices )
			{
				Callstacks.Add( FStreamInfo.GlobalInstance.CallStackArray[ Index ] );
			}

			SetHistoryCallStacks( Callstacks );
		}

		/// <summary> Set callstacks for the callstack history view based on selected class group and starts processing. </summary>
		public void SetHistoryCallStacks( ClassGroup InClassGroup )
		{
			// No callstack can be in multiple patterns, so there is no
			// need to check for duplicates here.
			List<FCallStack> Callstacks = new List<FCallStack>();
			foreach( CallStackPattern Pattern in InClassGroup.CallStackPatterns )
			{
				Callstacks.AddRange( Pattern.GetCallStacks() );
			}

			SetHistoryCallStacks( Callstacks );
		}

		/// <summary> Set callstacks for the callstack history view based on selected callstacks in the callgrahp view and starts processing. </summary>
		public void SetHistoryCallStacks( List<FCallStack> CallStacks )
		{
			if( !FStreamInfo.GlobalInstance.CreationOptions.GenerateSizeGraphsCheckBox.Checked )
			{
				return;
			}

			FCallStackHistoryView.SetHistoryCallstacks( CallStacks );
			MainTabControl.SelectedTab = CallstackHistoryTabPage;
		}

		/// <summary> Handles mouse click on the view history menu. </summary>
		private void ViewHistoryMenuItem_Click( object Sender, EventArgs e )
		{
			ToolStripMenuItem MenuItem = ( ( ToolStripMenuItem )Sender );
			object Tag = MenuItem.Owner.Tag;
			if( Tag is ListView )
			{
				SetHistoryCallStacks( GetSelectedCallStacks( ( ListView )Tag ) );
			}
			else if( Tag is FHistogramBar )
			{
				SetHistoryCallStacks( ( FHistogramBar )Tag );
			}
			else if( Tag is ClassGroup )
			{
				SetHistoryCallStacks( ( ClassGroup )Tag );
			}
			else if( Tag is string )
			{
				string TagString = ( string )Tag;

				if( TagString == AllHistoryTag )
				{
					DialogResult DlgResult = MessageBox.Show
									(
										"Displaying callstack history for all callstacks may take an hour or more.\nDo You want to continue?",
										"Show history for all callstacks?",
										MessageBoxButtons.OKCancel
									);

					if( DlgResult == DialogResult.OK )
					{
						SetHistoryCallStacks( FStreamInfo.GlobalInstance.CallStackArray );
					}
				}
			}
			else
			{
				throw new ArgumentException( "ViewHistoryContextMenu.Tag was not set to a supported object type" );
			}
		}	

		/// <summary> Gets callstack list from selection done in the listview. </summary>
		private static List<FCallStack> GetSelectedCallStacks( ListView InListView )
		{
			List<FCallStack> CallStacks = new List<FCallStack>( InListView.SelectedItems.Count );
			foreach( ListViewItem Item in InListView.SelectedItems )
			{
				FCallStack CallStack = null;
				if( Item.Tag is FCallStackAllocationInfo )
				{
					CallStack = FStreamInfo.GlobalInstance.CallStackArray[ ( ( FCallStackAllocationInfo )Item.Tag ).CallStackIndex ];
				}
				else if( Item.Tag is FCallStack )
				{
					CallStack = ( FCallStack )Item.Tag;
				}
				else if( Item.Tag is FShortLivedCallStackTag )
				{
					CallStack = ( ( FShortLivedCallStackTag )Item.Tag ).CallStack;
				}
				else if( Item.Tag is FCallStackTag )
				{
					CallStack = ( ( FCallStackTag )Item.Tag ).CallStack;
				}
				else
				{
					Debug.Assert( false, "Unsupported ListView tag type" );
				}

				CallStacks.Add( CallStack );
			}

			return CallStacks;
		}
#endregion
#region Drawing functions 


		/*-----------------------------------------------------------------------------
			Drawing functions
		-----------------------------------------------------------------------------*/

		/// <summary> Format a value into human readable string ended with "bytes", "KB" or "MB" based on size. </summary>
		/// <param name="Size"> Value that we want to be converted into a string. </param>
		public static string FormatSizeString( long Size )
		{
			string[] Units = { "bytes", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };

			int UnitIndex = 0;
			double AbsSize = Math.Abs(Size);
			while (AbsSize >= 1024)
			{
				AbsSize /= 1024;
				++UnitIndex;
			}

			return String.Format("{0} {1}", AbsSize.ToString("N2"), Units[UnitIndex]);
		}

		/// <summary> Format a value into human readable string ended ex. "5.50 MB (5767168 bytes). </summary>
		/// <param name="Size"> Value that we want to be converted into a string. </param>
		public static string FormatSizeString2( long Size )
		{
			return String.Format("{0} ({1} bytes)", FormatSizeString(Size), Size.ToString("N0"));
		}

		/// <summary> Setups string label for an axis based on tick. </summary>
		/// <param name="TickMultiplier">  </param>
		/// <param name="AxisHeightInBytes">  </param>
		/// <param name="AxisLabel"> Human readable value for size label. </param>
		/// <param name="MajorTick">  </param>
		public static void SetUpAxisLabel( int TickMultiplier, ref float AxisHeightInBytes, out string AxisLabel, out long MajorTick )
		{
			long MajorTicks = 16 * Math.Max( TickMultiplier, 1 );

			AxisLabel = "bytes";
			MajorTick = ( long )AxisHeightInBytes / MajorTicks;

			if( MajorTick > 4 * 1024 * 1024 )
			{
				AxisHeightInBytes /= 1024 * 1024;
				MajorTick /= 1024 * 1024;
				AxisLabel = "MB";
			}
			else if( MajorTick > 4 * 1024 )
			{
				AxisHeightInBytes /= 1024;
				MajorTick /= 1024;
				AxisLabel = "KB";
			}
		}

		public void DrawYAxis( Graphics InGraphics, Pen InPen, Color Color, long MajorTick, long MinorTick, float Right, float Top, float PixelHeight, string AxisLabel, float AxisHeight )
		{
			DrawYAxis( InGraphics, InPen, Color, MajorTick, DEFAULT_MAJOR_TICK_WIDTH, MinorTick, DEFAULT_MINOR_TICK_WIDTH, Right, Top, PixelHeight, 0, PixelHeight, AxisLabel, AxisHeight, false, false );
		}

		public void DrawYAxis
		( 
			Graphics InGraphics, Pen Pen, Color Color, long MajorTick, int MajorTickWidth, long MinorTick, int MinorTickWidth, 
			float Right, float Top, float WindowHeight, float WindowOffset, float PixelHeight, 
			string AxisLabel, float AxisHeight, bool InvertAxis, bool AxisLabelAtOrigin
		)
		{
			DrawYAxis( InGraphics, Pen, MajorTick, MajorTickWidth, MinorTick, MinorTickWidth, Right, Top, WindowHeight, WindowOffset, PixelHeight, AxisLabel, AxisHeight, InvertAxis, AxisLabelAtOrigin, AxisFont, Color);
		}

		public static void DrawYAxis
		(
			Graphics InGraphics, Pen InPen, long MajorTick, int MajorTickWidth, long MinorTick, int MinorTickWidth,
			float Right, float Top, float WindowHeight, float WindowOffset, float PixelHeight,
			string AxisLabel, float AxisHeight, bool InvertAxis, bool AxisLabelAtOrigin, Font AxisFont, Color AxisFontColor
		)
		{
			const int AxisLabelXOffset = 5;

			float YScale = PixelHeight / AxisHeight;

			SizeF LabelSize = InGraphics.MeasureString( AxisLabel, AxisFont );

			float AxisLabelX, AxisLabelY;
			if( AxisLabelAtOrigin )
			{
				AxisLabelX = Right - MajorTickWidth - AxisLabelXOffset - LabelSize.Width;
				AxisLabelY = Top;
			}
			else
			{
				AxisLabelX = Right - ( MajorTickWidth + LabelSize.Width ) / 2;
				AxisLabelY = Top - LabelSize.Height - 10;
			}

			TextRenderer.DrawText(InGraphics, AxisLabel, AxisFont, new Point((int)AxisLabelX, (int)AxisLabelY), AxisFontColor);

			for( long j = AxisLabelAtOrigin ? MinorTick : 0; j <= AxisHeight; j += MinorTick )
			{
				int TickY;
				if( InvertAxis )
				{
					TickY = ( int )( Top + YScale * j - WindowOffset );
				}
				else
				{
					TickY = ( int )( Top + YScale * ( AxisHeight - j ) - WindowOffset );
				}

				if( TickY >= Top && TickY < Top + WindowHeight + 1 )
				{
					if( j % MajorTick == 0 )
					{
						string Label = j.ToString();
						SizeF StringSize = InGraphics.MeasureString( Label, AxisFont );

						InGraphics.DrawLine( InPen, Right - MajorTickWidth, TickY, Right, TickY );
						TextRenderer.DrawText(InGraphics, Label, AxisFont, new Point((int)(Right - MajorTickWidth - AxisLabelXOffset - StringSize.Width), (int)(TickY - StringSize.Height / 2)), AxisFontColor);
					}
					else
					{
						InGraphics.DrawLine( InPen, Right - MinorTickWidth, TickY, Right, TickY );
					}
				}
			}
		}
#endregion

#region Histogram panel events 

		/*-----------------------------------------------------------------------------
			Histogram panel events
		-----------------------------------------------------------------------------*/

		private void HistogramPanel_Paint( object sender, PaintEventArgs e )
		{
			if( FStreamInfo.GlobalInstance == null )
			{
				return;
			}

#if !DEBUG
			try
#endif // !DEBUG
			{
				FHistogramParser.PaintPanel( e );
			}
#if !DEBUG
			catch( Exception ex )
			{
				Console.WriteLine( "Failed to repaint histogram " + ex.Message );
			}
#endif // !DEBUG
		}

		private void HistogramPanel_MouseClick( object sender, MouseEventArgs e )
		{
			if( FHistogramParser.HistogramBars == null )
			{
				return;
			}

			FHistogramParser.UnsafeMouseClick( sender, e );
		}
#endregion

#region Memory bitmap panel events 

		/*-----------------------------------------------------------------------------
			Memory bitmap panel events
		-----------------------------------------------------------------------------*/

		private void MemoryBitmap_AllocationHistoryListView_SelectedIndexChanged( object sender, EventArgs e )
		{
			FMemoryBitmapParser.UpdateAllocationHistory();
		}

		private void MemoryBitmapPanel_ResetButton_Click( object sender, EventArgs e )
		{
			FMemoryBitmapParser.ResetZoom();
		}

		private void MemoryBitmapPanel_ResetSelection_Click( object sender, EventArgs e )
		{
			FMemoryBitmapParser.ResetSelection();
		}

		private void MemoryBitmapPanel_MouseClick( object sender, MouseEventArgs e )
		{
			FMemoryBitmapParser.BitmapClick( e );
		}

		private void MemoryBitmap_UndoZoomButton_Click( object sender, EventArgs e )
		{
			FMemoryBitmapParser.UndoLastZoom();
		}

		private void MemoryBitmapPanel_MouseDown( object sender, MouseEventArgs e )
		{
			FMemoryBitmapParser.BitmapMouseDown( e );
		}

		private void MemoryBitmapPanel_MouseMove( object sender, MouseEventArgs e )
		{
			FMemoryBitmapParser.BitmapMouseMove( e );
		}

		private void MemoryBitmapPanel_MouseUp( object sender, MouseEventArgs e )
		{
			FMemoryBitmapParser.BitmapMouseUp( e );
		}

		private void MemoryBitmapPanel_Paint( object sender, PaintEventArgs e )
		{
			FMemoryBitmapParser.PaintPanel( e );
		}

		private void MemoryBitmapPanel_Resize( object sender, EventArgs e )
		{
			Debug.WriteLine( "MemoryBitmapPanel_Resize" );
			if( !bInResizingMode )
			{
				FMemoryBitmapParser.RefreshMemoryBitmap();
			}
		}

		private void MemoryBitmap_AllocationHistoryListView_MouseClick( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Right )
			{
				ViewHistoryContextMenu.Tag = sender;
				ViewHistoryContextMenu.Show( ( ListView )sender, e.Location );
			}
		}	

		private void MemoryBitmap_HeatMapButton_CheckedChanged( object sender, EventArgs e )
		{
			if( MemoryBitmapHeatMapButton.Checked )
			{
				MemoryBitmapHeatMapButton.Text = "Show Memory Map";
				MemoryBitmapHeatMapButton.ToolTipText = "'Show Memory Map' shows memory bitmap. Uncheck to enable.";
			}
			else
			{
				MemoryBitmapHeatMapButton.Text = "Show Heat Map";
				MemoryBitmapHeatMapButton.ToolTipText = "'Show Heat Map' shows how \"hot\" each address is the more allocations that have inhabited a given address, the whiter it will be. Check to enable.";
			}

			FMemoryBitmapParser.RefreshMemoryBitmap();
		}

		/// <summary> If True we are in the resizing mode so don't update bitmap panel to improve performance and usability. </summary>
		bool bInResizingMode = false;

		/// <summary> Last size of the main form used to detect when we really changed the size of the windows in the resizing mode. </summary>
		Size LastResizedSize = new Size();

		/// <summary> Occurs when a form enters resizing mode. </summary>
		private void MainWindow_ResizeBegin( object sender, EventArgs e )
		{
			bInResizingMode = true;	
		}

		/// <summary> Occurs when a form exits resizing mode. </summary>
		private void MainWindow_ResizeEnd( object sender, EventArgs e )
		{
			Debug.WriteLine( "MainWindow_ResizeEnd" );
			bInResizingMode = false;

			// Resize memory bitmap panel after resizing has finished.
			if( MainTabControl.SelectedTab == MemoryBitmapTabPage && Size != LastResizedSize )
			{
				MemoryBitmapPanel_Resize( sender, e );
			}

			LastResizedSize = Size;
		}
#endregion

#region Callgraph view events 

		/*-----------------------------------------------------------------------------
			Callgraph view events
		-----------------------------------------------------------------------------*/

		private void CallGraphTreeView_NodeMouseClick( object sender, TreeNodeMouseClickEventArgs EventArgs )
		{
			// Preserve eventargs with clicked node, etc.
			CallGraphContextMenu.Tag = EventArgs;
			if( EventArgs.Button == MouseButtons.Right )
			{
				CallGraphTreeView.SelectedNode = EventArgs.Node;
				CallGraphContextMenu.Show( CallGraphTreeView, EventArgs.Location );
			}
		}

		private void InvertCallgraphViewToolStripMenuItem_CheckedChanged( object sender, EventArgs e )
		{
			Options.InvertCallStacksInCallGraphView = InvertCallgraphViewMenuItem.Checked;
		}

		/// <summary> The search text. </summary>
		private string LastSearchText;

		/// <summary> Tree node that will be selected after the first element has been found. </summary>
		private TreeNode NodeToSelect;

		/// <summary> Whether wa want to use Regex in the search. </summary>
		private bool bSearchTextIsRegex;

		/// <summary> Whether we want to match case in the search. </summary>
		private bool bSearchTextMatchCase;

		private void SearchCallGraphView( string SearchText )
		{
			CallGraphTreeView.Select();

			if( SearchText == null )
			{
				FindDialog FindWindow = new FindDialog( LastSearchText, bSearchTextIsRegex, bSearchTextMatchCase );
				DialogResult Result = FindWindow.ShowDialog();
				if( Result == DialogResult.Cancel )
				{
					return;
				}

				SearchText = FindWindow.SearchTextBox.Text;

				bSearchTextIsRegex = FindWindow.RegexCheckButton.Checked;
				bSearchTextMatchCase = FindWindow.MatchCaseCheckButton.Checked;

				if( Result == DialogResult.Yes )
				{
					FCallGraphTreeViewParser.UnhighlightAll( CallGraphTreeView );

					TreeNode FirstResult;
					long AllocationSize;
					int AllocationCount;
					try
					{
						int NumResults = FCallGraphTreeViewParser.HighlightAllNodes( CallGraphTreeView, SearchText, bSearchTextIsRegex, bSearchTextMatchCase, out FirstResult, out AllocationSize, out AllocationCount );

						MessageBox.Show( NumResults + " results found\nTotal size: " + FormatSizeString( AllocationSize ) + "\nTotal count: " + AllocationCount, "Find all results" );
					}
					catch( ArgumentException )
					{
						// a message box is displayed when this exception is thrown,
						// so no need to inform the user of the error again
						FirstResult = null;
					}

					if( FirstResult != null )
					{
						SelectCallGraphViewNode( FirstResult );
					}

					LastSearchText = SearchText;
					return;
				}
			}

			LastSearchText = SearchText;

			try
			{
				TreeNode Node = FCallGraphTreeViewParser.FindNode( CallGraphTreeView, LastSearchText, bSearchTextIsRegex, bSearchTextMatchCase );

				if( Node != null )
				{
					SelectCallGraphViewNode( Node );
				}
				else
				{
					MessageBox.Show( "The search string was not found" );
				}
			}
			catch( ArgumentException )
			{
				// a message box is displayed when this exception is thrown,
				// so no need to inform the user of the error again
			}
		}

		private void SelectCallGraphViewNode( TreeNode Node )
		{
			// Unfortunately SelectedNode gets reset for some reason after searching
			// so we have to use a convoluted way of selecting the desired node later.
			NodeToSelect = Node;

			// Ensure that paint event is triggered for this tabpage and run the selection code there.
			CallGraphTabPage.Invalidate();
		}

		private void FindToolStripMenuItem_Click( object sender, EventArgs e )
		{
			if( MainTabControl.SelectedTab == CallGraphTabPage )
			{
				SearchCallGraphView( null );
			}
			else
			{
				MessageBox.Show( "Find is only implemented for the Callgraph View tab" );
			}

		}

		private void FindNextToolStripMenuItem_Click( object sender, EventArgs e )
		{
			if( MainTabControl.SelectedTab == CallGraphTabPage )
			{
				SearchCallGraphView( LastSearchText );
			}
			else
			{
				MessageBox.Show( "Find is only implemented for the Callgraph View tab" );
			}
		}

		private void UnhighlightAllToolStripMenuItem_Click( object sender, EventArgs e )
		{
			if( MainTabControl.SelectedTab == CallGraphTabPage )
			{
				FCallGraphTreeViewParser.UnhighlightAll( CallGraphTreeView );
			}
		}

		private void CopyHighlightedToClipboardToolStripMenuItem_Click( object sender, EventArgs e )
		{
			if( MainTabControl.SelectedTab == CallGraphTabPage )
			{
				List<string> StringArray = FCallGraphTreeViewParser.GetHighlightedNodesAsStrings( CallGraphTreeView );
				StringBuilder Result = new StringBuilder();
				foreach( string Line in StringArray )
				{
					Result.AppendLine( Line );
				}
				if( Result.Length > 0 )
				{
					Clipboard.SetText( Result.ToString() );
				}
			}
		}

		private void CopyFunctionToClipboardToolStripMenuItem_Click( object sender, EventArgs e )
		{
			TreeNodeMouseClickEventArgs ClickEventArgs = ( TreeNodeMouseClickEventArgs )CallGraphContextMenu.Tag;
			if( ClickEventArgs.Node.Tag != null )
			{
				Clipboard.SetText( FStreamInfo.GlobalInstance.NameArray[ FStreamInfo.GlobalInstance.CallStackAddressArray[ ( (FCallGraphNode)ClickEventArgs.Node.Tag ).AddressIndex ].FunctionIndex ] );
			}
		}

		private void CopySizeToClipboardToolStripMenuItem_Click( object sender, EventArgs e )
		{
			TreeNodeMouseClickEventArgs ClickEventArgs = ( TreeNodeMouseClickEventArgs )CallGraphContextMenu.Tag;
			if( ClickEventArgs.Node.Tag != null )
			{
				Clipboard.SetText( ( (FCallGraphNode)ClickEventArgs.Node.Tag ).AllocationSize.ToString() );
			}
		}

		private void CallGraphTabPage_Paint( object sender, PaintEventArgs e )
		{
			if( NodeToSelect != null )
			{
				CallGraphTreeView.SelectedNode = NodeToSelect;
				NodeToSelect = null;
			}
		}

		private void ViewHistoryToolStripMenuItem_Click( object sender, EventArgs e )
		{
			TreeNodeMouseClickEventArgs eventArgs = ( TreeNodeMouseClickEventArgs )CallGraphContextMenu.Tag;
			var Payload = (FCallGraphNode)eventArgs.Node.Tag;

			if( Payload == null )
			{
				// TODO: implement this!
				MessageBox.Show( "You can't view the history of the root node (not implemented yet). Please pick a child node." );
				return;
			}

			SetHistoryCallStacks( Payload.CallStacks );
		}

		private void ExpandAllCollapseAllToolStripMenuItem_Click( object sender, EventArgs e )
		{
			TreeNodeMouseClickEventArgs ClickEventArgs = ( TreeNodeMouseClickEventArgs )CallGraphContextMenu.Tag;

			// Expand/ collapse all nodes in subtree if double clicked on.
			if( ClickEventArgs.Node.IsExpanded )
			{
				ClickEventArgs.Node.Collapse( false );
			}
			// Expand subtree recursively... this might be slow.
			else
			{
				ClickEventArgs.Node.ExpandAll();
			}
		}

		private void SetParentToolStripMenuItem_Click( object sender, EventArgs e )
		{
			TreeNodeMouseClickEventArgs eventArgs = ( TreeNodeMouseClickEventArgs )CallGraphContextMenu.Tag;

			var Payload = (FCallGraphNode)eventArgs.Node.Tag;
			if( Payload != null )
			{
				FCallGraphTreeViewParser.ParentFunctionIndex = FStreamInfo.GlobalInstance.CallStackAddressArray[ Payload.AddressIndex ].FunctionIndex;
				ParentLabel.Text = FStreamInfo.GlobalInstance.NameArray[ FCallGraphTreeViewParser.ParentFunctionIndex ];
			}

			// Reparse with updated filter.
			GoButton_Click( sender, new EventArgs() );
			ResetParentMenuItem.Enabled = true;
		}

		private void ResetParentMenuItem_Click( object sender, EventArgs e )
		{
			FCallGraphTreeViewParser.ParentFunctionIndex = -1;
			ParentLabel.Text = "(not set)";

			// Reparse with updated filter.
			GoButton_Click( sender, new EventArgs() );
			ResetParentMenuItem.Enabled = false;
		}
#endregion

#region Short lived allocation panel events 

		/*-----------------------------------------------------------------------------
			Short lived allocation panel events
		-----------------------------------------------------------------------------*/

		private void ShortLivedListView_ColumnClick( object sender, ColumnClickEventArgs e )
		{
			FShortLivedAllocationView.ListColumnClick( e );
		}

		private void ShortLivedListView_MouseClick( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Right )
			{
				ViewHistoryContextMenu.Tag = sender;
				ViewHistoryContextMenu.Show( ( ListView )sender, e.Location );
			}
		}

		private void ShortLivedListView_SelectedIndexChanged( object sender, EventArgs e )
		{
			ShortLivedCallStackListView.BeginUpdate();
			ShortLivedCallStackListView.Items.Clear();

			// We can only display a single selected element.
			if( ShortLivedListView.SelectedItems.Count == 1 )
			{
				FCallStack CallStack = ( ( FShortLivedCallStackTag )ShortLivedListView.SelectedItems[ 0 ].Tag ).CallStack;
				CallStack.AddToListView( ShortLivedCallStackListView, true );

				// if sender is null this is being called from a callback, so no need to look up addresses
				if( sender != null && FStreamInfo.GlobalInstance.SymbolParser != null )
				{
					FStreamInfo.GlobalInstance.SymbolParser.ResolveCallstackSymbolInfoAsync(ESymbolResolutionMode.Full, CallStack, delegate(FCallStack CS)
					{
						BeginInvoke(new EventHandler(ShortLivedListView_SelectedIndexChanged), null, null);
					}, true);
				}
			}

			ShortLivedCallStackListView.EndUpdate();
		}
		#endregion

		#region Exclusive list view panel events 

		/*-----------------------------------------------------------------------------
			 Exclusive list view panel events
		-----------------------------------------------------------------------------*/

		/// <summary> Update 2nd list view with full graph of selected row. </summary>
		private void ExclusiveListView_SelectedIndexChanged( object sender, EventArgs e )
		{
			ExclusiveSingleCallStackView.BeginUpdate();
			ExclusiveSingleCallStackView.Items.Clear();

			// We can only display a single selected element.
			if( ExclusiveListView.SelectedItems.Count == 1 )
			{
				FCallStackAllocationInfo AllocationInfo = ( FCallStackAllocationInfo )ExclusiveListView.SelectedItems[ 0 ].Tag;
				FStreamInfo.GlobalInstance.CallStackArray[ AllocationInfo.CallStackIndex ].AddToListView( ExclusiveSingleCallStackView, true );

				// if sender is null this is being called from a callback, so no need to look up addresses
				if( sender != null && FStreamInfo.GlobalInstance.SymbolParser != null )
				{
					FCallStack CallStack = FStreamInfo.GlobalInstance.CallStackArray[AllocationInfo.CallStackIndex];
					FStreamInfo.GlobalInstance.SymbolParser.ResolveCallstackSymbolInfoAsync(ESymbolResolutionMode.Full, CallStack, delegate(FCallStack CS)
					{
						BeginInvoke(new EventHandler(ExclusiveListView_SelectedIndexChanged), null, null);
					}, true);
				}
			}

			ExclusiveSingleCallStackView.EndUpdate();

			// Sum up the size of all selected items
			int TotalSize = 0;
			int TotalCount = 0;
			foreach( ListViewItem Item in ExclusiveListView.SelectedItems )
			{
				int Size, Count;
				if( int.TryParse( Item.SubItems[ 0 ].Text, out Size ) && int.TryParse( Item.SubItems[ 2 ].Text, out Count ) )
				{
					TotalSize += Size;
					TotalCount += Count;
				}
			}

			if( ExclusiveListView.SelectedItems.Count > 0 )
			{
				string suffix = " KB in selection (in " + TotalCount.ToString() + " allocations)";
				SelectionSizeStatusLabel.Text = TotalSize.ToString( "N0" ) + suffix;
			}
			else
			{
				SelectionSizeStatusLabel.Text = "";
			}
		}

		private void ExclusiveListView_ColumnClick( object sender, ColumnClickEventArgs e )
		{
			ListViewEx MyListView = sender as ListViewEx;
			if( MyListView != null && MyListView.Items.Count > 0 )
			{
				FColumnSorter ColumnSorter = MyListView.ListViewItemSorter as FColumnSorter;
				if( ColumnSorter != null )
				{
					ColumnSorter.OnColumnClick( MyListView, e );
					MyListView.SetSortArrow( ColumnSorter.ColumnToSortBy, ColumnSorter.ColumnSortModeAscending );
				}
			}
		}

		private void ExclusiveListView_DoubleClick( object sender, EventArgs e )
		{
			if( ExclusiveListView.SelectedItems.Count > 0 )
			{
				FCallStackAllocationInfo AllocationInfo = ( FCallStackAllocationInfo )ExclusiveListView.SelectedItems[ 0 ].Tag;
				OpenCallstackEntryInVS( AllocationInfo, 0 );
			}
		}

		/// <summary> Class used to sort columns by specified column in the exclusive list view. </summary>
		public class FColumnSorter : IComparer
		{
			public int ColumnToSortBy = 0;
			public bool ColumnSortModeAscending = false;

			public IComparer ElementComparer = new CaseInsensitiveComparer();


			public int Compare( Object ObjectA, Object ObjectB )
			{
				ListViewItem ItemA = ( ListViewItem )ObjectA;
				ListViewItem ItemB = ( ListViewItem )ObjectB;

				string StringA = ItemA.SubItems[ ColumnToSortBy ].Text;
				string StringB = ItemB.SubItems[ ColumnToSortBy ].Text;

				long IntA;
				long IntB;
				if( long.TryParse( StringA, out IntA ) && long.TryParse( StringB, out IntB ) )
				{
					return ElementComparer.Compare( IntA, IntB ) * ( ColumnSortModeAscending ? 1 : -1 );
				}
				else
				{
					return ElementComparer.Compare( StringA, StringB ) * ( ColumnSortModeAscending ? 1 : -1 );
				}
			}

			public void OnColumnClick( ListView sender, ColumnClickEventArgs e )
			{
				if( e.Column == ColumnToSortBy )
				{
					ColumnSortModeAscending = !ColumnSortModeAscending;
				}
				else
				{
					ColumnToSortBy = e.Column;
					ColumnSortModeAscending = false;
				}

				sender.Sort();
			}
		}

#endregion

#region Callstack history view panel events 

		/*-----------------------------------------------------------------------------
			Callstack history view panel events
		-----------------------------------------------------------------------------*/

		private void CallstackHistoryAxisUnitFramesButton_Click( object sender, EventArgs e )
		{
			CallstackHistoryAxisUnitFramesButton.Checked = true;
			CallstackHistoryAxisUnitAllocationsButton.Checked = false;

			FCallStackHistoryView.RefreshCallStackHistoryGraph();
		}

		private void CallstackHistoryAxisUnitAllocationsButton_Click( object sender, EventArgs e )
		{
			CallstackHistoryAxisUnitAllocationsButton.Checked = true;
			CallstackHistoryAxisUnitFramesButton.Checked = false;

			FCallStackHistoryView.RefreshCallStackHistoryGraph();
		}

		private void CallStackHistoryPanel_Paint( object sender, PaintEventArgs e )
		{
			FCallStackHistoryView.PaintHistoryPanel( e );
		}

		private void CallStackHistoryPanel_Resize( object sender, EventArgs e )
		{
			FCallStackHistoryView.RefreshCallStackHistoryGraph();
		}

		private void ShowSnapshotsButton_CheckedChanged( object sender, EventArgs e )
		{
			FCallStackHistoryView.RefreshCallStackHistoryGraph();
		}

		private void CallstackHistoryResetZoomButton_Click( object sender, EventArgs e )
		{
			FCallStackHistoryView.ResetZoom();
		}

		private void ShowCompleteHistoryButton_CheckedChanged( object sender, EventArgs e )
		{
			FCallStackHistoryView.RefreshCallStackHistoryGraph();
		}

		private void CallStackHistoryVScroll_ValueChanged( object sender, EventArgs e )
		{
			CallStackHistoryPanel.Invalidate();
		}

		private void CallStackHistoryHScroll_ValueChanged( object sender, EventArgs e )
		{
			CallStackHistoryPanel.Invalidate();
		}

		/// <summary> The modifier flags for a KeyDown or KeyUp event used to update zoom factor in the callstack history view. </summary>
		Keys CallstackHistoryKeys = Keys.None;

		private void MainTabControl_MouseWheel( object sender, MouseEventArgs e )
		{
			if( MainTabControl.SelectedTab == CallstackHistoryTabPage )
			{
				if( (CallstackHistoryKeys & Keys.Control) != 0 )
				{
					FCallStackHistoryView.UpdateZoom( e.Delta > 0 ? 1 : -1, ref FCallStackHistoryView.HZoom );
				}

				if( ( CallstackHistoryKeys & Keys.Shift ) != 0 )
				{
					FCallStackHistoryView.UpdateZoom( e.Delta > 0 ? 1 : -1, ref FCallStackHistoryView.VZoom );
				}
			}
		}

		private void MainTabControl_KeyDown( object sender, KeyEventArgs e )
		{
			if( MainTabControl.SelectedTab == CallstackHistoryTabPage )
			{
				CallstackHistoryKeys = e.Modifiers;
			}
		}

		private void MainTabControl_KeyUp( object sender, KeyEventArgs e )
		{
			if( MainTabControl.SelectedTab == CallstackHistoryTabPage )
			{
				CallstackHistoryKeys = e.Modifiers;
			}
		}

		private void CallStackHistoryPanel_MouseMove( object sender, MouseEventArgs e )
		{
			CallStackHistoryPanel.Focus();
		}
#endregion

#region Generic copy to.. context menu support 

		/*-----------------------------------------------------------------------------
			Generic copy to.. context menu support
		-----------------------------------------------------------------------------*/

		private void ExclusiveListView_MouseClick( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Right )
			{
				ViewHistoryContextMenu.Tag = sender;
				ViewHistoryContextMenu.Show( ( ListView )sender, e.Location );
			}
		}

		/// <summary> Copies first selected listview item to the clipboard. </summary>
		private void CopyFunctionToClipboardMenuItem_Click( object sender, EventArgs e )
		{
			ListView MyListView = ( ListView )CallStackViewerContextMenu.Tag;
			if( MyListView.SelectedItems.Count > 0 )
			{
				Clipboard.SetText( MyListView.SelectedItems[ 0 ].Text );
			}
		}

		/// <summary> Copies whole callstack from the selected listview item to the clipboard. </summary>
		private void CopyCallstackToClipboardMenuItem_Click( object sender, EventArgs e )
		{
			ListView MyListView = ( ListView )CallStackViewerContextMenu.Tag;
			if( MyListView.Items.Count > 0 )
			{
				string SelectedCallstack = "";
				for( int ItemIndex = 0; ItemIndex < MyListView.Items.Count; ItemIndex++ )
				{
					SelectedCallstack += MyListView.Items[ ItemIndex ].Text;
					if( ItemIndex + 1 != MyListView.Items.Count )
					{
						SelectedCallstack += "\r\n";
					}
				}

				Clipboard.SetText( SelectedCallstack );
			}
		}

		private void ShortLivedCallStackListView_MouseClick( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Right )
			{
				CallStackViewerContextMenu.Tag = sender;
				CallStackViewerContextMenu.Show( ( ListView )sender, e.Location );
			}
		}

		private void ExclusiveSingleCallStackView_MouseClick( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Right )
			{
				CallStackViewerContextMenu.Tag = sender;
				CallStackViewerContextMenu.Show( ( ListView )sender, e.Location );
			}
		}

		private void MemoryBitmapCallStackListView_MouseClick( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Right )
			{
				CallStackViewerContextMenu.Tag = sender;
				CallStackViewerContextMenu.Show( ( ListView )sender, e.Location );
			}
		}

		private void HistogramViewCallStackListView_MouseClick( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Right )
			{
				CallStackViewerContextMenu.Tag = sender;
				CallStackViewerContextMenu.Show( ( ListView )sender, e.Location );
			}
		}
#endregion

#region Auto validating of changed options support 

		/*-----------------------------------------------------------------------------
			Auto validating of changed options support
		-----------------------------------------------------------------------------*/

		/// <summary> Enumerates all controls that are used to filtering states. </summary>
		enum EFilteringControl : int
		{
			FC_StartSnapshot,
			FC_EndSnapshot,
			FC_SortType,
			FC_AllocationType,
			FC_ContainersVisibilityType,
			FC_FilterType,
			FC_GroupArray,
			FC_TextFilter,
			FC_MemoryPool,
			FC_AllocTags,
			Count,
		}

		ToolStripItem[] FilteringControl = new ToolStripItem[(int)EFilteringControl.Count];

		/// <summary> Contains properties used to hold current filtering state. </summary>
		struct FFilteringState
		{
			public int StartSnapshot;
			public int EndSnapshot;
			public string SortType;
			public string AllocationType;
			public string ContainersVisibilityType;
			public string FilterType;
			public List<bool> GroupArray;
			public string TextFilter;
			public EMemoryPool MemoryPool;
			public HashSet<FAllocationTag> AllocTags;

			/// <summary> Default constructor. </summary>
			public FFilteringState( bool bFake )
			{
				StartSnapshot = -1;
				EndSnapshot = -1;
				SortType = "";
				AllocationType = "";
				ContainersVisibilityType = "";
				FilterType = "";
				GroupArray = new List<bool>( 40 );
				TextFilter = "";
				MemoryPool = EMemoryPool.MEMPOOL_None;
				AllocTags = new HashSet<FAllocationTag>();
			}

			/// <summary> Creates a new copy of this class. </summary>
			public FFilteringState DeepCopy()
			{
				FFilteringState Copy = ( FFilteringState )MemberwiseClone();
				Copy.GroupArray = new List<bool>( GroupArray );
				Copy.AllocTags = new HashSet<FAllocationTag>( AllocTags );
				return Copy;
			}
		};

		/// <summary> Starting filtering state, assigned after application launch or after pressing Go button. </summary>
		FFilteringState StartingState = new FFilteringState( false );

		/// <summary> Current filtering state. </summary>
		FFilteringState CurrentState = new FFilteringState( false );

		/// <summary> Is auto validation of filtering states is enabled? </summary>
		bool bAutoValidationIsEnabled = false;

		/// <summary> Occurs when the value of the ToolStripComboBox.SelectedIndex property has changed. </summary>
		private void DiffStartComboBox_SelectedIndexChanged( object sender, EventArgs e )
		{
			UpdateFilteringState();
			UpdateDetailsTab();
		}

		/// <summary> Occurs when the value of the ToolStripComboBox.SelectedIndex property has changed. </summary>
		private void DiffEndComboBox_SelectedIndexChanged( object sender, EventArgs e )
		{
			UpdateFilteringState();
			UpdateDetailsTab();
		}

		/// <summary> Occurs when the value of the ToolStripItem.Text property  changes. </summary>
		private void FilterTextBox_TextChanged( object sender, EventArgs e )
		{
			UpdateFilteringState();
		}

		/// <summary> Checks filtering states and updates controls by adding an exclamation mark as nessesary. </summary>
		void UpdateFilteringState()
		{
			if( bAutoValidationIsEnabled )
			{
				// Get actual states of filtering controls.
				CurrentState.StartSnapshot = DiffStartComboBox.SelectedIndex;
				CurrentState.EndSnapshot = DiffEndComboBox.SelectedIndex;
				CurrentState.SortType = SortCriteriaSplitButton.Text;
				CurrentState.AllocationType = AllocationsTypeSplitButton.Text;
				CurrentState.ContainersVisibilityType = ContainersSplitButton.Text;
				CurrentState.FilterType = FilterTypeSplitButton.Text;

				CurrentState.GroupArray.Clear();
				foreach( ClassGroup Group in Options.ClassGroups )
				{
					CurrentState.GroupArray.Add( Group.bFilter );
				}

				CurrentState.TextFilter = FilterTextBox.Text;
				CurrentState.MemoryPool = SelectedMemoryPool;
				CurrentState.AllocTags = FilterTagsDialog.GetActiveTags();

				//bool bDiffFound = !CurrentState.CompareWith( StartingState );

				// Find the changes.
				bool[] bEquals = new bool[ ( int )EFilteringControl.Count ];

				bEquals[ ( int )EFilteringControl.FC_StartSnapshot ] = CurrentState.StartSnapshot == StartingState.StartSnapshot;
				bEquals[ ( int )EFilteringControl.FC_EndSnapshot ] = CurrentState.EndSnapshot == StartingState.EndSnapshot;
				bEquals[ ( int )EFilteringControl.FC_SortType ] = CurrentState.SortType == StartingState.SortType;
				bEquals[ ( int )EFilteringControl.FC_AllocationType ] = CurrentState.AllocationType == StartingState.AllocationType;
				bEquals[ ( int )EFilteringControl.FC_ContainersVisibilityType ] = CurrentState.ContainersVisibilityType == StartingState.ContainersVisibilityType;
				bEquals[ ( int )EFilteringControl.FC_FilterType ] = CurrentState.FilterType == StartingState.FilterType;
				bEquals[ ( int )EFilteringControl.FC_TextFilter ] = CurrentState.TextFilter == StartingState.TextFilter;
				bEquals[ ( int )EFilteringControl.FC_MemoryPool ] = CurrentState.MemoryPool == StartingState.MemoryPool;

				bEquals[ ( int )EFilteringControl.FC_GroupArray ] = true;
				if( CurrentState.GroupArray.Count == StartingState.GroupArray.Count )
				{
					for( int Index = 0; Index < CurrentState.GroupArray.Count; Index++ )
					{
						bool bDiffGroup = CurrentState.GroupArray[ Index ] != StartingState.GroupArray[ Index ];
						if( bDiffGroup )
						{
							bEquals[ ( int )EFilteringControl.FC_GroupArray ] = false;	
						}

						ToolStripMenuItemEx MenuItem = FilterClassesDropDownButton.DropDownItems[ Index ] as ToolStripMenuItemEx;
						if( MenuItem != null )
						{
							MenuItem.bDrawExclamationMark = bDiffGroup;
						}
					}
				}
				else
				{
					bEquals[ ( int )EFilteringControl.FC_GroupArray ] = false;
				}

				bEquals[ ( int )EFilteringControl.FC_AllocTags ] = true;
				if( CurrentState.AllocTags.Count == StartingState.AllocTags.Count )
				{
					foreach (var AllocTag in CurrentState.AllocTags)
					{
						if (!StartingState.AllocTags.Contains(AllocTag))
						{
							bEquals[ ( int )EFilteringControl.FC_AllocTags ] = false;
							break;
						}
					}
				}
				else
				{
					bEquals[ ( int )EFilteringControl.FC_AllocTags ] = false;
				}

				bool bEqualProperties = true;
				for( int Eq = 0; Eq < ( int )EFilteringControl.Count; Eq++ )
				{
					bEqualProperties = bEqualProperties && bEquals[ Eq ];
				}

				// Update UI.
				GoButton.Image = bEqualProperties ? Properties.Resources.Play : Properties.Resources.PlayRed;

				for( int Eq = 0; Eq < ( int )EFilteringControl.Count; Eq++ )
				{
					FilteringControl[ Eq ].DisplayStyle = bEquals[ Eq ] ? ToolStripItemDisplayStyle.Text : ToolStripItemDisplayStyle.ImageAndText;
				}
			}
		}

		/// <summary> Sets starting and current filtering state to the default values. </summary>
		void ResetFilteringState()
		{
			bAutoValidationIsEnabled = true;

			StartingState.StartSnapshot = DiffStartComboBox.SelectedIndex;
			StartingState.EndSnapshot = DiffEndComboBox.SelectedIndex;
			StartingState.SortType = SortCriteriaSplitButton.Text;
			StartingState.AllocationType = AllocationsTypeSplitButton.Text;
			StartingState.ContainersVisibilityType = ContainersSplitButton.Text;
			StartingState.FilterType = FilterTypeSplitButton.Text;

			StartingState.GroupArray.Clear();
			foreach( ClassGroup Group in Options.ClassGroups )
			{
				StartingState.GroupArray.Add( Group.bFilter );
			}

			StartingState.TextFilter = FilterTextBox.Text;
			StartingState.MemoryPool = SelectedMemoryPool;
			StartingState.AllocTags = FilterTagsDialog.GetActiveTags();

			CurrentState = StartingState.DeepCopy();
			UpdateFilteringState();
		}

		/// <summary> Asigns filtering controls with internal list and setups drawing events. </summary>
		void SetupFilteringControls()
		{
			FilteringControl[ ( int )EFilteringControl.FC_StartSnapshot ] = DiffStartToolLabel;
			FilteringControl[ ( int )EFilteringControl.FC_EndSnapshot ] = DiffEndToolLabel;
			FilteringControl[ ( int )EFilteringControl.FC_SortType ] = SortCriteriaSplitButton;
			FilteringControl[ ( int )EFilteringControl.FC_AllocationType ] = AllocationsTypeSplitButton;
			FilteringControl[ ( int )EFilteringControl.FC_ContainersVisibilityType ] = ContainersSplitButton;
			FilteringControl[ ( int )EFilteringControl.FC_FilterType ] = FilterTypeSplitButton;
			FilteringControl[ ( int )EFilteringControl.FC_GroupArray ] = FilterClassesDropDownButton;
			FilteringControl[ ( int )EFilteringControl.FC_TextFilter ] = FilterLabel;
			FilteringControl[ ( int )EFilteringControl.FC_MemoryPool ] = MemoryPoolFilterButton;
			FilteringControl[ ( int )EFilteringControl.FC_AllocTags ] = FilterTagsButton;

			FilterClassesDropDownButton.DropDown.Renderer.RenderItemText += new ToolStripItemTextRenderEventHandler( Renderer_RenderItemText );

			for( int Eq = 0; Eq < ( int )EFilteringControl.Count; Eq++ )
			{
				FilteringControl[ Eq ].Paint += new PaintEventHandler( MainWindow_ToolStripItem_Paint );
			}
		}

		/// <summary> Occurs when the item is redrawn. Draws an exclamation mark icon on changed controls. </summary>
		void MainWindow_ToolStripItem_Paint( object sender, PaintEventArgs e )
		{
			// Hacky... instead of adding a bunch of new controls based on toolstrip item I used 'ImageAndText' as a style for drawing an exclamation mark icon.
			ToolStripItem Item = sender as ToolStripItem;
			if( Item != null && Item.DisplayStyle == ToolStripItemDisplayStyle.ImageAndText )
			{
				int MarkVerticalPos = ( e.ClipRectangle.Height - Properties.Resources.ExclamationMark.Height ) / 2;
				e.Graphics.DrawImage( Properties.Resources.ExclamationMark, 2, MarkVerticalPos );
			}	
		}

		/// <summary> Occurs when the text for a ToolStripItem is rendered. </summary>
		void Renderer_RenderItemText( object sender, ToolStripItemTextRenderEventArgs e )
		{
			ToolStripMenuItemEx MenuItem = e.Item as ToolStripMenuItemEx;
			if( MenuItem != null )
			{
				MenuItem.DrawItemExlamationMarkAndFilteringColorInfo( e );
			}
		}
#endregion

#region Details view panel methods 

		/*-----------------------------------------------------------------------------
			Details view panel methods
		-----------------------------------------------------------------------------*/

		void UpdateDetailsTab()
		{
			if( !MainTabControl.Enabled )
			{
				return;
			}

			int StartSnapshotIndex = DiffStartComboBox.SelectedIndex;
			int EndSnapshotIndex = DiffEndComboBox.SelectedIndex;
	
			FStreamSnapshot StartSnapshot = StartSnapshotIndex > 0 ? FStreamInfo.GlobalInstance.SnapshotList[ StartSnapshotIndex - 1 ] : null;
			FStreamSnapshot EndSnapshot = EndSnapshotIndex > 0 ? FStreamInfo.GlobalInstance.SnapshotList[ EndSnapshotIndex - 1 ] : null;

			FStreamSnapshot.PrepareLevelNamesForDetailsTab( StartSnapshot, CurrentSnapshot, EndSnapshot );

			DetailsStartTextBox.Text = StartSnapshot != null ? StartSnapshot.ToString() : "";
			DetailsEndTextBox.Text = EndSnapshot != null ? EndSnapshot.ToString() : "";
			DetailsDiffTextBox.Text = CurrentSnapshot != null ? CurrentSnapshot.ToString() : "";
		}
#endregion
	}

	/// <summary> A tool strip menu item expanded with ability to draw icon next to the text and filtering color information box. </summary>
	public class ToolStripMenuItemEx : ToolStripMenuItem
	{
		public ToolStripMenuItemEx( string InText )
		{
			// Hacky.. added eight spaces to make a space for additional indicators.
			Text = "        " + InText;
		}

		/// <summary> Filtering color used in the memory bitmap view and histogram view. </summary>
		public Color FilteringColor;

		/// <summary> If true draws exlamation mark icon which indicates that this menu item check state has changed. </summary>
		public bool bDrawExclamationMark = false;

		/// <summary> Draws additional information on tool strip menu item. </summary>
		internal void DrawItemExlamationMarkAndFilteringColorInfo( ToolStripItemTextRenderEventArgs e )
		{
			float MarkSize = 8;
			float ColorBoxSize = 16;
			float RequiredSize = MarkSize + ColorBoxSize;

// 			SizeF SpaceSize = e.Graphics.MeasureString( " ", Font );
// 			int NumSpaces = (int)Math.Ceiling( RequiredSize / SpaceSize.Width );

			int LeftPos = e.TextRectangle.Left;

			// Draw an exlamation mark icon.
			if( bDrawExclamationMark )
			{
				int MarkVerticalPos = ( e.Item.Height - Properties.Resources.ExclamationMark.Height ) / 2;
				e.Graphics.DrawImage( Properties.Resources.ExclamationMark, LeftPos, MarkVerticalPos );
			}

			LeftPos += 8;

			// Draw filtering color information box.
			int ColorVerticalPos = ( e.Item.Height - Properties.Resources.ExclamationMark.Height ) / 2;
			e.Graphics.FillRectangle( new SolidBrush( FilteringColor ), LeftPos, ColorVerticalPos, 16, 16 );
		}
	}

	// Class that brokers UI requests from the worker thread back to the UI thread and returns the result
	public class FUIBroker
	{
		public FUIBroker(Control InWidget)
		{
			Widget = InWidget;
		}

		public string ShowOpenFileDialog(OpenFileDialog InFileDialog)
		{
			return (string)Widget.Invoke(new Func<string>(delegate()
			{
				return (InFileDialog.ShowDialog() == DialogResult.OK) ? InFileDialog.FileName : null;
			}));
		}

		public DialogResult ShowMessageBox(string InTitle, string InMessage, MessageBoxButtons InButtons, MessageBoxIcon InIcon)
		{
			return (DialogResult)Widget.Invoke(new Func<DialogResult>(delegate ()
			{
				return MessageBox.Show(InMessage, InTitle, InButtons, InIcon);
			}));
		}

		private Control Widget;
	}

	public enum ESymbolResolutionMode
	{
		// Perform fast resolution. We always need the symbol name, however in this mode you may skip file and line look-up if it's too slow to do up-front.
		Fast,
		// Perform full resolution. Attempt to resolve the symbol name, line, and file, ignoring how slow that may be to complete.
		Full,
	}

	// Symbol parsing interface.
	// Derived types must implement a static method called StaticPlatformName which returns a string matching the value returned by FPlatformProperties::PlatformName() for the given platform.
	public abstract class ISymbolParser
	{
		public virtual bool InitializeSymbolService(string ExecutableName, FUIBroker UIBroker)
		{
			return false;
		}

		public virtual void ShutdownSymbolService()
		{
		}

		public virtual bool ResolveAddressToSymboInfo(ESymbolResolutionMode SymbolResolutionMode, ulong Address, out string OutFileName, out string OutFunction, out int OutLineNumber)
		{
			OutFileName = null;
			OutFunction = null;
			OutLineNumber = 0;
			return false;
		}

		public virtual bool ResolveAddressSymbolInfo(ESymbolResolutionMode SymbolResolutionMode, FCallStackAddress Address)
		{
			if (Address.LineNumber == 0)
			{
				string Filename;
				string Function;
				int LineNumber;
				if (ResolveAddressToSymboInfo(SymbolResolutionMode, Address.ProgramCounter, out Filename, out Function, out LineNumber))
				{
					if (Filename != null)
					{
						// Look up or add filename index.
						Address.FilenameIndex = FStreamInfo.GlobalInstance.GetNameIndex(Filename, true);
					}

					if (Function != null)
					{
						// Look up or add function index.
						Address.FunctionIndex = FStreamInfo.GlobalInstance.GetNameIndex(Function, true);
					}

					if (LineNumber != 0)
					{
						Address.LineNumber = LineNumber;
					}
				}
				else
				{
					return false;
				}
			}

			return true;
		}

		public virtual bool ResolveCallstackSymbolInfo(ESymbolResolutionMode SymbolResolutionMode, FCallStack CallStack)
		{
			bool bSuccess = true;

			foreach (int AddressIndex in CallStack.AddressIndices)
			{
				FCallStackAddress Address = FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndex];
				bSuccess &= ResolveAddressSymbolInfo(SymbolResolutionMode, Address);
			}

			return bSuccess;
		}

		public delegate void ResolveCallstackSymbolInfoCallback(FCallStack CallStack);

		public virtual void ResolveCallstackSymbolInfoAsync(ESymbolResolutionMode SymbolResolutionMode, FCallStack CallStack, ResolveCallstackSymbolInfoCallback Callback, bool bCancelCurrentWork)
		{
			if (bCancelCurrentWork)
			{
				foreach (var Worker in ResolveCallstackSymbolInfoAsyncTasks)
				{
					if (Worker.IsBusy && !Worker.CancellationPending)
					{
						Worker.CancelAsync();
					}
				}
			}

			for (int WorkerIndex = ResolveCallstackSymbolInfoAsyncTasks.Count - 1; WorkerIndex >= 0; --WorkerIndex)
			{
				var Worker = ResolveCallstackSymbolInfoAsyncTasks[WorkerIndex];
				if (!Worker.IsBusy)
				{
					ResolveCallstackSymbolInfoAsyncTasks.RemoveAt(WorkerIndex);
				}
			}

			var AsyncWorker = new BackgroundWorker();

			AsyncWorker.DoWork += new DoWorkEventHandler(delegate(object Obj, DoWorkEventArgs Args)
			{
				var Worker = Obj as BackgroundWorker;

				foreach (int AddressIndex in CallStack.AddressIndices)
				{
					if (Worker.CancellationPending)
					{
						break;
					}

					FCallStackAddress Address = FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndex];
					ResolveAddressSymbolInfo(SymbolResolutionMode, Address);
				}

				if (!Worker.CancellationPending)
				{
					Callback(CallStack);
				}
			});

			AsyncWorker.RunWorkerAsync();
			ResolveCallstackSymbolInfoAsyncTasks.Add(AsyncWorker);
		}

		public static ISymbolParser GetSymbolParserForPlatform(string PlatformName)
		{
			if (CachedSymbolParserTypes == null)
			{
				// Find all types that derive from LocalizationProvider in any of our DLLs
				CachedSymbolParserTypes = new Dictionary<string, Type>();
				var LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
				foreach (var Dll in LoadedAssemblies)
				{
					var AllTypes = Dll.GetTypes();
					foreach (var PotentialSymbolParserType in AllTypes)
					{
						if (PotentialSymbolParserType != typeof(ISymbolParser) && !PotentialSymbolParserType.IsAbstract && !PotentialSymbolParserType.IsInterface && typeof(ISymbolParser).IsAssignableFrom(PotentialSymbolParserType))
						{
							// Types should implement a static StaticPlatformName method
							var Method = PotentialSymbolParserType.GetMethod("StaticPlatformName");
							if (Method != null)
							{
								try
								{
									var TypePlatformName = Method.Invoke(null, null) as string;
									CachedSymbolParserTypes.Add(TypePlatformName, PotentialSymbolParserType);
								}
								catch
								{
									Console.Error.WriteLine("Type '{0}' threw when calling its StaticPlatformName method.", PotentialSymbolParserType.FullName);
								}
							}
							else
							{
								Console.Error.WriteLine("Type '{0}' derives from ISymbolParser but is missing its StaticPlatformName method.", PotentialSymbolParserType.FullName);
							}
						}
					}
				}
			}

			Type SymbolParserType;
			CachedSymbolParserTypes.TryGetValue(PlatformName, out SymbolParserType);
			if (SymbolParserType != null)
			{
				try
				{
					return Activator.CreateInstance(SymbolParserType) as ISymbolParser;
				}
				catch
				{
					Console.Error.WriteLine("Unable to create an instance of the type '{0}'", SymbolParserType.FullName);
				}
			}

			return null;
		}

		private static Dictionary<string, Type> CachedSymbolParserTypes;

		private List<BackgroundWorker> ResolveCallstackSymbolInfoAsyncTasks = new List<BackgroundWorker>();
	}

	// Ideally, this should be split up into 'StudioSettings' and 'Settings'
	public class SettableOptions
	{
		[XmlArray]
		[CategoryAttribute( "Settings" )]
		[DescriptionAttribute( "A list of class groups to define the callstack filtering." )]
		public List<ClassGroup> ClassGroups
		{
			get;
			set;
		}

		[XmlIgnore]
		[Browsable( false )]
		public ClassGroup UngroupedGroup
		{
			get
			{
				ClassGroup Result = ClassGroups.FirstOrDefault( x => x.Name == "Ungrouped" );
				if( Result == null )
				{
					// No ungrouped group found, so make one
					Result = new ClassGroup();
					Result.Name = "Ungrouped";
					Result.Color = Color.LightGray;
					Result.bFilter = true;
					ClassGroups.Add( Result );
				}

				if( Result.CallStackPatterns.Length != 1 )
				{
					// Ungrouped group should have exactly one pattern.
					// Overwrite whatever's there with a pattern that will match everything.
					CallStackPattern Pattern = new CallStackPattern();
					Pattern.StackFramePatterns = new string[] { "." };
					Pattern.UseRegexes = true;
					Pattern.PatternOrderGroup = PatternOrderGroup.CatchAll;
					Pattern.OrderBias = short.MaxValue;
					Result.CallStackPatterns = new CallStackPattern[] { Pattern };
				}

				return Result;
			}
		}

		[Browsable( false )]
		[XmlElement]
		public uint MemoryPoolFilterState
		{
			get;
			set;
		}

		[XmlElement]
		public bool InvertCallStacksInCallGraphView
		{
			get;
			set;
		}

		[XmlElement]
		[CategoryAttribute("Filters")]
		public bool FilterOutObjectVMFunctions
		{
			get;
			set;
		}

		[XmlElement]
		[CategoryAttribute("Filters")]
		public bool TrimAllocatorFunctions
		{
			get;
			set;
		}

		[XmlArray]
		[CategoryAttribute("Filters")]
		[DescriptionAttribute("A list of allocator functions to trim to (when TrimAllocatorFunctions is enabled).")]
		public List<FCallStackFunctionFilter> AllocatorFunctions
		{
			get;
			set;
		}

		public SettableOptions()
		{
			ClassGroups = new List<ClassGroup>();

			TrimAllocatorFunctions = true;
			AllocatorFunctions = new List<FCallStackFunctionFilter>();
		}

		public void ApplyDefaults()
		{
			if (AllocatorFunctions.Count == 0)
			{
				AllocatorFunctions.Add(new FCallStackFunctionFilter("FMallocProfiler::", FCallStackFunctionFilter.EFilterMode.StartsWith));
				AllocatorFunctions.Add(new FCallStackFunctionFilter("FMallocGcmProfiler::", FCallStackFunctionFilter.EFilterMode.StartsWith));
				AllocatorFunctions.Add(new FCallStackFunctionFilter("FMallocPoisonProxy::", FCallStackFunctionFilter.EFilterMode.StartsWith));
				AllocatorFunctions.Add(new FCallStackFunctionFilter("FMemory::", FCallStackFunctionFilter.EFilterMode.StartsWith));
				AllocatorFunctions.Add(new FCallStackFunctionFilter("operator new", FCallStackFunctionFilter.EFilterMode.StartsWith));
				AllocatorFunctions.Add(new FCallStackFunctionFilter("FSlateControlledConstruction::", FCallStackFunctionFilter.EFilterMode.StartsWith));
			}
		}

		public List<CallStackPattern> GetOrderedPatternList()
		{
			List<CallStackPattern> ResultArray = new List<CallStackPattern>();
			foreach( ClassGroup Group in ClassGroups )
			{
				ResultArray.AddRange( Group.CallStackPatterns );
			}

			ResultArray.Sort( ( x, y ) => x.Ordinal - y.Ordinal );

			return ResultArray;
		}
	}
}
