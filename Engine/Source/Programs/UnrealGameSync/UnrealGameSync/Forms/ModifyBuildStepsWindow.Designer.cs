// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class ModifyBuildStepsWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ModifyBuildStepsWindow));
			this.BuildStepList = new System.Windows.Forms.ListView();
			this.DescriptionColumn = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.NormalSyncColumn = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.ScheduledSyncColumn = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.ShowAsToolColumn = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.NewStepButton = new System.Windows.Forms.Button();
			this.EditStepButton = new System.Windows.Forms.Button();
			this.RemoveStepButton = new System.Windows.Forms.Button();
			this.CloseButton = new System.Windows.Forms.Button();
			this.MoveUp = new System.Windows.Forms.Button();
			this.MoveDown = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// BuildStepList
			// 
			this.BuildStepList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.BuildStepList.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.DescriptionColumn,
            this.NormalSyncColumn,
            this.ScheduledSyncColumn,
            this.ShowAsToolColumn});
			this.BuildStepList.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.BuildStepList.FullRowSelect = true;
			this.BuildStepList.GridLines = true;
			this.BuildStepList.Location = new System.Drawing.Point(12, 11);
			this.BuildStepList.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
			this.BuildStepList.MultiSelect = false;
			this.BuildStepList.Name = "BuildStepList";
			this.BuildStepList.OwnerDraw = true;
			this.BuildStepList.Size = new System.Drawing.Size(634, 371);
			this.BuildStepList.TabIndex = 0;
			this.BuildStepList.UseCompatibleStateImageBehavior = false;
			this.BuildStepList.View = System.Windows.Forms.View.Details;
			this.BuildStepList.DrawColumnHeader += new System.Windows.Forms.DrawListViewColumnHeaderEventHandler(this.BuildStepList_DrawColumnHeader);
			this.BuildStepList.DrawSubItem += new System.Windows.Forms.DrawListViewSubItemEventHandler(this.BuildStepList_DrawSubItem);
			this.BuildStepList.SelectedIndexChanged += new System.EventHandler(this.BuildStepList_SelectedIndexChanged);
			this.BuildStepList.MouseDown += new System.Windows.Forms.MouseEventHandler(this.BuildStepList_MouseDown);
			this.BuildStepList.MouseUp += new System.Windows.Forms.MouseEventHandler(this.BuildStepList_MouseUp);
			// 
			// DescriptionColumn
			// 
			this.DescriptionColumn.Text = "Description";
			this.DescriptionColumn.Width = 309;
			// 
			// NormalSyncColumn
			// 
			this.NormalSyncColumn.Text = "Normal Sync";
			this.NormalSyncColumn.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.NormalSyncColumn.Width = 104;
			// 
			// ScheduledSyncColumn
			// 
			this.ScheduledSyncColumn.Text = "Scheduled Sync";
			this.ScheduledSyncColumn.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.ScheduledSyncColumn.Width = 105;
			// 
			// ShowAsToolColumn
			// 
			this.ShowAsToolColumn.Text = "Show as Tool";
			this.ShowAsToolColumn.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.ShowAsToolColumn.Width = 107;
			// 
			// NewStepButton
			// 
			this.NewStepButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.NewStepButton.Location = new System.Drawing.Point(652, 11);
			this.NewStepButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
			this.NewStepButton.Name = "NewStepButton";
			this.NewStepButton.Size = new System.Drawing.Size(112, 26);
			this.NewStepButton.TabIndex = 1;
			this.NewStepButton.Text = "New Step...";
			this.NewStepButton.UseVisualStyleBackColor = true;
			this.NewStepButton.Click += new System.EventHandler(this.NewStepButton_Click);
			// 
			// EditStepButton
			// 
			this.EditStepButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.EditStepButton.Location = new System.Drawing.Point(652, 41);
			this.EditStepButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
			this.EditStepButton.Name = "EditStepButton";
			this.EditStepButton.Size = new System.Drawing.Size(112, 26);
			this.EditStepButton.TabIndex = 2;
			this.EditStepButton.Text = "Edit Step...";
			this.EditStepButton.UseVisualStyleBackColor = true;
			this.EditStepButton.Click += new System.EventHandler(this.EditStepButton_Click);
			// 
			// RemoveStepButton
			// 
			this.RemoveStepButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.RemoveStepButton.Location = new System.Drawing.Point(652, 71);
			this.RemoveStepButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
			this.RemoveStepButton.Name = "RemoveStepButton";
			this.RemoveStepButton.Size = new System.Drawing.Size(112, 26);
			this.RemoveStepButton.TabIndex = 3;
			this.RemoveStepButton.Text = "Remove Step";
			this.RemoveStepButton.UseVisualStyleBackColor = true;
			this.RemoveStepButton.Click += new System.EventHandler(this.RemoveStepButton_Click);
			// 
			// CloseButton
			// 
			this.CloseButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CloseButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CloseButton.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.CloseButton.Location = new System.Drawing.Point(653, 356);
			this.CloseButton.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
			this.CloseButton.Name = "CloseButton";
			this.CloseButton.Size = new System.Drawing.Size(111, 26);
			this.CloseButton.TabIndex = 6;
			this.CloseButton.Text = "Close";
			this.CloseButton.UseVisualStyleBackColor = true;
			this.CloseButton.Click += new System.EventHandler(this.CloseButton_Click);
			// 
			// MoveUp
			// 
			this.MoveUp.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.MoveUp.Location = new System.Drawing.Point(653, 164);
			this.MoveUp.Name = "MoveUp";
			this.MoveUp.Size = new System.Drawing.Size(111, 26);
			this.MoveUp.TabIndex = 4;
			this.MoveUp.Text = "Move Up";
			this.MoveUp.UseVisualStyleBackColor = true;
			this.MoveUp.Click += new System.EventHandler(this.MoveUp_Click);
			// 
			// MoveDown
			// 
			this.MoveDown.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.MoveDown.Location = new System.Drawing.Point(652, 196);
			this.MoveDown.Name = "MoveDown";
			this.MoveDown.Size = new System.Drawing.Size(111, 26);
			this.MoveDown.TabIndex = 5;
			this.MoveDown.Text = "Move Down";
			this.MoveDown.UseVisualStyleBackColor = true;
			this.MoveDown.Click += new System.EventHandler(this.MoveDown_Click);
			// 
			// ModifyBuildStepsWindow
			// 
			this.AcceptButton = this.CloseButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CloseButton;
			this.ClientSize = new System.Drawing.Size(776, 393);
			this.Controls.Add(this.MoveDown);
			this.Controls.Add(this.MoveUp);
			this.Controls.Add(this.CloseButton);
			this.Controls.Add(this.RemoveStepButton);
			this.Controls.Add(this.EditStepButton);
			this.Controls.Add(this.NewStepButton);
			this.Controls.Add(this.BuildStepList);
			this.Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ModifyBuildStepsWindow";
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Modify Build Steps";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.ModifyBuildStepsWindow_FormClosed);
			this.Load += new System.EventHandler(this.ModifyBuildStepsWindow_Load);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.ListView BuildStepList;
		private System.Windows.Forms.ColumnHeader DescriptionColumn;
		private System.Windows.Forms.ColumnHeader NormalSyncColumn;
		private System.Windows.Forms.ColumnHeader ScheduledSyncColumn;
		private System.Windows.Forms.Button NewStepButton;
		private System.Windows.Forms.Button EditStepButton;
		private System.Windows.Forms.Button RemoveStepButton;
		private System.Windows.Forms.Button CloseButton;
		private System.Windows.Forms.ColumnHeader ShowAsToolColumn;
		private System.Windows.Forms.Button MoveUp;
		private System.Windows.Forms.Button MoveDown;
	}
}