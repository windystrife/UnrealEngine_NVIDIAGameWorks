// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

 namespace NetworkProfiler
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
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			System.Windows.Forms.DataVisualization.Charting.ChartArea chartArea2 = new System.Windows.Forms.DataVisualization.Charting.ChartArea();
			System.Windows.Forms.DataVisualization.Charting.Legend legend2 = new System.Windows.Forms.DataVisualization.Charting.Legend();
			this.OpenButton = new System.Windows.Forms.Button();
			this.NetworkChart = new System.Windows.Forms.DataVisualization.Charting.Chart();
			this.ChartListBox = new System.Windows.Forms.CheckedListBox();
			this.ApplyFiltersButton = new System.Windows.Forms.Button();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.label3 = new System.Windows.Forms.Label();
			this.tabControl1 = new System.Windows.Forms.TabControl();
			this.tabPage1 = new System.Windows.Forms.TabPage();
			this.Details = new System.Windows.Forms.TabControl();
			this.tabPage2 = new System.Windows.Forms.TabPage();
			this.ActorSummaryView = new System.Windows.Forms.TreeView();
			this.tabPage6 = new System.Windows.Forms.TabPage();
			this.ActorPerfPropsDetailsListView = new System.Windows.Forms.ListView();
			this.ActorPerfPropsListView = new System.Windows.Forms.ListView();
			this.tabPage8 = new System.Windows.Forms.TabPage();
			this.TokenDetailsView = new System.Windows.Forms.TreeView();
			this.ConnectionListBox = new System.Windows.Forms.CheckedListBox();
			this.StackedBunchSizeRadioButton = new System.Windows.Forms.RadioButton();
			this.LineViewRadioButton = new System.Windows.Forms.RadioButton();
			this.panel1 = new System.Windows.Forms.Panel();
			this.RPCFilterBox = new System.Windows.Forms.ComboBox();
			this.PropertyFilterBox = new System.Windows.Forms.ComboBox();
			this.ActorFilterBox = new System.Windows.Forms.ComboBox();
			this.tabPage3 = new System.Windows.Forms.TabPage();
			this.ActorListView = new System.Windows.Forms.ListView();
			this.columnHeader1 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader2 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader3 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader4 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader5 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.tabPage4 = new System.Windows.Forms.TabPage();
			this.PropertyListView = new System.Windows.Forms.ListView();
			this.SizeBits = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.Count = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.AvgSizeBytes = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.AvgSizeBits = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.Property = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.tabPage5 = new System.Windows.Forms.TabPage();
			this.RPCListView = new System.Windows.Forms.ListView();
			this.columnHeader6 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader7 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader8 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader9 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader10 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.CurrentProgress = new System.Windows.Forms.ProgressBar();
			this.ContextMenuStrip1 = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.EarlyOutLabel = new System.Windows.Forms.Label();
			this.MaxProfileMinutesTextBox = new System.Windows.Forms.TextBox();
			((System.ComponentModel.ISupportInitialize)(this.NetworkChart)).BeginInit();
			this.tabControl1.SuspendLayout();
			this.tabPage1.SuspendLayout();
			this.Details.SuspendLayout();
			this.tabPage2.SuspendLayout();
			this.tabPage6.SuspendLayout();
			this.tabPage8.SuspendLayout();
			this.panel1.SuspendLayout();
			this.tabPage3.SuspendLayout();
			this.tabPage4.SuspendLayout();
			this.tabPage5.SuspendLayout();
			this.SuspendLayout();
			// 
			// OpenButton
			// 
			this.OpenButton.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.OpenButton.Location = new System.Drawing.Point(12, 12);
			this.OpenButton.Name = "OpenButton";
			this.OpenButton.Size = new System.Drawing.Size(1231, 31);
			this.OpenButton.TabIndex = 0;
			this.OpenButton.Text = "Open File";
			this.OpenButton.UseVisualStyleBackColor = true;
			this.OpenButton.Click += new System.EventHandler(this.OpenButton_Click);
			// 
			// NetworkChart
			// 
			this.NetworkChart.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.NetworkChart.AntiAliasing = System.Windows.Forms.DataVisualization.Charting.AntiAliasingStyles.Graphics;
			this.NetworkChart.BackColor = System.Drawing.Color.Transparent;
			chartArea2.AxisX.MajorGrid.LineColor = System.Drawing.Color.DarkGray;
			chartArea2.AxisX.ScrollBar.IsPositionedInside = false;
			chartArea2.AxisY.MajorGrid.LineColor = System.Drawing.Color.DarkGray;
			chartArea2.AxisY.ScrollBar.IsPositionedInside = false;
			chartArea2.BackColor = System.Drawing.Color.Transparent;
			chartArea2.CursorX.IsUserEnabled = true;
			chartArea2.CursorX.IsUserSelectionEnabled = true;
			chartArea2.Name = "DefaultChartArea";
			this.NetworkChart.ChartAreas.Add(chartArea2);
			legend2.BackColor = System.Drawing.Color.Transparent;
			legend2.DockedToChartArea = "DefaultChartArea";
			legend2.Docking = System.Windows.Forms.DataVisualization.Charting.Docking.Top;
			legend2.LegendStyle = System.Windows.Forms.DataVisualization.Charting.LegendStyle.Column;
			legend2.Name = "DefaultLegend";
			this.NetworkChart.Legends.Add(legend2);
			this.NetworkChart.Location = new System.Drawing.Point(15, 6);
			this.NetworkChart.Name = "NetworkChart";
			this.NetworkChart.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.Bright;
			this.NetworkChart.Size = new System.Drawing.Size(998, 356);
			this.NetworkChart.TabIndex = 2;
			this.NetworkChart.Text = "chart1";
			this.NetworkChart.CursorPositionChanged += new System.EventHandler<System.Windows.Forms.DataVisualization.Charting.CursorEventArgs>(this.NetworkChart_CursorPositionChanged);
			this.NetworkChart.SelectionRangeChanged += new System.EventHandler<System.Windows.Forms.DataVisualization.Charting.CursorEventArgs>(this.NetworkChart_SelectionRangeChanged);
			this.NetworkChart.PostPaint += new System.EventHandler<System.Windows.Forms.DataVisualization.Charting.ChartPaintEventArgs>(this.NetworkChart_PostPaint);
			this.NetworkChart.MouseClick += new System.Windows.Forms.MouseEventHandler(this.NetworkChart_MouseClick);
			// 
			// ChartListBox
			// 
			this.ChartListBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ChartListBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.ChartListBox.CheckOnClick = true;
			this.ChartListBox.FormattingEnabled = true;
			this.ChartListBox.Location = new System.Drawing.Point(1019, 36);
			this.ChartListBox.Name = "ChartListBox";
			this.ChartListBox.Size = new System.Drawing.Size(198, 330);
			this.ChartListBox.TabIndex = 3;
			this.ChartListBox.SelectedValueChanged += new System.EventHandler(this.ChartListBox_SelectedValueChanged);
			// 
			// ApplyFiltersButton
			// 
			this.ApplyFiltersButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.ApplyFiltersButton.Location = new System.Drawing.Point(893, 723);
			this.ApplyFiltersButton.Name = "ApplyFiltersButton";
			this.ApplyFiltersButton.Size = new System.Drawing.Size(231, 23);
			this.ApplyFiltersButton.TabIndex = 10;
			this.ApplyFiltersButton.Text = "Apply Filters";
			this.ApplyFiltersButton.UseVisualStyleBackColor = true;
			this.ApplyFiltersButton.Click += new System.EventHandler(this.ApplyFiltersButton_Click);
			// 
			// label1
			// 
			this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.label1.AutoSize = true;
			this.label1.Font = new System.Drawing.Font("Tahoma", 8.25F);
			this.label1.Location = new System.Drawing.Point(20, 12);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(60, 13);
			this.label1.TabIndex = 11;
			this.label1.Text = "Actor Filter";
			// 
			// label2
			// 
			this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.label2.AutoSize = true;
			this.label2.Font = new System.Drawing.Font("Tahoma", 8.25F);
			this.label2.Location = new System.Drawing.Point(4, 42);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(76, 13);
			this.label2.TabIndex = 12;
			this.label2.Text = "Property Filter";
			// 
			// label3
			// 
			this.label3.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.label3.AutoSize = true;
			this.label3.Font = new System.Drawing.Font("Tahoma", 8.25F);
			this.label3.Location = new System.Drawing.Point(26, 71);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(54, 13);
			this.label3.TabIndex = 13;
			this.label3.Text = "RPC Filter";
			// 
			// tabControl1
			// 
			this.tabControl1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.tabControl1.Controls.Add(this.tabPage1);
			this.tabControl1.Controls.Add(this.tabPage3);
			this.tabControl1.Controls.Add(this.tabPage4);
			this.tabControl1.Controls.Add(this.tabPage5);
			this.tabControl1.Location = new System.Drawing.Point(12, 56);
			this.tabControl1.Name = "tabControl1";
			this.tabControl1.SelectedIndex = 0;
			this.tabControl1.Size = new System.Drawing.Size(1231, 778);
			this.tabControl1.TabIndex = 14;
			// 
			// tabPage1
			// 
			this.tabPage1.Controls.Add(this.Details);
			this.tabPage1.Controls.Add(this.ApplyFiltersButton);
			this.tabPage1.Controls.Add(this.ConnectionListBox);
			this.tabPage1.Controls.Add(this.StackedBunchSizeRadioButton);
			this.tabPage1.Controls.Add(this.LineViewRadioButton);
			this.tabPage1.Controls.Add(this.panel1);
			this.tabPage1.Controls.Add(this.NetworkChart);
			this.tabPage1.Controls.Add(this.ChartListBox);
			this.tabPage1.Location = new System.Drawing.Point(4, 22);
			this.tabPage1.Name = "tabPage1";
			this.tabPage1.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage1.Size = new System.Drawing.Size(1223, 752);
			this.tabPage1.TabIndex = 0;
			this.tabPage1.Text = "Chart, Filters, Details";
			this.tabPage1.UseVisualStyleBackColor = true;
			// 
			// Details
			// 
			this.Details.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.Details.Controls.Add(this.tabPage2);
			this.Details.Controls.Add(this.tabPage6);
			this.Details.Controls.Add(this.tabPage8);
			this.Details.Location = new System.Drawing.Point(26, 368);
			this.Details.Name = "Details";
			this.Details.SelectedIndex = 0;
			this.Details.Size = new System.Drawing.Size(864, 384);
			this.Details.TabIndex = 22;
			// 
			// tabPage2
			// 
			this.tabPage2.Controls.Add(this.ActorSummaryView);
			this.tabPage2.Location = new System.Drawing.Point(4, 22);
			this.tabPage2.Name = "tabPage2";
			this.tabPage2.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage2.Size = new System.Drawing.Size(856, 358);
			this.tabPage2.TabIndex = 0;
			this.tabPage2.Text = "Summary";
			this.tabPage2.UseVisualStyleBackColor = true;
			// 
			// ActorSummaryView
			// 
			this.ActorSummaryView.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.ActorSummaryView.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.ActorSummaryView.Font = new System.Drawing.Font("Consolas", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.ActorSummaryView.Location = new System.Drawing.Point(3, 2);
			this.ActorSummaryView.Name = "ActorSummaryView";
			this.ActorSummaryView.Size = new System.Drawing.Size(787, 355);
			this.ActorSummaryView.TabIndex = 19;
			// 
			// tabPage6
			// 
			this.tabPage6.Controls.Add(this.ActorPerfPropsDetailsListView);
			this.tabPage6.Controls.Add(this.ActorPerfPropsListView);
			this.tabPage6.Location = new System.Drawing.Point(4, 22);
			this.tabPage6.Name = "tabPage6";
			this.tabPage6.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage6.Size = new System.Drawing.Size(856, 358);
			this.tabPage6.TabIndex = 1;
			this.tabPage6.Text = "Actors";
			this.tabPage6.UseVisualStyleBackColor = true;
			// 
			// ActorPerfPropsDetailsListView
			// 
			this.ActorPerfPropsDetailsListView.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ActorPerfPropsDetailsListView.Font = new System.Drawing.Font("Tahoma", 8.25F);
			this.ActorPerfPropsDetailsListView.FullRowSelect = true;
			this.ActorPerfPropsDetailsListView.GridLines = true;
			this.ActorPerfPropsDetailsListView.Location = new System.Drawing.Point(577, 0);
			this.ActorPerfPropsDetailsListView.Name = "ActorPerfPropsDetailsListView";
			this.ActorPerfPropsDetailsListView.Size = new System.Drawing.Size(283, 359);
			this.ActorPerfPropsDetailsListView.TabIndex = 22;
			this.ActorPerfPropsDetailsListView.UseCompatibleStateImageBehavior = false;
			this.ActorPerfPropsDetailsListView.View = System.Windows.Forms.View.Details;
			this.ActorPerfPropsDetailsListView.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.ActorPerfPropsDetailsListView_ColumnClick);
			// 
			// ActorPerfPropsListView
			// 
			this.ActorPerfPropsListView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ActorPerfPropsListView.Font = new System.Drawing.Font("Tahoma", 8.25F);
			this.ActorPerfPropsListView.FullRowSelect = true;
			this.ActorPerfPropsListView.GridLines = true;
			this.ActorPerfPropsListView.Location = new System.Drawing.Point(-4, 0);
			this.ActorPerfPropsListView.Name = "ActorPerfPropsListView";
			this.ActorPerfPropsListView.Size = new System.Drawing.Size(584, 359);
			this.ActorPerfPropsListView.TabIndex = 21;
			this.ActorPerfPropsListView.UseCompatibleStateImageBehavior = false;
			this.ActorPerfPropsListView.View = System.Windows.Forms.View.Details;
			this.ActorPerfPropsListView.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.ActorPerfPropsListView_ColumnClick);
			this.ActorPerfPropsListView.SelectedIndexChanged += new System.EventHandler(this.ActorPerfPropsListView_SelectedIndexChanged);
			// 
			// tabPage8
			// 
			this.tabPage8.Controls.Add(this.TokenDetailsView);
			this.tabPage8.Location = new System.Drawing.Point(4, 22);
			this.tabPage8.Name = "tabPage8";
			this.tabPage8.Size = new System.Drawing.Size(856, 358);
			this.tabPage8.TabIndex = 3;
			this.tabPage8.Text = "Token Details";
			this.tabPage8.UseVisualStyleBackColor = true;
			// 
			// TokenDetailsView
			// 
			this.TokenDetailsView.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.TokenDetailsView.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.TokenDetailsView.Font = new System.Drawing.Font("Consolas", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.TokenDetailsView.Location = new System.Drawing.Point(4, 3);
			this.TokenDetailsView.Name = "TokenDetailsView";
			this.TokenDetailsView.Size = new System.Drawing.Size(787, 355);
			this.TokenDetailsView.TabIndex = 21;
			// 
			// ConnectionListBox
			// 
			this.ConnectionListBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.ConnectionListBox.CheckOnClick = true;
			this.ConnectionListBox.Font = new System.Drawing.Font("Consolas", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.ConnectionListBox.FormattingEnabled = true;
			this.ConnectionListBox.Location = new System.Drawing.Point(893, 388);
			this.ConnectionListBox.Name = "ConnectionListBox";
			this.ConnectionListBox.Size = new System.Drawing.Size(322, 229);
			this.ConnectionListBox.TabIndex = 21;
			this.ConnectionListBox.SelectedValueChanged += new System.EventHandler(this.ConnectionListBox_SelectedValueChanged);
			// 
			// StackedBunchSizeRadioButton
			// 
			this.StackedBunchSizeRadioButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.StackedBunchSizeRadioButton.AutoSize = true;
			this.StackedBunchSizeRadioButton.Location = new System.Drawing.Point(1096, 7);
			this.StackedBunchSizeRadioButton.Name = "StackedBunchSizeRadioButton";
			this.StackedBunchSizeRadioButton.Size = new System.Drawing.Size(119, 17);
			this.StackedBunchSizeRadioButton.TabIndex = 18;
			this.StackedBunchSizeRadioButton.Text = "Stacked bunch size";
			this.StackedBunchSizeRadioButton.UseVisualStyleBackColor = true;
			this.StackedBunchSizeRadioButton.CheckedChanged += new System.EventHandler(this.StackedBunchSizeRadioButton_CheckChanged);
			// 
			// LineViewRadioButton
			// 
			this.LineViewRadioButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.LineViewRadioButton.AutoSize = true;
			this.LineViewRadioButton.Checked = true;
			this.LineViewRadioButton.Location = new System.Drawing.Point(1020, 7);
			this.LineViewRadioButton.Name = "LineViewRadioButton";
			this.LineViewRadioButton.Size = new System.Drawing.Size(70, 17);
			this.LineViewRadioButton.TabIndex = 17;
			this.LineViewRadioButton.TabStop = true;
			this.LineViewRadioButton.Text = "Line view";
			this.LineViewRadioButton.UseVisualStyleBackColor = true;
			this.LineViewRadioButton.CheckedChanged += new System.EventHandler(this.LineViewRadioButton_CheckChanged);
			// 
			// panel1
			// 
			this.panel1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.panel1.Controls.Add(this.RPCFilterBox);
			this.panel1.Controls.Add(this.PropertyFilterBox);
			this.panel1.Controls.Add(this.ActorFilterBox);
			this.panel1.Controls.Add(this.label1);
			this.panel1.Controls.Add(this.label3);
			this.panel1.Controls.Add(this.label2);
			this.panel1.Location = new System.Drawing.Point(893, 620);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(324, 98);
			this.panel1.TabIndex = 15;
			// 
			// RPCFilterBox
			// 
			this.RPCFilterBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.RPCFilterBox.AutoCompleteMode = System.Windows.Forms.AutoCompleteMode.Suggest;
			this.RPCFilterBox.AutoCompleteSource = System.Windows.Forms.AutoCompleteSource.ListItems;
			this.RPCFilterBox.FormattingEnabled = true;
			this.RPCFilterBox.Location = new System.Drawing.Point(81, 68);
			this.RPCFilterBox.Name = "RPCFilterBox";
			this.RPCFilterBox.Size = new System.Drawing.Size(243, 21);
			this.RPCFilterBox.TabIndex = 24;
			// 
			// PropertyFilterBox
			// 
			this.PropertyFilterBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.PropertyFilterBox.AutoCompleteMode = System.Windows.Forms.AutoCompleteMode.Suggest;
			this.PropertyFilterBox.AutoCompleteSource = System.Windows.Forms.AutoCompleteSource.ListItems;
			this.PropertyFilterBox.FormattingEnabled = true;
			this.PropertyFilterBox.Location = new System.Drawing.Point(81, 39);
			this.PropertyFilterBox.Name = "PropertyFilterBox";
			this.PropertyFilterBox.Size = new System.Drawing.Size(243, 21);
			this.PropertyFilterBox.TabIndex = 23;
			// 
			// ActorFilterBox
			// 
			this.ActorFilterBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.ActorFilterBox.AutoCompleteMode = System.Windows.Forms.AutoCompleteMode.Suggest;
			this.ActorFilterBox.AutoCompleteSource = System.Windows.Forms.AutoCompleteSource.ListItems;
			this.ActorFilterBox.FormattingEnabled = true;
			this.ActorFilterBox.Location = new System.Drawing.Point(81, 10);
			this.ActorFilterBox.Name = "ActorFilterBox";
			this.ActorFilterBox.Size = new System.Drawing.Size(243, 21);
			this.ActorFilterBox.TabIndex = 22;
			// 
			// tabPage3
			// 
			this.tabPage3.Controls.Add(this.ActorListView);
			this.tabPage3.Location = new System.Drawing.Point(4, 22);
			this.tabPage3.Name = "tabPage3";
			this.tabPage3.Size = new System.Drawing.Size(1223, 752);
			this.tabPage3.TabIndex = 2;
			this.tabPage3.Text = "All Actors";
			this.tabPage3.UseVisualStyleBackColor = true;
			// 
			// ActorListView
			// 
			this.ActorListView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ActorListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader1,
            this.columnHeader2,
            this.columnHeader3,
            this.columnHeader4,
            this.columnHeader5});
			this.ActorListView.Font = new System.Drawing.Font("Tahoma", 8.25F);
			this.ActorListView.FullRowSelect = true;
			this.ActorListView.GridLines = true;
			this.ActorListView.Location = new System.Drawing.Point(1, 2);
			this.ActorListView.Name = "ActorListView";
			this.ActorListView.Size = new System.Drawing.Size(1220, 750);
			this.ActorListView.TabIndex = 1;
			this.ActorListView.UseCompatibleStateImageBehavior = false;
			this.ActorListView.View = System.Windows.Forms.View.Details;
			this.ActorListView.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.ActorListView_ColumnClick);
			// 
			// columnHeader1
			// 
			this.columnHeader1.Text = "Total Size (KBytes)";
			// 
			// columnHeader2
			// 
			this.columnHeader2.Text = "Count";
			// 
			// columnHeader3
			// 
			this.columnHeader3.Text = "Average Size (Bytes)";
			// 
			// columnHeader4
			// 
			this.columnHeader4.Text = "Average Size Bits";
			// 
			// columnHeader5
			// 
			this.columnHeader5.Text = "Actor Class";
			this.columnHeader5.Width = 145;
			// 
			// tabPage4
			// 
			this.tabPage4.Controls.Add(this.PropertyListView);
			this.tabPage4.Location = new System.Drawing.Point(4, 22);
			this.tabPage4.Name = "tabPage4";
			this.tabPage4.Size = new System.Drawing.Size(1223, 752);
			this.tabPage4.TabIndex = 3;
			this.tabPage4.Text = "All Properties";
			this.tabPage4.UseVisualStyleBackColor = true;
			// 
			// PropertyListView
			// 
			this.PropertyListView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.PropertyListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.SizeBits,
            this.Count,
            this.AvgSizeBytes,
            this.AvgSizeBits,
            this.Property});
			this.PropertyListView.Font = new System.Drawing.Font("Tahoma", 8.25F);
			this.PropertyListView.FullRowSelect = true;
			this.PropertyListView.GridLines = true;
			this.PropertyListView.Location = new System.Drawing.Point(3, 3);
			this.PropertyListView.Name = "PropertyListView";
			this.PropertyListView.Size = new System.Drawing.Size(1220, 750);
			this.PropertyListView.TabIndex = 0;
			this.PropertyListView.UseCompatibleStateImageBehavior = false;
			this.PropertyListView.View = System.Windows.Forms.View.Details;
			this.PropertyListView.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.PropertyListView_ColumnClick);
			// 
			// SizeBits
			// 
			this.SizeBits.Text = "Total Size (KBytes)";
			// 
			// Count
			// 
			this.Count.Text = "Count";
			// 
			// AvgSizeBytes
			// 
			this.AvgSizeBytes.Text = "Average Size (Bytes)";
			// 
			// AvgSizeBits
			// 
			this.AvgSizeBits.Text = "Average Size Bits";
			// 
			// Property
			// 
			this.Property.Text = "Property";
			this.Property.Width = 145;
			// 
			// tabPage5
			// 
			this.tabPage5.Controls.Add(this.RPCListView);
			this.tabPage5.Location = new System.Drawing.Point(4, 22);
			this.tabPage5.Name = "tabPage5";
			this.tabPage5.Size = new System.Drawing.Size(1223, 752);
			this.tabPage5.TabIndex = 4;
			this.tabPage5.Text = "All RPCs";
			this.tabPage5.UseVisualStyleBackColor = true;
			// 
			// RPCListView
			// 
			this.RPCListView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.RPCListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader6,
            this.columnHeader7,
            this.columnHeader8,
            this.columnHeader9,
            this.columnHeader10});
			this.RPCListView.Font = new System.Drawing.Font("Tahoma", 8.25F);
			this.RPCListView.FullRowSelect = true;
			this.RPCListView.GridLines = true;
			this.RPCListView.Location = new System.Drawing.Point(1, 2);
			this.RPCListView.Name = "RPCListView";
			this.RPCListView.Size = new System.Drawing.Size(1220, 750);
			this.RPCListView.TabIndex = 1;
			this.RPCListView.UseCompatibleStateImageBehavior = false;
			this.RPCListView.View = System.Windows.Forms.View.Details;
			this.RPCListView.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.RPCListView_ColumnClick);
			// 
			// columnHeader6
			// 
			this.columnHeader6.Text = "Total Size (KBytes)";
			// 
			// columnHeader7
			// 
			this.columnHeader7.Text = "Count";
			// 
			// columnHeader8
			// 
			this.columnHeader8.Text = "Average Size (Bytes)";
			// 
			// columnHeader9
			// 
			this.columnHeader9.Text = "Average Size Bits";
			// 
			// columnHeader10
			// 
			this.columnHeader10.Text = "RPC";
			this.columnHeader10.Width = 145;
			// 
			// CurrentProgress
			// 
			this.CurrentProgress.Location = new System.Drawing.Point(485, 49);
			this.CurrentProgress.Name = "CurrentProgress";
			this.CurrentProgress.Size = new System.Drawing.Size(548, 23);
			this.CurrentProgress.TabIndex = 19;
			// 
			// ContextMenuStrip1
			// 
			this.ContextMenuStrip1.Name = "ContextMenuStrip1";
			this.ContextMenuStrip1.Size = new System.Drawing.Size(61, 4);
			// 
			// EarlyOutLabel
			// 
			this.EarlyOutLabel.AutoSize = true;
			this.EarlyOutLabel.Location = new System.Drawing.Point(315, 55);
			this.EarlyOutLabel.Name = "EarlyOutLabel";
			this.EarlyOutLabel.Size = new System.Drawing.Size(102, 13);
			this.EarlyOutLabel.TabIndex = 20;
			this.EarlyOutLabel.Text = "Max Profile Minutes:";
			// 
			// EarlyOutMinTextBox
			// 
			this.MaxProfileMinutesTextBox.Location = new System.Drawing.Point(419, 51);
			this.MaxProfileMinutesTextBox.Name = "EarlyOutMinTextBox";
			this.MaxProfileMinutesTextBox.Size = new System.Drawing.Size(58, 20);
			this.MaxProfileMinutesTextBox.TabIndex = 21;
			this.MaxProfileMinutesTextBox.TextChanged += new System.EventHandler(this.MaxProfileMinutesTextBox_TextChanged);
			// 
			// MainWindow
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(1255, 846);
			this.Controls.Add(this.MaxProfileMinutesTextBox);
			this.Controls.Add(this.EarlyOutLabel);
			this.Controls.Add(this.CurrentProgress);
			this.Controls.Add(this.tabControl1);
			this.Controls.Add(this.OpenButton);
			this.Name = "MainWindow";
			this.Text = "Network Profiler";
			((System.ComponentModel.ISupportInitialize)(this.NetworkChart)).EndInit();
			this.tabControl1.ResumeLayout(false);
			this.tabPage1.ResumeLayout(false);
			this.tabPage1.PerformLayout();
			this.Details.ResumeLayout(false);
			this.tabPage2.ResumeLayout(false);
			this.tabPage6.ResumeLayout(false);
			this.tabPage8.ResumeLayout(false);
			this.panel1.ResumeLayout(false);
			this.panel1.PerformLayout();
			this.tabPage3.ResumeLayout(false);
			this.tabPage4.ResumeLayout(false);
			this.tabPage5.ResumeLayout(false);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Button OpenButton;
		private System.Windows.Forms.DataVisualization.Charting.Chart NetworkChart;
		private System.Windows.Forms.CheckedListBox ChartListBox;
		private System.Windows.Forms.Button ApplyFiltersButton;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.TabControl tabControl1;
		private System.Windows.Forms.TabPage tabPage1;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.TabPage tabPage3;
		private System.Windows.Forms.TabPage tabPage4;
		private System.Windows.Forms.ListView PropertyListView;
		private System.Windows.Forms.ColumnHeader SizeBits;
		private System.Windows.Forms.ColumnHeader Count;
		private System.Windows.Forms.ColumnHeader AvgSizeBytes;
		private System.Windows.Forms.ColumnHeader Property;
		private System.Windows.Forms.TabPage tabPage5;
		private System.Windows.Forms.ColumnHeader AvgSizeBits;
		private System.Windows.Forms.ListView ActorListView;
		private System.Windows.Forms.ColumnHeader columnHeader1;
		private System.Windows.Forms.ColumnHeader columnHeader2;
		private System.Windows.Forms.ColumnHeader columnHeader3;
		private System.Windows.Forms.ColumnHeader columnHeader4;
		private System.Windows.Forms.ColumnHeader columnHeader5;
		private System.Windows.Forms.ListView RPCListView;
		private System.Windows.Forms.ColumnHeader columnHeader6;
		private System.Windows.Forms.ColumnHeader columnHeader7;
		private System.Windows.Forms.ColumnHeader columnHeader8;
		private System.Windows.Forms.ColumnHeader columnHeader9;
		private System.Windows.Forms.ColumnHeader columnHeader10;
		private System.Windows.Forms.RadioButton StackedBunchSizeRadioButton;
		private System.Windows.Forms.RadioButton LineViewRadioButton;
		private System.Windows.Forms.ProgressBar CurrentProgress;
		private System.Windows.Forms.TreeView ActorSummaryView;
		private System.Windows.Forms.CheckedListBox ConnectionListBox;
		private System.Windows.Forms.ContextMenuStrip ContextMenuStrip1;
		private System.Windows.Forms.ComboBox ActorFilterBox;
		private System.Windows.Forms.ComboBox PropertyFilterBox;
		private System.Windows.Forms.ComboBox RPCFilterBox;
		private System.Windows.Forms.TabControl Details;
		private System.Windows.Forms.TabPage tabPage2;
		private System.Windows.Forms.TabPage tabPage6;
		private System.Windows.Forms.TabPage tabPage8;
		private System.Windows.Forms.TreeView TokenDetailsView;
		private System.Windows.Forms.ListView ActorPerfPropsListView;
		private System.Windows.Forms.ListView ActorPerfPropsDetailsListView;
		private System.Windows.Forms.Label EarlyOutLabel;
		private System.Windows.Forms.TextBox MaxProfileMinutesTextBox;
	}
}

