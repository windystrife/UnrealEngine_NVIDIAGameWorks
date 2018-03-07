namespace MemoryProfiler2
{
	partial class OptionsDialog
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose( bool disposing )
		{
			if( disposing && ( components != null ) )
			{
				components.Dispose();
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.OptionsGrid = new System.Windows.Forms.PropertyGrid();
			this.SuspendLayout();
			// 
			// OptionsGrid
			// 
			this.OptionsGrid.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.OptionsGrid.Location = new System.Drawing.Point(12, 12);
			this.OptionsGrid.Name = "OptionsGrid";
			this.OptionsGrid.Size = new System.Drawing.Size(410, 395);
			this.OptionsGrid.TabIndex = 3;
			// 
			// OptionsDialog
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(434, 419);
			this.Controls.Add(this.OptionsGrid);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Name = "OptionsDialog";
			this.Text = "Options";
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.OptionsDialogClosing);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.PropertyGrid OptionsGrid;
	}
}