// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

 namespace UnrealControls
{
	partial class UnrealAboutBox
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
			if(disposing && (components != null))
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
			this.mPictureBox_AppIcon = new System.Windows.Forms.PictureBox();
			this.label1 = new System.Windows.Forms.Label();
			this.mLabel_EngineVersion = new System.Windows.Forms.Label();
			this.mLabel_Changelist = new System.Windows.Forms.Label();
			this.mButton_OK = new System.Windows.Forms.Button();
			this.mLabel_AppVersion = new System.Windows.Forms.Label();
			( ( System.ComponentModel.ISupportInitialize )( this.mPictureBox_AppIcon ) ).BeginInit();
			this.SuspendLayout();
			// 
			// mPictureBox_AppIcon
			// 
			this.mPictureBox_AppIcon.Location = new System.Drawing.Point( 12, 12 );
			this.mPictureBox_AppIcon.Name = "mPictureBox_AppIcon";
			this.mPictureBox_AppIcon.Size = new System.Drawing.Size( 64, 64 );
			this.mPictureBox_AppIcon.SizeMode = System.Windows.Forms.PictureBoxSizeMode.StretchImage;
			this.mPictureBox_AppIcon.TabIndex = 0;
			this.mPictureBox_AppIcon.TabStop = false;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point( 82, 84 );
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size( 261, 13 );
			this.label1.TabIndex = 1;
			this.label1.Text = "Copyright 2012-2017 Epic Games, Inc. All Rights Reserved.";
			// 
			// mLabel_EngineVersion
			// 
			this.mLabel_EngineVersion.AutoSize = true;
			this.mLabel_EngineVersion.Location = new System.Drawing.Point( 82, 12 );
			this.mLabel_EngineVersion.Name = "mLabel_EngineVersion";
			this.mLabel_EngineVersion.Size = new System.Drawing.Size( 104, 13 );
			this.mLabel_EngineVersion.TabIndex = 2;
			this.mLabel_EngineVersion.Text = "Engine Version: N/A";
			// 
			// mLabel_Changelist
			// 
			this.mLabel_Changelist.AutoSize = true;
			this.mLabel_Changelist.Location = new System.Drawing.Point( 82, 36 );
			this.mLabel_Changelist.Name = "mLabel_Changelist";
			this.mLabel_Changelist.Size = new System.Drawing.Size( 82, 13 );
			this.mLabel_Changelist.TabIndex = 3;
			this.mLabel_Changelist.Text = "Changelist: N/A";
			// 
			// mButton_OK
			// 
			this.mButton_OK.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.mButton_OK.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.mButton_OK.Location = new System.Drawing.Point( 327, 111 );
			this.mButton_OK.Name = "mButton_OK";
			this.mButton_OK.Size = new System.Drawing.Size( 75, 23 );
			this.mButton_OK.TabIndex = 4;
			this.mButton_OK.Text = "OK";
			this.mButton_OK.UseVisualStyleBackColor = true;
			// 
			// mLabel_AppVersion
			// 
			this.mLabel_AppVersion.AutoSize = true;
			this.mLabel_AppVersion.Location = new System.Drawing.Point( 82, 60 );
			this.mLabel_AppVersion.Name = "mLabel_AppVersion";
			this.mLabel_AppVersion.Size = new System.Drawing.Size( 123, 13 );
			this.mLabel_AppVersion.TabIndex = 5;
			this.mLabel_AppVersion.Text = "Application Version: N/A";
			// 
			// UnrealAboutBox
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 6F, 13F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 414, 146 );
			this.Controls.Add( this.mLabel_AppVersion );
			this.Controls.Add( this.mButton_OK );
			this.Controls.Add( this.mLabel_Changelist );
			this.Controls.Add( this.mLabel_EngineVersion );
			this.Controls.Add( this.label1 );
			this.Controls.Add( this.mPictureBox_AppIcon );
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.MaximizeBox = false;
			this.Name = "UnrealAboutBox";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "UnrealAboutBox";
			( ( System.ComponentModel.ISupportInitialize )( this.mPictureBox_AppIcon ) ).EndInit();
			this.ResumeLayout( false );
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.PictureBox mPictureBox_AppIcon;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label mLabel_EngineVersion;
		private System.Windows.Forms.Label mLabel_Changelist;
		private System.Windows.Forms.Button mButton_OK;
		private System.Windows.Forms.Label mLabel_AppVersion;
	}
}