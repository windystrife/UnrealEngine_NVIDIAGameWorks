// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealGameSyncLauncher
{
	partial class UpdateErrorWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(UpdateErrorWindow));
			this.UpdateLog = new System.Windows.Forms.TextBox();
			this.OkBtn = new System.Windows.Forms.Button();
			this.label1 = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// UpdateLog
			// 
			this.UpdateLog.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.UpdateLog.Font = new System.Drawing.Font("Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.UpdateLog.Location = new System.Drawing.Point(12, 36);
			this.UpdateLog.Multiline = true;
			this.UpdateLog.Name = "UpdateLog";
			this.UpdateLog.ReadOnly = true;
			this.UpdateLog.ScrollBars = System.Windows.Forms.ScrollBars.Both;
			this.UpdateLog.Size = new System.Drawing.Size(859, 188);
			this.UpdateLog.TabIndex = 0;
			this.UpdateLog.WordWrap = false;
			// 
			// OkBtn
			// 
			this.OkBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.OkBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.OkBtn.Location = new System.Drawing.Point(796, 232);
			this.OkBtn.Name = "OkBtn";
			this.OkBtn.Size = new System.Drawing.Size(75, 23);
			this.OkBtn.TabIndex = 1;
			this.OkBtn.Text = "Ok";
			this.OkBtn.UseVisualStyleBackColor = true;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(12, 13);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(470, 13);
			this.label1.TabIndex = 2;
			this.label1.Text = "UnrealGameSync couldn\'t update to the latest version from Perforce.  The update l" +
    "og is as follows:";
			// 
			// UpdateErrorWindow
			// 
			this.AcceptButton = this.OkBtn;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.OkBtn;
			this.ClientSize = new System.Drawing.Size(883, 267);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.OkBtn);
			this.Controls.Add(this.UpdateLog);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "UpdateErrorWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Update Error";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TextBox UpdateLog;
		private System.Windows.Forms.Button OkBtn;
		private System.Windows.Forms.Label label1;
	}
}