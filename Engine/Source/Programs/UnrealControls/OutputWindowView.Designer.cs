// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

 namespace UnrealControls
{
	partial class OutputWindowView
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

		#region Component Designer generated code

		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager( typeof( OutputWindowView ) );
			this.mHScrollBar = new System.Windows.Forms.HScrollBar();
			this.mVScrollBar = new System.Windows.Forms.VScrollBar();
			this.mDefaultCtxMenu = new System.Windows.Forms.ContextMenuStrip( this.components );
			this.mCtxDefault_Copy = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.mCtxDefault_ScrollToEnd = new System.Windows.Forms.ToolStripMenuItem();
			this.mCtxDefault_ScrollToHome = new System.Windows.Forms.ToolStripMenuItem();
			this.mScrollSelectedTextTimer = new System.Windows.Forms.Timer( this.components );
			this.mToolStrip_Find = new System.Windows.Forms.ToolStrip();
			this.toolStripLabel_Find = new System.Windows.Forms.ToolStripLabel();
			this.mToolStripTextBox_Find = new System.Windows.Forms.ToolStripTextBox();
			this.mToolStripButton_FindNext = new System.Windows.Forms.ToolStripButton();
			this.mToolStripButton_FindPrevious = new System.Windows.Forms.ToolStripButton();
			this.ToolStripMenuItemClear = new System.Windows.Forms.ToolStripMenuItem();
			this.mDefaultCtxMenu.SuspendLayout();
			this.mToolStrip_Find.SuspendLayout();
			this.SuspendLayout();
			// 
			// mHScrollBar
			// 
			this.mHScrollBar.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.mHScrollBar.Cursor = System.Windows.Forms.Cursors.Arrow;
			this.mHScrollBar.Location = new System.Drawing.Point( 0, 225 );
			this.mHScrollBar.Name = "mHScrollBar";
			this.mHScrollBar.Size = new System.Drawing.Size( 422, 17 );
			this.mHScrollBar.TabIndex = 0;
			this.mHScrollBar.ValueChanged += new System.EventHandler( this.mHScrollBar_ValueChanged );
			// 
			// mVScrollBar
			// 
			this.mVScrollBar.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.mVScrollBar.Cursor = System.Windows.Forms.Cursors.Arrow;
			this.mVScrollBar.Location = new System.Drawing.Point( 422, 0 );
			this.mVScrollBar.Name = "mVScrollBar";
			this.mVScrollBar.Size = new System.Drawing.Size( 17, 225 );
			this.mVScrollBar.TabIndex = 1;
			this.mVScrollBar.ValueChanged += new System.EventHandler( this.mVScrollBar_ValueChanged );
			// 
			// mDefaultCtxMenu
			// 
			this.mDefaultCtxMenu.Items.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.mCtxDefault_Copy,
            this.ToolStripMenuItemClear,
            this.toolStripSeparator1,
            this.mCtxDefault_ScrollToEnd,
            this.mCtxDefault_ScrollToHome} );
			this.mDefaultCtxMenu.Name = "mDefaultCtxMenu";
			this.mDefaultCtxMenu.Size = new System.Drawing.Size( 153, 120 );
			// 
			// mCtxDefault_Copy
			// 
			this.mCtxDefault_Copy.Name = "mCtxDefault_Copy";
			this.mCtxDefault_Copy.Size = new System.Drawing.Size( 152, 22 );
			this.mCtxDefault_Copy.Text = "Copy";
			this.mCtxDefault_Copy.Click += new System.EventHandler( this.mCtxDefault_Copy_Click );
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			this.toolStripSeparator1.Size = new System.Drawing.Size( 149, 6 );
			// 
			// mCtxDefault_ScrollToEnd
			// 
			this.mCtxDefault_ScrollToEnd.Name = "mCtxDefault_ScrollToEnd";
			this.mCtxDefault_ScrollToEnd.Size = new System.Drawing.Size( 152, 22 );
			this.mCtxDefault_ScrollToEnd.Text = "Scroll To End";
			this.mCtxDefault_ScrollToEnd.Click += new System.EventHandler( this.mCtxDefault_ScrollToEnd_Click );
			// 
			// mCtxDefault_ScrollToHome
			// 
			this.mCtxDefault_ScrollToHome.Name = "mCtxDefault_ScrollToHome";
			this.mCtxDefault_ScrollToHome.Size = new System.Drawing.Size( 152, 22 );
			this.mCtxDefault_ScrollToHome.Text = "Scroll To Home";
			this.mCtxDefault_ScrollToHome.Click += new System.EventHandler( this.mCtxDefault_ScrollToHome_Click );
			// 
			// mScrollSelectedTextTimer
			// 
			this.mScrollSelectedTextTimer.Interval = 20;
			this.mScrollSelectedTextTimer.Tick += new System.EventHandler( this.mScrollSelectedTextTimer_Tick );
			// 
			// mToolStrip_Find
			// 
			this.mToolStrip_Find.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.mToolStrip_Find.Font = new System.Drawing.Font( "Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.mToolStrip_Find.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
			this.mToolStrip_Find.Items.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.toolStripLabel_Find,
            this.mToolStripTextBox_Find,
            this.mToolStripButton_FindNext,
            this.mToolStripButton_FindPrevious} );
			this.mToolStrip_Find.Location = new System.Drawing.Point( 0, 242 );
			this.mToolStrip_Find.Name = "mToolStrip_Find";
			this.mToolStrip_Find.RenderMode = System.Windows.Forms.ToolStripRenderMode.System;
			this.mToolStrip_Find.Size = new System.Drawing.Size( 439, 25 );
			this.mToolStrip_Find.TabIndex = 2;
			this.mToolStrip_Find.Text = "toolStrip1";
			this.mToolStrip_Find.VisibleChanged += new System.EventHandler( this.mToolStrip_Find_VisibleChanged );
			this.mToolStrip_Find.MouseEnter += new System.EventHandler( this.ChildControl_MouseEnter );
			// 
			// toolStripLabel_Find
			// 
			this.toolStripLabel_Find.BackColor = System.Drawing.SystemColors.Control;
			this.toolStripLabel_Find.Name = "toolStripLabel_Find";
			this.toolStripLabel_Find.Size = new System.Drawing.Size( 33, 22 );
			this.toolStripLabel_Find.Text = "Find:";
			// 
			// mToolStripTextBox_Find
			// 
			this.mToolStripTextBox_Find.Name = "mToolStripTextBox_Find";
			this.mToolStripTextBox_Find.Size = new System.Drawing.Size( 100, 25 );
			this.mToolStripTextBox_Find.TextChanged += new System.EventHandler( this.mToolStripTextBox_Find_TextChanged );
			// 
			// mToolStripButton_FindNext
			// 
			this.mToolStripButton_FindNext.Image = ( ( System.Drawing.Image )( resources.GetObject( "mToolStripButton_FindNext.Image" ) ) );
			this.mToolStripButton_FindNext.ImageTransparentColor = System.Drawing.Color.Gray;
			this.mToolStripButton_FindNext.Name = "mToolStripButton_FindNext";
			this.mToolStripButton_FindNext.Size = new System.Drawing.Size( 51, 22 );
			this.mToolStripButton_FindNext.Text = "Next";
			this.mToolStripButton_FindNext.Click += new System.EventHandler( this.mToolStripButton_FindNext_Click );
			// 
			// mToolStripButton_FindPrevious
			// 
			this.mToolStripButton_FindPrevious.Image = ( ( System.Drawing.Image )( resources.GetObject( "mToolStripButton_FindPrevious.Image" ) ) );
			this.mToolStripButton_FindPrevious.ImageTransparentColor = System.Drawing.Color.Gray;
			this.mToolStripButton_FindPrevious.Name = "mToolStripButton_FindPrevious";
			this.mToolStripButton_FindPrevious.Size = new System.Drawing.Size( 72, 22 );
			this.mToolStripButton_FindPrevious.Text = "Previous";
			this.mToolStripButton_FindPrevious.Click += new System.EventHandler( this.mToolStripButton_FindPrevious_Click );
			// 
			// ToolStripMenuItemClear
			// 
			this.ToolStripMenuItemClear.Name = "ToolStripMenuItemClear";
			this.ToolStripMenuItemClear.Size = new System.Drawing.Size( 152, 22 );
			this.ToolStripMenuItemClear.Text = "Clear";
			this.ToolStripMenuItemClear.Click += new System.EventHandler( this.ToolStripMenuItem_Clear_Click );
			// 
			// OutputWindowView
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 15F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.BackColor = System.Drawing.SystemColors.Window;
			this.ContextMenuStrip = this.mDefaultCtxMenu;
			this.Controls.Add( this.mToolStrip_Find );
			this.Controls.Add( this.mVScrollBar );
			this.Controls.Add( this.mHScrollBar );
			this.Cursor = System.Windows.Forms.Cursors.Arrow;
			this.DoubleBuffered = true;
			this.Font = new System.Drawing.Font( "Courier New", 9F );
			this.ForeColor = System.Drawing.SystemColors.WindowText;
			this.Name = "OutputWindowView";
			this.Size = new System.Drawing.Size( 439, 267 );
			this.MouseMove += new System.Windows.Forms.MouseEventHandler( this.OutputWindowView_MouseMove );
			this.mDefaultCtxMenu.ResumeLayout( false );
			this.mToolStrip_Find.ResumeLayout( false );
			this.mToolStrip_Find.PerformLayout();
			this.ResumeLayout( false );
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.HScrollBar mHScrollBar;
		private System.Windows.Forms.VScrollBar mVScrollBar;
		private System.Windows.Forms.ContextMenuStrip mDefaultCtxMenu;
		private System.Windows.Forms.ToolStripMenuItem mCtxDefault_Copy;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripMenuItem mCtxDefault_ScrollToEnd;
		private System.Windows.Forms.ToolStripMenuItem mCtxDefault_ScrollToHome;
		private System.Windows.Forms.Timer mScrollSelectedTextTimer;
		private System.Windows.Forms.ToolStrip mToolStrip_Find;
		private System.Windows.Forms.ToolStripLabel toolStripLabel_Find;
		private System.Windows.Forms.ToolStripButton mToolStripButton_FindNext;
		private System.Windows.Forms.ToolStripButton mToolStripButton_FindPrevious;
		private System.Windows.Forms.ToolStripTextBox mToolStripTextBox_Find;
		private System.Windows.Forms.ToolStripMenuItem ToolStripMenuItemClear;
	}
}
