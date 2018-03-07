// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class ArgumentsWindow
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
			System.Windows.Forms.ListViewItem listViewItem1 = new System.Windows.Forms.ListViewItem("-debug");
			System.Windows.Forms.ListViewItem listViewItem2 = new System.Windows.Forms.ListViewItem("Add another...");
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ArgumentsWindow));
			this.OkButton = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ArgumentsList = new System.Windows.Forms.ListView();
			this.columnHeader1 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.MoveUpButton = new System.Windows.Forms.Button();
			this.MoveDownButton = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// OkButton
			// 
			this.OkButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkButton.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.OkButton.Location = new System.Drawing.Point(671, 381);
			this.OkButton.Name = "OkButton";
			this.OkButton.Size = new System.Drawing.Size(108, 26);
			this.OkButton.TabIndex = 0;
			this.OkButton.Text = "Ok";
			this.OkButton.UseVisualStyleBackColor = true;
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(671, 349);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(108, 26);
			this.CancelBtn.TabIndex = 2;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// ArgumentsList
			// 
			this.ArgumentsList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ArgumentsList.CheckBoxes = true;
			this.ArgumentsList.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader1});
			this.ArgumentsList.FullRowSelect = true;
			this.ArgumentsList.GridLines = true;
			this.ArgumentsList.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.None;
			listViewItem1.Checked = true;
			listViewItem1.StateImageIndex = 1;
			listViewItem2.Checked = true;
			listViewItem2.StateImageIndex = 2;
			this.ArgumentsList.Items.AddRange(new System.Windows.Forms.ListViewItem[] {
            listViewItem1,
            listViewItem2});
			this.ArgumentsList.LabelEdit = true;
			this.ArgumentsList.Location = new System.Drawing.Point(12, 12);
			this.ArgumentsList.MultiSelect = false;
			this.ArgumentsList.Name = "ArgumentsList";
			this.ArgumentsList.OwnerDraw = true;
			this.ArgumentsList.Size = new System.Drawing.Size(647, 393);
			this.ArgumentsList.TabIndex = 5;
			this.ArgumentsList.UseCompatibleStateImageBehavior = false;
			this.ArgumentsList.View = System.Windows.Forms.View.Details;
			this.ArgumentsList.AfterLabelEdit += new System.Windows.Forms.LabelEditEventHandler(this.ArgumentsList_AfterLabelEdit);
			this.ArgumentsList.BeforeLabelEdit += new System.Windows.Forms.LabelEditEventHandler(this.ArgumentsList_BeforeLabelEdit);
			this.ArgumentsList.DrawItem += new System.Windows.Forms.DrawListViewItemEventHandler(this.ArgumentsList_DrawItem);
			this.ArgumentsList.SelectedIndexChanged += new System.EventHandler(this.ArgumentsList_SelectedIndexChanged);
			this.ArgumentsList.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.ArgumentsList_KeyPress);
			this.ArgumentsList.KeyUp += new System.Windows.Forms.KeyEventHandler(this.ArgumentsList_KeyUp);
			this.ArgumentsList.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ArgumentsList_MouseClick);
			// 
			// columnHeader1
			// 
			this.columnHeader1.Text = "Arguments";
			this.columnHeader1.Width = 536;
			// 
			// MoveUpButton
			// 
			this.MoveUpButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.MoveUpButton.Location = new System.Drawing.Point(671, 12);
			this.MoveUpButton.Name = "MoveUpButton";
			this.MoveUpButton.Size = new System.Drawing.Size(108, 26);
			this.MoveUpButton.TabIndex = 6;
			this.MoveUpButton.Text = "Move Up";
			this.MoveUpButton.UseVisualStyleBackColor = true;
			this.MoveUpButton.Click += new System.EventHandler(this.MoveUpButton_Click);
			// 
			// MoveDownButton
			// 
			this.MoveDownButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.MoveDownButton.Location = new System.Drawing.Point(671, 44);
			this.MoveDownButton.Name = "MoveDownButton";
			this.MoveDownButton.Size = new System.Drawing.Size(108, 26);
			this.MoveDownButton.TabIndex = 7;
			this.MoveDownButton.Text = "Move Down";
			this.MoveDownButton.UseVisualStyleBackColor = true;
			this.MoveDownButton.Click += new System.EventHandler(this.MoveDownButton_Click);
			// 
			// ArgumentsWindow
			// 
			this.AcceptButton = this.OkButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(791, 419);
			this.Controls.Add(this.MoveDownButton);
			this.Controls.Add(this.MoveUpButton);
			this.Controls.Add(this.ArgumentsList);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkButton);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "ArgumentsWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Command Line Arguments";
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Button OkButton;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.ListView ArgumentsList;
		private System.Windows.Forms.ColumnHeader columnHeader1;
		private System.Windows.Forms.Button MoveUpButton;
		private System.Windows.Forms.Button MoveDownButton;
	}
}