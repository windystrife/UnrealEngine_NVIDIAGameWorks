// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class ScheduleWindow
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
			this.TimePicker = new System.Windows.Forms.DateTimePicker();
			this.EnableCheckBox = new System.Windows.Forms.CheckBox();
			this.label1 = new System.Windows.Forms.Label();
			this.OkBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.ChangeComboBox = new System.Windows.Forms.ComboBox();
			this.SuspendLayout();
			// 
			// TimePicker
			// 
			this.TimePicker.CustomFormat = "h:mm tt";
			this.TimePicker.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
			this.TimePicker.Location = new System.Drawing.Point(460, 15);
			this.TimePicker.Name = "TimePicker";
			this.TimePicker.ShowUpDown = true;
			this.TimePicker.Size = new System.Drawing.Size(118, 23);
			this.TimePicker.TabIndex = 3;
			// 
			// EnableCheckBox
			// 
			this.EnableCheckBox.AutoSize = true;
			this.EnableCheckBox.Location = new System.Drawing.Point(15, 17);
			this.EnableCheckBox.Name = "EnableCheckBox";
			this.EnableCheckBox.Size = new System.Drawing.Size(180, 19);
			this.EnableCheckBox.TabIndex = 1;
			this.EnableCheckBox.Text = "Automatically sync and build";
			this.EnableCheckBox.UseVisualStyleBackColor = true;
			this.EnableCheckBox.CheckedChanged += new System.EventHandler(this.EnableCheckBox_CheckedChanged);
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(379, 18);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(70, 15);
			this.label1.TabIndex = 3;
			this.label1.Text = "every day at";
			// 
			// OkBtn
			// 
			this.OkBtn.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.OkBtn.Location = new System.Drawing.Point(494, 54);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(87, 26);
			this.OkBtn.TabIndex = 5;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			// 
			// CancelBtn
			// 
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(401, 54);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size(87, 26);
			this.CancelBtn.TabIndex = 4;
			this.CancelBtn.Text = "Cancel";
			this.CancelBtn.UseVisualStyleBackColor = true;
			// 
			// ChangeComboBox
			// 
			this.ChangeComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.ChangeComboBox.FormattingEnabled = true;
			this.ChangeComboBox.Items.AddRange(new object[] {
            "latest change",
            "latest good change",
            "latest starred change"});
			this.ChangeComboBox.Location = new System.Drawing.Point(203, 14);
			this.ChangeComboBox.Name = "ChangeComboBox";
			this.ChangeComboBox.Size = new System.Drawing.Size(171, 23);
			this.ChangeComboBox.TabIndex = 2;
			// 
			// ScheduleWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size(593, 92);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.EnableCheckBox);
			this.Controls.Add(this.ChangeComboBox);
			this.Controls.Add(this.TimePicker);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Name = "ScheduleWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Schedule";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.DateTimePicker TimePicker;
		private System.Windows.Forms.CheckBox EnableCheckBox;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.ComboBox ChangeComboBox;
	}
}