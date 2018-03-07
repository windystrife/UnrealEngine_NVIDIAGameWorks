namespace iPhonePackager
{
    partial class GenerateSigningRequestDialog
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(GenerateSigningRequestDialog));
            this.LabelPrompt = new System.Windows.Forms.Label();
            this.GenerateRequestButton = new System.Windows.Forms.Button();
            this.LabelEMailAddress = new System.Windows.Forms.Label();
            this.EMailEditBox = new System.Windows.Forms.TextBox();
            this.CommonNameEditBox = new System.Windows.Forms.TextBox();
            this.LabelCommonName = new System.Windows.Forms.Label();
            this.SaveDialog = new System.Windows.Forms.SaveFileDialog();
            this.KeyPairFilenameBox = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.OpenDialog = new System.Windows.Forms.OpenFileDialog();
            this.label6 = new System.Windows.Forms.Label();
            this.label11 = new System.Windows.Forms.Label();
            this.EmailCheck = new System.Windows.Forms.PictureBox();
            this.label2 = new System.Windows.Forms.Label();
            this.CommonNameCheck = new System.Windows.Forms.PictureBox();
            this.label3 = new System.Windows.Forms.Label();
            this.KeyPairCheck = new System.Windows.Forms.PictureBox();
            this.label4 = new System.Windows.Forms.Label();
            this.GenerateRequestCheck = new System.Windows.Forms.PictureBox();
            this.panel1 = new System.Windows.Forms.Panel();
            this.GenerateKeyPairButton = new System.Windows.Forms.Button();
            this.ChooseKeyPairButton = new System.Windows.Forms.Button();
            this.label5 = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.EmailCheck)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.CommonNameCheck)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.KeyPairCheck)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.GenerateRequestCheck)).BeginInit();
            this.panel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // LabelPrompt
            // 
            this.LabelPrompt.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.LabelPrompt.Location = new System.Drawing.Point(13, 13);
            this.LabelPrompt.Name = "LabelPrompt";
            this.LabelPrompt.Size = new System.Drawing.Size(558, 38);
            this.LabelPrompt.TabIndex = 0;
            this.LabelPrompt.Text = "You need to generate a certificate signing request (.csr) to upload to the iOS Pr" +
                "ovisioning Portal in order to create a signing certificate (.cer) to run on iOS " +
                "hardware\r\n.";
            // 
            // GenerateRequestButton
            // 
            this.GenerateRequestButton.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.GenerateRequestButton.Location = new System.Drawing.Point(98, 367);
            this.GenerateRequestButton.Name = "GenerateRequestButton";
            this.GenerateRequestButton.Size = new System.Drawing.Size(473, 32);
            this.GenerateRequestButton.TabIndex = 3;
            this.GenerateRequestButton.Text = "Generate Certificate Request...";
            this.GenerateRequestButton.UseVisualStyleBackColor = true;
            this.GenerateRequestButton.Click += new System.EventHandler(this.GenerateRequestButton_Click);
            // 
            // LabelEMailAddress
            // 
            this.LabelEMailAddress.AutoSize = true;
            this.LabelEMailAddress.Location = new System.Drawing.Point(95, 78);
            this.LabelEMailAddress.Name = "LabelEMailAddress";
            this.LabelEMailAddress.Size = new System.Drawing.Size(76, 13);
            this.LabelEMailAddress.TabIndex = 2;
            this.LabelEMailAddress.Text = "Email Address:";
            // 
            // EMailEditBox
            // 
            this.EMailEditBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.EMailEditBox.Location = new System.Drawing.Point(98, 95);
            this.EMailEditBox.Name = "EMailEditBox";
            this.EMailEditBox.Size = new System.Drawing.Size(473, 20);
            this.EMailEditBox.TabIndex = 0;
            this.EMailEditBox.TextChanged += new System.EventHandler(this.ValidateCertSteps);
            // 
            // CommonNameEditBox
            // 
            this.CommonNameEditBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.CommonNameEditBox.Location = new System.Drawing.Point(98, 159);
            this.CommonNameEditBox.Name = "CommonNameEditBox";
            this.CommonNameEditBox.Size = new System.Drawing.Size(473, 20);
            this.CommonNameEditBox.TabIndex = 1;
            this.CommonNameEditBox.TextChanged += new System.EventHandler(this.ValidateCertSteps);
            // 
            // LabelCommonName
            // 
            this.LabelCommonName.AutoSize = true;
            this.LabelCommonName.Location = new System.Drawing.Point(95, 142);
            this.LabelCommonName.Name = "LabelCommonName";
            this.LabelCommonName.Size = new System.Drawing.Size(306, 13);
            this.LabelCommonName.TabIndex = 4;
            this.LabelCommonName.Text = "Common Name (e.g., FirstName LastName or Company Name) :";
            // 
            // KeyPairFilenameBox
            // 
            this.KeyPairFilenameBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.KeyPairFilenameBox.BackColor = System.Drawing.SystemColors.Control;
            this.KeyPairFilenameBox.Location = new System.Drawing.Point(98, 307);
            this.KeyPairFilenameBox.Name = "KeyPairFilenameBox";
            this.KeyPairFilenameBox.Size = new System.Drawing.Size(473, 20);
            this.KeyPairFilenameBox.TabIndex = 7;
            this.KeyPairFilenameBox.TabStop = false;
            this.KeyPairFilenameBox.TextChanged += new System.EventHandler(this.ValidateCertSteps);
            // 
            // label1
            // 
            this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.label1.Location = new System.Drawing.Point(95, 197);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(476, 70);
            this.label1.TabIndex = 6;
            this.label1.Text = resources.GetString("label1.Text");
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(15, 95);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(16, 13);
            this.label6.TabIndex = 44;
            this.label6.Text = "1.";
            // 
            // label11
            // 
            this.label11.Location = new System.Drawing.Point(21, 54);
            this.label11.Name = "label11";
            this.label11.Size = new System.Drawing.Size(65, 28);
            this.label11.TabIndex = 43;
            this.label11.Text = "Finished Steps";
            this.label11.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // EmailCheck
            // 
            this.EmailCheck.Image = global::iPhonePackager.Properties.Resources.GreyCheck;
            this.EmailCheck.Location = new System.Drawing.Point(37, 85);
            this.EmailCheck.Name = "EmailCheck";
            this.EmailCheck.Size = new System.Drawing.Size(32, 32);
            this.EmailCheck.TabIndex = 42;
            this.EmailCheck.TabStop = false;
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(15, 159);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(16, 13);
            this.label2.TabIndex = 46;
            this.label2.Text = "2.";
            // 
            // CommonNameCheck
            // 
            this.CommonNameCheck.Image = global::iPhonePackager.Properties.Resources.GreyCheck;
            this.CommonNameCheck.Location = new System.Drawing.Point(37, 149);
            this.CommonNameCheck.Name = "CommonNameCheck";
            this.CommonNameCheck.Size = new System.Drawing.Size(32, 32);
            this.CommonNameCheck.TabIndex = 45;
            this.CommonNameCheck.TabStop = false;
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(15, 289);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(16, 13);
            this.label3.TabIndex = 48;
            this.label3.Text = "3.";
            // 
            // KeyPairCheck
            // 
            this.KeyPairCheck.Image = global::iPhonePackager.Properties.Resources.GreyCheck;
            this.KeyPairCheck.Location = new System.Drawing.Point(37, 279);
            this.KeyPairCheck.Name = "KeyPairCheck";
            this.KeyPairCheck.Size = new System.Drawing.Size(32, 32);
            this.KeyPairCheck.TabIndex = 47;
            this.KeyPairCheck.TabStop = false;
            // 
            // label4
            // 
            this.label4.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(15, 376);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(16, 13);
            this.label4.TabIndex = 50;
            this.label4.Text = "4.";
            // 
            // GenerateRequestCheck
            // 
            this.GenerateRequestCheck.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.GenerateRequestCheck.Image = global::iPhonePackager.Properties.Resources.GreyCheck;
            this.GenerateRequestCheck.Location = new System.Drawing.Point(37, 367);
            this.GenerateRequestCheck.Name = "GenerateRequestCheck";
            this.GenerateRequestCheck.Size = new System.Drawing.Size(32, 32);
            this.GenerateRequestCheck.TabIndex = 49;
            this.GenerateRequestCheck.TabStop = false;
            // 
            // panel1
            // 
            this.panel1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.panel1.Controls.Add(this.GenerateKeyPairButton);
            this.panel1.Controls.Add(this.ChooseKeyPairButton);
            this.panel1.Location = new System.Drawing.Point(98, 273);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(473, 28);
            this.panel1.TabIndex = 58;
            // 
            // GenerateKeyPairButton
            // 
            this.GenerateKeyPairButton.Dock = System.Windows.Forms.DockStyle.Left;
            this.GenerateKeyPairButton.Location = new System.Drawing.Point(0, 0);
            this.GenerateKeyPairButton.Name = "GenerateKeyPairButton";
            this.GenerateKeyPairButton.Size = new System.Drawing.Size(160, 28);
            this.GenerateKeyPairButton.TabIndex = 2;
            this.GenerateKeyPairButton.Text = "Generate a key pair...";
            this.GenerateKeyPairButton.UseVisualStyleBackColor = true;
            this.GenerateKeyPairButton.Click += new System.EventHandler(this.GenerateKeyPairButton_Click);
            // 
            // ChooseKeyPairButton
            // 
            this.ChooseKeyPairButton.Dock = System.Windows.Forms.DockStyle.Right;
            this.ChooseKeyPairButton.Location = new System.Drawing.Point(313, 0);
            this.ChooseKeyPairButton.Name = "ChooseKeyPairButton";
            this.ChooseKeyPairButton.Size = new System.Drawing.Size(160, 28);
            this.ChooseKeyPairButton.TabIndex = 4;
            this.ChooseKeyPairButton.Text = "Use an existing pair...";
            this.ChooseKeyPairButton.UseVisualStyleBackColor = true;
            this.ChooseKeyPairButton.Click += new System.EventHandler(this.ChooseKeyPairButton_Click);
            // 
            // label5
            // 
            this.label5.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(95, 351);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(359, 13);
            this.label5.TabIndex = 59;
            this.label5.Text = "The information above will be used to generate the certificate request (.csr)";
            // 
            // GenerateSigningRequestDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(583, 416);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.panel1);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.GenerateRequestCheck);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.KeyPairCheck);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.CommonNameCheck);
            this.Controls.Add(this.label6);
            this.Controls.Add(this.label11);
            this.Controls.Add(this.EmailCheck);
            this.Controls.Add(this.KeyPairFilenameBox);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.CommonNameEditBox);
            this.Controls.Add(this.LabelCommonName);
            this.Controls.Add(this.EMailEditBox);
            this.Controls.Add(this.LabelEMailAddress);
            this.Controls.Add(this.GenerateRequestButton);
            this.Controls.Add(this.LabelPrompt);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "GenerateSigningRequestDialog";
            this.Text = "Generate Certificate Request";
            this.Load += new System.EventHandler(this.GenerateSigningRequestDialog_Load);
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.GenerateSigningRequestDialog_FormClosed);
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.GenerateSigningRequestDialog_FormClosing);
            ((System.ComponentModel.ISupportInitialize)(this.EmailCheck)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.CommonNameCheck)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.KeyPairCheck)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.GenerateRequestCheck)).EndInit();
            this.panel1.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label LabelPrompt;
        private System.Windows.Forms.Button GenerateRequestButton;
        private System.Windows.Forms.Label LabelEMailAddress;
        private System.Windows.Forms.TextBox EMailEditBox;
        private System.Windows.Forms.TextBox CommonNameEditBox;
        private System.Windows.Forms.Label LabelCommonName;
        private System.Windows.Forms.SaveFileDialog SaveDialog;
        private System.Windows.Forms.TextBox KeyPairFilenameBox;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.OpenFileDialog OpenDialog;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.Label label11;
        private System.Windows.Forms.PictureBox EmailCheck;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.PictureBox CommonNameCheck;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.PictureBox KeyPairCheck;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.PictureBox GenerateRequestCheck;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.Button GenerateKeyPairButton;
        private System.Windows.Forms.Button ChooseKeyPairButton;
        private System.Windows.Forms.Label label5;
    }
}