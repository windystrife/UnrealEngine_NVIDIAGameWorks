/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */
namespace MemoryProfiler2
{
	partial class MainWindow
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose( bool disposing )
		{
			if( disposing && ( components != null ) )
			{
				components.Dispose();
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainWindow));
			System.Windows.Forms.DataVisualization.Charting.ChartArea chartArea1 = new System.Windows.Forms.DataVisualization.Charting.ChartArea();
			System.Windows.Forms.DataVisualization.Charting.Legend legend1 = new System.Windows.Forms.DataVisualization.Charting.Legend();
			System.Windows.Forms.DataVisualization.Charting.Series series1 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series2 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series3 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series4 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series5 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series6 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series7 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series8 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series9 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series10 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series11 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series12 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series13 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series14 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series15 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series16 = new System.Windows.Forms.DataVisualization.Charting.Series();
			System.Windows.Forms.DataVisualization.Charting.Series series17 = new System.Windows.Forms.DataVisualization.Charting.Series();
			this.MainMenu = new System.Windows.Forms.MenuStrip();
			this.FileMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.OpenMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.ExportToCSVMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator12 = new System.Windows.Forms.ToolStripSeparator();
			this.quitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripMenuItem1 = new System.Windows.Forms.ToolStripMenuItem();
			this.findToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.findNextToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.copyHighlightedToClipboardToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.unhighlightAllToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.InvertCallgraphViewMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterObjectVMFunctionsMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.HelpMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.QuickStartMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.MainToolStrip = new System.Windows.Forms.ToolStrip();
			this.DiffStartToolLabel = new System.Windows.Forms.ToolStripLabel();
			this.DiffStartComboBox = new System.Windows.Forms.ToolStripComboBox();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.DiffEndToolLabel = new System.Windows.Forms.ToolStripLabel();
			this.DiffEndComboBox = new System.Windows.Forms.ToolStripComboBox();
			this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
			this.SortCriteriaSplitButton = new System.Windows.Forms.ToolStripSplitButton();
			this.SortBySizeMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.SortByCountMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator3 = new System.Windows.Forms.ToolStripSeparator();
			this.AllocationsTypeSplitButton = new System.Windows.Forms.ToolStripSplitButton();
			this.ActiveAllocsMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.LifetimeAllocsMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator4 = new System.Windows.Forms.ToolStripSeparator();
			this.ContainersSplitButton = new System.Windows.Forms.ToolStripSplitButton();
			this.HideTemplatesMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.ShowTemplatesMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator9 = new System.Windows.Forms.ToolStripSeparator();
			this.FilterTypeSplitButton = new System.Windows.Forms.ToolStripSplitButton();
			this.FilterOutClassesMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.FilterInClassesMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator5 = new System.Windows.Forms.ToolStripSeparator();
			this.FilterClassesDropDownButton = new System.Windows.Forms.ToolStripDropDownButton();
			this.FilterTagsButton = new System.Windows.Forms.ToolStripButton();
			this.FilterLabel = new System.Windows.Forms.ToolStripLabel();
			this.FilterTextBox = new System.Windows.Forms.ToolStripTextBox();
			this.MemoryPoolFilterButton = new System.Windows.Forms.ToolStripDropDownButton();
			this.PoolMainItem = new System.Windows.Forms.ToolStripMenuItem();
			this.PoolLocalItem = new System.Windows.Forms.ToolStripMenuItem();
			this.PoolHostDefaultItem = new System.Windows.Forms.ToolStripMenuItem();
			this.PoolHostMoviesItem = new System.Windows.Forms.ToolStripMenuItem();
			this.PoolSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.PoolAllItem = new System.Windows.Forms.ToolStripMenuItem();
			this.PoolNoneItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator10 = new System.Windows.Forms.ToolStripSeparator();
			this.GoButton = new System.Windows.Forms.ToolStripButton();
			this.MainStatusStrip = new System.Windows.Forms.StatusStrip();
			this.StatusStripLabel = new System.Windows.Forms.ToolStripStatusLabel();
			this.SelectionSizeStatusLabel = new System.Windows.Forms.ToolStripStatusLabel();
			this.ToolStripProgressBar = new System.Windows.Forms.ToolStripProgressBar();
			this.MainTabControl = new System.Windows.Forms.TabControl();
			this.CallGraphTabPage = new System.Windows.Forms.TabPage();
			this.CallGraphViewSplitContainer = new System.Windows.Forms.SplitContainer();
			this.ParentLabel = new System.Windows.Forms.Label();
			this.label5 = new System.Windows.Forms.Label();
			this.CallGraphTreeView = new MemoryProfiler2.TreeViewEx();
			this.ExclusiveTabPage = new System.Windows.Forms.TabPage();
			this.ExclusiveViewSplitContainer = new System.Windows.Forms.SplitContainer();
			this.ExclusiveListView = new MemoryProfiler2.ListViewEx();
			this.SizeInKByte = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.SizePercentage = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.Count = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.CountPercentage = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.GroupName = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.FunctionName = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.ExclusiveSingleCallStackView = new MemoryProfiler2.ListViewEx();
			this.Function = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.File = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.Line = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.TimeLineTabPage = new System.Windows.Forms.TabPage();
			this.TimeLineChart = new System.Windows.Forms.DataVisualization.Charting.Chart();
			this.CallstackHistoryTabPage = new System.Windows.Forms.TabPage();
			this.CallStackHistoryPanel = new MemoryProfiler2.DoubleBufferedPanel();
			this.CallStackHistoryHScroll = new System.Windows.Forms.HScrollBar();
			this.CallStackHistoryVScroll = new System.Windows.Forms.VScrollBar();
			this.CallstackHistoryToolStrip = new System.Windows.Forms.ToolStrip();
			this.toolStripLabel1 = new System.Windows.Forms.ToolStripLabel();
			this.CallstackHistoryAxisUnitFramesButton = new System.Windows.Forms.ToolStripButton();
			this.CallstackHistoryAxisUnitAllocationsButton = new System.Windows.Forms.ToolStripButton();
			this.toolStripSeparator15 = new System.Windows.Forms.ToolStripSeparator();
			this.CallstackHistoryShowCompleteHistoryButton = new System.Windows.Forms.ToolStripButton();
			this.CallstackHistoryShowSnapshotsButton = new System.Windows.Forms.ToolStripButton();
			this.toolStripSeparator6 = new System.Windows.Forms.ToolStripSeparator();
			this.CallstackHistoryResetZoomButton = new System.Windows.Forms.ToolStripButton();
			this.toolStripSeparator7 = new System.Windows.Forms.ToolStripSeparator();
			this.CallstackHistoryZoomLabel = new System.Windows.Forms.ToolStripLabel();
			this.CallstackHistoryZoomLabelH = new System.Windows.Forms.ToolStripLabel();
			this.toolStripLabel4 = new System.Windows.Forms.ToolStripLabel();
			this.CallstackHistoryZoomLabelV = new System.Windows.Forms.ToolStripLabel();
			this.toolStripSeparator8 = new System.Windows.Forms.ToolStripSeparator();
			this.CallstackHistoryHelpLabel = new System.Windows.Forms.ToolStripLabel();
			this.HistogramTabPage = new System.Windows.Forms.TabPage();
			this.HistrogramViewSplitContainer = new System.Windows.Forms.SplitContainer();
			this.HistogramPanel = new MemoryProfiler2.DoubleBufferedPanel();
			this.HistogramViewCallStackListView = new System.Windows.Forms.ListView();
			this.CallstackHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.CallStackViewerContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.CopyStackFrameMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.CopyCallstackToClipboardMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.HistogramViewAllocationsLabel = new System.Windows.Forms.Label();
			this.label4 = new System.Windows.Forms.Label();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.HistogramViewSizeLabel = new System.Windows.Forms.Label();
			this.label3 = new System.Windows.Forms.Label();
			this.HistogramViewNameLabel = new System.Windows.Forms.Label();
			this.HistogramViewDetailsLabel = new System.Windows.Forms.Label();
			this.MemoryBitmapTabPage = new System.Windows.Forms.TabPage();
			this.MemoryViewSplitContainer = new System.Windows.Forms.SplitContainer();
			this.MemoryBitmapPanel = new MemoryProfiler2.DoubleBufferedPanel();
			this.MemoryBitmapAllocationHistoryLabel = new System.Windows.Forms.Label();
			this.MemoryBitmapAllocationHistoryListView = new MemoryProfiler2.ListViewEx();
			this.AllocFrameColumn = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.FreeFrameColumn = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.SizeColumn = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.MemoryBitmapCallStackListView = new MemoryProfiler2.ListViewEx();
			this.FunctionColumn = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.MemoryBitmapCallstackGroupNameLabel = new System.Windows.Forms.Label();
			this.MemoryBitmapCallStackLabel = new System.Windows.Forms.Label();
			this.MemoryBitmapToolStrip = new System.Windows.Forms.ToolStrip();
			this.MemoryBitmapResetSelectionButton = new System.Windows.Forms.ToolStripButton();
			this.MemoryBitmapResetButton = new System.Windows.Forms.ToolStripButton();
			this.toolStripSeparator22 = new System.Windows.Forms.ToolStripSeparator();
			this.MemoryBitmapZoomRowsButton = new System.Windows.Forms.ToolStripButton();
			this.toolStripSeparator16 = new System.Windows.Forms.ToolStripSeparator();
			this.MemoryBitmapUndoZoomButton = new System.Windows.Forms.ToolStripButton();
			this.toolStripSeparator19 = new System.Windows.Forms.ToolStripSeparator();
			this.MemoryBitmapHeatMapButton = new System.Windows.Forms.ToolStripButton();
			this.toolStripSeparator18 = new System.Windows.Forms.ToolStripSeparator();
			this.toolStripLabel14 = new System.Windows.Forms.ToolStripLabel();
			this.MemoryBitmapBytesPerPixelLabel = new System.Windows.Forms.ToolStripLabel();
			this.toolStripSeparator17 = new System.Windows.Forms.ToolStripSeparator();
			this.toolStripLabel13 = new System.Windows.Forms.ToolStripLabel();
			this.MemoryBitmapMemorySpaceLabel = new System.Windows.Forms.ToolStripLabel();
			this.toolStripSeparator20 = new System.Windows.Forms.ToolStripSeparator();
			this.label10 = new System.Windows.Forms.ToolStripLabel();
			this.MemoryBitmapSpaceSizeLabel = new System.Windows.Forms.ToolStripLabel();
			this.toolStripSeparator21 = new System.Windows.Forms.ToolStripSeparator();
			this.toolStripLabel15 = new System.Windows.Forms.ToolStripLabel();
			this.MemoryBitmapAllocatedMemoryLabel = new System.Windows.Forms.ToolStripLabel();
			this.ShortLivedAllocationsTabPage = new System.Windows.Forms.TabPage();
			this.ShortLivedAllocationsSplittContainer = new System.Windows.Forms.SplitContainer();
			this.ShortLivedListView = new MemoryProfiler2.ListViewEx();
			this.columnHeader6 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader12 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader5 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader10 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader11 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.ShortLivedCallStackListView = new MemoryProfiler2.ListViewEx();
			this.columnHeader7 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader8 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader9 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.DetailsTabPage = new System.Windows.Forms.TabPage();
			this.DetailsLayoutPanel = new System.Windows.Forms.TableLayoutPanel();
			this.DetailsEndTextBox = new System.Windows.Forms.TextBox();
			this.DetailsDiffTextBox = new System.Windows.Forms.TextBox();
			this.DetailsViewEndLabel = new System.Windows.Forms.Label();
			this.DetailsViewStartLabel = new System.Windows.Forms.Label();
			this.DetailsViewDiffLabel = new System.Windows.Forms.Label();
			this.DetailsStartTextBox = new System.Windows.Forms.TextBox();
			this.CallGraphContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.ResetParentMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.SetParentMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.CallGraphViewHistoryMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator13 = new System.Windows.Forms.ToolStripSeparator();
			this.CopyFunctionToClipboardToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.CopySizeToClipboardMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator14 = new System.Windows.Forms.ToolStripSeparator();
			this.ExpandAllCollapseAllMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.ViewHistoryContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.ViewHistoryMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.MainMenu.SuspendLayout();
			this.MainToolStrip.SuspendLayout();
			this.MainStatusStrip.SuspendLayout();
			this.MainTabControl.SuspendLayout();
			this.CallGraphTabPage.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.CallGraphViewSplitContainer)).BeginInit();
			this.CallGraphViewSplitContainer.Panel1.SuspendLayout();
			this.CallGraphViewSplitContainer.Panel2.SuspendLayout();
			this.CallGraphViewSplitContainer.SuspendLayout();
			this.ExclusiveTabPage.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.ExclusiveViewSplitContainer)).BeginInit();
			this.ExclusiveViewSplitContainer.Panel1.SuspendLayout();
			this.ExclusiveViewSplitContainer.Panel2.SuspendLayout();
			this.ExclusiveViewSplitContainer.SuspendLayout();
			this.TimeLineTabPage.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.TimeLineChart)).BeginInit();
			this.CallstackHistoryTabPage.SuspendLayout();
			this.CallStackHistoryPanel.SuspendLayout();
			this.CallstackHistoryToolStrip.SuspendLayout();
			this.HistogramTabPage.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.HistrogramViewSplitContainer)).BeginInit();
			this.HistrogramViewSplitContainer.Panel1.SuspendLayout();
			this.HistrogramViewSplitContainer.Panel2.SuspendLayout();
			this.HistrogramViewSplitContainer.SuspendLayout();
			this.CallStackViewerContextMenu.SuspendLayout();
			this.MemoryBitmapTabPage.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.MemoryViewSplitContainer)).BeginInit();
			this.MemoryViewSplitContainer.Panel1.SuspendLayout();
			this.MemoryViewSplitContainer.Panel2.SuspendLayout();
			this.MemoryViewSplitContainer.SuspendLayout();
			this.MemoryBitmapToolStrip.SuspendLayout();
			this.ShortLivedAllocationsTabPage.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.ShortLivedAllocationsSplittContainer)).BeginInit();
			this.ShortLivedAllocationsSplittContainer.Panel1.SuspendLayout();
			this.ShortLivedAllocationsSplittContainer.Panel2.SuspendLayout();
			this.ShortLivedAllocationsSplittContainer.SuspendLayout();
			this.DetailsTabPage.SuspendLayout();
			this.DetailsLayoutPanel.SuspendLayout();
			this.CallGraphContextMenu.SuspendLayout();
			this.ViewHistoryContextMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// MainMenu
			// 
			this.MainMenu.BackColor = System.Drawing.Color.Transparent;
			this.MainMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.FileMenuItem,
            this.toolStripMenuItem1,
            this.ToolMenuItem,
            this.HelpMenuItem});
			resources.ApplyResources(this.MainMenu, "MainMenu");
			this.MainMenu.Name = "MainMenu";
			// 
			// FileMenuItem
			// 
			this.FileMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.OpenMenuItem,
            this.ExportToCSVMenuItem,
            this.toolStripSeparator12,
            this.quitToolStripMenuItem});
			this.FileMenuItem.Name = "FileMenuItem";
			resources.ApplyResources(this.FileMenuItem, "FileMenuItem");
			// 
			// OpenMenuItem
			// 
			this.OpenMenuItem.Name = "OpenMenuItem";
			resources.ApplyResources(this.OpenMenuItem, "OpenMenuItem");
			this.OpenMenuItem.Click += new System.EventHandler(this.OpenToolStripMenuItem_Click);
			// 
			// ExportToCSVMenuItem
			// 
			this.ExportToCSVMenuItem.Name = "ExportToCSVMenuItem";
			resources.ApplyResources(this.ExportToCSVMenuItem, "ExportToCSVMenuItem");
			this.ExportToCSVMenuItem.Click += new System.EventHandler(this.ExportToCSVToolStripMenuItem_Click);
			// 
			// toolStripSeparator12
			// 
			this.toolStripSeparator12.Name = "toolStripSeparator12";
			resources.ApplyResources(this.toolStripSeparator12, "toolStripSeparator12");
			// 
			// quitToolStripMenuItem
			// 
			this.quitToolStripMenuItem.Name = "quitToolStripMenuItem";
			resources.ApplyResources(this.quitToolStripMenuItem, "quitToolStripMenuItem");
			this.quitToolStripMenuItem.Click += new System.EventHandler(this.QuitMenuItem_Click);
			// 
			// toolStripMenuItem1
			// 
			this.toolStripMenuItem1.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.findToolStripMenuItem,
            this.findNextToolStripMenuItem,
            this.copyHighlightedToClipboardToolStripMenuItem,
            this.unhighlightAllToolStripMenuItem});
			this.toolStripMenuItem1.Name = "toolStripMenuItem1";
			resources.ApplyResources(this.toolStripMenuItem1, "toolStripMenuItem1");
			// 
			// findToolStripMenuItem
			// 
			this.findToolStripMenuItem.Name = "findToolStripMenuItem";
			resources.ApplyResources(this.findToolStripMenuItem, "findToolStripMenuItem");
			this.findToolStripMenuItem.Click += new System.EventHandler(this.FindToolStripMenuItem_Click);
			// 
			// findNextToolStripMenuItem
			// 
			this.findNextToolStripMenuItem.Name = "findNextToolStripMenuItem";
			resources.ApplyResources(this.findNextToolStripMenuItem, "findNextToolStripMenuItem");
			this.findNextToolStripMenuItem.Click += new System.EventHandler(this.FindNextToolStripMenuItem_Click);
			// 
			// copyHighlightedToClipboardToolStripMenuItem
			// 
			this.copyHighlightedToClipboardToolStripMenuItem.Name = "copyHighlightedToClipboardToolStripMenuItem";
			resources.ApplyResources(this.copyHighlightedToClipboardToolStripMenuItem, "copyHighlightedToClipboardToolStripMenuItem");
			this.copyHighlightedToClipboardToolStripMenuItem.Click += new System.EventHandler(this.CopyHighlightedToClipboardToolStripMenuItem_Click);
			// 
			// unhighlightAllToolStripMenuItem
			// 
			this.unhighlightAllToolStripMenuItem.Name = "unhighlightAllToolStripMenuItem";
			resources.ApplyResources(this.unhighlightAllToolStripMenuItem, "unhighlightAllToolStripMenuItem");
			this.unhighlightAllToolStripMenuItem.Click += new System.EventHandler(this.UnhighlightAllToolStripMenuItem_Click);
			// 
			// ToolMenuItem
			// 
			this.ToolMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.OptionsMenuItem,
            this.InvertCallgraphViewMenuItem,
            this.FilterObjectVMFunctionsMenuItem});
			this.ToolMenuItem.Name = "ToolMenuItem";
			resources.ApplyResources(this.ToolMenuItem, "ToolMenuItem");
			// 
			// OptionsMenuItem
			// 
			this.OptionsMenuItem.Name = "OptionsMenuItem";
			resources.ApplyResources(this.OptionsMenuItem, "OptionsMenuItem");
			this.OptionsMenuItem.Click += new System.EventHandler(this.OptionsToolStripMenuItem_Click);
			// 
			// InvertCallgraphViewMenuItem
			// 
			this.InvertCallgraphViewMenuItem.CheckOnClick = true;
			this.InvertCallgraphViewMenuItem.Name = "InvertCallgraphViewMenuItem";
			resources.ApplyResources(this.InvertCallgraphViewMenuItem, "InvertCallgraphViewMenuItem");
			this.InvertCallgraphViewMenuItem.CheckedChanged += new System.EventHandler(this.InvertCallgraphViewToolStripMenuItem_CheckedChanged);
			// 
			// FilterObjectVMFunctionsMenuItem
			// 
			this.FilterObjectVMFunctionsMenuItem.Checked = true;
			this.FilterObjectVMFunctionsMenuItem.CheckOnClick = true;
			this.FilterObjectVMFunctionsMenuItem.CheckState = System.Windows.Forms.CheckState.Checked;
			this.FilterObjectVMFunctionsMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.FilterObjectVMFunctionsMenuItem, "FilterObjectVMFunctionsMenuItem");
			this.FilterObjectVMFunctionsMenuItem.Name = "FilterObjectVMFunctionsMenuItem";
			this.FilterObjectVMFunctionsMenuItem.CheckedChanged += new System.EventHandler(this.FilterObjectVMFunctionsMenuItem_CheckedChanged);
			// 
			// HelpMenuItem
			// 
			this.HelpMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.QuickStartMenuItem});
			this.HelpMenuItem.Name = "HelpMenuItem";
			resources.ApplyResources(this.HelpMenuItem, "HelpMenuItem");
			// 
			// QuickStartMenuItem
			// 
			this.QuickStartMenuItem.Name = "QuickStartMenuItem";
			resources.ApplyResources(this.QuickStartMenuItem, "QuickStartMenuItem");
			this.QuickStartMenuItem.Click += new System.EventHandler(this.QuickStartMenuClick);
			// 
			// MainToolStrip
			// 
			resources.ApplyResources(this.MainToolStrip, "MainToolStrip");
			this.MainToolStrip.GripMargin = new System.Windows.Forms.Padding(0);
			this.MainToolStrip.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
			this.MainToolStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.DiffStartToolLabel,
            this.DiffStartComboBox,
            this.toolStripSeparator1,
            this.DiffEndToolLabel,
            this.DiffEndComboBox,
            this.toolStripSeparator2,
            this.SortCriteriaSplitButton,
            this.toolStripSeparator3,
            this.AllocationsTypeSplitButton,
            this.toolStripSeparator4,
            this.ContainersSplitButton,
            this.toolStripSeparator9,
            this.FilterTypeSplitButton,
            this.toolStripSeparator5,
            this.FilterClassesDropDownButton,
            this.FilterTagsButton,
            this.FilterLabel,
            this.FilterTextBox,
            this.MemoryPoolFilterButton,
            this.toolStripSeparator10,
            this.GoButton});
			this.MainToolStrip.Name = "MainToolStrip";
			// 
			// DiffStartToolLabel
			// 
			this.DiffStartToolLabel.AutoToolTip = true;
			this.DiffStartToolLabel.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.DiffStartToolLabel, "DiffStartToolLabel");
			this.DiffStartToolLabel.Name = "DiffStartToolLabel";
			// 
			// DiffStartComboBox
			// 
			this.DiffStartComboBox.AutoToolTip = true;
			this.DiffStartComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.DiffStartComboBox.DropDownWidth = 256;
			resources.ApplyResources(this.DiffStartComboBox, "DiffStartComboBox");
			this.DiffStartComboBox.Name = "DiffStartComboBox";
			this.DiffStartComboBox.SelectedIndexChanged += new System.EventHandler(this.DiffStartComboBox_SelectedIndexChanged);
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			resources.ApplyResources(this.toolStripSeparator1, "toolStripSeparator1");
			// 
			// DiffEndToolLabel
			// 
			this.DiffEndToolLabel.AutoToolTip = true;
			this.DiffEndToolLabel.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.DiffEndToolLabel.Name = "DiffEndToolLabel";
			resources.ApplyResources(this.DiffEndToolLabel, "DiffEndToolLabel");
			// 
			// DiffEndComboBox
			// 
			this.DiffEndComboBox.AutoToolTip = true;
			this.DiffEndComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.DiffEndComboBox.DropDownWidth = 256;
			resources.ApplyResources(this.DiffEndComboBox, "DiffEndComboBox");
			this.DiffEndComboBox.Name = "DiffEndComboBox";
			this.DiffEndComboBox.SelectedIndexChanged += new System.EventHandler(this.DiffEndComboBox_SelectedIndexChanged);
			// 
			// toolStripSeparator2
			// 
			this.toolStripSeparator2.Name = "toolStripSeparator2";
			resources.ApplyResources(this.toolStripSeparator2, "toolStripSeparator2");
			// 
			// SortCriteriaSplitButton
			// 
			this.SortCriteriaSplitButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.SortCriteriaSplitButton.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.SortBySizeMenuItem,
            this.SortByCountMenuItem});
			this.SortCriteriaSplitButton.ForeColor = System.Drawing.SystemColors.ControlText;
			resources.ApplyResources(this.SortCriteriaSplitButton, "SortCriteriaSplitButton");
			this.SortCriteriaSplitButton.Name = "SortCriteriaSplitButton";
			this.SortCriteriaSplitButton.ButtonClick += new System.EventHandler(this.SplitButtonClick);
			// 
			// SortBySizeMenuItem
			// 
			this.SortBySizeMenuItem.AutoToolTip = true;
			this.SortBySizeMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.SortBySizeMenuItem.Name = "SortBySizeMenuItem";
			resources.ApplyResources(this.SortBySizeMenuItem, "SortBySizeMenuItem");
			this.SortBySizeMenuItem.Click += new System.EventHandler(this.SortByClick);
			// 
			// SortByCountMenuItem
			// 
			this.SortByCountMenuItem.AutoToolTip = true;
			this.SortByCountMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.SortByCountMenuItem.Name = "SortByCountMenuItem";
			resources.ApplyResources(this.SortByCountMenuItem, "SortByCountMenuItem");
			this.SortByCountMenuItem.Click += new System.EventHandler(this.SortByClick);
			// 
			// toolStripSeparator3
			// 
			this.toolStripSeparator3.Name = "toolStripSeparator3";
			resources.ApplyResources(this.toolStripSeparator3, "toolStripSeparator3");
			// 
			// AllocationsTypeSplitButton
			// 
			this.AllocationsTypeSplitButton.AutoToolTip = false;
			this.AllocationsTypeSplitButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.AllocationsTypeSplitButton.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.ActiveAllocsMenuItem,
            this.LifetimeAllocsMenuItem});
			resources.ApplyResources(this.AllocationsTypeSplitButton, "AllocationsTypeSplitButton");
			this.AllocationsTypeSplitButton.Name = "AllocationsTypeSplitButton";
			this.AllocationsTypeSplitButton.ButtonClick += new System.EventHandler(this.SplitButtonClick);
			// 
			// ActiveAllocsMenuItem
			// 
			this.ActiveAllocsMenuItem.AutoToolTip = true;
			this.ActiveAllocsMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.ActiveAllocsMenuItem.Name = "ActiveAllocsMenuItem";
			resources.ApplyResources(this.ActiveAllocsMenuItem, "ActiveAllocsMenuItem");
			this.ActiveAllocsMenuItem.Click += new System.EventHandler(this.AllocationsTypeClick);
			// 
			// LifetimeAllocsMenuItem
			// 
			this.LifetimeAllocsMenuItem.AutoToolTip = true;
			this.LifetimeAllocsMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.LifetimeAllocsMenuItem.Name = "LifetimeAllocsMenuItem";
			resources.ApplyResources(this.LifetimeAllocsMenuItem, "LifetimeAllocsMenuItem");
			this.LifetimeAllocsMenuItem.Click += new System.EventHandler(this.AllocationsTypeClick);
			// 
			// toolStripSeparator4
			// 
			this.toolStripSeparator4.Name = "toolStripSeparator4";
			resources.ApplyResources(this.toolStripSeparator4, "toolStripSeparator4");
			// 
			// ContainersSplitButton
			// 
			this.ContainersSplitButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.ContainersSplitButton.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.HideTemplatesMenuItem,
            this.ShowTemplatesMenuItem});
			resources.ApplyResources(this.ContainersSplitButton, "ContainersSplitButton");
			this.ContainersSplitButton.Name = "ContainersSplitButton";
			this.ContainersSplitButton.ButtonClick += new System.EventHandler(this.SplitButtonClick);
			// 
			// HideTemplatesMenuItem
			// 
			this.HideTemplatesMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.HideTemplatesMenuItem.Name = "HideTemplatesMenuItem";
			resources.ApplyResources(this.HideTemplatesMenuItem, "HideTemplatesMenuItem");
			this.HideTemplatesMenuItem.Click += new System.EventHandler(this.ContainersSplitClick);
			// 
			// ShowTemplatesMenuItem
			// 
			this.ShowTemplatesMenuItem.AutoToolTip = true;
			this.ShowTemplatesMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.ShowTemplatesMenuItem.Name = "ShowTemplatesMenuItem";
			resources.ApplyResources(this.ShowTemplatesMenuItem, "ShowTemplatesMenuItem");
			this.ShowTemplatesMenuItem.Click += new System.EventHandler(this.ContainersSplitClick);
			// 
			// toolStripSeparator9
			// 
			this.toolStripSeparator9.Name = "toolStripSeparator9";
			resources.ApplyResources(this.toolStripSeparator9, "toolStripSeparator9");
			// 
			// FilterTypeSplitButton
			// 
			this.FilterTypeSplitButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.FilterTypeSplitButton.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.FilterOutClassesMenuItem,
            this.FilterInClassesMenuItem});
			resources.ApplyResources(this.FilterTypeSplitButton, "FilterTypeSplitButton");
			this.FilterTypeSplitButton.Name = "FilterTypeSplitButton";
			this.FilterTypeSplitButton.ButtonClick += new System.EventHandler(this.SplitButtonClick);
			// 
			// FilterOutClassesMenuItem
			// 
			this.FilterOutClassesMenuItem.AutoToolTip = true;
			this.FilterOutClassesMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.FilterOutClassesMenuItem.Name = "FilterOutClassesMenuItem";
			resources.ApplyResources(this.FilterOutClassesMenuItem, "FilterOutClassesMenuItem");
			this.FilterOutClassesMenuItem.Click += new System.EventHandler(this.FilterTypeClick);
			// 
			// FilterInClassesMenuItem
			// 
			this.FilterInClassesMenuItem.AutoToolTip = true;
			this.FilterInClassesMenuItem.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.FilterInClassesMenuItem.Name = "FilterInClassesMenuItem";
			resources.ApplyResources(this.FilterInClassesMenuItem, "FilterInClassesMenuItem");
			this.FilterInClassesMenuItem.Click += new System.EventHandler(this.FilterTypeClick);
			// 
			// toolStripSeparator5
			// 
			this.toolStripSeparator5.Name = "toolStripSeparator5";
			resources.ApplyResources(this.toolStripSeparator5, "toolStripSeparator5");
			// 
			// FilterClassesDropDownButton
			// 
			this.FilterClassesDropDownButton.BackColor = System.Drawing.Color.Transparent;
			this.FilterClassesDropDownButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.FilterClassesDropDownButton, "FilterClassesDropDownButton");
			this.FilterClassesDropDownButton.Name = "FilterClassesDropDownButton";
			// 
			// FilterTagsButton
			// 
			this.FilterTagsButton.ForeColor = System.Drawing.Color.Black;
			resources.ApplyResources(this.FilterTagsButton, "FilterTagsButton");
			this.FilterTagsButton.Name = "FilterTagsButton";
			this.FilterTagsButton.Click += new System.EventHandler(this.FilterTagsButton_Click);
			// 
			// FilterLabel
			// 
			this.FilterLabel.AutoToolTip = true;
			this.FilterLabel.BackColor = System.Drawing.Color.Gainsboro;
			this.FilterLabel.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.FilterLabel.Name = "FilterLabel";
			resources.ApplyResources(this.FilterLabel, "FilterLabel");
			// 
			// FilterTextBox
			// 
			this.FilterTextBox.AutoToolTip = true;
			this.FilterTextBox.BackColor = System.Drawing.Color.White;
			this.FilterTextBox.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.FilterTextBox.Name = "FilterTextBox";
			resources.ApplyResources(this.FilterTextBox, "FilterTextBox");
			this.FilterTextBox.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.FilterTextBox_KeyPress);
			this.FilterTextBox.TextChanged += new System.EventHandler(this.FilterTextBox_TextChanged);
			// 
			// MemoryPoolFilterButton
			// 
			this.MemoryPoolFilterButton.BackColor = System.Drawing.Color.Transparent;
			this.MemoryPoolFilterButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.MemoryPoolFilterButton.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.PoolMainItem,
            this.PoolLocalItem,
            this.PoolHostDefaultItem,
            this.PoolHostMoviesItem,
            this.PoolSeparator,
            this.PoolAllItem,
            this.PoolNoneItem});
			resources.ApplyResources(this.MemoryPoolFilterButton, "MemoryPoolFilterButton");
			this.MemoryPoolFilterButton.Name = "MemoryPoolFilterButton";
			// 
			// PoolMainItem
			// 
			this.PoolMainItem.AutoToolTip = true;
			this.PoolMainItem.Checked = true;
			this.PoolMainItem.CheckOnClick = true;
			this.PoolMainItem.CheckState = System.Windows.Forms.CheckState.Checked;
			this.PoolMainItem.Name = "PoolMainItem";
			resources.ApplyResources(this.PoolMainItem, "PoolMainItem");
			this.PoolMainItem.Click += new System.EventHandler(this.MemoryPoolFilterClick);
			this.PoolMainItem.MouseUp += new System.Windows.Forms.MouseEventHandler(this.PoolFilterButtonItem_MouseUp);
			// 
			// PoolLocalItem
			// 
			this.PoolLocalItem.AutoToolTip = true;
			this.PoolLocalItem.Checked = true;
			this.PoolLocalItem.CheckOnClick = true;
			this.PoolLocalItem.CheckState = System.Windows.Forms.CheckState.Checked;
			this.PoolLocalItem.Name = "PoolLocalItem";
			resources.ApplyResources(this.PoolLocalItem, "PoolLocalItem");
			this.PoolLocalItem.Click += new System.EventHandler(this.MemoryPoolFilterClick);
			this.PoolLocalItem.MouseUp += new System.Windows.Forms.MouseEventHandler(this.PoolFilterButtonItem_MouseUp);
			// 
			// PoolHostDefaultItem
			// 
			this.PoolHostDefaultItem.AutoToolTip = true;
			this.PoolHostDefaultItem.Checked = true;
			this.PoolHostDefaultItem.CheckOnClick = true;
			this.PoolHostDefaultItem.CheckState = System.Windows.Forms.CheckState.Checked;
			this.PoolHostDefaultItem.Name = "PoolHostDefaultItem";
			resources.ApplyResources(this.PoolHostDefaultItem, "PoolHostDefaultItem");
			this.PoolHostDefaultItem.Click += new System.EventHandler(this.MemoryPoolFilterClick);
			this.PoolHostDefaultItem.MouseUp += new System.Windows.Forms.MouseEventHandler(this.PoolFilterButtonItem_MouseUp);
			// 
			// PoolHostMoviesItem
			// 
			this.PoolHostMoviesItem.AutoToolTip = true;
			this.PoolHostMoviesItem.Checked = true;
			this.PoolHostMoviesItem.CheckOnClick = true;
			this.PoolHostMoviesItem.CheckState = System.Windows.Forms.CheckState.Checked;
			this.PoolHostMoviesItem.Name = "PoolHostMoviesItem";
			resources.ApplyResources(this.PoolHostMoviesItem, "PoolHostMoviesItem");
			this.PoolHostMoviesItem.Click += new System.EventHandler(this.MemoryPoolFilterClick);
			this.PoolHostMoviesItem.MouseUp += new System.Windows.Forms.MouseEventHandler(this.PoolFilterButtonItem_MouseUp);
			// 
			// PoolSeparator
			// 
			this.PoolSeparator.Name = "PoolSeparator";
			resources.ApplyResources(this.PoolSeparator, "PoolSeparator");
			// 
			// PoolAllItem
			// 
			this.PoolAllItem.AutoToolTip = true;
			this.PoolAllItem.Name = "PoolAllItem";
			resources.ApplyResources(this.PoolAllItem, "PoolAllItem");
			this.PoolAllItem.Click += new System.EventHandler(this.PoolAllItem_Click);
			this.PoolAllItem.MouseUp += new System.Windows.Forms.MouseEventHandler(this.PoolFilterButtonItem_MouseUp);
			// 
			// PoolNoneItem
			// 
			this.PoolNoneItem.AutoToolTip = true;
			this.PoolNoneItem.Name = "PoolNoneItem";
			resources.ApplyResources(this.PoolNoneItem, "PoolNoneItem");
			this.PoolNoneItem.Click += new System.EventHandler(this.PoolNoneItem_Click);
			this.PoolNoneItem.MouseUp += new System.Windows.Forms.MouseEventHandler(this.PoolFilterButtonItem_MouseUp);
			// 
			// toolStripSeparator10
			// 
			this.toolStripSeparator10.Name = "toolStripSeparator10";
			resources.ApplyResources(this.toolStripSeparator10, "toolStripSeparator10");
			// 
			// GoButton
			// 
			this.GoButton.ForeColor = System.Drawing.Color.Black;
			resources.ApplyResources(this.GoButton, "GoButton");
			this.GoButton.Name = "GoButton";
			this.GoButton.Click += new System.EventHandler(this.GoButton_Click);
			// 
			// MainStatusStrip
			// 
			this.MainStatusStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.StatusStripLabel,
            this.SelectionSizeStatusLabel,
            this.ToolStripProgressBar});
			resources.ApplyResources(this.MainStatusStrip, "MainStatusStrip");
			this.MainStatusStrip.Name = "MainStatusStrip";
			// 
			// StatusStripLabel
			// 
			this.StatusStripLabel.Name = "StatusStripLabel";
			resources.ApplyResources(this.StatusStripLabel, "StatusStripLabel");
			// 
			// SelectionSizeStatusLabel
			// 
			this.SelectionSizeStatusLabel.Name = "SelectionSizeStatusLabel";
			resources.ApplyResources(this.SelectionSizeStatusLabel, "SelectionSizeStatusLabel");
			// 
			// ToolStripProgressBar
			// 
			this.ToolStripProgressBar.Maximum = 20;
			this.ToolStripProgressBar.Name = "ToolStripProgressBar";
			resources.ApplyResources(this.ToolStripProgressBar, "ToolStripProgressBar");
			this.ToolStripProgressBar.Step = 1;
			// 
			// MainTabControl
			// 
			resources.ApplyResources(this.MainTabControl, "MainTabControl");
			this.MainTabControl.Controls.Add(this.CallGraphTabPage);
			this.MainTabControl.Controls.Add(this.ExclusiveTabPage);
			this.MainTabControl.Controls.Add(this.TimeLineTabPage);
			this.MainTabControl.Controls.Add(this.CallstackHistoryTabPage);
			this.MainTabControl.Controls.Add(this.HistogramTabPage);
			this.MainTabControl.Controls.Add(this.MemoryBitmapTabPage);
			this.MainTabControl.Controls.Add(this.ShortLivedAllocationsTabPage);
			this.MainTabControl.Controls.Add(this.DetailsTabPage);
			this.MainTabControl.Name = "MainTabControl";
			this.MainTabControl.SelectedIndex = 0;
			this.MainTabControl.Selected += new System.Windows.Forms.TabControlEventHandler(this.MainTabControl_Selected);
			this.MainTabControl.KeyDown += new System.Windows.Forms.KeyEventHandler(this.MainTabControl_KeyDown);
			this.MainTabControl.KeyUp += new System.Windows.Forms.KeyEventHandler(this.MainTabControl_KeyUp);
			this.MainTabControl.MouseWheel += new System.Windows.Forms.MouseEventHandler(this.MainTabControl_MouseWheel);
			// 
			// CallGraphTabPage
			// 
			this.CallGraphTabPage.Controls.Add(this.CallGraphViewSplitContainer);
			resources.ApplyResources(this.CallGraphTabPage, "CallGraphTabPage");
			this.CallGraphTabPage.Name = "CallGraphTabPage";
			this.CallGraphTabPage.UseVisualStyleBackColor = true;
			this.CallGraphTabPage.Paint += new System.Windows.Forms.PaintEventHandler(this.CallGraphTabPage_Paint);
			// 
			// CallGraphViewSplitContainer
			// 
			resources.ApplyResources(this.CallGraphViewSplitContainer, "CallGraphViewSplitContainer");
			this.CallGraphViewSplitContainer.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
			this.CallGraphViewSplitContainer.Name = "CallGraphViewSplitContainer";
			// 
			// CallGraphViewSplitContainer.Panel1
			// 
			this.CallGraphViewSplitContainer.Panel1.Controls.Add(this.ParentLabel);
			this.CallGraphViewSplitContainer.Panel1.Controls.Add(this.label5);
			// 
			// CallGraphViewSplitContainer.Panel2
			// 
			this.CallGraphViewSplitContainer.Panel2.Controls.Add(this.CallGraphTreeView);
			// 
			// ParentLabel
			// 
			resources.ApplyResources(this.ParentLabel, "ParentLabel");
			this.ParentLabel.Name = "ParentLabel";
			// 
			// label5
			// 
			resources.ApplyResources(this.label5, "label5");
			this.label5.Name = "label5";
			// 
			// CallGraphTreeView
			// 
			this.CallGraphTreeView.BackColor = System.Drawing.Color.White;
			this.CallGraphTreeView.BorderStyle = System.Windows.Forms.BorderStyle.None;
			resources.ApplyResources(this.CallGraphTreeView, "CallGraphTreeView");
			this.CallGraphTreeView.Name = "CallGraphTreeView";
			this.CallGraphTreeView.NodeMouseClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.CallGraphTreeView_NodeMouseClick);
			// 
			// ExclusiveTabPage
			// 
			this.ExclusiveTabPage.Controls.Add(this.ExclusiveViewSplitContainer);
			resources.ApplyResources(this.ExclusiveTabPage, "ExclusiveTabPage");
			this.ExclusiveTabPage.Name = "ExclusiveTabPage";
			this.ExclusiveTabPage.UseVisualStyleBackColor = true;
			// 
			// ExclusiveViewSplitContainer
			// 
			resources.ApplyResources(this.ExclusiveViewSplitContainer, "ExclusiveViewSplitContainer");
			this.ExclusiveViewSplitContainer.Name = "ExclusiveViewSplitContainer";
			// 
			// ExclusiveViewSplitContainer.Panel1
			// 
			this.ExclusiveViewSplitContainer.Panel1.Controls.Add(this.ExclusiveListView);
			// 
			// ExclusiveViewSplitContainer.Panel2
			// 
			this.ExclusiveViewSplitContainer.Panel2.Controls.Add(this.ExclusiveSingleCallStackView);
			// 
			// ExclusiveListView
			// 
			this.ExclusiveListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.SizeInKByte,
            this.SizePercentage,
            this.Count,
            this.CountPercentage,
            this.GroupName,
            this.FunctionName});
			this.ExclusiveListView.ColumnsTooltips.AddRange(new string[] {
            resources.GetString("ExclusiveListView.ColumnsTooltips"),
            resources.GetString("ExclusiveListView.ColumnsTooltips1"),
            resources.GetString("ExclusiveListView.ColumnsTooltips2"),
            resources.GetString("ExclusiveListView.ColumnsTooltips3"),
            resources.GetString("ExclusiveListView.ColumnsTooltips4"),
            resources.GetString("ExclusiveListView.ColumnsTooltips5")});
			resources.ApplyResources(this.ExclusiveListView, "ExclusiveListView");
			this.ExclusiveListView.FullRowSelect = true;
			this.ExclusiveListView.GridLines = true;
			this.ExclusiveListView.Name = "ExclusiveListView";
			this.ExclusiveListView.ShowItemToolTips = true;
			this.ExclusiveListView.UseCompatibleStateImageBehavior = false;
			this.ExclusiveListView.View = System.Windows.Forms.View.Details;
			this.ExclusiveListView.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.ExclusiveListView_ColumnClick);
			this.ExclusiveListView.SelectedIndexChanged += new System.EventHandler(this.ExclusiveListView_SelectedIndexChanged);
			this.ExclusiveListView.DoubleClick += new System.EventHandler(this.ExclusiveListView_DoubleClick);
			this.ExclusiveListView.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ExclusiveListView_MouseClick);
			// 
			// SizeInKByte
			// 
			resources.ApplyResources(this.SizeInKByte, "SizeInKByte");
			// 
			// SizePercentage
			// 
			resources.ApplyResources(this.SizePercentage, "SizePercentage");
			// 
			// Count
			// 
			resources.ApplyResources(this.Count, "Count");
			// 
			// CountPercentage
			// 
			resources.ApplyResources(this.CountPercentage, "CountPercentage");
			// 
			// GroupName
			// 
			resources.ApplyResources(this.GroupName, "GroupName");
			// 
			// FunctionName
			// 
			resources.ApplyResources(this.FunctionName, "FunctionName");
			// 
			// ExclusiveSingleCallStackView
			// 
			this.ExclusiveSingleCallStackView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.Function,
            this.File,
            this.Line});
			this.ExclusiveSingleCallStackView.ColumnsTooltips.AddRange(new string[] {
            resources.GetString("ExclusiveSingleCallStackView.ColumnsTooltips"),
            resources.GetString("ExclusiveSingleCallStackView.ColumnsTooltips1"),
            resources.GetString("ExclusiveSingleCallStackView.ColumnsTooltips2")});
			resources.ApplyResources(this.ExclusiveSingleCallStackView, "ExclusiveSingleCallStackView");
			this.ExclusiveSingleCallStackView.FullRowSelect = true;
			this.ExclusiveSingleCallStackView.GridLines = true;
			this.ExclusiveSingleCallStackView.Name = "ExclusiveSingleCallStackView";
			this.ExclusiveSingleCallStackView.ShowItemToolTips = true;
			this.ExclusiveSingleCallStackView.UseCompatibleStateImageBehavior = false;
			this.ExclusiveSingleCallStackView.View = System.Windows.Forms.View.Details;
			this.ExclusiveSingleCallStackView.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ExclusiveSingleCallStackView_MouseClick);
			// 
			// Function
			// 
			resources.ApplyResources(this.Function, "Function");
			// 
			// File
			// 
			resources.ApplyResources(this.File, "File");
			// 
			// Line
			// 
			resources.ApplyResources(this.Line, "Line");
			// 
			// TimeLineTabPage
			// 
			this.TimeLineTabPage.BackColor = System.Drawing.Color.Transparent;
			this.TimeLineTabPage.Controls.Add(this.TimeLineChart);
			resources.ApplyResources(this.TimeLineTabPage, "TimeLineTabPage");
			this.TimeLineTabPage.Name = "TimeLineTabPage";
			this.TimeLineTabPage.UseVisualStyleBackColor = true;
			// 
			// TimeLineChart
			// 
			this.TimeLineChart.BackColor = System.Drawing.SystemColors.Control;
			chartArea1.BackColor = System.Drawing.SystemColors.Control;
			chartArea1.CursorX.IsUserSelectionEnabled = true;
			chartArea1.Name = "ChartArea1";
			this.TimeLineChart.ChartAreas.Add(chartArea1);
			resources.ApplyResources(this.TimeLineChart, "TimeLineChart");
			legend1.BackColor = System.Drawing.SystemColors.Control;
			legend1.Name = "Legend1";
			this.TimeLineChart.Legends.Add(legend1);
			this.TimeLineChart.Name = "TimeLineChart";
			this.TimeLineChart.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.Bright;
			series1.ChartArea = "ChartArea1";
			series1.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series1.Legend = "Legend1";
			series1.Name = "Image Size";
			series2.ChartArea = "ChartArea1";
			series2.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series2.Legend = "Legend1";
			series2.Name = "OS Overhead";
			series3.ChartArea = "ChartArea1";
			series3.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series3.Legend = "Legend1";
			series3.Name = "Virtual Used";
			series4.ChartArea = "ChartArea1";
			series4.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series4.Legend = "Legend1";
			series4.Name = "Virtual Slack";
			series5.ChartArea = "ChartArea1";
			series5.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series5.Legend = "Legend1";
			series5.Name = "Virtual Waste";
			series6.BorderColor = System.Drawing.Color.White;
			series6.ChartArea = "ChartArea1";
			series6.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series6.Legend = "Legend1";
			series6.Name = "Physical Used";
			series7.ChartArea = "ChartArea1";
			series7.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series7.Legend = "Legend1";
			series7.Name = "Physical Slack";
			series8.ChartArea = "ChartArea1";
			series8.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series8.Legend = "Legend1";
			series8.Name = "Physical Waste";
			series9.ChartArea = "ChartArea1";
			series9.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series9.Legend = "Legend1";
			series9.Name = "Host Used";
			series10.ChartArea = "ChartArea1";
			series10.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series10.Legend = "Legend1";
			series10.Name = "Host Slack";
			series11.ChartArea = "ChartArea1";
			series11.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series11.Legend = "Legend1";
			series11.Name = "Host Waste";
			series12.ChartArea = "ChartArea1";
			series12.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series12.Color = System.Drawing.Color.Gray;
			series12.Legend = "Legend1";
			series12.Name = "Memory Profiling Overhead";
			series13.ChartArea = "ChartArea1";
			series13.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series13.Color = System.Drawing.Color.Plum;
			series13.Legend = "Legend1";
			series13.Name = "Binned Waste Current";
			series14.ChartArea = "ChartArea1";
			series14.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series14.Color = System.Drawing.Color.Green;
			series14.Legend = "Legend1";
			series14.Name = "Binned Slack Current";
			series15.ChartArea = "ChartArea1";
			series15.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.StackedArea;
			series15.Color = System.Drawing.Color.SkyBlue;
			series15.Legend = "Legend1";
			series15.Name = "Binned Used Current";
			series16.BorderWidth = 2;
			series16.ChartArea = "ChartArea1";
			series16.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.Line;
			series16.Color = System.Drawing.Color.White;
			series16.LabelBackColor = System.Drawing.Color.Blue;
			series16.LabelBorderColor = System.Drawing.Color.Magenta;
			series16.Legend = "Legend1";
			series16.MarkerBorderColor = System.Drawing.Color.Green;
			series16.MarkerBorderWidth = 2;
			series16.MarkerColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(255)))), ((int)(((byte)(128)))));
			series16.Name = "Allocated Memory";
			series16.ShadowOffset = 1;
			series17.BorderColor = System.Drawing.Color.White;
			series17.BorderWidth = 2;
			series17.ChartArea = "ChartArea1";
			series17.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.Line;
			series17.Color = System.Drawing.Color.Red;
			series17.LabelBackColor = System.Drawing.Color.Blue;
			series17.LabelBorderColor = System.Drawing.Color.Magenta;
			series17.Legend = "Legend1";
			series17.MarkerBorderColor = System.Drawing.Color.Green;
			series17.MarkerBorderWidth = 2;
			series17.MarkerColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(255)))), ((int)(((byte)(128)))));
			series17.Name = "Used Physical";
			series17.ShadowOffset = 1;
			this.TimeLineChart.Series.Add(series1);
			this.TimeLineChart.Series.Add(series2);
			this.TimeLineChart.Series.Add(series3);
			this.TimeLineChart.Series.Add(series4);
			this.TimeLineChart.Series.Add(series5);
			this.TimeLineChart.Series.Add(series6);
			this.TimeLineChart.Series.Add(series7);
			this.TimeLineChart.Series.Add(series8);
			this.TimeLineChart.Series.Add(series9);
			this.TimeLineChart.Series.Add(series10);
			this.TimeLineChart.Series.Add(series11);
			this.TimeLineChart.Series.Add(series12);
			this.TimeLineChart.Series.Add(series13);
			this.TimeLineChart.Series.Add(series14);
			this.TimeLineChart.Series.Add(series15);
			this.TimeLineChart.Series.Add(series16);
			this.TimeLineChart.Series.Add(series17);
			this.TimeLineChart.SelectionRangeChanged += new System.EventHandler<System.Windows.Forms.DataVisualization.Charting.CursorEventArgs>(this.TimeLineChart_SelectionRangeChanged);
			// 
			// CallstackHistoryTabPage
			// 
			this.CallstackHistoryTabPage.BackColor = System.Drawing.Color.Transparent;
			this.CallstackHistoryTabPage.Controls.Add(this.CallStackHistoryPanel);
			this.CallstackHistoryTabPage.Controls.Add(this.CallstackHistoryToolStrip);
			resources.ApplyResources(this.CallstackHistoryTabPage, "CallstackHistoryTabPage");
			this.CallstackHistoryTabPage.Name = "CallstackHistoryTabPage";
			this.CallstackHistoryTabPage.UseVisualStyleBackColor = true;
			// 
			// CallStackHistoryPanel
			// 
			this.CallStackHistoryPanel.BackColor = System.Drawing.Color.Transparent;
			this.CallStackHistoryPanel.Controls.Add(this.CallStackHistoryHScroll);
			this.CallStackHistoryPanel.Controls.Add(this.CallStackHistoryVScroll);
			resources.ApplyResources(this.CallStackHistoryPanel, "CallStackHistoryPanel");
			this.CallStackHistoryPanel.Name = "CallStackHistoryPanel";
			this.CallStackHistoryPanel.Paint += new System.Windows.Forms.PaintEventHandler(this.CallStackHistoryPanel_Paint);
			this.CallStackHistoryPanel.MouseMove += new System.Windows.Forms.MouseEventHandler(this.CallStackHistoryPanel_MouseMove);
			this.CallStackHistoryPanel.Resize += new System.EventHandler(this.CallStackHistoryPanel_Resize);
			// 
			// CallStackHistoryHScroll
			// 
			resources.ApplyResources(this.CallStackHistoryHScroll, "CallStackHistoryHScroll");
			this.CallStackHistoryHScroll.LargeChange = 1330;
			this.CallStackHistoryHScroll.Maximum = 1330;
			this.CallStackHistoryHScroll.Name = "CallStackHistoryHScroll";
			this.CallStackHistoryHScroll.ValueChanged += new System.EventHandler(this.CallStackHistoryHScroll_ValueChanged);
			// 
			// CallStackHistoryVScroll
			// 
			resources.ApplyResources(this.CallStackHistoryVScroll, "CallStackHistoryVScroll");
			this.CallStackHistoryVScroll.LargeChange = 620;
			this.CallStackHistoryVScroll.Maximum = 620;
			this.CallStackHistoryVScroll.Name = "CallStackHistoryVScroll";
			this.CallStackHistoryVScroll.ValueChanged += new System.EventHandler(this.CallStackHistoryVScroll_ValueChanged);
			// 
			// CallstackHistoryToolStrip
			// 
			this.CallstackHistoryToolStrip.GripMargin = new System.Windows.Forms.Padding(0);
			this.CallstackHistoryToolStrip.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
			this.CallstackHistoryToolStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripLabel1,
            this.CallstackHistoryAxisUnitFramesButton,
            this.CallstackHistoryAxisUnitAllocationsButton,
            this.toolStripSeparator15,
            this.CallstackHistoryShowCompleteHistoryButton,
            this.CallstackHistoryShowSnapshotsButton,
            this.toolStripSeparator6,
            this.CallstackHistoryResetZoomButton,
            this.toolStripSeparator7,
            this.CallstackHistoryZoomLabel,
            this.CallstackHistoryZoomLabelH,
            this.toolStripLabel4,
            this.CallstackHistoryZoomLabelV,
            this.toolStripSeparator8,
            this.CallstackHistoryHelpLabel});
			resources.ApplyResources(this.CallstackHistoryToolStrip, "CallstackHistoryToolStrip");
			this.CallstackHistoryToolStrip.Name = "CallstackHistoryToolStrip";
			// 
			// toolStripLabel1
			// 
			resources.ApplyResources(this.toolStripLabel1, "toolStripLabel1");
			this.toolStripLabel1.Name = "toolStripLabel1";
			// 
			// CallstackHistoryAxisUnitFramesButton
			// 
			this.CallstackHistoryAxisUnitFramesButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.CallstackHistoryAxisUnitFramesButton, "CallstackHistoryAxisUnitFramesButton");
			this.CallstackHistoryAxisUnitFramesButton.Name = "CallstackHistoryAxisUnitFramesButton";
			this.CallstackHistoryAxisUnitFramesButton.Click += new System.EventHandler(this.CallstackHistoryAxisUnitFramesButton_Click);
			// 
			// CallstackHistoryAxisUnitAllocationsButton
			// 
			this.CallstackHistoryAxisUnitAllocationsButton.Checked = true;
			this.CallstackHistoryAxisUnitAllocationsButton.CheckState = System.Windows.Forms.CheckState.Checked;
			this.CallstackHistoryAxisUnitAllocationsButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.CallstackHistoryAxisUnitAllocationsButton, "CallstackHistoryAxisUnitAllocationsButton");
			this.CallstackHistoryAxisUnitAllocationsButton.Name = "CallstackHistoryAxisUnitAllocationsButton";
			this.CallstackHistoryAxisUnitAllocationsButton.Click += new System.EventHandler(this.CallstackHistoryAxisUnitAllocationsButton_Click);
			// 
			// toolStripSeparator15
			// 
			this.toolStripSeparator15.Name = "toolStripSeparator15";
			resources.ApplyResources(this.toolStripSeparator15, "toolStripSeparator15");
			// 
			// CallstackHistoryShowCompleteHistoryButton
			// 
			this.CallstackHistoryShowCompleteHistoryButton.CheckOnClick = true;
			this.CallstackHistoryShowCompleteHistoryButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.CallstackHistoryShowCompleteHistoryButton, "CallstackHistoryShowCompleteHistoryButton");
			this.CallstackHistoryShowCompleteHistoryButton.Name = "CallstackHistoryShowCompleteHistoryButton";
			this.CallstackHistoryShowCompleteHistoryButton.CheckedChanged += new System.EventHandler(this.ShowCompleteHistoryButton_CheckedChanged);
			// 
			// CallstackHistoryShowSnapshotsButton
			// 
			this.CallstackHistoryShowSnapshotsButton.Checked = true;
			this.CallstackHistoryShowSnapshotsButton.CheckOnClick = true;
			this.CallstackHistoryShowSnapshotsButton.CheckState = System.Windows.Forms.CheckState.Checked;
			this.CallstackHistoryShowSnapshotsButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.CallstackHistoryShowSnapshotsButton, "CallstackHistoryShowSnapshotsButton");
			this.CallstackHistoryShowSnapshotsButton.Name = "CallstackHistoryShowSnapshotsButton";
			this.CallstackHistoryShowSnapshotsButton.CheckedChanged += new System.EventHandler(this.ShowSnapshotsButton_CheckedChanged);
			// 
			// toolStripSeparator6
			// 
			this.toolStripSeparator6.Name = "toolStripSeparator6";
			resources.ApplyResources(this.toolStripSeparator6, "toolStripSeparator6");
			// 
			// CallstackHistoryResetZoomButton
			// 
			this.CallstackHistoryResetZoomButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.CallstackHistoryResetZoomButton, "CallstackHistoryResetZoomButton");
			this.CallstackHistoryResetZoomButton.Name = "CallstackHistoryResetZoomButton";
			this.CallstackHistoryResetZoomButton.Click += new System.EventHandler(this.CallstackHistoryResetZoomButton_Click);
			// 
			// toolStripSeparator7
			// 
			this.toolStripSeparator7.Name = "toolStripSeparator7";
			resources.ApplyResources(this.toolStripSeparator7, "toolStripSeparator7");
			// 
			// CallstackHistoryZoomLabel
			// 
			this.CallstackHistoryZoomLabel.Name = "CallstackHistoryZoomLabel";
			resources.ApplyResources(this.CallstackHistoryZoomLabel, "CallstackHistoryZoomLabel");
			// 
			// CallstackHistoryZoomLabelH
			// 
			resources.ApplyResources(this.CallstackHistoryZoomLabelH, "CallstackHistoryZoomLabelH");
			this.CallstackHistoryZoomLabelH.Name = "CallstackHistoryZoomLabelH";
			// 
			// toolStripLabel4
			// 
			this.toolStripLabel4.Name = "toolStripLabel4";
			resources.ApplyResources(this.toolStripLabel4, "toolStripLabel4");
			// 
			// CallstackHistoryZoomLabelV
			// 
			resources.ApplyResources(this.CallstackHistoryZoomLabelV, "CallstackHistoryZoomLabelV");
			this.CallstackHistoryZoomLabelV.Name = "CallstackHistoryZoomLabelV";
			// 
			// toolStripSeparator8
			// 
			this.toolStripSeparator8.Name = "toolStripSeparator8";
			resources.ApplyResources(this.toolStripSeparator8, "toolStripSeparator8");
			// 
			// CallstackHistoryHelpLabel
			// 
			resources.ApplyResources(this.CallstackHistoryHelpLabel, "CallstackHistoryHelpLabel");
			this.CallstackHistoryHelpLabel.Name = "CallstackHistoryHelpLabel";
			// 
			// HistogramTabPage
			// 
			this.HistogramTabPage.Controls.Add(this.HistrogramViewSplitContainer);
			resources.ApplyResources(this.HistogramTabPage, "HistogramTabPage");
			this.HistogramTabPage.Name = "HistogramTabPage";
			this.HistogramTabPage.UseVisualStyleBackColor = true;
			// 
			// HistrogramViewSplitContainer
			// 
			resources.ApplyResources(this.HistrogramViewSplitContainer, "HistrogramViewSplitContainer");
			this.HistrogramViewSplitContainer.FixedPanel = System.Windows.Forms.FixedPanel.Panel2;
			this.HistrogramViewSplitContainer.Name = "HistrogramViewSplitContainer";
			// 
			// HistrogramViewSplitContainer.Panel1
			// 
			this.HistrogramViewSplitContainer.Panel1.Controls.Add(this.HistogramPanel);
			// 
			// HistrogramViewSplitContainer.Panel2
			// 
			this.HistrogramViewSplitContainer.Panel2.BackColor = System.Drawing.Color.Transparent;
			this.HistrogramViewSplitContainer.Panel2.Controls.Add(this.HistogramViewCallStackListView);
			this.HistrogramViewSplitContainer.Panel2.Controls.Add(this.HistogramViewAllocationsLabel);
			this.HistrogramViewSplitContainer.Panel2.Controls.Add(this.label4);
			this.HistrogramViewSplitContainer.Panel2.Controls.Add(this.label1);
			this.HistrogramViewSplitContainer.Panel2.Controls.Add(this.label2);
			this.HistrogramViewSplitContainer.Panel2.Controls.Add(this.HistogramViewSizeLabel);
			this.HistrogramViewSplitContainer.Panel2.Controls.Add(this.label3);
			this.HistrogramViewSplitContainer.Panel2.Controls.Add(this.HistogramViewNameLabel);
			this.HistrogramViewSplitContainer.Panel2.Controls.Add(this.HistogramViewDetailsLabel);
			// 
			// HistogramPanel
			// 
			this.HistogramPanel.BackColor = System.Drawing.Color.White;
			resources.ApplyResources(this.HistogramPanel, "HistogramPanel");
			this.HistogramPanel.Name = "HistogramPanel";
			this.HistogramPanel.Paint += new System.Windows.Forms.PaintEventHandler(this.HistogramPanel_Paint);
			this.HistogramPanel.MouseClick += new System.Windows.Forms.MouseEventHandler(this.HistogramPanel_MouseClick);
			// 
			// HistogramViewCallStackListView
			// 
			resources.ApplyResources(this.HistogramViewCallStackListView, "HistogramViewCallStackListView");
			this.HistogramViewCallStackListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.CallstackHeader});
			this.HistogramViewCallStackListView.ContextMenuStrip = this.CallStackViewerContextMenu;
			this.HistogramViewCallStackListView.GridLines = true;
			this.HistogramViewCallStackListView.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.None;
			this.HistogramViewCallStackListView.Name = "HistogramViewCallStackListView";
			this.HistogramViewCallStackListView.ShowItemToolTips = true;
			this.HistogramViewCallStackListView.UseCompatibleStateImageBehavior = false;
			this.HistogramViewCallStackListView.View = System.Windows.Forms.View.Details;
			this.HistogramViewCallStackListView.MouseClick += new System.Windows.Forms.MouseEventHandler(this.HistogramViewCallStackListView_MouseClick);
			// 
			// CallstackHeader
			// 
			resources.ApplyResources(this.CallstackHeader, "CallstackHeader");
			// 
			// CallStackViewerContextMenu
			// 
			this.CallStackViewerContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.CopyStackFrameMenuItem,
            this.CopyCallstackToClipboardMenuItem});
			this.CallStackViewerContextMenu.Name = "HistogramDetailsContextMenu";
			resources.ApplyResources(this.CallStackViewerContextMenu, "CallStackViewerContextMenu");
			// 
			// CopyStackFrameMenuItem
			// 
			this.CopyStackFrameMenuItem.Name = "CopyStackFrameMenuItem";
			resources.ApplyResources(this.CopyStackFrameMenuItem, "CopyStackFrameMenuItem");
			this.CopyStackFrameMenuItem.Click += new System.EventHandler(this.CopyFunctionToClipboardMenuItem_Click);
			// 
			// CopyCallstackToClipboardMenuItem
			// 
			this.CopyCallstackToClipboardMenuItem.Name = "CopyCallstackToClipboardMenuItem";
			resources.ApplyResources(this.CopyCallstackToClipboardMenuItem, "CopyCallstackToClipboardMenuItem");
			this.CopyCallstackToClipboardMenuItem.Click += new System.EventHandler(this.CopyCallstackToClipboardMenuItem_Click);
			// 
			// HistogramViewAllocationsLabel
			// 
			resources.ApplyResources(this.HistogramViewAllocationsLabel, "HistogramViewAllocationsLabel");
			this.HistogramViewAllocationsLabel.BackColor = System.Drawing.Color.Transparent;
			this.HistogramViewAllocationsLabel.Name = "HistogramViewAllocationsLabel";
			// 
			// label4
			// 
			resources.ApplyResources(this.label4, "label4");
			this.label4.Name = "label4";
			// 
			// label1
			// 
			resources.ApplyResources(this.label1, "label1");
			this.label1.Name = "label1";
			// 
			// label2
			// 
			resources.ApplyResources(this.label2, "label2");
			this.label2.Name = "label2";
			// 
			// HistogramViewSizeLabel
			// 
			resources.ApplyResources(this.HistogramViewSizeLabel, "HistogramViewSizeLabel");
			this.HistogramViewSizeLabel.BackColor = System.Drawing.Color.Transparent;
			this.HistogramViewSizeLabel.Name = "HistogramViewSizeLabel";
			// 
			// label3
			// 
			resources.ApplyResources(this.label3, "label3");
			this.label3.BackColor = System.Drawing.Color.Transparent;
			this.label3.Name = "label3";
			// 
			// HistogramViewNameLabel
			// 
			resources.ApplyResources(this.HistogramViewNameLabel, "HistogramViewNameLabel");
			this.HistogramViewNameLabel.BackColor = System.Drawing.Color.Transparent;
			this.HistogramViewNameLabel.Name = "HistogramViewNameLabel";
			// 
			// HistogramViewDetailsLabel
			// 
			resources.ApplyResources(this.HistogramViewDetailsLabel, "HistogramViewDetailsLabel");
			this.HistogramViewDetailsLabel.BackColor = System.Drawing.SystemColors.ActiveCaption;
			this.HistogramViewDetailsLabel.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.HistogramViewDetailsLabel.ForeColor = System.Drawing.SystemColors.ActiveCaptionText;
			this.HistogramViewDetailsLabel.Name = "HistogramViewDetailsLabel";
			// 
			// MemoryBitmapTabPage
			// 
			this.MemoryBitmapTabPage.Controls.Add(this.MemoryViewSplitContainer);
			this.MemoryBitmapTabPage.Controls.Add(this.MemoryBitmapToolStrip);
			resources.ApplyResources(this.MemoryBitmapTabPage, "MemoryBitmapTabPage");
			this.MemoryBitmapTabPage.Name = "MemoryBitmapTabPage";
			this.MemoryBitmapTabPage.UseVisualStyleBackColor = true;
			// 
			// MemoryViewSplitContainer
			// 
			resources.ApplyResources(this.MemoryViewSplitContainer, "MemoryViewSplitContainer");
			this.MemoryViewSplitContainer.FixedPanel = System.Windows.Forms.FixedPanel.Panel2;
			this.MemoryViewSplitContainer.Name = "MemoryViewSplitContainer";
			// 
			// MemoryViewSplitContainer.Panel1
			// 
			this.MemoryViewSplitContainer.Panel1.Controls.Add(this.MemoryBitmapPanel);
			// 
			// MemoryViewSplitContainer.Panel2
			// 
			this.MemoryViewSplitContainer.Panel2.BackColor = System.Drawing.Color.Transparent;
			this.MemoryViewSplitContainer.Panel2.Controls.Add(this.MemoryBitmapAllocationHistoryLabel);
			this.MemoryViewSplitContainer.Panel2.Controls.Add(this.MemoryBitmapAllocationHistoryListView);
			this.MemoryViewSplitContainer.Panel2.Controls.Add(this.MemoryBitmapCallStackListView);
			this.MemoryViewSplitContainer.Panel2.Controls.Add(this.MemoryBitmapCallstackGroupNameLabel);
			this.MemoryViewSplitContainer.Panel2.Controls.Add(this.MemoryBitmapCallStackLabel);
			// 
			// MemoryBitmapPanel
			// 
			resources.ApplyResources(this.MemoryBitmapPanel, "MemoryBitmapPanel");
			this.MemoryBitmapPanel.BackColor = System.Drawing.Color.White;
			this.MemoryBitmapPanel.Name = "MemoryBitmapPanel";
			this.MemoryBitmapPanel.Paint += new System.Windows.Forms.PaintEventHandler(this.MemoryBitmapPanel_Paint);
			this.MemoryBitmapPanel.MouseClick += new System.Windows.Forms.MouseEventHandler(this.MemoryBitmapPanel_MouseClick);
			this.MemoryBitmapPanel.MouseDown += new System.Windows.Forms.MouseEventHandler(this.MemoryBitmapPanel_MouseDown);
			this.MemoryBitmapPanel.MouseMove += new System.Windows.Forms.MouseEventHandler(this.MemoryBitmapPanel_MouseMove);
			this.MemoryBitmapPanel.MouseUp += new System.Windows.Forms.MouseEventHandler(this.MemoryBitmapPanel_MouseUp);
			this.MemoryBitmapPanel.Resize += new System.EventHandler(this.MemoryBitmapPanel_Resize);
			// 
			// MemoryBitmapAllocationHistoryLabel
			// 
			resources.ApplyResources(this.MemoryBitmapAllocationHistoryLabel, "MemoryBitmapAllocationHistoryLabel");
			this.MemoryBitmapAllocationHistoryLabel.BackColor = System.Drawing.Color.Transparent;
			this.MemoryBitmapAllocationHistoryLabel.Name = "MemoryBitmapAllocationHistoryLabel";
			// 
			// MemoryBitmapAllocationHistoryListView
			// 
			resources.ApplyResources(this.MemoryBitmapAllocationHistoryListView, "MemoryBitmapAllocationHistoryListView");
			this.MemoryBitmapAllocationHistoryListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.AllocFrameColumn,
            this.FreeFrameColumn,
            this.SizeColumn});
			this.MemoryBitmapAllocationHistoryListView.ColumnsTooltips.AddRange(new string[] {
            resources.GetString("MemoryBitmapAllocationHistoryListView.ColumnsTooltips"),
            resources.GetString("MemoryBitmapAllocationHistoryListView.ColumnsTooltips1"),
            resources.GetString("MemoryBitmapAllocationHistoryListView.ColumnsTooltips2")});
			this.MemoryBitmapAllocationHistoryListView.FullRowSelect = true;
			this.MemoryBitmapAllocationHistoryListView.GridLines = true;
			this.MemoryBitmapAllocationHistoryListView.HideSelection = false;
			this.MemoryBitmapAllocationHistoryListView.Name = "MemoryBitmapAllocationHistoryListView";
			this.MemoryBitmapAllocationHistoryListView.ShowItemToolTips = true;
			this.MemoryBitmapAllocationHistoryListView.UseCompatibleStateImageBehavior = false;
			this.MemoryBitmapAllocationHistoryListView.View = System.Windows.Forms.View.Details;
			this.MemoryBitmapAllocationHistoryListView.SelectedIndexChanged += new System.EventHandler(this.MemoryBitmap_AllocationHistoryListView_SelectedIndexChanged);
			this.MemoryBitmapAllocationHistoryListView.MouseClick += new System.Windows.Forms.MouseEventHandler(this.MemoryBitmap_AllocationHistoryListView_MouseClick);
			// 
			// AllocFrameColumn
			// 
			resources.ApplyResources(this.AllocFrameColumn, "AllocFrameColumn");
			// 
			// FreeFrameColumn
			// 
			resources.ApplyResources(this.FreeFrameColumn, "FreeFrameColumn");
			// 
			// SizeColumn
			// 
			resources.ApplyResources(this.SizeColumn, "SizeColumn");
			// 
			// MemoryBitmapCallStackListView
			// 
			resources.ApplyResources(this.MemoryBitmapCallStackListView, "MemoryBitmapCallStackListView");
			this.MemoryBitmapCallStackListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.FunctionColumn});
			this.MemoryBitmapCallStackListView.ColumnsTooltips.AddRange(new string[] {
            resources.GetString("MemoryBitmapCallStackListView.ColumnsTooltips")});
			this.MemoryBitmapCallStackListView.FullRowSelect = true;
			this.MemoryBitmapCallStackListView.GridLines = true;
			this.MemoryBitmapCallStackListView.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.None;
			this.MemoryBitmapCallStackListView.HideSelection = false;
			this.MemoryBitmapCallStackListView.Name = "MemoryBitmapCallStackListView";
			this.MemoryBitmapCallStackListView.ShowItemToolTips = true;
			this.MemoryBitmapCallStackListView.UseCompatibleStateImageBehavior = false;
			this.MemoryBitmapCallStackListView.View = System.Windows.Forms.View.Details;
			this.MemoryBitmapCallStackListView.MouseClick += new System.Windows.Forms.MouseEventHandler(this.MemoryBitmapCallStackListView_MouseClick);
			// 
			// FunctionColumn
			// 
			resources.ApplyResources(this.FunctionColumn, "FunctionColumn");
			// 
			// MemoryBitmapCallstackGroupNameLabel
			// 
			resources.ApplyResources(this.MemoryBitmapCallstackGroupNameLabel, "MemoryBitmapCallstackGroupNameLabel");
			this.MemoryBitmapCallstackGroupNameLabel.BackColor = System.Drawing.Color.Transparent;
			this.MemoryBitmapCallstackGroupNameLabel.Name = "MemoryBitmapCallstackGroupNameLabel";
			// 
			// MemoryBitmapCallStackLabel
			// 
			resources.ApplyResources(this.MemoryBitmapCallStackLabel, "MemoryBitmapCallStackLabel");
			this.MemoryBitmapCallStackLabel.BackColor = System.Drawing.Color.Transparent;
			this.MemoryBitmapCallStackLabel.Name = "MemoryBitmapCallStackLabel";
			// 
			// MemoryBitmapToolStrip
			// 
			this.MemoryBitmapToolStrip.GripMargin = new System.Windows.Forms.Padding(0);
			this.MemoryBitmapToolStrip.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
			this.MemoryBitmapToolStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.MemoryBitmapResetSelectionButton,
            this.MemoryBitmapResetButton,
            this.toolStripSeparator22,
            this.MemoryBitmapZoomRowsButton,
            this.toolStripSeparator16,
            this.MemoryBitmapUndoZoomButton,
            this.toolStripSeparator19,
            this.MemoryBitmapHeatMapButton,
            this.toolStripSeparator18,
            this.toolStripLabel14,
            this.MemoryBitmapBytesPerPixelLabel,
            this.toolStripSeparator17,
            this.toolStripLabel13,
            this.MemoryBitmapMemorySpaceLabel,
            this.toolStripSeparator20,
            this.label10,
            this.MemoryBitmapSpaceSizeLabel,
            this.toolStripSeparator21,
            this.toolStripLabel15,
            this.MemoryBitmapAllocatedMemoryLabel});
			resources.ApplyResources(this.MemoryBitmapToolStrip, "MemoryBitmapToolStrip");
			this.MemoryBitmapToolStrip.Name = "MemoryBitmapToolStrip";
			// 
			// MemoryBitmapResetSelectionButton
			// 
			this.MemoryBitmapResetSelectionButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.MemoryBitmapResetSelectionButton, "MemoryBitmapResetSelectionButton");
			this.MemoryBitmapResetSelectionButton.Name = "MemoryBitmapResetSelectionButton";
			this.MemoryBitmapResetSelectionButton.Click += new System.EventHandler(this.MemoryBitmapPanel_ResetSelection_Click);
			// 
			// MemoryBitmapResetButton
			// 
			this.MemoryBitmapResetButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.MemoryBitmapResetButton, "MemoryBitmapResetButton");
			this.MemoryBitmapResetButton.Name = "MemoryBitmapResetButton";
			this.MemoryBitmapResetButton.Click += new System.EventHandler(this.MemoryBitmapPanel_ResetButton_Click);
			// 
			// toolStripSeparator22
			// 
			this.toolStripSeparator22.Name = "toolStripSeparator22";
			resources.ApplyResources(this.toolStripSeparator22, "toolStripSeparator22");
			// 
			// MemoryBitmapZoomRowsButton
			// 
			this.MemoryBitmapZoomRowsButton.CheckOnClick = true;
			this.MemoryBitmapZoomRowsButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.MemoryBitmapZoomRowsButton, "MemoryBitmapZoomRowsButton");
			this.MemoryBitmapZoomRowsButton.Name = "MemoryBitmapZoomRowsButton";
			// 
			// toolStripSeparator16
			// 
			this.toolStripSeparator16.Name = "toolStripSeparator16";
			resources.ApplyResources(this.toolStripSeparator16, "toolStripSeparator16");
			// 
			// MemoryBitmapUndoZoomButton
			// 
			this.MemoryBitmapUndoZoomButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.MemoryBitmapUndoZoomButton, "MemoryBitmapUndoZoomButton");
			this.MemoryBitmapUndoZoomButton.Name = "MemoryBitmapUndoZoomButton";
			this.MemoryBitmapUndoZoomButton.Click += new System.EventHandler(this.MemoryBitmap_UndoZoomButton_Click);
			// 
			// toolStripSeparator19
			// 
			this.toolStripSeparator19.Name = "toolStripSeparator19";
			resources.ApplyResources(this.toolStripSeparator19, "toolStripSeparator19");
			// 
			// MemoryBitmapHeatMapButton
			// 
			this.MemoryBitmapHeatMapButton.CheckOnClick = true;
			this.MemoryBitmapHeatMapButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			resources.ApplyResources(this.MemoryBitmapHeatMapButton, "MemoryBitmapHeatMapButton");
			this.MemoryBitmapHeatMapButton.Name = "MemoryBitmapHeatMapButton";
			this.MemoryBitmapHeatMapButton.CheckedChanged += new System.EventHandler(this.MemoryBitmap_HeatMapButton_CheckedChanged);
			// 
			// toolStripSeparator18
			// 
			this.toolStripSeparator18.Name = "toolStripSeparator18";
			resources.ApplyResources(this.toolStripSeparator18, "toolStripSeparator18");
			// 
			// toolStripLabel14
			// 
			this.toolStripLabel14.Name = "toolStripLabel14";
			resources.ApplyResources(this.toolStripLabel14, "toolStripLabel14");
			// 
			// MemoryBitmapBytesPerPixelLabel
			// 
			resources.ApplyResources(this.MemoryBitmapBytesPerPixelLabel, "MemoryBitmapBytesPerPixelLabel");
			this.MemoryBitmapBytesPerPixelLabel.Name = "MemoryBitmapBytesPerPixelLabel";
			// 
			// toolStripSeparator17
			// 
			this.toolStripSeparator17.Name = "toolStripSeparator17";
			resources.ApplyResources(this.toolStripSeparator17, "toolStripSeparator17");
			// 
			// toolStripLabel13
			// 
			this.toolStripLabel13.AutoToolTip = true;
			this.toolStripLabel13.Name = "toolStripLabel13";
			resources.ApplyResources(this.toolStripLabel13, "toolStripLabel13");
			// 
			// MemoryBitmapMemorySpaceLabel
			// 
			resources.ApplyResources(this.MemoryBitmapMemorySpaceLabel, "MemoryBitmapMemorySpaceLabel");
			this.MemoryBitmapMemorySpaceLabel.AutoToolTip = true;
			this.MemoryBitmapMemorySpaceLabel.Name = "MemoryBitmapMemorySpaceLabel";
			// 
			// toolStripSeparator20
			// 
			this.toolStripSeparator20.Name = "toolStripSeparator20";
			resources.ApplyResources(this.toolStripSeparator20, "toolStripSeparator20");
			// 
			// label10
			// 
			this.label10.AutoToolTip = true;
			this.label10.Name = "label10";
			resources.ApplyResources(this.label10, "label10");
			// 
			// MemoryBitmapSpaceSizeLabel
			// 
			resources.ApplyResources(this.MemoryBitmapSpaceSizeLabel, "MemoryBitmapSpaceSizeLabel");
			this.MemoryBitmapSpaceSizeLabel.AutoToolTip = true;
			this.MemoryBitmapSpaceSizeLabel.Name = "MemoryBitmapSpaceSizeLabel";
			// 
			// toolStripSeparator21
			// 
			this.toolStripSeparator21.Name = "toolStripSeparator21";
			resources.ApplyResources(this.toolStripSeparator21, "toolStripSeparator21");
			// 
			// toolStripLabel15
			// 
			this.toolStripLabel15.AutoToolTip = true;
			this.toolStripLabel15.Name = "toolStripLabel15";
			resources.ApplyResources(this.toolStripLabel15, "toolStripLabel15");
			// 
			// MemoryBitmapAllocatedMemoryLabel
			// 
			resources.ApplyResources(this.MemoryBitmapAllocatedMemoryLabel, "MemoryBitmapAllocatedMemoryLabel");
			this.MemoryBitmapAllocatedMemoryLabel.AutoToolTip = true;
			this.MemoryBitmapAllocatedMemoryLabel.Name = "MemoryBitmapAllocatedMemoryLabel";
			// 
			// ShortLivedAllocationsTabPage
			// 
			this.ShortLivedAllocationsTabPage.Controls.Add(this.ShortLivedAllocationsSplittContainer);
			resources.ApplyResources(this.ShortLivedAllocationsTabPage, "ShortLivedAllocationsTabPage");
			this.ShortLivedAllocationsTabPage.Name = "ShortLivedAllocationsTabPage";
			this.ShortLivedAllocationsTabPage.UseVisualStyleBackColor = true;
			// 
			// ShortLivedAllocationsSplittContainer
			// 
			resources.ApplyResources(this.ShortLivedAllocationsSplittContainer, "ShortLivedAllocationsSplittContainer");
			this.ShortLivedAllocationsSplittContainer.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
			this.ShortLivedAllocationsSplittContainer.Name = "ShortLivedAllocationsSplittContainer";
			// 
			// ShortLivedAllocationsSplittContainer.Panel1
			// 
			this.ShortLivedAllocationsSplittContainer.Panel1.Controls.Add(this.ShortLivedListView);
			// 
			// ShortLivedAllocationsSplittContainer.Panel2
			// 
			this.ShortLivedAllocationsSplittContainer.Panel2.Controls.Add(this.ShortLivedCallStackListView);
			// 
			// ShortLivedListView
			// 
			this.ShortLivedListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader6,
            this.columnHeader12,
            this.columnHeader5,
            this.columnHeader10,
            this.columnHeader11});
			this.ShortLivedListView.ColumnsTooltips.AddRange(new string[] {
            resources.GetString("ShortLivedListView.ColumnsTooltips"),
            resources.GetString("ShortLivedListView.ColumnsTooltips1"),
            resources.GetString("ShortLivedListView.ColumnsTooltips2"),
            resources.GetString("ShortLivedListView.ColumnsTooltips3"),
            resources.GetString("ShortLivedListView.ColumnsTooltips4")});
			resources.ApplyResources(this.ShortLivedListView, "ShortLivedListView");
			this.ShortLivedListView.FullRowSelect = true;
			this.ShortLivedListView.GridLines = true;
			this.ShortLivedListView.HideSelection = false;
			this.ShortLivedListView.Name = "ShortLivedListView";
			this.ShortLivedListView.ShowItemToolTips = true;
			this.ShortLivedListView.UseCompatibleStateImageBehavior = false;
			this.ShortLivedListView.View = System.Windows.Forms.View.Details;
			this.ShortLivedListView.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.ShortLivedListView_ColumnClick);
			this.ShortLivedListView.SelectedIndexChanged += new System.EventHandler(this.ShortLivedListView_SelectedIndexChanged);
			this.ShortLivedListView.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ShortLivedListView_MouseClick);
			// 
			// columnHeader6
			// 
			resources.ApplyResources(this.columnHeader6, "columnHeader6");
			// 
			// columnHeader12
			// 
			resources.ApplyResources(this.columnHeader12, "columnHeader12");
			// 
			// columnHeader5
			// 
			resources.ApplyResources(this.columnHeader5, "columnHeader5");
			// 
			// columnHeader10
			// 
			resources.ApplyResources(this.columnHeader10, "columnHeader10");
			// 
			// columnHeader11
			// 
			resources.ApplyResources(this.columnHeader11, "columnHeader11");
			// 
			// ShortLivedCallStackListView
			// 
			this.ShortLivedCallStackListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader7,
            this.columnHeader8,
            this.columnHeader9});
			this.ShortLivedCallStackListView.ColumnsTooltips.AddRange(new string[] {
            resources.GetString("ShortLivedCallStackListView.ColumnsTooltips"),
            resources.GetString("ShortLivedCallStackListView.ColumnsTooltips1"),
            resources.GetString("ShortLivedCallStackListView.ColumnsTooltips2")});
			resources.ApplyResources(this.ShortLivedCallStackListView, "ShortLivedCallStackListView");
			this.ShortLivedCallStackListView.FullRowSelect = true;
			this.ShortLivedCallStackListView.GridLines = true;
			this.ShortLivedCallStackListView.Name = "ShortLivedCallStackListView";
			this.ShortLivedCallStackListView.ShowItemToolTips = true;
			this.ShortLivedCallStackListView.UseCompatibleStateImageBehavior = false;
			this.ShortLivedCallStackListView.View = System.Windows.Forms.View.Details;
			this.ShortLivedCallStackListView.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ShortLivedCallStackListView_MouseClick);
			// 
			// columnHeader7
			// 
			resources.ApplyResources(this.columnHeader7, "columnHeader7");
			// 
			// columnHeader8
			// 
			resources.ApplyResources(this.columnHeader8, "columnHeader8");
			// 
			// columnHeader9
			// 
			resources.ApplyResources(this.columnHeader9, "columnHeader9");
			// 
			// DetailsTabPage
			// 
			this.DetailsTabPage.Controls.Add(this.DetailsLayoutPanel);
			resources.ApplyResources(this.DetailsTabPage, "DetailsTabPage");
			this.DetailsTabPage.Name = "DetailsTabPage";
			this.DetailsTabPage.UseVisualStyleBackColor = true;
			// 
			// DetailsLayoutPanel
			// 
			resources.ApplyResources(this.DetailsLayoutPanel, "DetailsLayoutPanel");
			this.DetailsLayoutPanel.Controls.Add(this.DetailsEndTextBox, 0, 1);
			this.DetailsLayoutPanel.Controls.Add(this.DetailsDiffTextBox, 0, 1);
			this.DetailsLayoutPanel.Controls.Add(this.DetailsViewEndLabel, 2, 0);
			this.DetailsLayoutPanel.Controls.Add(this.DetailsViewStartLabel, 0, 0);
			this.DetailsLayoutPanel.Controls.Add(this.DetailsViewDiffLabel, 1, 0);
			this.DetailsLayoutPanel.Controls.Add(this.DetailsStartTextBox, 0, 1);
			this.DetailsLayoutPanel.Name = "DetailsLayoutPanel";
			// 
			// DetailsEndTextBox
			// 
			resources.ApplyResources(this.DetailsEndTextBox, "DetailsEndTextBox");
			this.DetailsEndTextBox.Name = "DetailsEndTextBox";
			this.DetailsEndTextBox.ReadOnly = true;
			// 
			// DetailsDiffTextBox
			// 
			resources.ApplyResources(this.DetailsDiffTextBox, "DetailsDiffTextBox");
			this.DetailsDiffTextBox.Name = "DetailsDiffTextBox";
			this.DetailsDiffTextBox.ReadOnly = true;
			// 
			// DetailsViewEndLabel
			// 
			this.DetailsViewEndLabel.BackColor = System.Drawing.SystemColors.ActiveCaption;
			this.DetailsViewEndLabel.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			resources.ApplyResources(this.DetailsViewEndLabel, "DetailsViewEndLabel");
			this.DetailsViewEndLabel.ForeColor = System.Drawing.SystemColors.ActiveCaptionText;
			this.DetailsViewEndLabel.Name = "DetailsViewEndLabel";
			// 
			// DetailsViewStartLabel
			// 
			this.DetailsViewStartLabel.BackColor = System.Drawing.SystemColors.ActiveCaption;
			this.DetailsViewStartLabel.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			resources.ApplyResources(this.DetailsViewStartLabel, "DetailsViewStartLabel");
			this.DetailsViewStartLabel.ForeColor = System.Drawing.SystemColors.ActiveCaptionText;
			this.DetailsViewStartLabel.Name = "DetailsViewStartLabel";
			// 
			// DetailsViewDiffLabel
			// 
			this.DetailsViewDiffLabel.BackColor = System.Drawing.SystemColors.ActiveCaption;
			this.DetailsViewDiffLabel.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			resources.ApplyResources(this.DetailsViewDiffLabel, "DetailsViewDiffLabel");
			this.DetailsViewDiffLabel.ForeColor = System.Drawing.SystemColors.ActiveCaptionText;
			this.DetailsViewDiffLabel.Name = "DetailsViewDiffLabel";
			// 
			// DetailsStartTextBox
			// 
			resources.ApplyResources(this.DetailsStartTextBox, "DetailsStartTextBox");
			this.DetailsStartTextBox.Name = "DetailsStartTextBox";
			this.DetailsStartTextBox.ReadOnly = true;
			// 
			// CallGraphContextMenu
			// 
			this.CallGraphContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.ResetParentMenuItem,
            this.SetParentMenuItem,
            this.CallGraphViewHistoryMenuItem,
            this.toolStripSeparator13,
            this.CopyFunctionToClipboardToolStripMenuItem,
            this.CopySizeToClipboardMenuItem,
            this.toolStripSeparator14,
            this.ExpandAllCollapseAllMenuItem});
			this.CallGraphContextMenu.Name = "CallGraphContextMenu";
			resources.ApplyResources(this.CallGraphContextMenu, "CallGraphContextMenu");
			// 
			// ResetParentMenuItem
			// 
			this.ResetParentMenuItem.AutoToolTip = true;
			resources.ApplyResources(this.ResetParentMenuItem, "ResetParentMenuItem");
			this.ResetParentMenuItem.Name = "ResetParentMenuItem";
			this.ResetParentMenuItem.Click += new System.EventHandler(this.ResetParentMenuItem_Click);
			// 
			// SetParentMenuItem
			// 
			this.SetParentMenuItem.AutoToolTip = true;
			this.SetParentMenuItem.Name = "SetParentMenuItem";
			resources.ApplyResources(this.SetParentMenuItem, "SetParentMenuItem");
			this.SetParentMenuItem.Click += new System.EventHandler(this.SetParentToolStripMenuItem_Click);
			// 
			// CallGraphViewHistoryMenuItem
			// 
			this.CallGraphViewHistoryMenuItem.AutoToolTip = true;
			this.CallGraphViewHistoryMenuItem.Name = "CallGraphViewHistoryMenuItem";
			resources.ApplyResources(this.CallGraphViewHistoryMenuItem, "CallGraphViewHistoryMenuItem");
			this.CallGraphViewHistoryMenuItem.Click += new System.EventHandler(this.ViewHistoryToolStripMenuItem_Click);
			// 
			// toolStripSeparator13
			// 
			this.toolStripSeparator13.Name = "toolStripSeparator13";
			resources.ApplyResources(this.toolStripSeparator13, "toolStripSeparator13");
			// 
			// CopyFunctionToClipboardToolStripMenuItem
			// 
			this.CopyFunctionToClipboardToolStripMenuItem.AutoToolTip = true;
			this.CopyFunctionToClipboardToolStripMenuItem.Name = "CopyFunctionToClipboardToolStripMenuItem";
			resources.ApplyResources(this.CopyFunctionToClipboardToolStripMenuItem, "CopyFunctionToClipboardToolStripMenuItem");
			this.CopyFunctionToClipboardToolStripMenuItem.Click += new System.EventHandler(this.CopyFunctionToClipboardToolStripMenuItem_Click);
			// 
			// CopySizeToClipboardMenuItem
			// 
			this.CopySizeToClipboardMenuItem.AutoToolTip = true;
			this.CopySizeToClipboardMenuItem.Name = "CopySizeToClipboardMenuItem";
			resources.ApplyResources(this.CopySizeToClipboardMenuItem, "CopySizeToClipboardMenuItem");
			this.CopySizeToClipboardMenuItem.Click += new System.EventHandler(this.CopySizeToClipboardToolStripMenuItem_Click);
			// 
			// toolStripSeparator14
			// 
			this.toolStripSeparator14.Name = "toolStripSeparator14";
			resources.ApplyResources(this.toolStripSeparator14, "toolStripSeparator14");
			// 
			// ExpandAllCollapseAllMenuItem
			// 
			this.ExpandAllCollapseAllMenuItem.AutoToolTip = true;
			this.ExpandAllCollapseAllMenuItem.Name = "ExpandAllCollapseAllMenuItem";
			resources.ApplyResources(this.ExpandAllCollapseAllMenuItem, "ExpandAllCollapseAllMenuItem");
			this.ExpandAllCollapseAllMenuItem.Click += new System.EventHandler(this.ExpandAllCollapseAllToolStripMenuItem_Click);
			// 
			// ViewHistoryContextMenu
			// 
			this.ViewHistoryContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.ViewHistoryMenuItem});
			this.ViewHistoryContextMenu.Name = "MemoryBitmapContextMenu";
			resources.ApplyResources(this.ViewHistoryContextMenu, "ViewHistoryContextMenu");
			// 
			// ViewHistoryMenuItem
			// 
			this.ViewHistoryMenuItem.Name = "ViewHistoryMenuItem";
			resources.ApplyResources(this.ViewHistoryMenuItem, "ViewHistoryMenuItem");
			this.ViewHistoryMenuItem.Click += new System.EventHandler(this.ViewHistoryMenuItem_Click);
			// 
			// MainWindow
			// 
			resources.ApplyResources(this, "$this");
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.MainStatusStrip);
			this.Controls.Add(this.MainToolStrip);
			this.Controls.Add(this.MainMenu);
			this.Controls.Add(this.MainTabControl);
			this.MainMenuStrip = this.MainMenu;
			this.Name = "MainWindow";
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.MainWindowFormClosing);
			this.ResizeBegin += new System.EventHandler(this.MainWindow_ResizeBegin);
			this.ResizeEnd += new System.EventHandler(this.MainWindow_ResizeEnd);
			this.MainMenu.ResumeLayout(false);
			this.MainMenu.PerformLayout();
			this.MainToolStrip.ResumeLayout(false);
			this.MainToolStrip.PerformLayout();
			this.MainStatusStrip.ResumeLayout(false);
			this.MainStatusStrip.PerformLayout();
			this.MainTabControl.ResumeLayout(false);
			this.CallGraphTabPage.ResumeLayout(false);
			this.CallGraphViewSplitContainer.Panel1.ResumeLayout(false);
			this.CallGraphViewSplitContainer.Panel1.PerformLayout();
			this.CallGraphViewSplitContainer.Panel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.CallGraphViewSplitContainer)).EndInit();
			this.CallGraphViewSplitContainer.ResumeLayout(false);
			this.ExclusiveTabPage.ResumeLayout(false);
			this.ExclusiveViewSplitContainer.Panel1.ResumeLayout(false);
			this.ExclusiveViewSplitContainer.Panel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.ExclusiveViewSplitContainer)).EndInit();
			this.ExclusiveViewSplitContainer.ResumeLayout(false);
			this.TimeLineTabPage.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.TimeLineChart)).EndInit();
			this.CallstackHistoryTabPage.ResumeLayout(false);
			this.CallstackHistoryTabPage.PerformLayout();
			this.CallStackHistoryPanel.ResumeLayout(false);
			this.CallstackHistoryToolStrip.ResumeLayout(false);
			this.CallstackHistoryToolStrip.PerformLayout();
			this.HistogramTabPage.ResumeLayout(false);
			this.HistrogramViewSplitContainer.Panel1.ResumeLayout(false);
			this.HistrogramViewSplitContainer.Panel2.ResumeLayout(false);
			this.HistrogramViewSplitContainer.Panel2.PerformLayout();
			((System.ComponentModel.ISupportInitialize)(this.HistrogramViewSplitContainer)).EndInit();
			this.HistrogramViewSplitContainer.ResumeLayout(false);
			this.CallStackViewerContextMenu.ResumeLayout(false);
			this.MemoryBitmapTabPage.ResumeLayout(false);
			this.MemoryBitmapTabPage.PerformLayout();
			this.MemoryViewSplitContainer.Panel1.ResumeLayout(false);
			this.MemoryViewSplitContainer.Panel2.ResumeLayout(false);
			this.MemoryViewSplitContainer.Panel2.PerformLayout();
			((System.ComponentModel.ISupportInitialize)(this.MemoryViewSplitContainer)).EndInit();
			this.MemoryViewSplitContainer.ResumeLayout(false);
			this.MemoryBitmapToolStrip.ResumeLayout(false);
			this.MemoryBitmapToolStrip.PerformLayout();
			this.ShortLivedAllocationsTabPage.ResumeLayout(false);
			this.ShortLivedAllocationsSplittContainer.Panel1.ResumeLayout(false);
			this.ShortLivedAllocationsSplittContainer.Panel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.ShortLivedAllocationsSplittContainer)).EndInit();
			this.ShortLivedAllocationsSplittContainer.ResumeLayout(false);
			this.DetailsTabPage.ResumeLayout(false);
			this.DetailsLayoutPanel.ResumeLayout(false);
			this.DetailsLayoutPanel.PerformLayout();
			this.CallGraphContextMenu.ResumeLayout(false);
			this.ViewHistoryContextMenu.ResumeLayout(false);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.ColumnHeader AllocFrameColumn;
		private System.Windows.Forms.ColumnHeader CallstackHeader;
		private System.Windows.Forms.ColumnHeader Count;
		private System.Windows.Forms.ColumnHeader CountPercentage;
		private System.Windows.Forms.ColumnHeader File;
		private System.Windows.Forms.ColumnHeader FreeFrameColumn;
		private System.Windows.Forms.ColumnHeader Function;
		private System.Windows.Forms.ColumnHeader FunctionColumn;
		private System.Windows.Forms.ColumnHeader FunctionName;
		private System.Windows.Forms.ColumnHeader GroupName;
		private System.Windows.Forms.ColumnHeader Line;
		private System.Windows.Forms.ColumnHeader SizeColumn;
		private System.Windows.Forms.ColumnHeader SizeInKByte;
		private System.Windows.Forms.ColumnHeader SizePercentage;
		private System.Windows.Forms.ColumnHeader columnHeader10;
		private System.Windows.Forms.ColumnHeader columnHeader11;
		private System.Windows.Forms.ColumnHeader columnHeader12;
		private System.Windows.Forms.ColumnHeader columnHeader5;
		private System.Windows.Forms.ColumnHeader columnHeader6;
		private System.Windows.Forms.ColumnHeader columnHeader7;
		private System.Windows.Forms.ColumnHeader columnHeader8;
		private System.Windows.Forms.ColumnHeader columnHeader9;
		private System.Windows.Forms.ContextMenuStrip CallGraphContextMenu;
		private System.Windows.Forms.ContextMenuStrip CallStackViewerContextMenu;
		private System.Windows.Forms.Label HistogramViewDetailsLabel;
		private System.Windows.Forms.Label MemoryBitmapAllocationHistoryLabel;
		private System.Windows.Forms.Label MemoryBitmapCallStackLabel;
		private System.Windows.Forms.Label ParentLabel;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.Label label5;
		private System.Windows.Forms.MenuStrip MainMenu;
		private System.Windows.Forms.SplitContainer CallGraphViewSplitContainer;
		private System.Windows.Forms.SplitContainer ExclusiveViewSplitContainer;
		private System.Windows.Forms.SplitContainer HistrogramViewSplitContainer;
		private System.Windows.Forms.SplitContainer MemoryViewSplitContainer;
		private System.Windows.Forms.SplitContainer ShortLivedAllocationsSplittContainer;
		private System.Windows.Forms.StatusStrip MainStatusStrip;
		private System.Windows.Forms.TabControl MainTabControl;
		private System.Windows.Forms.TabPage CallGraphTabPage;
		private System.Windows.Forms.TabPage CallstackHistoryTabPage;
		private System.Windows.Forms.TabPage ExclusiveTabPage;
		private System.Windows.Forms.TabPage HistogramTabPage;
		private System.Windows.Forms.TabPage MemoryBitmapTabPage;
		private System.Windows.Forms.TabPage ShortLivedAllocationsTabPage;
		private System.Windows.Forms.TabPage TimeLineTabPage;
		private System.Windows.Forms.ToolStrip CallstackHistoryToolStrip;
		private System.Windows.Forms.ToolStrip MainToolStrip;
		private System.Windows.Forms.ToolStrip MemoryBitmapToolStrip;
		private System.Windows.Forms.ToolStripButton GoButton;
		private System.Windows.Forms.ToolStripDropDownButton FilterClassesDropDownButton;
		private System.Windows.Forms.ToolStripDropDownButton MemoryPoolFilterButton;
		private System.Windows.Forms.ToolStripLabel CallstackHistoryHelpLabel;
		private System.Windows.Forms.ToolStripLabel DiffEndToolLabel;
		private System.Windows.Forms.ToolStripLabel DiffStartToolLabel;
		private System.Windows.Forms.ToolStripLabel FilterLabel;
		private System.Windows.Forms.ToolStripLabel label10;
		private System.Windows.Forms.ToolStripLabel toolStripLabel13;
		private System.Windows.Forms.ToolStripLabel toolStripLabel14;
		private System.Windows.Forms.ToolStripLabel toolStripLabel15;
		private System.Windows.Forms.ToolStripLabel toolStripLabel1;
		private System.Windows.Forms.ToolStripMenuItem ActiveAllocsMenuItem;
		private System.Windows.Forms.ToolStripMenuItem CallGraphViewHistoryMenuItem;
		private System.Windows.Forms.ToolStripMenuItem CopyCallstackToClipboardMenuItem;
		private System.Windows.Forms.ToolStripMenuItem CopyFunctionToClipboardToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem CopySizeToClipboardMenuItem;
		private System.Windows.Forms.ToolStripMenuItem CopyStackFrameMenuItem;
		private System.Windows.Forms.ToolStripMenuItem ExpandAllCollapseAllMenuItem;
		private System.Windows.Forms.ToolStripMenuItem ExportToCSVMenuItem;
		private System.Windows.Forms.ToolStripMenuItem FileMenuItem;
		private System.Windows.Forms.ToolStripMenuItem FilterInClassesMenuItem;
		private System.Windows.Forms.ToolStripMenuItem FilterObjectVMFunctionsMenuItem;
		private System.Windows.Forms.ToolStripMenuItem FilterOutClassesMenuItem;
		private System.Windows.Forms.ToolStripMenuItem HelpMenuItem;
		private System.Windows.Forms.ToolStripMenuItem HideTemplatesMenuItem;
		private System.Windows.Forms.ToolStripMenuItem InvertCallgraphViewMenuItem;
		private System.Windows.Forms.ToolStripMenuItem LifetimeAllocsMenuItem;
		private System.Windows.Forms.ToolStripMenuItem OpenMenuItem;
		private System.Windows.Forms.ToolStripMenuItem OptionsMenuItem;
		private System.Windows.Forms.ToolStripMenuItem PoolAllItem;
		private System.Windows.Forms.ToolStripMenuItem PoolHostDefaultItem;
		private System.Windows.Forms.ToolStripMenuItem PoolHostMoviesItem;
		private System.Windows.Forms.ToolStripMenuItem PoolLocalItem;
		private System.Windows.Forms.ToolStripMenuItem PoolMainItem;
		private System.Windows.Forms.ToolStripMenuItem PoolNoneItem;
		private System.Windows.Forms.ToolStripMenuItem QuickStartMenuItem;
		private System.Windows.Forms.ToolStripMenuItem ResetParentMenuItem;
		private System.Windows.Forms.ToolStripMenuItem SetParentMenuItem;
		private System.Windows.Forms.ToolStripMenuItem ShowTemplatesMenuItem;
		private System.Windows.Forms.ToolStripMenuItem SortByCountMenuItem;
		private System.Windows.Forms.ToolStripMenuItem SortBySizeMenuItem;
		private System.Windows.Forms.ToolStripMenuItem ToolMenuItem;
		private System.Windows.Forms.ToolStripMenuItem ViewHistoryMenuItem;
		private System.Windows.Forms.ToolStripMenuItem copyHighlightedToClipboardToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem findNextToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem findToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem quitToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem toolStripMenuItem1;
		private System.Windows.Forms.ToolStripMenuItem unhighlightAllToolStripMenuItem;
		private System.Windows.Forms.ToolStripSeparator PoolSeparator;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator10;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator12;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator13;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator14;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator15;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator16;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator17;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator18;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator19;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator20;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator21;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator22;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator4;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator5;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator6;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator9;
		private System.Windows.Forms.ToolStripSplitButton AllocationsTypeSplitButton;
		private System.Windows.Forms.ToolStripSplitButton SortCriteriaSplitButton;
		private System.Windows.Forms.ToolStripStatusLabel SelectionSizeStatusLabel;
		private System.Windows.Forms.ToolStripStatusLabel StatusStripLabel;
		private MemoryProfiler2.TreeViewEx CallGraphTreeView;
		public DoubleBufferedPanel CallStackHistoryPanel;
		public DoubleBufferedPanel MemoryBitmapPanel;
		public DoubleBufferedPanel HistogramPanel;
		public ListViewEx ExclusiveListView;
		public ListViewEx ExclusiveSingleCallStackView;
		public ListViewEx MemoryBitmapAllocationHistoryListView;
		public ListViewEx MemoryBitmapCallStackListView;
		public ListViewEx ShortLivedCallStackListView;
		public ListViewEx ShortLivedListView;
		public System.Windows.Forms.ContextMenuStrip ViewHistoryContextMenu;
		public System.Windows.Forms.DataVisualization.Charting.Chart TimeLineChart;
		public System.Windows.Forms.HScrollBar CallStackHistoryHScroll;
		public System.Windows.Forms.Label HistogramViewAllocationsLabel;
		public System.Windows.Forms.Label HistogramViewNameLabel;
		public System.Windows.Forms.Label HistogramViewSizeLabel;
		public System.Windows.Forms.Label MemoryBitmapCallstackGroupNameLabel;
		public System.Windows.Forms.ListView HistogramViewCallStackListView;
		public System.Windows.Forms.ToolStripButton CallstackHistoryAxisUnitAllocationsButton;
		public System.Windows.Forms.ToolStripButton CallstackHistoryAxisUnitFramesButton;
		public System.Windows.Forms.ToolStripButton CallstackHistoryResetZoomButton;
		public System.Windows.Forms.ToolStripButton CallstackHistoryShowCompleteHistoryButton;
		public System.Windows.Forms.ToolStripButton CallstackHistoryShowSnapshotsButton;
		public System.Windows.Forms.ToolStripButton MemoryBitmapHeatMapButton;
		public System.Windows.Forms.ToolStripButton MemoryBitmapResetButton;
		public System.Windows.Forms.ToolStripButton MemoryBitmapResetSelectionButton;
		public System.Windows.Forms.ToolStripButton MemoryBitmapUndoZoomButton;
		public System.Windows.Forms.ToolStripButton MemoryBitmapZoomRowsButton;
		public System.Windows.Forms.ToolStripComboBox DiffEndComboBox;
		public System.Windows.Forms.ToolStripComboBox DiffStartComboBox;
		public System.Windows.Forms.ToolStripLabel MemoryBitmapAllocatedMemoryLabel;
		public System.Windows.Forms.ToolStripLabel MemoryBitmapBytesPerPixelLabel;
		public System.Windows.Forms.ToolStripLabel MemoryBitmapMemorySpaceLabel;
		public System.Windows.Forms.ToolStripLabel MemoryBitmapSpaceSizeLabel;
		public System.Windows.Forms.ToolStripProgressBar ToolStripProgressBar;
		public System.Windows.Forms.ToolStripSplitButton ContainersSplitButton;
		public System.Windows.Forms.ToolStripSplitButton FilterTypeSplitButton;
		public System.Windows.Forms.ToolStripTextBox FilterTextBox;
		public System.Windows.Forms.VScrollBar CallStackHistoryVScroll;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator7;
		public System.Windows.Forms.ToolStripLabel CallstackHistoryZoomLabel;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator8;
		public System.Windows.Forms.ToolStripLabel CallstackHistoryZoomLabelH;
		private System.Windows.Forms.ToolStripLabel toolStripLabel4;
		public System.Windows.Forms.ToolStripLabel CallstackHistoryZoomLabelV;
		private System.Windows.Forms.TabPage DetailsTabPage;
		private System.Windows.Forms.TableLayoutPanel DetailsLayoutPanel;
		private System.Windows.Forms.Label DetailsViewEndLabel;
		private System.Windows.Forms.Label DetailsViewDiffLabel;
		private System.Windows.Forms.TextBox DetailsStartTextBox;
		private System.Windows.Forms.TextBox DetailsEndTextBox;
		private System.Windows.Forms.TextBox DetailsDiffTextBox;
		private System.Windows.Forms.Label DetailsViewStartLabel;
		private System.Windows.Forms.ToolStripButton FilterTagsButton;
	}
}
