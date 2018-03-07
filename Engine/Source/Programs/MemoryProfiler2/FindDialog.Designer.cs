/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

namespace MemoryProfiler2
{
    partial class FindDialog
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
			this.label1 = new System.Windows.Forms.Label();
			this.SearchTextBox = new System.Windows.Forms.TextBox();
			this.btnFindNext = new System.Windows.Forms.Button();
			this.btnCancel = new System.Windows.Forms.Button();
			this.MatchCaseCheckButton = new System.Windows.Forms.CheckBox();
			this.btnFindAll = new System.Windows.Forms.Button();
			this.RegexCheckButton = new System.Windows.Forms.CheckBox();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(7, 10);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(72, 13);
			this.label1.TabIndex = 0;
			this.label1.Text = "Search string:";
			// 
			// SearchTextBox
			// 
			this.SearchTextBox.Location = new System.Drawing.Point(85, 7);
			this.SearchTextBox.Name = "SearchTextBox";
			this.SearchTextBox.Size = new System.Drawing.Size(197, 20);
			this.SearchTextBox.TabIndex = 1;
			// 
			// btnFindNext
			// 
			this.btnFindNext.Location = new System.Drawing.Point(288, 5);
			this.btnFindNext.Name = "btnFindNext";
			this.btnFindNext.Size = new System.Drawing.Size(74, 23);
			this.btnFindNext.TabIndex = 2;
			this.btnFindNext.Text = "&Find Next";
			this.btnFindNext.UseVisualStyleBackColor = true;
			this.btnFindNext.Click += new System.EventHandler(this.FindNextButton_Click);
			// 
			// btnCancel
			// 
			this.btnCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.btnCancel.Location = new System.Drawing.Point(288, 61);
			this.btnCancel.Name = "btnCancel";
			this.btnCancel.Size = new System.Drawing.Size(74, 23);
			this.btnCancel.TabIndex = 3;
			this.btnCancel.Text = "Cancel";
			this.btnCancel.UseVisualStyleBackColor = true;
			this.btnCancel.Click += new System.EventHandler(this.CancelButton_Click);
			// 
			// MatchCaseCheckButton
			// 
			this.MatchCaseCheckButton.AutoSize = true;
			this.MatchCaseCheckButton.Location = new System.Drawing.Point(8, 40);
			this.MatchCaseCheckButton.Name = "MatchCaseCheckButton";
			this.MatchCaseCheckButton.Size = new System.Drawing.Size(82, 17);
			this.MatchCaseCheckButton.TabIndex = 4;
			this.MatchCaseCheckButton.Text = "Match &case";
			this.MatchCaseCheckButton.UseVisualStyleBackColor = true;
			// 
			// btnFindAll
			// 
			this.btnFindAll.Location = new System.Drawing.Point(288, 34);
			this.btnFindAll.Name = "btnFindAll";
			this.btnFindAll.Size = new System.Drawing.Size(74, 23);
			this.btnFindAll.TabIndex = 6;
			this.btnFindAll.Text = "Find &All";
			this.btnFindAll.UseVisualStyleBackColor = true;
			this.btnFindAll.Click += new System.EventHandler(this.FindAllButton_Click);
			// 
			// RegexCheckButton
			// 
			this.RegexCheckButton.AutoSize = true;
			this.RegexCheckButton.Location = new System.Drawing.Point(8, 63);
			this.RegexCheckButton.Name = "RegexCheckButton";
			this.RegexCheckButton.Size = new System.Drawing.Size(138, 17);
			this.RegexCheckButton.TabIndex = 7;
			this.RegexCheckButton.Text = "Use &regular expressions";
			this.RegexCheckButton.UseVisualStyleBackColor = true;
			// 
			// FindDialog
			// 
			this.AcceptButton = this.btnFindNext;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.btnCancel;
			this.ClientSize = new System.Drawing.Size(371, 94);
			this.Controls.Add(this.RegexCheckButton);
			this.Controls.Add(this.btnFindAll);
			this.Controls.Add(this.MatchCaseCheckButton);
			this.Controls.Add(this.btnCancel);
			this.Controls.Add(this.btnFindNext);
			this.Controls.Add(this.SearchTextBox);
			this.Controls.Add(this.label1);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "FindDialog";
			this.Text = "Find";
			this.ResumeLayout(false);
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button btnCancel;
        public System.Windows.Forms.TextBox SearchTextBox;
        private System.Windows.Forms.Button btnFindNext;
        private System.Windows.Forms.Button btnFindAll;
        public System.Windows.Forms.CheckBox RegexCheckButton;
        public System.Windows.Forms.CheckBox MatchCaseCheckButton;
    }
}