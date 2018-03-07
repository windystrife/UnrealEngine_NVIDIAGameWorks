// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSync
{
	partial class ModalTaskWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ModalTaskWindow));
			this.MessageLabel = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// MessageLabel
			// 
			this.MessageLabel.Dock = System.Windows.Forms.DockStyle.Fill;
			this.MessageLabel.Location = new System.Drawing.Point(0, 0);
			this.MessageLabel.Name = "MessageLabel";
			this.MessageLabel.Size = new System.Drawing.Size(401, 53);
			this.MessageLabel.TabIndex = 0;
			this.MessageLabel.Text = "Please wait...";
			this.MessageLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// ModalTaskWindow
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(401, 53);
			this.Controls.Add(this.MessageLabel);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "ModalTaskWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.OpenProjectWindow_FormClosing);
			this.Load += new System.EventHandler(this.OpenProjectWindow_Load);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Label MessageLabel;
	}
}