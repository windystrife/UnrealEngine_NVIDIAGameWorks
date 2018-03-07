// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class VariablesWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(VariablesWindow));
			this.OkButton = new System.Windows.Forms.Button();
			this.MacrosList = new System.Windows.Forms.ListView();
			this.columnHeader1 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader2 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.InsertButton = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// OkButton
			// 
			this.OkButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.OkButton.Location = new System.Drawing.Point(565, 416);
			this.OkButton.Name = "OkButton";
			this.OkButton.Size = new System.Drawing.Size(104, 27);
			this.OkButton.TabIndex = 0;
			this.OkButton.Text = "Ok";
			this.OkButton.UseVisualStyleBackColor = true;
			this.OkButton.Click += new System.EventHandler(this.OkButton_Click);
			// 
			// MacrosList
			// 
			this.MacrosList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.MacrosList.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader1,
            this.columnHeader2});
			this.MacrosList.HideSelection = false;
			this.MacrosList.Location = new System.Drawing.Point(12, 15);
			this.MacrosList.Name = "MacrosList";
			this.MacrosList.Size = new System.Drawing.Size(657, 395);
			this.MacrosList.TabIndex = 1;
			this.MacrosList.UseCompatibleStateImageBehavior = false;
			this.MacrosList.View = System.Windows.Forms.View.Details;
			this.MacrosList.SelectedIndexChanged += new System.EventHandler(this.MacrosList_SelectedIndexChanged);
			this.MacrosList.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.MacrosList_MouseDoubleClick);
			// 
			// columnHeader1
			// 
			this.columnHeader1.Text = "Name";
			this.columnHeader1.Width = 189;
			// 
			// columnHeader2
			// 
			this.columnHeader2.Text = "Value";
			this.columnHeader2.Width = 354;
			// 
			// InsertButton
			// 
			this.InsertButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.InsertButton.Enabled = false;
			this.InsertButton.Location = new System.Drawing.Point(12, 416);
			this.InsertButton.Name = "InsertButton";
			this.InsertButton.Size = new System.Drawing.Size(108, 27);
			this.InsertButton.TabIndex = 2;
			this.InsertButton.Text = "Insert";
			this.InsertButton.UseVisualStyleBackColor = true;
			this.InsertButton.Click += new System.EventHandler(this.InsertButton_Click);
			// 
			// VariablesWindow
			// 
			this.AcceptButton = this.OkButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.OkButton;
			this.ClientSize = new System.Drawing.Size(681, 455);
			this.Controls.Add(this.InsertButton);
			this.Controls.Add(this.MacrosList);
			this.Controls.Add(this.OkButton);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.SizableToolWindow;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "VariablesWindow";
			this.ShowInTaskbar = false;
			this.StartPosition = System.Windows.Forms.FormStartPosition.Manual;
			this.Text = "Variables";
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Button OkButton;
		private System.Windows.Forms.ListView MacrosList;
		private System.Windows.Forms.ColumnHeader columnHeader1;
		private System.Windows.Forms.ColumnHeader columnHeader2;
		private System.Windows.Forms.Button InsertButton;
	}
}