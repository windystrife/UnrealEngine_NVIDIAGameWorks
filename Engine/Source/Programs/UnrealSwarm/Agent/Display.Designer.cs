// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace Agent
{
	partial class SwarmAgentWindow
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
			UnrealControls.OutputWindowDocument outputWindowDocument1 = new UnrealControls.OutputWindowDocument();
			System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle3 = new System.Windows.Forms.DataGridViewCellStyle();
			System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle4 = new System.Windows.Forms.DataGridViewCellStyle();
			System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle1 = new System.Windows.Forms.DataGridViewCellStyle();
			System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle2 = new System.Windows.Forms.DataGridViewCellStyle();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager( typeof( SwarmAgentWindow ) );
			this.AgentTabs = new System.Windows.Forms.TabControl();
			this.LoggingTab = new System.Windows.Forms.TabPage();
			this.LogOutputWindowView = new UnrealControls.OutputWindowView();
			this.VisualiserTab = new System.Windows.Forms.TabPage();
			this.ProgressBarGroupBox = new System.Windows.Forms.GroupBox();
			this.OverallProgressBar = new System.Windows.Forms.Label();
			this.KeyGroupBox = new System.Windows.Forms.GroupBox();
			this.ImportingLabel = new System.Windows.Forms.Label();
			this.ImportingBox = new System.Windows.Forms.Label();
			this.label1 = new System.Windows.Forms.Label();
			this.StartingBox = new System.Windows.Forms.Label();
			this.ExportingResultsLabel = new System.Windows.Forms.Label();
			this.ProcessingLabel = new System.Windows.Forms.Label();
			this.IrradianceCacheLabel = new System.Windows.Forms.Label();
			this.EmitPhotonsLabel = new System.Windows.Forms.Label();
			this.ExportingLabel = new System.Windows.Forms.Label();
			this.ExportingResultsBox = new System.Windows.Forms.Label();
			this.ProcessingBox = new System.Windows.Forms.Label();
			this.IrradianceCacheBox = new System.Windows.Forms.Label();
			this.EmitPhotonBox = new System.Windows.Forms.Label();
			this.ExportingBox = new System.Windows.Forms.Label();
			this.VisualiserGridView = new System.Windows.Forms.DataGridView();
			this.Machine = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.Progress = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.SettingsTab = new System.Windows.Forms.TabPage();
			this.SettingsPropertyGrid = new System.Windows.Forms.PropertyGrid();
			this.DeveloperSettingsTab = new System.Windows.Forms.TabPage();
			this.DeveloperSettingsPropertyGrid = new System.Windows.Forms.PropertyGrid();
			this.AgentMenu = new System.Windows.Forms.MenuStrip();
			this.FileMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.ExitMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.EditMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.ClearMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.CacheMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.CacheClearMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.CacheValidateMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.NetworkMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.NetworkPingCoordinatorMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.NetworkPingRemoteAgentsMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.DeveloperMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.DeveloperRestartQAAgentsMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.DeveloperRestartWorkerAgentsMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.HelpMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.aboutToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.SwarmAgentNotifyIcon = new System.Windows.Forms.NotifyIcon( this.components );
			this.TaskTrayContextMenu = new System.Windows.Forms.ContextMenuStrip( this.components );
			this.ExitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.AgentTabs.SuspendLayout();
			this.LoggingTab.SuspendLayout();
			this.VisualiserTab.SuspendLayout();
			this.ProgressBarGroupBox.SuspendLayout();
			this.KeyGroupBox.SuspendLayout();
			( ( System.ComponentModel.ISupportInitialize )( this.VisualiserGridView ) ).BeginInit();
			this.SettingsTab.SuspendLayout();
			this.DeveloperSettingsTab.SuspendLayout();
			this.AgentMenu.SuspendLayout();
			this.TaskTrayContextMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// AgentTabs
			// 
			this.AgentTabs.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.AgentTabs.Controls.Add( this.LoggingTab );
			this.AgentTabs.Controls.Add( this.VisualiserTab );
			this.AgentTabs.Controls.Add( this.SettingsTab );
			this.AgentTabs.Controls.Add( this.DeveloperSettingsTab );
			this.AgentTabs.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.AgentTabs.HotTrack = true;
			this.AgentTabs.Location = new System.Drawing.Point( 2, 27 );
			this.AgentTabs.Name = "AgentTabs";
			this.AgentTabs.SelectedIndex = 0;
			this.AgentTabs.Size = new System.Drawing.Size( 1281, 704 );
			this.AgentTabs.TabIndex = 0;
			// 
			// LoggingTab
			// 
			this.LoggingTab.Controls.Add( this.LogOutputWindowView );
			this.LoggingTab.Location = new System.Drawing.Point( 4, 23 );
			this.LoggingTab.Name = "LoggingTab";
			this.LoggingTab.Size = new System.Drawing.Size( 1273, 677 );
			this.LoggingTab.TabIndex = 2;
			this.LoggingTab.Text = "Log";
			this.LoggingTab.ToolTipText = "The log output from the Agent";
			this.LoggingTab.UseVisualStyleBackColor = true;
			// 
			// LogOutputWindowView
			// 
			this.LogOutputWindowView.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.LogOutputWindowView.AutoScroll = true;
			this.LogOutputWindowView.BackColor = System.Drawing.SystemColors.Window;
			this.LogOutputWindowView.Cursor = System.Windows.Forms.Cursors.Arrow;
			outputWindowDocument1.Text = "";
			this.LogOutputWindowView.Document = outputWindowDocument1;
			this.LogOutputWindowView.FindTextBackColor = System.Drawing.Color.Yellow;
			this.LogOutputWindowView.FindTextForeColor = System.Drawing.Color.Black;
			this.LogOutputWindowView.FindTextLineHighlight = System.Drawing.Color.FromArgb( ( ( int )( ( ( byte )( 239 ) ) ) ), ( ( int )( ( ( byte )( 248 ) ) ) ), ( ( int )( ( ( byte )( 255 ) ) ) ) );
			this.LogOutputWindowView.Font = new System.Drawing.Font( "Tahoma", 9F );
			this.LogOutputWindowView.ForeColor = System.Drawing.SystemColors.WindowText;
			this.LogOutputWindowView.Location = new System.Drawing.Point( 0, 0 );
			this.LogOutputWindowView.Name = "LogOutputWindowView";
			this.LogOutputWindowView.Size = new System.Drawing.Size( 1273, 677 );
			this.LogOutputWindowView.TabIndex = 0;
			// 
			// VisualiserTab
			// 
			this.VisualiserTab.Controls.Add( this.ProgressBarGroupBox );
			this.VisualiserTab.Controls.Add( this.KeyGroupBox );
			this.VisualiserTab.Controls.Add( this.VisualiserGridView );
			this.VisualiserTab.Location = new System.Drawing.Point( 4, 23 );
			this.VisualiserTab.Name = "VisualiserTab";
			this.VisualiserTab.Padding = new System.Windows.Forms.Padding( 3 );
			this.VisualiserTab.Size = new System.Drawing.Size( 1273, 677 );
			this.VisualiserTab.TabIndex = 0;
			this.VisualiserTab.Tag = "";
			this.VisualiserTab.Text = "Swarm Status";
			this.VisualiserTab.ToolTipText = "A detailed set of progress bars";
			this.VisualiserTab.UseVisualStyleBackColor = true;
			// 
			// ProgressBarGroupBox
			// 
			this.ProgressBarGroupBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ProgressBarGroupBox.Controls.Add( this.OverallProgressBar );
			this.ProgressBarGroupBox.Location = new System.Drawing.Point( 6, 562 );
			this.ProgressBarGroupBox.Name = "ProgressBarGroupBox";
			this.ProgressBarGroupBox.Size = new System.Drawing.Size( 1260, 51 );
			this.ProgressBarGroupBox.TabIndex = 2;
			this.ProgressBarGroupBox.TabStop = false;
			this.ProgressBarGroupBox.Text = "Distributed Progress";
			// 
			// OverallProgressBar
			// 
			this.OverallProgressBar.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.OverallProgressBar.Location = new System.Drawing.Point( 6, 18 );
			this.OverallProgressBar.Name = "OverallProgressBar";
			this.OverallProgressBar.Size = new System.Drawing.Size( 1248, 22 );
			this.OverallProgressBar.TabIndex = 0;
			this.OverallProgressBar.Text = "0%";
			this.OverallProgressBar.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			this.OverallProgressBar.Paint += new System.Windows.Forms.PaintEventHandler( this.OverallProgressBarPaint );
			// 
			// KeyGroupBox
			// 
			this.KeyGroupBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.KeyGroupBox.Controls.Add( this.ImportingLabel );
			this.KeyGroupBox.Controls.Add( this.ImportingBox );
			this.KeyGroupBox.Controls.Add( this.label1 );
			this.KeyGroupBox.Controls.Add( this.StartingBox );
			this.KeyGroupBox.Controls.Add( this.ExportingResultsLabel );
			this.KeyGroupBox.Controls.Add( this.ProcessingLabel );
			this.KeyGroupBox.Controls.Add( this.IrradianceCacheLabel );
			this.KeyGroupBox.Controls.Add( this.EmitPhotonsLabel );
			this.KeyGroupBox.Controls.Add( this.ExportingLabel );
			this.KeyGroupBox.Controls.Add( this.ExportingResultsBox );
			this.KeyGroupBox.Controls.Add( this.ProcessingBox );
			this.KeyGroupBox.Controls.Add( this.IrradianceCacheBox );
			this.KeyGroupBox.Controls.Add( this.EmitPhotonBox );
			this.KeyGroupBox.Controls.Add( this.ExportingBox );
			this.KeyGroupBox.Location = new System.Drawing.Point( 6, 619 );
			this.KeyGroupBox.Name = "KeyGroupBox";
			this.KeyGroupBox.Size = new System.Drawing.Size( 1260, 51 );
			this.KeyGroupBox.TabIndex = 1;
			this.KeyGroupBox.TabStop = false;
			this.KeyGroupBox.Text = "Key";
			// 
			// ImportingLabel
			// 
			this.ImportingLabel.AutoSize = true;
			this.ImportingLabel.Location = new System.Drawing.Point( 1024, 17 );
			this.ImportingLabel.Name = "ImportingLabel";
			this.ImportingLabel.Size = new System.Drawing.Size( 102, 14 );
			this.ImportingLabel.TabIndex = 13;
			this.ImportingLabel.Text = "Importing Results";
			// 
			// ImportingBox
			// 
			this.ImportingBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.ImportingBox.BackColor = System.Drawing.Color.Gold;
			this.ImportingBox.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.ImportingBox.Location = new System.Drawing.Point( 994, 17 );
			this.ImportingBox.Name = "ImportingBox";
			this.ImportingBox.Size = new System.Drawing.Size( 24, 20 );
			this.ImportingBox.TabIndex = 12;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point( 221, 18 );
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size( 107, 14 );
			this.label1.TabIndex = 11;
			this.label1.Text = "Lightmass Starting";
			// 
			// StartingBox
			// 
			this.StartingBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.StartingBox.BackColor = System.Drawing.Color.Maroon;
			this.StartingBox.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.StartingBox.Location = new System.Drawing.Point( 191, 18 );
			this.StartingBox.Name = "StartingBox";
			this.StartingBox.Size = new System.Drawing.Size( 24, 20 );
			this.StartingBox.TabIndex = 10;
			// 
			// ExportingResultsLabel
			// 
			this.ExportingResultsLabel.AutoSize = true;
			this.ExportingResultsLabel.Location = new System.Drawing.Point( 876, 17 );
			this.ExportingResultsLabel.Name = "ExportingResultsLabel";
			this.ExportingResultsLabel.Size = new System.Drawing.Size( 101, 14 );
			this.ExportingResultsLabel.TabIndex = 9;
			this.ExportingResultsLabel.Text = "Exporting Results";
			// 
			// ProcessingLabel
			// 
			this.ProcessingLabel.AutoSize = true;
			this.ProcessingLabel.Location = new System.Drawing.Point( 713, 19 );
			this.ProcessingLabel.Name = "ProcessingLabel";
			this.ProcessingLabel.Size = new System.Drawing.Size( 118, 14 );
			this.ProcessingLabel.TabIndex = 8;
			this.ProcessingLabel.Text = "Processing Mappings";
			// 
			// IrradianceCacheLabel
			// 
			this.IrradianceCacheLabel.AutoSize = true;
			this.IrradianceCacheLabel.Location = new System.Drawing.Point( 551, 20 );
			this.IrradianceCacheLabel.Name = "IrradianceCacheLabel";
			this.IrradianceCacheLabel.Size = new System.Drawing.Size( 108, 14 );
			this.IrradianceCacheLabel.TabIndex = 7;
			this.IrradianceCacheLabel.Text = "Collecting Photons";
			// 
			// EmitPhotonsLabel
			// 
			this.EmitPhotonsLabel.AutoSize = true;
			this.EmitPhotonsLabel.Location = new System.Drawing.Point( 392, 19 );
			this.EmitPhotonsLabel.Name = "EmitPhotonsLabel";
			this.EmitPhotonsLabel.Size = new System.Drawing.Size( 101, 14 );
			this.EmitPhotonsLabel.TabIndex = 6;
			this.EmitPhotonsLabel.Text = "Emitting Photons";
			// 
			// ExportingLabel
			// 
			this.ExportingLabel.AutoSize = true;
			this.ExportingLabel.Location = new System.Drawing.Point( 60, 19 );
			this.ExportingLabel.Name = "ExportingLabel";
			this.ExportingLabel.Size = new System.Drawing.Size( 95, 14 );
			this.ExportingLabel.TabIndex = 5;
			this.ExportingLabel.Text = "Exporting scene";
			// 
			// ExportingResultsBox
			// 
			this.ExportingResultsBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.ExportingResultsBox.BackColor = System.Drawing.Color.Cyan;
			this.ExportingResultsBox.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.ExportingResultsBox.Location = new System.Drawing.Point( 846, 17 );
			this.ExportingResultsBox.Name = "ExportingResultsBox";
			this.ExportingResultsBox.Size = new System.Drawing.Size( 24, 20 );
			this.ExportingResultsBox.TabIndex = 4;
			// 
			// ProcessingBox
			// 
			this.ProcessingBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.ProcessingBox.BackColor = System.Drawing.Color.MediumBlue;
			this.ProcessingBox.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.ProcessingBox.Location = new System.Drawing.Point( 683, 19 );
			this.ProcessingBox.Name = "ProcessingBox";
			this.ProcessingBox.Size = new System.Drawing.Size( 24, 20 );
			this.ProcessingBox.TabIndex = 3;
			// 
			// IrradianceCacheBox
			// 
			this.IrradianceCacheBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.IrradianceCacheBox.BackColor = System.Drawing.Color.Orange;
			this.IrradianceCacheBox.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.IrradianceCacheBox.Location = new System.Drawing.Point( 521, 19 );
			this.IrradianceCacheBox.Name = "IrradianceCacheBox";
			this.IrradianceCacheBox.Size = new System.Drawing.Size( 24, 20 );
			this.IrradianceCacheBox.TabIndex = 2;
			// 
			// EmitPhotonBox
			// 
			this.EmitPhotonBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.EmitPhotonBox.BackColor = System.Drawing.Color.Red;
			this.EmitPhotonBox.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.EmitPhotonBox.Location = new System.Drawing.Point( 362, 19 );
			this.EmitPhotonBox.Name = "EmitPhotonBox";
			this.EmitPhotonBox.Size = new System.Drawing.Size( 24, 20 );
			this.EmitPhotonBox.TabIndex = 1;
			// 
			// ExportingBox
			// 
			this.ExportingBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.ExportingBox.BackColor = System.Drawing.Color.WhiteSmoke;
			this.ExportingBox.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.ExportingBox.Location = new System.Drawing.Point( 30, 18 );
			this.ExportingBox.Name = "ExportingBox";
			this.ExportingBox.Size = new System.Drawing.Size( 24, 20 );
			this.ExportingBox.TabIndex = 0;
			// 
			// VisualiserGridView
			// 
			this.VisualiserGridView.AllowUserToAddRows = false;
			this.VisualiserGridView.AllowUserToDeleteRows = false;
			this.VisualiserGridView.AllowUserToResizeColumns = false;
			this.VisualiserGridView.AllowUserToResizeRows = false;
			this.VisualiserGridView.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.VisualiserGridView.BackgroundColor = System.Drawing.SystemColors.Control;
			this.VisualiserGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
			this.VisualiserGridView.Columns.AddRange( new System.Windows.Forms.DataGridViewColumn[] {
            this.Machine,
            this.Progress} );
			dataGridViewCellStyle3.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
			dataGridViewCellStyle3.BackColor = System.Drawing.SystemColors.Window;
			dataGridViewCellStyle3.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			dataGridViewCellStyle3.ForeColor = System.Drawing.SystemColors.ControlText;
			dataGridViewCellStyle3.SelectionBackColor = System.Drawing.SystemColors.ControlLight;
			dataGridViewCellStyle3.SelectionForeColor = System.Drawing.SystemColors.ControlText;
			dataGridViewCellStyle3.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
			this.VisualiserGridView.DefaultCellStyle = dataGridViewCellStyle3;
			this.VisualiserGridView.Location = new System.Drawing.Point( 2, 2 );
			this.VisualiserGridView.Margin = new System.Windows.Forms.Padding( 0 );
			this.VisualiserGridView.MultiSelect = false;
			this.VisualiserGridView.Name = "VisualiserGridView";
			this.VisualiserGridView.ReadOnly = true;
			this.VisualiserGridView.RowHeadersVisible = false;
			this.VisualiserGridView.RowHeadersWidthSizeMode = System.Windows.Forms.DataGridViewRowHeadersWidthSizeMode.DisableResizing;
			dataGridViewCellStyle4.SelectionBackColor = System.Drawing.Color.Silver;
			dataGridViewCellStyle4.SelectionForeColor = System.Drawing.Color.Black;
			this.VisualiserGridView.RowsDefaultCellStyle = dataGridViewCellStyle4;
			this.VisualiserGridView.RowTemplate.DefaultCellStyle.SelectionBackColor = System.Drawing.Color.Silver;
			this.VisualiserGridView.RowTemplate.DefaultCellStyle.SelectionForeColor = System.Drawing.Color.Black;
			this.VisualiserGridView.RowTemplate.Height = 40;
			this.VisualiserGridView.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.CellSelect;
			this.VisualiserGridView.ShowCellErrors = false;
			this.VisualiserGridView.ShowEditingIcon = false;
			this.VisualiserGridView.ShowRowErrors = false;
			this.VisualiserGridView.Size = new System.Drawing.Size( 1270, 557 );
			this.VisualiserGridView.TabIndex = 0;
			this.VisualiserGridView.MouseWheel += new System.Windows.Forms.MouseEventHandler( this.MouseWheelHandler );
			this.VisualiserGridView.MouseMove += new System.Windows.Forms.MouseEventHandler( this.MouseMoveHandler );
			this.VisualiserGridView.CellPainting += new System.Windows.Forms.DataGridViewCellPaintingEventHandler( this.ProgressCellPaint );
			this.VisualiserGridView.MouseLeave += new System.EventHandler( this.MouseLeaveHandler );
			this.VisualiserGridView.Resize += new System.EventHandler( this.GridViewResized );
			this.VisualiserGridView.SelectionChanged += new System.EventHandler( this.GridViewSelectionChanged );
			// 
			// Machine
			// 
			this.Machine.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
			dataGridViewCellStyle1.BackColor = System.Drawing.SystemColors.ControlLightLight;
			dataGridViewCellStyle1.ForeColor = System.Drawing.SystemColors.InfoText;
			dataGridViewCellStyle1.SelectionBackColor = System.Drawing.SystemColors.ControlLight;
			dataGridViewCellStyle1.SelectionForeColor = System.Drawing.SystemColors.ControlLightLight;
			this.Machine.DefaultCellStyle = dataGridViewCellStyle1;
			this.Machine.HeaderText = "Machine";
			this.Machine.Name = "Machine";
			this.Machine.ReadOnly = true;
			this.Machine.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.NotSortable;
			this.Machine.Width = 57;
			// 
			// Progress
			// 
			this.Progress.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.Fill;
			dataGridViewCellStyle2.BackColor = System.Drawing.SystemColors.ControlLightLight;
			dataGridViewCellStyle2.ForeColor = System.Drawing.SystemColors.InfoText;
			dataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.ControlLight;
			dataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.ControlDarkDark;
			this.Progress.DefaultCellStyle = dataGridViewCellStyle2;
			this.Progress.HeaderText = "";
			this.Progress.Name = "Progress";
			this.Progress.ReadOnly = true;
			this.Progress.Resizable = System.Windows.Forms.DataGridViewTriState.True;
			this.Progress.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.NotSortable;
			// 
			// SettingsTab
			// 
			this.SettingsTab.Controls.Add( this.SettingsPropertyGrid );
			this.SettingsTab.Location = new System.Drawing.Point( 4, 23 );
			this.SettingsTab.Name = "SettingsTab";
			this.SettingsTab.Padding = new System.Windows.Forms.Padding( 3 );
			this.SettingsTab.Size = new System.Drawing.Size( 1273, 677 );
			this.SettingsTab.TabIndex = 1;
			this.SettingsTab.Text = "Settings";
			this.SettingsTab.ToolTipText = "All settings local to the agent";
			this.SettingsTab.UseVisualStyleBackColor = true;
			// 
			// SettingsPropertyGrid
			// 
			this.SettingsPropertyGrid.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.SettingsPropertyGrid.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.SettingsPropertyGrid.Location = new System.Drawing.Point( -2, 0 );
			this.SettingsPropertyGrid.Name = "SettingsPropertyGrid";
			this.SettingsPropertyGrid.Size = new System.Drawing.Size( 1270, 677 );
			this.SettingsPropertyGrid.TabIndex = 0;
			this.SettingsPropertyGrid.PropertyValueChanged += new System.Windows.Forms.PropertyValueChangedEventHandler( this.OptionsValueChanged );
			// 
			// DeveloperSettingsTab
			// 
			this.DeveloperSettingsTab.Controls.Add( this.DeveloperSettingsPropertyGrid );
			this.DeveloperSettingsTab.Location = new System.Drawing.Point( 4, 23 );
			this.DeveloperSettingsTab.Name = "DeveloperSettingsTab";
			this.DeveloperSettingsTab.Padding = new System.Windows.Forms.Padding( 3 );
			this.DeveloperSettingsTab.Size = new System.Drawing.Size( 1273, 677 );
			this.DeveloperSettingsTab.TabIndex = 3;
			this.DeveloperSettingsTab.Text = "DeveloperSettings";
			this.DeveloperSettingsTab.UseVisualStyleBackColor = true;
			// 
			// DeveloperSettingsPropertyGrid
			// 
			this.DeveloperSettingsPropertyGrid.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.DeveloperSettingsPropertyGrid.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.DeveloperSettingsPropertyGrid.Location = new System.Drawing.Point( -2, 0 );
			this.DeveloperSettingsPropertyGrid.Name = "DeveloperSettingsPropertyGrid";
			this.DeveloperSettingsPropertyGrid.Size = new System.Drawing.Size( 1270, 677 );
			this.DeveloperSettingsPropertyGrid.TabIndex = 1;
			this.DeveloperSettingsPropertyGrid.PropertyValueChanged += new System.Windows.Forms.PropertyValueChangedEventHandler( this.OptionsValueChanged );
			// 
			// AgentMenu
			// 
			this.AgentMenu.Items.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.FileMenuItem,
            this.EditMenuItem,
            this.CacheMenuItem,
            this.NetworkMenuItem,
            this.DeveloperMenuItem,
            this.HelpMenuItem} );
			this.AgentMenu.Location = new System.Drawing.Point( 0, 0 );
			this.AgentMenu.Name = "AgentMenu";
			this.AgentMenu.Size = new System.Drawing.Size( 1284, 24 );
			this.AgentMenu.TabIndex = 1;
			this.AgentMenu.Text = "File";
			// 
			// FileMenuItem
			// 
			this.FileMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.ExitMenuItem} );
			this.FileMenuItem.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.FileMenuItem.Name = "FileMenuItem";
			this.FileMenuItem.Size = new System.Drawing.Size( 36, 20 );
			this.FileMenuItem.Text = "File";
			// 
			// ExitMenuItem
			// 
			this.ExitMenuItem.Name = "ExitMenuItem";
			this.ExitMenuItem.Size = new System.Drawing.Size( 94, 22 );
			this.ExitMenuItem.Text = "E&xit";
			this.ExitMenuItem.Click += new System.EventHandler( this.ClickExitMenu );
			// 
			// EditMenuItem
			// 
			this.EditMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.ClearMenuItem} );
			this.EditMenuItem.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.EditMenuItem.Name = "EditMenuItem";
			this.EditMenuItem.Size = new System.Drawing.Size( 40, 20 );
			this.EditMenuItem.Text = "Edit";
			// 
			// ClearMenuItem
			// 
			this.ClearMenuItem.Name = "ClearMenuItem";
			this.ClearMenuItem.Size = new System.Drawing.Size( 167, 22 );
			this.ClearMenuItem.Text = "Clear log window";
			this.ClearMenuItem.Click += new System.EventHandler( this.EditClearClick );
			// 
			// CacheMenuItem
			// 
			this.CacheMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.CacheClearMenuItem,
            this.CacheValidateMenuItem} );
			this.CacheMenuItem.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.CacheMenuItem.Name = "CacheMenuItem";
			this.CacheMenuItem.Size = new System.Drawing.Size( 52, 20 );
			this.CacheMenuItem.Text = "Cache";
			// 
			// CacheClearMenuItem
			// 
			this.CacheClearMenuItem.Name = "CacheClearMenuItem";
			this.CacheClearMenuItem.Size = new System.Drawing.Size( 117, 22 );
			this.CacheClearMenuItem.Text = "Clean";
			this.CacheClearMenuItem.Click += new System.EventHandler( this.CacheClearClick );
			// 
			// CacheValidateMenuItem
			// 
			this.CacheValidateMenuItem.Name = "CacheValidateMenuItem";
			this.CacheValidateMenuItem.Size = new System.Drawing.Size( 117, 22 );
			this.CacheValidateMenuItem.Text = "Validate";
			this.CacheValidateMenuItem.Click += new System.EventHandler( this.CacheValidateClick );
			// 
			// NetworkMenuItem
			// 
			this.NetworkMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.NetworkPingCoordinatorMenuItem,
            this.NetworkPingRemoteAgentsMenuItem} );
			this.NetworkMenuItem.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.NetworkMenuItem.Name = "NetworkMenuItem";
			this.NetworkMenuItem.Size = new System.Drawing.Size( 66, 20 );
			this.NetworkMenuItem.Text = "Network";
			// 
			// NetworkPingCoordinatorMenuItem
			// 
			this.NetworkPingCoordinatorMenuItem.Name = "NetworkPingCoordinatorMenuItem";
			this.NetworkPingCoordinatorMenuItem.Size = new System.Drawing.Size( 187, 22 );
			this.NetworkPingCoordinatorMenuItem.Text = "Ping Coordinator";
			this.NetworkPingCoordinatorMenuItem.Click += new System.EventHandler( this.NetworkPingCoordinatorMenuItem_Click );
			// 
			// NetworkPingRemoteAgentsMenuItem
			// 
			this.NetworkPingRemoteAgentsMenuItem.Name = "NetworkPingRemoteAgentsMenuItem";
			this.NetworkPingRemoteAgentsMenuItem.Size = new System.Drawing.Size( 187, 22 );
			this.NetworkPingRemoteAgentsMenuItem.Text = "Ping Remote Agents";
			this.NetworkPingRemoteAgentsMenuItem.Click += new System.EventHandler( this.NetworkPingRemoteAgentsMenuItem_Click );
			// 
			// DeveloperMenuItem
			// 
			this.DeveloperMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.DeveloperRestartQAAgentsMenuItem,
            this.DeveloperRestartWorkerAgentsMenuItem} );
			this.DeveloperMenuItem.Font = new System.Drawing.Font( "Tahoma", 9F );
			this.DeveloperMenuItem.Name = "DeveloperMenuItem";
			this.DeveloperMenuItem.Size = new System.Drawing.Size( 74, 20 );
			this.DeveloperMenuItem.Text = "Developer";
			// 
			// DeveloperRestartQAAgentsMenuItem
			// 
			this.DeveloperRestartQAAgentsMenuItem.Name = "DeveloperRestartQAAgentsMenuItem";
			this.DeveloperRestartQAAgentsMenuItem.Size = new System.Drawing.Size( 200, 22 );
			this.DeveloperRestartQAAgentsMenuItem.Text = "Restart QA Agents";
			this.DeveloperRestartQAAgentsMenuItem.Click += new System.EventHandler( this.DeveloperRestartQAAgentsMenuItem_Click );
			// 
			// DeveloperRestartWorkerAgentsMenuItem
			// 
			this.DeveloperRestartWorkerAgentsMenuItem.Name = "DeveloperRestartWorkerAgentsMenuItem";
			this.DeveloperRestartWorkerAgentsMenuItem.Size = new System.Drawing.Size( 200, 22 );
			this.DeveloperRestartWorkerAgentsMenuItem.Text = "Restart Worker Agents";
			this.DeveloperRestartWorkerAgentsMenuItem.Click += new System.EventHandler( this.DeveloperRestartWorkerAgentsMenuItem_Click );
			// 
			// HelpMenuItem
			// 
			this.HelpMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.aboutToolStripMenuItem} );
			this.HelpMenuItem.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.HelpMenuItem.Name = "HelpMenuItem";
			this.HelpMenuItem.Size = new System.Drawing.Size( 43, 20 );
			this.HelpMenuItem.Text = "Help";
			// 
			// aboutToolStripMenuItem
			// 
			this.aboutToolStripMenuItem.Name = "aboutToolStripMenuItem";
			this.aboutToolStripMenuItem.Size = new System.Drawing.Size( 108, 22 );
			this.aboutToolStripMenuItem.Text = "About";
			this.aboutToolStripMenuItem.Click += new System.EventHandler( this.MenuAboutClick );
			// 
			// SwarmAgentNotifyIcon
			// 
#if !__MonoCS__
			this.SwarmAgentNotifyIcon.ContextMenuStrip = this.TaskTrayContextMenu;
			this.SwarmAgentNotifyIcon.Icon = ( ( System.Drawing.Icon )( resources.GetObject( "SwarmAgentNotifyIcon.Icon" ) ) );
			this.SwarmAgentNotifyIcon.Text = "Swarm Agent";
			this.SwarmAgentNotifyIcon.Visible = true;
			this.SwarmAgentNotifyIcon.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler( this.ShowSwarmAgentWindow );
#endif
			// 
			// TaskTrayContextMenu
			// 
			this.TaskTrayContextMenu.Items.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.ExitToolStripMenuItem} );
			this.TaskTrayContextMenu.Name = "TaskTrayContextMenu";
			this.TaskTrayContextMenu.Size = new System.Drawing.Size( 93, 26 );
			// 
			// ExitToolStripMenuItem
			// 
			this.ExitToolStripMenuItem.Name = "ExitToolStripMenuItem";
			this.ExitToolStripMenuItem.Size = new System.Drawing.Size( 92, 22 );
			this.ExitToolStripMenuItem.Text = "Exit";
			this.ExitToolStripMenuItem.Click += new System.EventHandler( this.ClickExitMenu );
			// 
			// SwarmAgentWindow
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 14F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 1284, 732 );
			this.Controls.Add( this.AgentTabs );
			this.Controls.Add( this.AgentMenu );
			this.Font = new System.Drawing.Font( "Tahoma", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
#if !__MonoCS__
			this.Icon = ( ( System.Drawing.Icon )( resources.GetObject( "$this.Icon" ) ) );
#endif
			this.MainMenuStrip = this.AgentMenu;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.MinimumSize = new System.Drawing.Size( 300, 200 );
			this.Name = "SwarmAgentWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.Manual;
			this.Text = "Swarm Agent";
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler( this.SwarmAgentWindowClosing );
			this.AgentTabs.ResumeLayout( false );
			this.LoggingTab.ResumeLayout( false );
			this.VisualiserTab.ResumeLayout( false );
			this.ProgressBarGroupBox.ResumeLayout( false );
			this.KeyGroupBox.ResumeLayout( false );
			this.KeyGroupBox.PerformLayout();
			( ( System.ComponentModel.ISupportInitialize )( this.VisualiserGridView ) ).EndInit();
			this.SettingsTab.ResumeLayout( false );
			this.DeveloperSettingsTab.ResumeLayout( false );
			this.AgentMenu.ResumeLayout( false );
			this.AgentMenu.PerformLayout();
			this.TaskTrayContextMenu.ResumeLayout( false );
			this.ResumeLayout( false );
			this.PerformLayout();

		}

		#endregion

        private System.Windows.Forms.TabControl AgentTabs;
        private System.Windows.Forms.TabPage VisualiserTab;
        private System.Windows.Forms.TabPage SettingsTab;
        private System.Windows.Forms.TabPage LoggingTab;
		private System.Windows.Forms.PropertyGrid SettingsPropertyGrid;
        private System.Windows.Forms.MenuStrip AgentMenu;
        private System.Windows.Forms.ToolStripMenuItem FileMenuItem;
        private System.Windows.Forms.ToolStripMenuItem ExitMenuItem;
		private System.Windows.Forms.ToolStripMenuItem HelpMenuItem;
        public System.Windows.Forms.NotifyIcon SwarmAgentNotifyIcon;
        private System.Windows.Forms.ContextMenuStrip TaskTrayContextMenu;
		private System.Windows.Forms.ToolStripMenuItem ExitToolStripMenuItem;
		private System.Windows.Forms.DataGridView VisualiserGridView;
		private System.Windows.Forms.ToolStripMenuItem EditMenuItem;
		private System.Windows.Forms.ToolStripMenuItem ClearMenuItem;
		public UnrealControls.OutputWindowView LogOutputWindowView;
		private System.Windows.Forms.DataGridViewTextBoxColumn Machine;
		private System.Windows.Forms.DataGridViewTextBoxColumn Progress;
		private System.Windows.Forms.GroupBox KeyGroupBox;
		private System.Windows.Forms.Label ExportingBox;
		private System.Windows.Forms.Label ExportingResultsBox;
		private System.Windows.Forms.Label ProcessingBox;
		private System.Windows.Forms.Label IrradianceCacheBox;
		private System.Windows.Forms.Label EmitPhotonBox;
		private System.Windows.Forms.Label ProcessingLabel;
		private System.Windows.Forms.Label IrradianceCacheLabel;
		private System.Windows.Forms.Label EmitPhotonsLabel;
		private System.Windows.Forms.Label ExportingLabel;
		private System.Windows.Forms.Label ExportingResultsLabel;
		private System.Windows.Forms.GroupBox ProgressBarGroupBox;
		private System.Windows.Forms.Label OverallProgressBar;
		private System.Windows.Forms.ToolStripMenuItem aboutToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem CacheMenuItem;
		private System.Windows.Forms.ToolStripMenuItem CacheClearMenuItem;
		private System.Windows.Forms.ToolStripMenuItem CacheValidateMenuItem;
		private System.Windows.Forms.ToolStripMenuItem NetworkMenuItem;
		private System.Windows.Forms.ToolStripMenuItem NetworkPingCoordinatorMenuItem;
		private System.Windows.Forms.ToolStripMenuItem NetworkPingRemoteAgentsMenuItem;
		private System.Windows.Forms.ToolTip BarToolTip;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label StartingBox;
		private System.Windows.Forms.Label ImportingLabel;
		private System.Windows.Forms.Label ImportingBox;
		private System.Windows.Forms.ToolStripMenuItem DeveloperMenuItem;
		private System.Windows.Forms.ToolStripMenuItem DeveloperRestartQAAgentsMenuItem;
		private System.Windows.Forms.ToolStripMenuItem DeveloperRestartWorkerAgentsMenuItem;
		private System.Windows.Forms.TabPage DeveloperSettingsTab;
		private System.Windows.Forms.PropertyGrid DeveloperSettingsPropertyGrid;
	}
}

