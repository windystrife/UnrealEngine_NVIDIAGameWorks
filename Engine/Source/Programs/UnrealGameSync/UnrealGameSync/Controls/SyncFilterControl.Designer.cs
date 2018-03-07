namespace UnrealGameSync.Controls
{
	partial class SyncFilterControl
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

		#region Component Designer generated code

		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.ViewGroupBox = new System.Windows.Forms.GroupBox();
			this.SyntaxButton = new System.Windows.Forms.LinkLabel();
			this.ViewTextBox = new System.Windows.Forms.TextBox();
			this.CategoriesGroupBox = new System.Windows.Forms.GroupBox();
			this.CategoriesCheckList = new System.Windows.Forms.CheckedListBox();
			this.SplitContainer = new System.Windows.Forms.SplitContainer();
			this.ViewGroupBox.SuspendLayout();
			this.CategoriesGroupBox.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.SplitContainer)).BeginInit();
			this.SplitContainer.Panel1.SuspendLayout();
			this.SplitContainer.Panel2.SuspendLayout();
			this.SplitContainer.SuspendLayout();
			this.SuspendLayout();
			// 
			// ViewGroupBox
			// 
			this.ViewGroupBox.Controls.Add(this.SyntaxButton);
			this.ViewGroupBox.Controls.Add(this.ViewTextBox);
			this.ViewGroupBox.Dock = System.Windows.Forms.DockStyle.Fill;
			this.ViewGroupBox.Location = new System.Drawing.Point(0, 0);
			this.ViewGroupBox.Name = "ViewGroupBox";
			this.ViewGroupBox.Size = new System.Drawing.Size(1008, 216);
			this.ViewGroupBox.TabIndex = 5;
			this.ViewGroupBox.TabStop = false;
			this.ViewGroupBox.Text = "Custom View";
			// 
			// SyntaxButton
			// 
			this.SyntaxButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.SyntaxButton.AutoSize = true;
			this.SyntaxButton.Location = new System.Drawing.Point(946, 0);
			this.SyntaxButton.Name = "SyntaxButton";
			this.SyntaxButton.Size = new System.Drawing.Size(41, 15);
			this.SyntaxButton.TabIndex = 7;
			this.SyntaxButton.TabStop = true;
			this.SyntaxButton.Text = "Syntax";
			this.SyntaxButton.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.SyntaxButton_LinkClicked);
			// 
			// ViewTextBox
			// 
			this.ViewTextBox.AcceptsReturn = true;
			this.ViewTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ViewTextBox.Font = new System.Drawing.Font("Courier New", 8F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.ViewTextBox.Location = new System.Drawing.Point(15, 25);
			this.ViewTextBox.Margin = new System.Windows.Forms.Padding(7);
			this.ViewTextBox.Multiline = true;
			this.ViewTextBox.Name = "ViewTextBox";
			this.ViewTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
			this.ViewTextBox.Size = new System.Drawing.Size(976, 179);
			this.ViewTextBox.TabIndex = 6;
			this.ViewTextBox.WordWrap = false;
			// 
			// CategoriesGroupBox
			// 
			this.CategoriesGroupBox.Controls.Add(this.CategoriesCheckList);
			this.CategoriesGroupBox.Dock = System.Windows.Forms.DockStyle.Fill;
			this.CategoriesGroupBox.Location = new System.Drawing.Point(0, 0);
			this.CategoriesGroupBox.Name = "CategoriesGroupBox";
			this.CategoriesGroupBox.Size = new System.Drawing.Size(1008, 415);
			this.CategoriesGroupBox.TabIndex = 4;
			this.CategoriesGroupBox.TabStop = false;
			this.CategoriesGroupBox.Text = "Categories";
			// 
			// CategoriesCheckList
			// 
			this.CategoriesCheckList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.CategoriesCheckList.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.CategoriesCheckList.CheckOnClick = true;
			this.CategoriesCheckList.FormattingEnabled = true;
			this.CategoriesCheckList.IntegralHeight = false;
			this.CategoriesCheckList.Location = new System.Drawing.Point(15, 25);
			this.CategoriesCheckList.Margin = new System.Windows.Forms.Padding(7);
			this.CategoriesCheckList.Name = "CategoriesCheckList";
			this.CategoriesCheckList.Size = new System.Drawing.Size(976, 379);
			this.CategoriesCheckList.Sorted = true;
			this.CategoriesCheckList.TabIndex = 7;
			// 
			// SplitContainer
			// 
			this.SplitContainer.Dock = System.Windows.Forms.DockStyle.Fill;
			this.SplitContainer.Location = new System.Drawing.Point(7, 7);
			this.SplitContainer.Name = "SplitContainer";
			this.SplitContainer.Orientation = System.Windows.Forms.Orientation.Horizontal;
			// 
			// SplitContainer.Panel1
			// 
			this.SplitContainer.Panel1.Controls.Add(this.CategoriesGroupBox);
			// 
			// SplitContainer.Panel2
			// 
			this.SplitContainer.Panel2.Controls.Add(this.ViewGroupBox);
			this.SplitContainer.Size = new System.Drawing.Size(1008, 643);
			this.SplitContainer.SplitterDistance = 415;
			this.SplitContainer.SplitterWidth = 12;
			this.SplitContainer.TabIndex = 8;
			// 
			// SyncFilterControl
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.BackColor = System.Drawing.SystemColors.Window;
			this.Controls.Add(this.SplitContainer);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Name = "SyncFilterControl";
			this.Padding = new System.Windows.Forms.Padding(7);
			this.Size = new System.Drawing.Size(1022, 657);
			this.ViewGroupBox.ResumeLayout(false);
			this.ViewGroupBox.PerformLayout();
			this.CategoriesGroupBox.ResumeLayout(false);
			this.SplitContainer.Panel1.ResumeLayout(false);
			this.SplitContainer.Panel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.SplitContainer)).EndInit();
			this.SplitContainer.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.GroupBox ViewGroupBox;
		private System.Windows.Forms.GroupBox CategoriesGroupBox;
		public System.Windows.Forms.CheckedListBox CategoriesCheckList;
		private System.Windows.Forms.SplitContainer SplitContainer;
		private System.Windows.Forms.LinkLabel SyntaxButton;
		private System.Windows.Forms.TextBox ViewTextBox;

	}
}
