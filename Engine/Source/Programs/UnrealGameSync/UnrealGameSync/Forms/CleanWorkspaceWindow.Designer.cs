// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class CleanWorkspaceWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(CleanWorkspaceWindow));
			this.TreeView = new System.Windows.Forms.TreeView();
			this.DeleteBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.label1 = new System.Windows.Forms.Label();
			this.SelectAllBtn = new System.Windows.Forms.Button();
			this.FolderContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.FolderContextMenu_SelectAll = new System.Windows.Forms.ToolStripMenuItem();
			this.FolderContextMenu_SelectSafeToDelete = new System.Windows.Forms.ToolStripMenuItem();
			this.FolderContextMenu_SelectEmptyFolder = new System.Windows.Forms.ToolStripMenuItem();
			this.FolderContextMenu_SelectNone = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.FolderContextMenu_OpenWithExplorer = new System.Windows.Forms.ToolStripMenuItem();
			this.FolderContextMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// TreeView
			// 
			this.TreeView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TreeView.HideSelection = false;
			this.TreeView.Location = new System.Drawing.Point(14, 44);
			this.TreeView.MinimumSize = new System.Drawing.Size(793, 461);
			this.TreeView.Name = "TreeView";
			this.TreeView.Size = new System.Drawing.Size(801, 506);
			this.TreeView.TabIndex = 0;
			this.TreeView.DrawNode += new System.Windows.Forms.DrawTreeNodeEventHandler(this.TreeView_DrawNode);
			this.TreeView.NodeMouseClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.TreeView_NodeMouseClick);
			// 
			// DeleteBtn
			// 
			this.DeleteBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.DeleteBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.DeleteBtn.Location = new System.Drawing.Point(714, 560);
			this.DeleteBtn.Name = "DeleteBtn";
			this.DeleteBtn.Size = new System.Drawing.Size(101, 26);
			this.DeleteBtn.TabIndex = 1;
			this.DeleteBtn.Text = "Delete Files";
			this.DeleteBtn.UseVisualStyleBackColor = true;
			this.DeleteBtn.Click += new System.EventHandler(this.DeleteBtn_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(607, 560);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(101, 26);
			this.CancelBtn.TabIndex = 2;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// label1
			// 
			this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(12, 16);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(727, 15);
			this.label1.TabIndex = 3;
			this.label1.Text = "The files below are not tracked by Perforce; check any you wish to delete. Interm" +
    "ediate files that are safe to delete are checked by default.";
			// 
			// SelectAllBtn
			// 
			this.SelectAllBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.SelectAllBtn.Location = new System.Drawing.Point(12, 560);
			this.SelectAllBtn.Name = "SelectAllBtn";
			this.SelectAllBtn.Size = new System.Drawing.Size(125, 26);
			this.SelectAllBtn.TabIndex = 4;
			this.SelectAllBtn.Text = "Select All";
			this.SelectAllBtn.UseVisualStyleBackColor = true;
			this.SelectAllBtn.Click += new System.EventHandler(this.SelectAllBtn_Click);
			// 
			// FolderContextMenu
			// 
			this.FolderContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.FolderContextMenu_SelectAll,
            this.FolderContextMenu_SelectSafeToDelete,
            this.FolderContextMenu_SelectEmptyFolder,
            this.FolderContextMenu_SelectNone,
            this.toolStripSeparator1,
            this.FolderContextMenu_OpenWithExplorer});
			this.FolderContextMenu.Name = "FolderContextMenu";
			this.FolderContextMenu.Size = new System.Drawing.Size(184, 120);
			// 
			// FolderContextMenu_SelectAll
			// 
			this.FolderContextMenu_SelectAll.Name = "FolderContextMenu_SelectAll";
			this.FolderContextMenu_SelectAll.Size = new System.Drawing.Size(183, 22);
			this.FolderContextMenu_SelectAll.Text = "Select All";
			this.FolderContextMenu_SelectAll.Click += new System.EventHandler(this.FolderContextMenu_SelectAll_Click);
			// 
			// FolderContextMenu_SelectSafeToDelete
			// 
			this.FolderContextMenu_SelectSafeToDelete.Name = "FolderContextMenu_SelectSafeToDelete";
			this.FolderContextMenu_SelectSafeToDelete.Size = new System.Drawing.Size(183, 22);
			this.FolderContextMenu_SelectSafeToDelete.Text = "Select Safe to Delete";
			this.FolderContextMenu_SelectSafeToDelete.Click += new System.EventHandler(this.FolderContextMenu_SelectSafeToDelete_Click);
			// 
			// FolderContextMenu_SelectEmptyFolder
			// 
			this.FolderContextMenu_SelectEmptyFolder.Name = "FolderContextMenu_SelectEmptyFolder";
			this.FolderContextMenu_SelectEmptyFolder.Size = new System.Drawing.Size(183, 22);
			this.FolderContextMenu_SelectEmptyFolder.Text = "Select Empty Folders";
			this.FolderContextMenu_SelectEmptyFolder.Click += new System.EventHandler(this.FolderContextMenu_SelectEmptyFolder_Click);
			// 
			// FolderContextMenu_SelectNone
			// 
			this.FolderContextMenu_SelectNone.Name = "FolderContextMenu_SelectNone";
			this.FolderContextMenu_SelectNone.Size = new System.Drawing.Size(183, 22);
			this.FolderContextMenu_SelectNone.Text = "Select None";
			this.FolderContextMenu_SelectNone.Click += new System.EventHandler(this.FolderContextMenu_SelectNone_Click);
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			this.toolStripSeparator1.Size = new System.Drawing.Size(180, 6);
			// 
			// FolderContextMenu_OpenWithExplorer
			// 
			this.FolderContextMenu_OpenWithExplorer.Name = "FolderContextMenu_OpenWithExplorer";
			this.FolderContextMenu_OpenWithExplorer.Size = new System.Drawing.Size(183, 22);
			this.FolderContextMenu_OpenWithExplorer.Text = "Open with Explorer...";
			this.FolderContextMenu_OpenWithExplorer.Click += new System.EventHandler(this.FolderContextMenu_OpenWithExplorer_Click);
			// 
			// CleanWorkspaceWindow
			// 
			this.AcceptButton = this.DeleteBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(827, 598);
			this.ControlBox = false;
			this.Controls.Add(this.SelectAllBtn);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.DeleteBtn);
			this.Controls.Add(this.TreeView);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MinimumSize = new System.Drawing.Size(843, 594);
			this.Name = "CleanWorkspaceWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Clean Workspace";
			this.Load += new System.EventHandler(this.CleanWorkspaceWindow_Load);
			this.FolderContextMenu.ResumeLayout(false);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TreeView TreeView;
		private System.Windows.Forms.Button DeleteBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Button SelectAllBtn;
		private System.Windows.Forms.ContextMenuStrip FolderContextMenu;
		private System.Windows.Forms.ToolStripMenuItem FolderContextMenu_SelectAll;
		private System.Windows.Forms.ToolStripMenuItem FolderContextMenu_SelectSafeToDelete;
		private System.Windows.Forms.ToolStripMenuItem FolderContextMenu_SelectEmptyFolder;
		private System.Windows.Forms.ToolStripMenuItem FolderContextMenu_SelectNone;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripMenuItem FolderContextMenu_OpenWithExplorer;
	}
}