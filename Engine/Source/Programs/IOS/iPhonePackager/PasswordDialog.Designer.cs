namespace iPhonePackager
{
    partial class PasswordDialog
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(PasswordDialog));
            this.PasswordEntryBox = new System.Windows.Forms.MaskedTextBox();
            this.PasswordEntryPrompt = new System.Windows.Forms.Label();
            this.OKButton = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // PasswordEntryBox
            // 
            this.PasswordEntryBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.PasswordEntryBox.Location = new System.Drawing.Point(16, 39);
            this.PasswordEntryBox.Name = "PasswordEntryBox";
            this.PasswordEntryBox.Size = new System.Drawing.Size(264, 20);
            this.PasswordEntryBox.TabIndex = 0;
            this.PasswordEntryBox.UseSystemPasswordChar = true;
            // 
            // PasswordEntryPrompt
            // 
            this.PasswordEntryPrompt.AutoSize = true;
            this.PasswordEntryPrompt.Location = new System.Drawing.Point(13, 13);
            this.PasswordEntryPrompt.Name = "PasswordEntryPrompt";
            this.PasswordEntryPrompt.Size = new System.Drawing.Size(147, 13);
            this.PasswordEntryPrompt.TabIndex = 1;
            this.PasswordEntryPrompt.Text = "Enter password for certificate:";
            // 
            // OKButton
            // 
            this.OKButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.OKButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.OKButton.Location = new System.Drawing.Point(204, 81);
            this.OKButton.Name = "OKButton";
            this.OKButton.Size = new System.Drawing.Size(75, 23);
            this.OKButton.TabIndex = 2;
            this.OKButton.Text = "OK";
            this.OKButton.UseVisualStyleBackColor = true;
            this.OKButton.Click += new System.EventHandler(this.OKButton_Click);
            // 
            // PasswordDialog
            // 
            this.AcceptButton = this.OKButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(291, 113);
            this.Controls.Add(this.OKButton);
            this.Controls.Add(this.PasswordEntryPrompt);
            this.Controls.Add(this.PasswordEntryBox);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "PasswordDialog";
            this.Text = "Password Entry";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.MaskedTextBox PasswordEntryBox;
        private System.Windows.Forms.Label PasswordEntryPrompt;
        private System.Windows.Forms.Button OKButton;
    }
}