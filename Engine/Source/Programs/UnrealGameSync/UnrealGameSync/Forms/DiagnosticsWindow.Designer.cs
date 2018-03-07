// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class DiagnosticsWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(DiagnosticsWindow));
			this.ViewLogsButton = new System.Windows.Forms.Button();
			this.button2 = new System.Windows.Forms.Button();
			this.DiagnosticsTextBox = new System.Windows.Forms.TextBox();
			this.SaveButton = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// ViewLogsButton
			// 
			this.ViewLogsButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.ViewLogsButton.Location = new System.Drawing.Point(138, 366);
			this.ViewLogsButton.Name = "ViewLogsButton";
			this.ViewLogsButton.Size = new System.Drawing.Size(122, 26);
			this.ViewLogsButton.TabIndex = 0;
			this.ViewLogsButton.Text = "View logs";
			this.ViewLogsButton.UseVisualStyleBackColor = true;
			this.ViewLogsButton.Click += new System.EventHandler(this.ViewLogsButton_Click);
			// 
			// button2
			// 
			this.button2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.button2.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.button2.Location = new System.Drawing.Point(831, 366);
			this.button2.Name = "button2";
			this.button2.Size = new System.Drawing.Size(105, 26);
			this.button2.TabIndex = 1;
			this.button2.Text = "Ok";
			this.button2.UseVisualStyleBackColor = true;
			// 
			// DiagnosticsTextBox
			// 
			this.DiagnosticsTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DiagnosticsTextBox.Font = new System.Drawing.Font("Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.DiagnosticsTextBox.Location = new System.Drawing.Point(15, 15);
			this.DiagnosticsTextBox.Multiline = true;
			this.DiagnosticsTextBox.Name = "DiagnosticsTextBox";
			this.DiagnosticsTextBox.ReadOnly = true;
			this.DiagnosticsTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Both;
			this.DiagnosticsTextBox.Size = new System.Drawing.Size(918, 342);
			this.DiagnosticsTextBox.TabIndex = 2;
			this.DiagnosticsTextBox.WordWrap = false;
			// 
			// SaveButton
			// 
			this.SaveButton.Location = new System.Drawing.Point(15, 366);
			this.SaveButton.Name = "SaveButton";
			this.SaveButton.Size = new System.Drawing.Size(117, 26);
			this.SaveButton.TabIndex = 3;
			this.SaveButton.Text = "Save...";
			this.SaveButton.UseVisualStyleBackColor = true;
			this.SaveButton.Click += new System.EventHandler(this.SaveButton_Click);
			// 
			// DiagnosticsWindow
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(948, 404);
			this.Controls.Add(this.SaveButton);
			this.Controls.Add(this.DiagnosticsTextBox);
			this.Controls.Add(this.button2);
			this.Controls.Add(this.ViewLogsButton);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "DiagnosticsWindow";
			this.Text = "Diagnostics";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Button ViewLogsButton;
		private System.Windows.Forms.Button button2;
		private System.Windows.Forms.TextBox DiagnosticsTextBox;
		private System.Windows.Forms.Button SaveButton;
	}
}