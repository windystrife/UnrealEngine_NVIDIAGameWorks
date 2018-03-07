namespace UnrealGameSync
{
	partial class PerforceSettingsWindow
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(PerforceSettingsWindow));
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.NumThreadsTextBox = new System.Windows.Forms.TextBox();
			this.label3 = new System.Windows.Forms.Label();
			this.TcpBufferSizeText = new System.Windows.Forms.TextBox();
			this.label2 = new System.Windows.Forms.Label();
			this.NumRetriesTextBox = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.OkButton = new System.Windows.Forms.Button();
			this.CancButton = new System.Windows.Forms.Button();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// groupBox1
			// 
			this.groupBox1.Controls.Add(this.NumThreadsTextBox);
			this.groupBox1.Controls.Add(this.label3);
			this.groupBox1.Controls.Add(this.TcpBufferSizeText);
			this.groupBox1.Controls.Add(this.label2);
			this.groupBox1.Controls.Add(this.NumRetriesTextBox);
			this.groupBox1.Controls.Add(this.label1);
			this.groupBox1.Location = new System.Drawing.Point(14, 21);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(439, 137);
			this.groupBox1.TabIndex = 0;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Syncing";
			// 
			// NumThreadsTextBox
			// 
			this.NumThreadsTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.NumThreadsTextBox.Location = new System.Drawing.Point(253, 59);
			this.NumThreadsTextBox.Name = "NumThreadsTextBox";
			this.NumThreadsTextBox.Size = new System.Drawing.Size(159, 23);
			this.NumThreadsTextBox.TabIndex = 4;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(21, 62);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(196, 15);
			this.label3.TabIndex = 6;
			this.label3.Text = "Number of threads for parallel sync:";
			// 
			// TcpBufferSizeText
			// 
			this.TcpBufferSizeText.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.TcpBufferSizeText.Location = new System.Drawing.Point(253, 90);
			this.TcpBufferSizeText.Name = "TcpBufferSizeText";
			this.TcpBufferSizeText.Size = new System.Drawing.Size(159, 23);
			this.TcpBufferSizeText.TabIndex = 5;
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(21, 93);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(90, 15);
			this.label2.TabIndex = 4;
			this.label2.Text = "TCP Buffer Size:";
			// 
			// NumRetriesTextBox
			// 
			this.NumRetriesTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.NumRetriesTextBox.Location = new System.Drawing.Point(253, 29);
			this.NumRetriesTextBox.Name = "NumRetriesTextBox";
			this.NumRetriesTextBox.Size = new System.Drawing.Size(159, 23);
			this.NumRetriesTextBox.TabIndex = 3;
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(21, 32);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(165, 15);
			this.label1.TabIndex = 2;
			this.label1.Text = "Number of retries on timeout:";
			// 
			// OkButton
			// 
			this.OkButton.Location = new System.Drawing.Point(371, 167);
			this.OkButton.Name = "OkButton";
			this.OkButton.Size = new System.Drawing.Size(87, 26);
			this.OkButton.TabIndex = 1;
			this.OkButton.Text = "Ok";
			this.OkButton.UseVisualStyleBackColor = true;
			this.OkButton.Click += new System.EventHandler(this.OkButton_Click);
			// 
			// CancButton
			// 
			this.CancButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancButton.Location = new System.Drawing.Point(278, 167);
			this.CancButton.Name = "CancButton";
			this.CancButton.Size = new System.Drawing.Size(87, 26);
			this.CancButton.TabIndex = 2;
			this.CancButton.Text = "Cancel";
			this.CancButton.UseVisualStyleBackColor = true;
			this.CancButton.Click += new System.EventHandler(this.CancButton_Click);
			// 
			// PerforceSettingsWindow
			// 
			this.AcceptButton = this.OkButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.CancButton;
			this.ClientSize = new System.Drawing.Size(470, 203);
			this.Controls.Add(this.CancButton);
			this.Controls.Add(this.OkButton);
			this.Controls.Add(this.groupBox1);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "PerforceSettingsWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Perforce Settings";
			this.Load += new System.EventHandler(this.PerforceSettingsWindow_Load);
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.TextBox TcpBufferSizeText;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox NumRetriesTextBox;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Button OkButton;
		private System.Windows.Forms.Button CancButton;
		private System.Windows.Forms.TextBox NumThreadsTextBox;
		private System.Windows.Forms.Label label3;
	}
}