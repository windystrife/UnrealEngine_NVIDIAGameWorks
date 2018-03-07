// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class MainWindow
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainWindow));
			this.NotifyIcon = new System.Windows.Forms.NotifyIcon(this.components);
			this.NotifyMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.NotifyMenu_OpenUnrealGameSync = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.NotifyMenu_SyncNow = new System.Windows.Forms.ToolStripMenuItem();
			this.NotifyMenu_LaunchEditor = new System.Windows.Forms.ToolStripMenuItem();
			this.NotifyMenu_ExitSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.NotifyMenu_Exit = new System.Windows.Forms.ToolStripMenuItem();
			this.TabPanel = new System.Windows.Forms.Panel();
			this.DefaultControl = new UnrealGameSync.StatusPanel();
			this.TabMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.TabMenu_OpenProject = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_RecentProjects = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_Recent_Separator = new System.Windows.Forms.ToolStripSeparator();
			this.TabMenu_RecentProjects_ClearList = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
			this.labelsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_TabNames_Stream = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_TabNames_WorkspaceName = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_TabNames_WorkspaceRoot = new System.Windows.Forms.ToolStripMenuItem();
			this.TabMenu_TabNames_ProjectFile = new System.Windows.Forms.ToolStripMenuItem();
			this.TabControl = new UnrealGameSync.TabControl();
			this.NotifyMenu.SuspendLayout();
			this.TabPanel.SuspendLayout();
			this.TabMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// NotifyIcon
			// 
			this.NotifyIcon.ContextMenuStrip = this.NotifyMenu;
			this.NotifyIcon.Icon = ((System.Drawing.Icon)(resources.GetObject("NotifyIcon.Icon")));
			this.NotifyIcon.Text = "UnrealGameSync";
			this.NotifyIcon.Visible = true;
			this.NotifyIcon.DoubleClick += new System.EventHandler(this.NotifyIcon_DoubleClick);
			this.NotifyIcon.MouseDown += new System.Windows.Forms.MouseEventHandler(this.NotifyIcon_MouseDown);
			// 
			// NotifyMenu
			// 
			this.NotifyMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.NotifyMenu_OpenUnrealGameSync,
            this.toolStripSeparator1,
            this.NotifyMenu_SyncNow,
            this.NotifyMenu_LaunchEditor,
            this.NotifyMenu_ExitSeparator,
            this.NotifyMenu_Exit});
			this.NotifyMenu.Name = "NotifyMenu";
			this.NotifyMenu.Size = new System.Drawing.Size(197, 104);
			// 
			// NotifyMenu_OpenUnrealGameSync
			// 
			this.NotifyMenu_OpenUnrealGameSync.Name = "NotifyMenu_OpenUnrealGameSync";
			this.NotifyMenu_OpenUnrealGameSync.Size = new System.Drawing.Size(196, 22);
			this.NotifyMenu_OpenUnrealGameSync.Text = "Open UnrealGameSync";
			this.NotifyMenu_OpenUnrealGameSync.Click += new System.EventHandler(this.NotifyMenu_OpenUnrealGameSync_Click);
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			this.toolStripSeparator1.Size = new System.Drawing.Size(193, 6);
			// 
			// NotifyMenu_SyncNow
			// 
			this.NotifyMenu_SyncNow.Name = "NotifyMenu_SyncNow";
			this.NotifyMenu_SyncNow.Size = new System.Drawing.Size(196, 22);
			this.NotifyMenu_SyncNow.Text = "Sync Now";
			this.NotifyMenu_SyncNow.Click += new System.EventHandler(this.NotifyMenu_SyncNow_Click);
			// 
			// NotifyMenu_LaunchEditor
			// 
			this.NotifyMenu_LaunchEditor.Name = "NotifyMenu_LaunchEditor";
			this.NotifyMenu_LaunchEditor.Size = new System.Drawing.Size(196, 22);
			this.NotifyMenu_LaunchEditor.Text = "Launch Editor";
			this.NotifyMenu_LaunchEditor.Click += new System.EventHandler(this.NotifyMenu_LaunchEditor_Click);
			// 
			// NotifyMenu_ExitSeparator
			// 
			this.NotifyMenu_ExitSeparator.Name = "NotifyMenu_ExitSeparator";
			this.NotifyMenu_ExitSeparator.Size = new System.Drawing.Size(193, 6);
			// 
			// NotifyMenu_Exit
			// 
			this.NotifyMenu_Exit.Name = "NotifyMenu_Exit";
			this.NotifyMenu_Exit.Size = new System.Drawing.Size(196, 22);
			this.NotifyMenu_Exit.Text = "Exit";
			this.NotifyMenu_Exit.Click += new System.EventHandler(this.NotifyMenu_Exit_Click);
			// 
			// TabPanel
			// 
			this.TabPanel.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TabPanel.Controls.Add(this.DefaultControl);
			this.TabPanel.Location = new System.Drawing.Point(16, 51);
			this.TabPanel.Margin = new System.Windows.Forms.Padding(0);
			this.TabPanel.Name = "TabPanel";
			this.TabPanel.Size = new System.Drawing.Size(1140, 589);
			this.TabPanel.TabIndex = 3;
			// 
			// DefaultControl
			// 
			this.DefaultControl.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DefaultControl.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(250)))), ((int)(((byte)(250)))), ((int)(((byte)(250)))));
			this.DefaultControl.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.DefaultControl.Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.DefaultControl.Location = new System.Drawing.Point(0, 0);
			this.DefaultControl.Margin = new System.Windows.Forms.Padding(0);
			this.DefaultControl.Name = "DefaultControl";
			this.DefaultControl.Size = new System.Drawing.Size(1140, 589);
			this.DefaultControl.TabIndex = 0;
			// 
			// TabMenu
			// 
			this.TabMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.TabMenu_OpenProject,
            this.TabMenu_RecentProjects,
            this.toolStripSeparator2,
            this.labelsToolStripMenuItem});
			this.TabMenu.Name = "TabMenu";
			this.TabMenu.Size = new System.Drawing.Size(156, 76);
			this.TabMenu.Closed += new System.Windows.Forms.ToolStripDropDownClosedEventHandler(this.TabMenu_Closed);
			// 
			// TabMenu_OpenProject
			// 
			this.TabMenu_OpenProject.Name = "TabMenu_OpenProject";
			this.TabMenu_OpenProject.Size = new System.Drawing.Size(155, 22);
			this.TabMenu_OpenProject.Text = "Open Project...";
			this.TabMenu_OpenProject.Click += new System.EventHandler(this.TabMenu_OpenProject_Click);
			// 
			// TabMenu_RecentProjects
			// 
			this.TabMenu_RecentProjects.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.TabMenu_Recent_Separator,
            this.TabMenu_RecentProjects_ClearList});
			this.TabMenu_RecentProjects.Name = "TabMenu_RecentProjects";
			this.TabMenu_RecentProjects.Size = new System.Drawing.Size(155, 22);
			this.TabMenu_RecentProjects.Text = "Recent Projects";
			// 
			// TabMenu_Recent_Separator
			// 
			this.TabMenu_Recent_Separator.Name = "TabMenu_Recent_Separator";
			this.TabMenu_Recent_Separator.Size = new System.Drawing.Size(119, 6);
			// 
			// TabMenu_RecentProjects_ClearList
			// 
			this.TabMenu_RecentProjects_ClearList.Name = "TabMenu_RecentProjects_ClearList";
			this.TabMenu_RecentProjects_ClearList.Size = new System.Drawing.Size(122, 22);
			this.TabMenu_RecentProjects_ClearList.Text = "Clear List";
			this.TabMenu_RecentProjects_ClearList.Click += new System.EventHandler(this.TabMenu_RecentProjects_ClearList_Click);
			// 
			// toolStripSeparator2
			// 
			this.toolStripSeparator2.Name = "toolStripSeparator2";
			this.toolStripSeparator2.Size = new System.Drawing.Size(152, 6);
			// 
			// labelsToolStripMenuItem
			// 
			this.labelsToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.TabMenu_TabNames_Stream,
            this.TabMenu_TabNames_WorkspaceName,
            this.TabMenu_TabNames_WorkspaceRoot,
            this.TabMenu_TabNames_ProjectFile});
			this.labelsToolStripMenuItem.Name = "labelsToolStripMenuItem";
			this.labelsToolStripMenuItem.Size = new System.Drawing.Size(155, 22);
			this.labelsToolStripMenuItem.Text = "Labels";
			// 
			// TabMenu_TabNames_Stream
			// 
			this.TabMenu_TabNames_Stream.Name = "TabMenu_TabNames_Stream";
			this.TabMenu_TabNames_Stream.Size = new System.Drawing.Size(167, 22);
			this.TabMenu_TabNames_Stream.Text = "Stream";
			this.TabMenu_TabNames_Stream.Click += new System.EventHandler(this.TabMenu_TabNames_Stream_Click);
			// 
			// TabMenu_TabNames_WorkspaceName
			// 
			this.TabMenu_TabNames_WorkspaceName.Name = "TabMenu_TabNames_WorkspaceName";
			this.TabMenu_TabNames_WorkspaceName.Size = new System.Drawing.Size(167, 22);
			this.TabMenu_TabNames_WorkspaceName.Text = "Workspace Name";
			this.TabMenu_TabNames_WorkspaceName.Click += new System.EventHandler(this.TabMenu_TabNames_WorkspaceName_Click);
			// 
			// TabMenu_TabNames_WorkspaceRoot
			// 
			this.TabMenu_TabNames_WorkspaceRoot.Name = "TabMenu_TabNames_WorkspaceRoot";
			this.TabMenu_TabNames_WorkspaceRoot.Size = new System.Drawing.Size(167, 22);
			this.TabMenu_TabNames_WorkspaceRoot.Text = "Workspace Root";
			this.TabMenu_TabNames_WorkspaceRoot.Click += new System.EventHandler(this.TabMenu_TabNames_WorkspaceRoot_Click);
			// 
			// TabMenu_TabNames_ProjectFile
			// 
			this.TabMenu_TabNames_ProjectFile.Name = "TabMenu_TabNames_ProjectFile";
			this.TabMenu_TabNames_ProjectFile.Size = new System.Drawing.Size(167, 22);
			this.TabMenu_TabNames_ProjectFile.Text = "Project File";
			this.TabMenu_TabNames_ProjectFile.Click += new System.EventHandler(this.TabMenu_TabNames_ProjectFile_Click);
			// 
			// TabControl
			// 
			this.TabControl.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TabControl.Location = new System.Drawing.Point(16, 13);
			this.TabControl.Name = "TabControl";
			this.TabControl.Size = new System.Drawing.Size(1140, 31);
			this.TabControl.TabIndex = 2;
			this.TabControl.Text = "TabControl";
			// 
			// MainWindow
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(1168, 652);
			this.Controls.Add(this.TabPanel);
			this.Controls.Add(this.TabControl);
			this.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MinimumSize = new System.Drawing.Size(800, 350);
			this.Name = "MainWindow";
			this.Text = "UnrealGameSync";
			this.Activated += new System.EventHandler(this.MainWindow_Activated);
			this.Deactivate += new System.EventHandler(this.MainWindow_Deactivate);
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.MainWindow_FormClosing);
			this.Load += new System.EventHandler(this.MainWindow_Load);
			this.NotifyMenu.ResumeLayout(false);
			this.TabPanel.ResumeLayout(false);
			this.TabMenu.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.NotifyIcon NotifyIcon;
		private System.Windows.Forms.ContextMenuStrip NotifyMenu;
		private System.Windows.Forms.ToolStripMenuItem NotifyMenu_OpenUnrealGameSync;
		private System.Windows.Forms.ToolStripSeparator NotifyMenu_ExitSeparator;
		private System.Windows.Forms.ToolStripMenuItem NotifyMenu_Exit;
		private System.Windows.Forms.ToolStripMenuItem NotifyMenu_SyncNow;
		private System.Windows.Forms.ToolStripMenuItem NotifyMenu_LaunchEditor;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private TabControl TabControl;
		private System.Windows.Forms.Panel TabPanel;
		private StatusPanel DefaultControl;
		private System.Windows.Forms.ContextMenuStrip TabMenu;
		private System.Windows.Forms.ToolStripMenuItem labelsToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_TabNames_Stream;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_TabNames_WorkspaceName;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_TabNames_WorkspaceRoot;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_TabNames_ProjectFile;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_OpenProject;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_RecentProjects;
		private System.Windows.Forms.ToolStripSeparator TabMenu_Recent_Separator;
		private System.Windows.Forms.ToolStripMenuItem TabMenu_RecentProjects_ClearList;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
	}
}

