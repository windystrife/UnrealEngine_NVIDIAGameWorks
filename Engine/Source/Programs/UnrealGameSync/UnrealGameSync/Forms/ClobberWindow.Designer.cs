// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class ClobberWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ClobberWindow));
			this.FileList = new System.Windows.Forms.ListView();
			this.columnHeader1 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader2 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.label1 = new System.Windows.Forms.Label();
			this.UncheckAll = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ContinueButton = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// FileList
			// 
			this.FileList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.FileList.CheckBoxes = true;
			this.FileList.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader1,
            this.columnHeader2});
			this.FileList.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.Nonclickable;
			this.FileList.Location = new System.Drawing.Point(13, 42);
			this.FileList.Name = "FileList";
			this.FileList.ShowGroups = false;
			this.FileList.Size = new System.Drawing.Size(526, 366);
			this.FileList.TabIndex = 1;
			this.FileList.UseCompatibleStateImageBehavior = false;
			this.FileList.View = System.Windows.Forms.View.Details;
			// 
			// columnHeader1
			// 
			this.columnHeader1.Text = "File";
			this.columnHeader1.Width = 102;
			// 
			// columnHeader2
			// 
			this.columnHeader2.Text = "In Folder";
			this.columnHeader2.Width = 268;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(10, 15);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(484, 15);
			this.label1.TabIndex = 0;
			this.label1.Text = "The following files are writable in your workspace. Select which files you want t" +
    "o overwrite:";
			// 
			// UncheckAll
			// 
			this.UncheckAll.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.UncheckAll.Location = new System.Drawing.Point(12, 415);
			this.UncheckAll.Name = "UncheckAll";
			this.UncheckAll.Size = new System.Drawing.Size(104, 26);
			this.UncheckAll.TabIndex = 2;
			this.UncheckAll.Text = "Uncheck all";
			this.UncheckAll.UseVisualStyleBackColor = true;
			this.UncheckAll.Click += new System.EventHandler(this.UncheckAll_Click);
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(452, 415);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(87, 26);
			this.CancelBtn.TabIndex = 4;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// ContinueButton
			// 
			this.ContinueButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.ContinueButton.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.ContinueButton.Location = new System.Drawing.Point(359, 415);
			this.ContinueButton.Name = "ContinueButton";
			this.ContinueButton.Size = new System.Drawing.Size(87, 26);
			this.ContinueButton.TabIndex = 3;
			this.ContinueButton.Text = "Continue";
			this.ContinueButton.UseVisualStyleBackColor = true;
			this.ContinueButton.Click += new System.EventHandler(this.ContinueButton_Click);
			// 
			// ClobberWindow
			// 
			this.AcceptButton = this.CancelBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(551, 453);
			this.Controls.Add(this.ContinueButton);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.UncheckAll);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.FileList);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "ClobberWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Clobber Files";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.ListView FileList;
		private System.Windows.Forms.ColumnHeader columnHeader1;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Button UncheckAll;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button ContinueButton;
		private System.Windows.Forms.ColumnHeader columnHeader2;
	}
}