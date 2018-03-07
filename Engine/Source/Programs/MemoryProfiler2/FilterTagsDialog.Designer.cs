namespace MemoryProfiler2
{
	partial class FilterTagsDialog
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
			this.FilterTagsSearch = new System.Windows.Forms.TextBox();
			this.ResetFilterButton = new System.Windows.Forms.Button();
			this.FilterTagsTreeView = new MemoryProfiler2.TreeViewEx();
			this.SuspendLayout();
			// 
			// FilterTagsSearch
			// 
			this.FilterTagsSearch.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.FilterTagsSearch.Location = new System.Drawing.Point(5, 3);
			this.FilterTagsSearch.Name = "FilterTagsSearch";
			this.FilterTagsSearch.Size = new System.Drawing.Size(586, 20);
			this.FilterTagsSearch.TabIndex = 0;
			// 
			// ResetFilterButton
			// 
			this.ResetFilterButton.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ResetFilterButton.Location = new System.Drawing.Point(5, 490);
			this.ResetFilterButton.Name = "ResetFilterButton";
			this.ResetFilterButton.Size = new System.Drawing.Size(586, 23);
			this.ResetFilterButton.TabIndex = 3;
			this.ResetFilterButton.Text = "Reset Filter";
			this.ResetFilterButton.UseVisualStyleBackColor = true;
			// 
			// FilterTagsTreeView
			// 
			this.FilterTagsTreeView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.FilterTagsTreeView.BackColor = System.Drawing.Color.White;
			this.FilterTagsTreeView.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.FilterTagsTreeView.Location = new System.Drawing.Point(5, 26);
			this.FilterTagsTreeView.Name = "FilterTagsTreeView";
			this.FilterTagsTreeView.Size = new System.Drawing.Size(586, 458);
			this.FilterTagsTreeView.TabIndex = 2;
			// 
			// FilterTagsDialog
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(596, 517);
			this.Controls.Add(this.FilterTagsSearch);
			this.Controls.Add(this.ResetFilterButton);
			this.Controls.Add(this.FilterTagsTreeView);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "FilterTagsDialog";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Tags to Filter...";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TextBox FilterTagsSearch;
		private TreeViewEx FilterTagsTreeView;
		private System.Windows.Forms.Button ResetFilterButton;
	}
}