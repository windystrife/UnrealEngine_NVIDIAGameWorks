/*
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

namespace iPhonePackager
{
    partial class ConfigureMobileGame
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ConfigureMobileGame));
            this.label1 = new System.Windows.Forms.Label();
            this.BundleNameEdit = new System.Windows.Forms.TextBox();
            this.GenerateResignFileButton = new System.Windows.Forms.Button();
            this.BundleDisplayNameEdit = new System.Windows.Forms.TextBox();
            this.BundleIdentifierEdit = new System.Windows.Forms.TextBox();
            this.EditManuallyButton = new System.Windows.Forms.Button();
            this.PickDestFolderDialog = new System.Windows.Forms.FolderBrowserDialog();
            this.AppleReferenceHyperlink = new System.Windows.Forms.LinkLabel();
            this.label4 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.label5 = new System.Windows.Forms.Label();
            this.linkLabel1 = new System.Windows.Forms.LinkLabel();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.label1.Location = new System.Drawing.Point(31, 112);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(411, 31);
            this.label1.TabIndex = 0;
            this.label1.Text = "The bundle name (CFBundleName) is a short name for the bundle.\r\nIt should be less" +
                " than 16 characters long.";
            // 
            // BundleNameEdit
            // 
            this.BundleNameEdit.Location = new System.Drawing.Point(34, 146);
            this.BundleNameEdit.Name = "BundleNameEdit";
            this.BundleNameEdit.Size = new System.Drawing.Size(124, 20);
            this.BundleNameEdit.TabIndex = 1;
            // 
            // GenerateResignFileButton
            // 
            this.GenerateResignFileButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.GenerateResignFileButton.Location = new System.Drawing.Point(283, 388);
            this.GenerateResignFileButton.Name = "GenerateResignFileButton";
            this.GenerateResignFileButton.Size = new System.Drawing.Size(159, 33);
            this.GenerateResignFileButton.TabIndex = 3;
            this.GenerateResignFileButton.Text = "Save Changes";
            this.GenerateResignFileButton.UseVisualStyleBackColor = true;
            this.GenerateResignFileButton.Click += new System.EventHandler(this.GenerateResignFileButton_Click);
            // 
            // BundleDisplayNameEdit
            // 
            this.BundleDisplayNameEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.BundleDisplayNameEdit.Location = new System.Drawing.Point(34, 77);
            this.BundleDisplayNameEdit.Name = "BundleDisplayNameEdit";
            this.BundleDisplayNameEdit.Size = new System.Drawing.Size(408, 20);
            this.BundleDisplayNameEdit.TabIndex = 0;
            // 
            // BundleIdentifierEdit
            // 
            this.BundleIdentifierEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.BundleIdentifierEdit.Location = new System.Drawing.Point(31, 284);
            this.BundleIdentifierEdit.Name = "BundleIdentifierEdit";
            this.BundleIdentifierEdit.Size = new System.Drawing.Size(408, 20);
            this.BundleIdentifierEdit.TabIndex = 2;
            // 
            // EditManuallyButton
            // 
            this.EditManuallyButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.EditManuallyButton.Location = new System.Drawing.Point(16, 388);
            this.EditManuallyButton.Name = "EditManuallyButton";
            this.EditManuallyButton.Size = new System.Drawing.Size(129, 33);
            this.EditManuallyButton.TabIndex = 4;
            this.EditManuallyButton.Text = "Find in Explorer...";
            this.EditManuallyButton.UseVisualStyleBackColor = true;
            this.EditManuallyButton.Click += new System.EventHandler(this.EditManuallyButton_Click);
            // 
            // AppleReferenceHyperlink
            // 
            this.AppleReferenceHyperlink.AutoSize = true;
            this.AppleReferenceHyperlink.Location = new System.Drawing.Point(28, 339);
            this.AppleReferenceHyperlink.Name = "AppleReferenceHyperlink";
            this.AppleReferenceHyperlink.Size = new System.Drawing.Size(219, 13);
            this.AppleReferenceHyperlink.TabIndex = 5;
            this.AppleReferenceHyperlink.TabStop = true;
            this.AppleReferenceHyperlink.Tag = "http://developer.apple.com/library/ios/#documentation/general/Reference/InfoPlist" +
                "KeyReference/Introduction/Introduction.html";
            this.AppleReferenceHyperlink.Text = "Apple Reference Documentation for Info.plist";
            this.AppleReferenceHyperlink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.ShowHyperlink);
            // 
            // label4
            // 
            this.label4.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.label4.Location = new System.Drawing.Point(31, 43);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(411, 31);
            this.label4.TabIndex = 11;
            this.label4.Text = "The display name (CFBundleDisplayName) is the one that will be displayed under th" +
                "e icon, in dialog titles, and the settings menu.";
            // 
            // label2
            // 
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.label2.Location = new System.Drawing.Point(31, 185);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(411, 92);
            this.label2.TabIndex = 12;
            this.label2.Text = resources.GetString("label2.Text");
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label3.Location = new System.Drawing.Point(16, 13);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(224, 20);
            this.label3.TabIndex = 13;
            this.label3.Text = "Customize settings in Info.plist";
            // 
            // label5
            // 
            this.label5.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(13, 321);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(105, 13);
            this.label5.TabIndex = 14;
            this.label5.Text = "For more information:";
            // 
            // linkLabel1
            // 
            this.linkLabel1.AutoSize = true;
            this.linkLabel1.Location = new System.Drawing.Point(28, 358);
            this.linkLabel1.Name = "linkLabel1";
            this.linkLabel1.Size = new System.Drawing.Size(160, 13);
            this.linkLabel1.TabIndex = 15;
            this.linkLabel1.TabStop = true;
            this.linkLabel1.Tag = "http://udn.epicgames.com/Three/UDKInfoPListApple_iOS.html";
            this.linkLabel1.Text = "UDN guide to Info.plist overrides";
            this.linkLabel1.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.ShowHyperlink);
            // 
            // ConfigureMobileGame
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(460, 433);
            this.Controls.Add(this.linkLabel1);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.AppleReferenceHyperlink);
            this.Controls.Add(this.EditManuallyButton);
            this.Controls.Add(this.BundleIdentifierEdit);
            this.Controls.Add(this.BundleDisplayNameEdit);
            this.Controls.Add(this.GenerateResignFileButton);
            this.Controls.Add(this.BundleNameEdit);
            this.Controls.Add(this.label1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "ConfigureMobileGame";
            this.Text = "Edit Info.plist Overrides";
            this.Load += new System.EventHandler(this.ConfigureMobileGame_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox BundleNameEdit;
        private System.Windows.Forms.Button GenerateResignFileButton;
        private System.Windows.Forms.TextBox BundleDisplayNameEdit;
        private System.Windows.Forms.TextBox BundleIdentifierEdit;
        private System.Windows.Forms.Button EditManuallyButton;
        private System.Windows.Forms.FolderBrowserDialog PickDestFolderDialog;
        private System.Windows.Forms.LinkLabel AppleReferenceHyperlink;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.LinkLabel linkLabel1;
    }
}