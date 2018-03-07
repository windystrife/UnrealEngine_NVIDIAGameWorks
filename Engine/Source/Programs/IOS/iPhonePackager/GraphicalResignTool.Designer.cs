namespace iPhonePackager
{
    partial class GraphicalResignTool
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
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(GraphicalResignTool));
			this.ProfilesMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.copySelectedToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.copyFullPathToProvisionToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.exportCertificateToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.UDIDContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.copyUDIDToClipboardToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripMenuItem2 = new System.Windows.Forms.ToolStripSeparator();
			this.CheckThisUDIDAgainstAMobileprovMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.CheckUDIDInIPAMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.saveFileDialog1 = new System.Windows.Forms.SaveFileDialog();
			this.DeploymentPage = new System.Windows.Forms.TabPage();
			this.InstallIPAButton = new System.Windows.Forms.Button();
			this.FetchUDIDButton = new System.Windows.Forms.Button();
			this.ConnectedDevicesList = new System.Windows.Forms.ListView();
			this.columnHeader1 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.columnHeader2 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.DeviceTypeHeader = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
			this.label2 = new System.Windows.Forms.Label();
			this.ScanningUDIDEdit = new System.Windows.Forms.TextBox();
			this.label3 = new System.Windows.Forms.Label();
			this.CheckIPAForUDIDButton = new System.Windows.Forms.Button();
			this.CheckProvisionForUDIDButton = new System.Windows.Forms.Button();
			this.BackupDocumentsButton = new System.Windows.Forms.Button();
			this.UninstallIPAButton = new System.Windows.Forms.Button();
			this.ResignPage = new System.Windows.Forms.TabPage();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.MobileProvisionEdit = new System.Windows.Forms.TextBox();
			this.MobileProvisionBrowseButton = new System.Windows.Forms.Button();
			this.RBUseEmbeddedProvision = new System.Windows.Forms.RadioButton();
			this.RBSpecifyMobileProvision = new System.Windows.Forms.RadioButton();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.CertificateEdit = new System.Windows.Forms.TextBox();
			this.CertificateBrowseButton = new System.Windows.Forms.Button();
			this.RBSearchForMatchingCert = new System.Windows.Forms.RadioButton();
			this.RBUseExplicitCert = new System.Windows.Forms.RadioButton();
			this.groupBox3 = new System.Windows.Forms.GroupBox();
			this.RBLeaveInfoPListAlone = new System.Windows.Forms.RadioButton();
			this.RBModifyInfoPList = new System.Windows.Forms.RadioButton();
			this.RBReplaceInfoPList = new System.Windows.Forms.RadioButton();
			this.ExportInfoButton = new System.Windows.Forms.Button();
			this.ImportInfoButton = new System.Windows.Forms.Button();
			this.label5 = new System.Windows.Forms.Label();
			this.DisplayNameEdit = new System.Windows.Forms.TextBox();
			this.label6 = new System.Windows.Forms.Label();
			this.BundleIDEdit = new System.Windows.Forms.TextBox();
			this.groupBox4 = new System.Windows.Forms.GroupBox();
			this.IPAFilenameEdit = new System.Windows.Forms.TextBox();
			this.InputIPABrowseButton = new System.Windows.Forms.Button();
			this.groupBox5 = new System.Windows.Forms.GroupBox();
			this.ResignButton = new System.Windows.Forms.Button();
			this.CBCompressModifiedFiles = new System.Windows.Forms.CheckBox();
			this.TabBook = new System.Windows.Forms.TabControl();
			this.ProfilesMenu.SuspendLayout();
			this.UDIDContextMenu.SuspendLayout();
			this.DeploymentPage.SuspendLayout();
			this.ResignPage.SuspendLayout();
			this.groupBox1.SuspendLayout();
			this.groupBox2.SuspendLayout();
			this.groupBox3.SuspendLayout();
			this.groupBox4.SuspendLayout();
			this.groupBox5.SuspendLayout();
			this.TabBook.SuspendLayout();
			this.SuspendLayout();
			// 
			// ProfilesMenu
			// 
			this.ProfilesMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.copySelectedToolStripMenuItem,
            this.copyFullPathToProvisionToolStripMenuItem,
            this.exportCertificateToolStripMenuItem});
			this.ProfilesMenu.Name = "ProfilesMenu";
			this.ProfilesMenu.Size = new System.Drawing.Size(216, 70);
			// 
			// copySelectedToolStripMenuItem
			// 
			this.copySelectedToolStripMenuItem.Name = "copySelectedToolStripMenuItem";
			this.copySelectedToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.C)));
			this.copySelectedToolStripMenuItem.Size = new System.Drawing.Size(215, 22);
			this.copySelectedToolStripMenuItem.Text = "Copy selected";
			this.copySelectedToolStripMenuItem.Click += new System.EventHandler(this.copyToolStripMenuItem_Click);
			// 
			// copyFullPathToProvisionToolStripMenuItem
			// 
			this.copyFullPathToProvisionToolStripMenuItem.Name = "copyFullPathToProvisionToolStripMenuItem";
			this.copyFullPathToProvisionToolStripMenuItem.Size = new System.Drawing.Size(215, 22);
			this.copyFullPathToProvisionToolStripMenuItem.Text = "Copy full path to provision";
			this.copyFullPathToProvisionToolStripMenuItem.Click += new System.EventHandler(this.copyFullPathToProvisionToolStripMenuItem_Click);
			// 
			// exportCertificateToolStripMenuItem
			// 
			this.exportCertificateToolStripMenuItem.Name = "exportCertificateToolStripMenuItem";
			this.exportCertificateToolStripMenuItem.Size = new System.Drawing.Size(215, 22);
			this.exportCertificateToolStripMenuItem.Text = "Export certificate...";
			this.exportCertificateToolStripMenuItem.Click += new System.EventHandler(this.exportCertificateToolStripMenuItem_Click);
			// 
			// UDIDContextMenu
			// 
			this.UDIDContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.copyUDIDToClipboardToolStripMenuItem,
            this.toolStripMenuItem2,
            this.CheckThisUDIDAgainstAMobileprovMenuItem,
            this.CheckUDIDInIPAMenuItem});
			this.UDIDContextMenu.Name = "UDIDContextMenu";
			this.UDIDContextMenu.Size = new System.Drawing.Size(271, 76);
			this.UDIDContextMenu.Opening += new System.ComponentModel.CancelEventHandler(this.UDIDContextMenu_Opening);
			// 
			// copyUDIDToClipboardToolStripMenuItem
			// 
			this.copyUDIDToClipboardToolStripMenuItem.Name = "copyUDIDToClipboardToolStripMenuItem";
			this.copyUDIDToClipboardToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.C)));
			this.copyUDIDToClipboardToolStripMenuItem.Size = new System.Drawing.Size(270, 22);
			this.copyUDIDToClipboardToolStripMenuItem.Text = "Copy to clipboard";
			this.copyUDIDToClipboardToolStripMenuItem.Click += new System.EventHandler(this.copyUDIDToClipboardToolStripMenuItem_Click);
			// 
			// toolStripMenuItem2
			// 
			this.toolStripMenuItem2.Name = "toolStripMenuItem2";
			this.toolStripMenuItem2.Size = new System.Drawing.Size(267, 6);
			// 
			// CheckThisUDIDAgainstAMobileprovMenuItem
			// 
			this.CheckThisUDIDAgainstAMobileprovMenuItem.Name = "CheckThisUDIDAgainstAMobileprovMenuItem";
			this.CheckThisUDIDAgainstAMobileprovMenuItem.Size = new System.Drawing.Size(270, 22);
			this.CheckThisUDIDAgainstAMobileprovMenuItem.Text = "Check this UDID against a provision...";
			this.CheckThisUDIDAgainstAMobileprovMenuItem.Click += new System.EventHandler(this.CheckThisUDIDAgainstAMobileprovMenuItem_Click);
			// 
			// CheckUDIDInIPAMenuItem
			// 
			this.CheckUDIDInIPAMenuItem.Name = "CheckUDIDInIPAMenuItem";
			this.CheckUDIDInIPAMenuItem.Size = new System.Drawing.Size(270, 22);
			this.CheckUDIDInIPAMenuItem.Text = "Check this UDID against an IPA...";
			this.CheckUDIDInIPAMenuItem.Click += new System.EventHandler(this.CheckUDIDInIPAMenuItem_Click);
			// 
			// DeploymentPage
			// 
			this.DeploymentPage.Controls.Add(this.UninstallIPAButton);
			this.DeploymentPage.Controls.Add(this.BackupDocumentsButton);
			this.DeploymentPage.Controls.Add(this.CheckProvisionForUDIDButton);
			this.DeploymentPage.Controls.Add(this.CheckIPAForUDIDButton);
			this.DeploymentPage.Controls.Add(this.label3);
			this.DeploymentPage.Controls.Add(this.ScanningUDIDEdit);
			this.DeploymentPage.Controls.Add(this.label2);
			this.DeploymentPage.Controls.Add(this.ConnectedDevicesList);
			this.DeploymentPage.Controls.Add(this.FetchUDIDButton);
			this.DeploymentPage.Controls.Add(this.InstallIPAButton);
			this.DeploymentPage.Location = new System.Drawing.Point(4, 22);
			this.DeploymentPage.Name = "DeploymentPage";
			this.DeploymentPage.Padding = new System.Windows.Forms.Padding(3);
			this.DeploymentPage.Size = new System.Drawing.Size(515, 485);
			this.DeploymentPage.TabIndex = 5;
			this.DeploymentPage.Text = "Deployment Tools";
			this.DeploymentPage.UseVisualStyleBackColor = true;
			// 
			// InstallIPAButton
			// 
			this.InstallIPAButton.Location = new System.Drawing.Point(15, 362);
			this.InstallIPAButton.Name = "InstallIPAButton";
			this.InstallIPAButton.Size = new System.Drawing.Size(168, 27);
			this.InstallIPAButton.TabIndex = 0;
			this.InstallIPAButton.Text = "Install IPA to all devices...";
			this.InstallIPAButton.UseVisualStyleBackColor = true;
			this.InstallIPAButton.Click += new System.EventHandler(this.InstallIPAButton_Click);
			// 
			// FetchUDIDButton
			// 
			this.FetchUDIDButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.FetchUDIDButton.Location = new System.Drawing.Point(317, 12);
			this.FetchUDIDButton.Name = "FetchUDIDButton";
			this.FetchUDIDButton.Size = new System.Drawing.Size(168, 37);
			this.FetchUDIDButton.TabIndex = 2;
			this.FetchUDIDButton.Text = "Rescan connected devices...";
			this.FetchUDIDButton.UseVisualStyleBackColor = true;
			this.FetchUDIDButton.Click += new System.EventHandler(this.FetchUDIDButton_Click);
			// 
			// ConnectedDevicesList
			// 
			this.ConnectedDevicesList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ConnectedDevicesList.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader1,
            this.columnHeader2,
            this.DeviceTypeHeader});
			this.ConnectedDevicesList.ContextMenuStrip = this.UDIDContextMenu;
			this.ConnectedDevicesList.FullRowSelect = true;
			this.ConnectedDevicesList.HeaderStyle = System.Windows.Forms.ColumnHeaderStyle.Nonclickable;
			this.ConnectedDevicesList.Location = new System.Drawing.Point(15, 59);
			this.ConnectedDevicesList.Name = "ConnectedDevicesList";
			this.ConnectedDevicesList.Size = new System.Drawing.Size(470, 297);
			this.ConnectedDevicesList.TabIndex = 3;
			this.ConnectedDevicesList.UseCompatibleStateImageBehavior = false;
			this.ConnectedDevicesList.View = System.Windows.Forms.View.Details;
			// 
			// columnHeader1
			// 
			this.columnHeader1.Text = "Name";
			this.columnHeader1.Width = 122;
			// 
			// columnHeader2
			// 
			this.columnHeader2.Text = "UDID";
			this.columnHeader2.Width = 260;
			// 
			// DeviceTypeHeader
			// 
			this.DeviceTypeHeader.Text = "Type";
			this.DeviceTypeHeader.Width = 84;
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(12, 43);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(101, 13);
			this.label2.TabIndex = 4;
			this.label2.Text = "Connected Devices";
			// 
			// ScanningUDIDEdit
			// 
			this.ScanningUDIDEdit.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.ScanningUDIDEdit.Location = new System.Drawing.Point(259, 383);
			this.ScanningUDIDEdit.Name = "ScanningUDIDEdit";
			this.ScanningUDIDEdit.Size = new System.Drawing.Size(226, 20);
			this.ScanningUDIDEdit.TabIndex = 5;
			this.ScanningUDIDEdit.TextChanged += new System.EventHandler(this.textBox1_TextChanged);
			// 
			// label3
			// 
			this.label3.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(256, 367);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(37, 13);
			this.label3.TabIndex = 6;
			this.label3.Text = "UDID:";
			// 
			// CheckIPAForUDIDButton
			// 
			this.CheckIPAForUDIDButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CheckIPAForUDIDButton.Location = new System.Drawing.Point(259, 410);
			this.CheckIPAForUDIDButton.Name = "CheckIPAForUDIDButton";
			this.CheckIPAForUDIDButton.Size = new System.Drawing.Size(226, 23);
			this.CheckIPAForUDIDButton.TabIndex = 7;
			this.CheckIPAForUDIDButton.Text = "Check IPA for UDID...";
			this.CheckIPAForUDIDButton.UseVisualStyleBackColor = true;
			this.CheckIPAForUDIDButton.Click += new System.EventHandler(this.CheckIPAForUDIDButton_Click);
			// 
			// CheckProvisionForUDIDButton
			// 
			this.CheckProvisionForUDIDButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.CheckProvisionForUDIDButton.Location = new System.Drawing.Point(259, 439);
			this.CheckProvisionForUDIDButton.Name = "CheckProvisionForUDIDButton";
			this.CheckProvisionForUDIDButton.Size = new System.Drawing.Size(226, 23);
			this.CheckProvisionForUDIDButton.TabIndex = 8;
			this.CheckProvisionForUDIDButton.Text = "Check mobile provision for UDID...";
			this.CheckProvisionForUDIDButton.UseVisualStyleBackColor = true;
			this.CheckProvisionForUDIDButton.Click += new System.EventHandler(this.CheckProvisionForUDIDButton_Click);
			// 
			// BackupDocumentsButton
			// 
			this.BackupDocumentsButton.Location = new System.Drawing.Point(15, 428);
			this.BackupDocumentsButton.Name = "BackupDocumentsButton";
			this.BackupDocumentsButton.Size = new System.Drawing.Size(168, 27);
			this.BackupDocumentsButton.TabIndex = 9;
			this.BackupDocumentsButton.Text = "Backup documents...";
			this.BackupDocumentsButton.UseVisualStyleBackColor = true;
			this.BackupDocumentsButton.Click += new System.EventHandler(this.BackupDocumentsButton_Click);
			// 
			// UninstallIPAButton
			// 
			this.UninstallIPAButton.Location = new System.Drawing.Point(15, 395);
			this.UninstallIPAButton.Name = "UninstallIPAButton";
			this.UninstallIPAButton.Size = new System.Drawing.Size(168, 27);
			this.UninstallIPAButton.TabIndex = 10;
			this.UninstallIPAButton.Text = "Uninstall IPA from all devices...";
			this.UninstallIPAButton.UseVisualStyleBackColor = true;
			this.UninstallIPAButton.Click += new System.EventHandler(this.UninstallIPAButton_Click_1);
			// 
			// ResignPage
			// 
			this.ResignPage.Controls.Add(this.groupBox5);
			this.ResignPage.Controls.Add(this.groupBox4);
			this.ResignPage.Controls.Add(this.groupBox3);
			this.ResignPage.Controls.Add(this.groupBox2);
			this.ResignPage.Controls.Add(this.groupBox1);
			this.ResignPage.Location = new System.Drawing.Point(4, 22);
			this.ResignPage.Name = "ResignPage";
			this.ResignPage.Padding = new System.Windows.Forms.Padding(3);
			this.ResignPage.Size = new System.Drawing.Size(515, 485);
			this.ResignPage.TabIndex = 0;
			this.ResignPage.Text = "Re-signing Tool";
			this.ResignPage.UseVisualStyleBackColor = true;
			// 
			// groupBox1
			// 
			this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox1.Controls.Add(this.RBSpecifyMobileProvision);
			this.groupBox1.Controls.Add(this.RBUseEmbeddedProvision);
			this.groupBox1.Controls.Add(this.MobileProvisionBrowseButton);
			this.groupBox1.Controls.Add(this.MobileProvisionEdit);
			this.groupBox1.Location = new System.Drawing.Point(17, 67);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(480, 77);
			this.groupBox1.TabIndex = 18;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = " Choose Mobile Provision ";
			// 
			// MobileProvisionEdit
			// 
			this.MobileProvisionEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.MobileProvisionEdit.Location = new System.Drawing.Point(188, 47);
			this.MobileProvisionEdit.Name = "MobileProvisionEdit";
			this.MobileProvisionEdit.Size = new System.Drawing.Size(250, 20);
			this.MobileProvisionEdit.TabIndex = 13;
			// 
			// MobileProvisionBrowseButton
			// 
			this.MobileProvisionBrowseButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.MobileProvisionBrowseButton.Location = new System.Drawing.Point(444, 47);
			this.MobileProvisionBrowseButton.Name = "MobileProvisionBrowseButton";
			this.MobileProvisionBrowseButton.Size = new System.Drawing.Size(25, 20);
			this.MobileProvisionBrowseButton.TabIndex = 14;
			this.MobileProvisionBrowseButton.Text = "...";
			this.MobileProvisionBrowseButton.UseVisualStyleBackColor = true;
			this.MobileProvisionBrowseButton.Click += new System.EventHandler(this.MobileProvisionBrowseButton_Click);
			// 
			// RBUseEmbeddedProvision
			// 
			this.RBUseEmbeddedProvision.AutoSize = true;
			this.RBUseEmbeddedProvision.Checked = true;
			this.RBUseEmbeddedProvision.Location = new System.Drawing.Point(23, 25);
			this.RBUseEmbeddedProvision.Name = "RBUseEmbeddedProvision";
			this.RBUseEmbeddedProvision.Size = new System.Drawing.Size(210, 17);
			this.RBUseEmbeddedProvision.TabIndex = 15;
			this.RBUseEmbeddedProvision.TabStop = true;
			this.RBUseEmbeddedProvision.Text = "Use existing embedded.mobileprovision";
			this.RBUseEmbeddedProvision.UseVisualStyleBackColor = true;
			this.RBUseEmbeddedProvision.CheckedChanged += new System.EventHandler(this.SwitchingMobileProvisionMode);
			// 
			// RBSpecifyMobileProvision
			// 
			this.RBSpecifyMobileProvision.AutoSize = true;
			this.RBSpecifyMobileProvision.Location = new System.Drawing.Point(23, 48);
			this.RBSpecifyMobileProvision.Name = "RBSpecifyMobileProvision";
			this.RBSpecifyMobileProvision.Size = new System.Drawing.Size(154, 17);
			this.RBSpecifyMobileProvision.TabIndex = 16;
			this.RBSpecifyMobileProvision.Text = "Specify mobile provision file";
			this.RBSpecifyMobileProvision.UseVisualStyleBackColor = true;
			this.RBSpecifyMobileProvision.CheckedChanged += new System.EventHandler(this.SwitchingMobileProvisionMode);
			// 
			// groupBox2
			// 
			this.groupBox2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox2.Controls.Add(this.RBUseExplicitCert);
			this.groupBox2.Controls.Add(this.RBSearchForMatchingCert);
			this.groupBox2.Controls.Add(this.CertificateBrowseButton);
			this.groupBox2.Controls.Add(this.CertificateEdit);
			this.groupBox2.Location = new System.Drawing.Point(17, 150);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(480, 81);
			this.groupBox2.TabIndex = 19;
			this.groupBox2.TabStop = false;
			this.groupBox2.Text = " Choose signing certificate ";
			// 
			// CertificateEdit
			// 
			this.CertificateEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.CertificateEdit.Location = new System.Drawing.Point(188, 52);
			this.CertificateEdit.Name = "CertificateEdit";
			this.CertificateEdit.Size = new System.Drawing.Size(250, 20);
			this.CertificateEdit.TabIndex = 18;
			// 
			// CertificateBrowseButton
			// 
			this.CertificateBrowseButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.CertificateBrowseButton.Location = new System.Drawing.Point(444, 52);
			this.CertificateBrowseButton.Name = "CertificateBrowseButton";
			this.CertificateBrowseButton.Size = new System.Drawing.Size(25, 20);
			this.CertificateBrowseButton.TabIndex = 19;
			this.CertificateBrowseButton.Text = "...";
			this.CertificateBrowseButton.UseVisualStyleBackColor = true;
			this.CertificateBrowseButton.Click += new System.EventHandler(this.CertificateBrowseButton_Click);
			// 
			// RBSearchForMatchingCert
			// 
			this.RBSearchForMatchingCert.AutoSize = true;
			this.RBSearchForMatchingCert.Checked = true;
			this.RBSearchForMatchingCert.Location = new System.Drawing.Point(23, 28);
			this.RBSearchForMatchingCert.Name = "RBSearchForMatchingCert";
			this.RBSearchForMatchingCert.Size = new System.Drawing.Size(178, 17);
			this.RBSearchForMatchingCert.TabIndex = 20;
			this.RBSearchForMatchingCert.TabStop = true;
			this.RBSearchForMatchingCert.Text = "Search for a matching certificate";
			this.RBSearchForMatchingCert.UseVisualStyleBackColor = true;
			this.RBSearchForMatchingCert.CheckedChanged += new System.EventHandler(this.SwitchingSigningMode);
			// 
			// RBUseExplicitCert
			// 
			this.RBUseExplicitCert.AutoSize = true;
			this.RBUseExplicitCert.Location = new System.Drawing.Point(23, 51);
			this.RBUseExplicitCert.Name = "RBUseExplicitCert";
			this.RBUseExplicitCert.Size = new System.Drawing.Size(159, 17);
			this.RBUseExplicitCert.TabIndex = 21;
			this.RBUseExplicitCert.Text = "Specify an explicit certificate";
			this.RBUseExplicitCert.UseVisualStyleBackColor = true;
			this.RBUseExplicitCert.CheckedChanged += new System.EventHandler(this.SwitchingSigningMode);
			// 
			// groupBox3
			// 
			this.groupBox3.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox3.Controls.Add(this.BundleIDEdit);
			this.groupBox3.Controls.Add(this.label6);
			this.groupBox3.Controls.Add(this.DisplayNameEdit);
			this.groupBox3.Controls.Add(this.label5);
			this.groupBox3.Controls.Add(this.ImportInfoButton);
			this.groupBox3.Controls.Add(this.ExportInfoButton);
			this.groupBox3.Controls.Add(this.RBReplaceInfoPList);
			this.groupBox3.Controls.Add(this.RBModifyInfoPList);
			this.groupBox3.Controls.Add(this.RBLeaveInfoPListAlone);
			this.groupBox3.Location = new System.Drawing.Point(16, 237);
			this.groupBox3.Name = "groupBox3";
			this.groupBox3.Size = new System.Drawing.Size(481, 172);
			this.groupBox3.TabIndex = 20;
			this.groupBox3.TabStop = false;
			this.groupBox3.Text = " Adjust Info.plist ";
			// 
			// RBLeaveInfoPListAlone
			// 
			this.RBLeaveInfoPListAlone.AutoSize = true;
			this.RBLeaveInfoPListAlone.Checked = true;
			this.RBLeaveInfoPListAlone.Location = new System.Drawing.Point(20, 19);
			this.RBLeaveInfoPListAlone.Name = "RBLeaveInfoPListAlone";
			this.RBLeaveInfoPListAlone.Size = new System.Drawing.Size(154, 17);
			this.RBLeaveInfoPListAlone.TabIndex = 19;
			this.RBLeaveInfoPListAlone.TabStop = true;
			this.RBLeaveInfoPListAlone.Text = "Leave Info.plist unchanged";
			this.RBLeaveInfoPListAlone.UseVisualStyleBackColor = true;
			this.RBLeaveInfoPListAlone.CheckedChanged += new System.EventHandler(this.SwitchingInfoEditMode);
			// 
			// RBModifyInfoPList
			// 
			this.RBModifyInfoPList.AutoSize = true;
			this.RBModifyInfoPList.Location = new System.Drawing.Point(20, 42);
			this.RBModifyInfoPList.Name = "RBModifyInfoPList";
			this.RBModifyInfoPList.Size = new System.Drawing.Size(131, 17);
			this.RBModifyInfoPList.TabIndex = 20;
			this.RBModifyInfoPList.Text = "Common Modifications";
			this.RBModifyInfoPList.UseVisualStyleBackColor = true;
			this.RBModifyInfoPList.CheckedChanged += new System.EventHandler(this.SwitchingInfoEditMode);
			// 
			// RBReplaceInfoPList
			// 
			this.RBReplaceInfoPList.AutoSize = true;
			this.RBReplaceInfoPList.Location = new System.Drawing.Point(20, 118);
			this.RBReplaceInfoPList.Name = "RBReplaceInfoPList";
			this.RBReplaceInfoPList.Size = new System.Drawing.Size(76, 17);
			this.RBReplaceInfoPList.TabIndex = 21;
			this.RBReplaceInfoPList.Text = "Full Editing";
			this.RBReplaceInfoPList.UseVisualStyleBackColor = true;
			this.RBReplaceInfoPList.CheckedChanged += new System.EventHandler(this.SwitchingInfoEditMode);
			// 
			// ExportInfoButton
			// 
			this.ExportInfoButton.Location = new System.Drawing.Point(52, 141);
			this.ExportInfoButton.Name = "ExportInfoButton";
			this.ExportInfoButton.Size = new System.Drawing.Size(122, 23);
			this.ExportInfoButton.TabIndex = 22;
			this.ExportInfoButton.Text = "Export to File...";
			this.ExportInfoButton.UseVisualStyleBackColor = true;
			this.ExportInfoButton.Click += new System.EventHandler(this.ExportInfoButton_Click);
			// 
			// ImportInfoButton
			// 
			this.ImportInfoButton.Location = new System.Drawing.Point(180, 141);
			this.ImportInfoButton.Name = "ImportInfoButton";
			this.ImportInfoButton.Size = new System.Drawing.Size(122, 23);
			this.ImportInfoButton.TabIndex = 23;
			this.ImportInfoButton.Text = "Import from File...";
			this.ImportInfoButton.UseVisualStyleBackColor = true;
			this.ImportInfoButton.Click += new System.EventHandler(this.ImportInfoButton_Click);
			// 
			// label5
			// 
			this.label5.AutoSize = true;
			this.label5.Location = new System.Drawing.Point(59, 68);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(115, 13);
			this.label5.TabIndex = 24;
			this.label5.Text = "CFBundleDisplayName";
			// 
			// DisplayNameEdit
			// 
			this.DisplayNameEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.DisplayNameEdit.Location = new System.Drawing.Point(180, 65);
			this.DisplayNameEdit.Name = "DisplayNameEdit";
			this.DisplayNameEdit.Size = new System.Drawing.Size(259, 20);
			this.DisplayNameEdit.TabIndex = 25;
			// 
			// label6
			// 
			this.label6.AutoSize = true;
			this.label6.Location = new System.Drawing.Point(59, 94);
			this.label6.Name = "label6";
			this.label6.Size = new System.Drawing.Size(93, 13);
			this.label6.TabIndex = 26;
			this.label6.Text = "CFBundleIdentifier";
			// 
			// BundleIDEdit
			// 
			this.BundleIDEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.BundleIDEdit.Location = new System.Drawing.Point(180, 91);
			this.BundleIDEdit.Name = "BundleIDEdit";
			this.BundleIDEdit.Size = new System.Drawing.Size(259, 20);
			this.BundleIDEdit.TabIndex = 27;
			// 
			// groupBox4
			// 
			this.groupBox4.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox4.Controls.Add(this.InputIPABrowseButton);
			this.groupBox4.Controls.Add(this.IPAFilenameEdit);
			this.groupBox4.Location = new System.Drawing.Point(16, 12);
			this.groupBox4.Name = "groupBox4";
			this.groupBox4.Size = new System.Drawing.Size(481, 49);
			this.groupBox4.TabIndex = 21;
			this.groupBox4.TabStop = false;
			this.groupBox4.Text = " Input IPA ";
			// 
			// IPAFilenameEdit
			// 
			this.IPAFilenameEdit.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.IPAFilenameEdit.Location = new System.Drawing.Point(20, 19);
			this.IPAFilenameEdit.Name = "IPAFilenameEdit";
			this.IPAFilenameEdit.Size = new System.Drawing.Size(420, 20);
			this.IPAFilenameEdit.TabIndex = 3;
			// 
			// InputIPABrowseButton
			// 
			this.InputIPABrowseButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.InputIPABrowseButton.Location = new System.Drawing.Point(445, 19);
			this.InputIPABrowseButton.Name = "InputIPABrowseButton";
			this.InputIPABrowseButton.Size = new System.Drawing.Size(25, 20);
			this.InputIPABrowseButton.TabIndex = 4;
			this.InputIPABrowseButton.Text = "...";
			this.InputIPABrowseButton.UseVisualStyleBackColor = true;
			this.InputIPABrowseButton.Click += new System.EventHandler(this.InputIPABrowseButton_Click);
			// 
			// groupBox5
			// 
			this.groupBox5.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.groupBox5.Controls.Add(this.CBCompressModifiedFiles);
			this.groupBox5.Controls.Add(this.ResignButton);
			this.groupBox5.Location = new System.Drawing.Point(16, 416);
			this.groupBox5.Name = "groupBox5";
			this.groupBox5.Size = new System.Drawing.Size(481, 58);
			this.groupBox5.TabIndex = 22;
			this.groupBox5.TabStop = false;
			this.groupBox5.Text = " Output Settings ";
			// 
			// ResignButton
			// 
			this.ResignButton.Location = new System.Drawing.Point(20, 19);
			this.ResignButton.Name = "ResignButton";
			this.ResignButton.Size = new System.Drawing.Size(195, 27);
			this.ResignButton.TabIndex = 2;
			this.ResignButton.Text = "Create re-signed IPA...";
			this.ResignButton.UseVisualStyleBackColor = true;
			this.ResignButton.Click += new System.EventHandler(this.ResignButton_Click);
			// 
			// CBCompressModifiedFiles
			// 
			this.CBCompressModifiedFiles.AutoSize = true;
			this.CBCompressModifiedFiles.Location = new System.Drawing.Point(240, 25);
			this.CBCompressModifiedFiles.Name = "CBCompressModifiedFiles";
			this.CBCompressModifiedFiles.Size = new System.Drawing.Size(141, 17);
			this.CBCompressModifiedFiles.TabIndex = 3;
			this.CBCompressModifiedFiles.Text = "Compress modified files?";
			this.CBCompressModifiedFiles.UseVisualStyleBackColor = true;
			// 
			// TabBook
			// 
			this.TabBook.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TabBook.Controls.Add(this.ResignPage);
			this.TabBook.Controls.Add(this.DeploymentPage);
			this.TabBook.Location = new System.Drawing.Point(12, 12);
			this.TabBook.Name = "TabBook";
			this.TabBook.SelectedIndex = 0;
			this.TabBook.Size = new System.Drawing.Size(523, 511);
			this.TabBook.TabIndex = 0;
			this.TabBook.SelectedIndexChanged += new System.EventHandler(this.TabBook_SelectedIndexChanged);
			// 
			// GraphicalResignTool
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(547, 535);
			this.Controls.Add(this.TabBook);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "GraphicalResignTool";
			this.Text = "iOS Tools";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.GraphicalResignTool_FormClosed);
			this.Load += new System.EventHandler(this.GraphicalResignTool_Load);
			this.Shown += new System.EventHandler(this.GraphicalResignTool_Shown);
			this.ProfilesMenu.ResumeLayout(false);
			this.UDIDContextMenu.ResumeLayout(false);
			this.DeploymentPage.ResumeLayout(false);
			this.DeploymentPage.PerformLayout();
			this.ResignPage.ResumeLayout(false);
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.groupBox2.ResumeLayout(false);
			this.groupBox2.PerformLayout();
			this.groupBox3.ResumeLayout(false);
			this.groupBox3.PerformLayout();
			this.groupBox4.ResumeLayout(false);
			this.groupBox4.PerformLayout();
			this.groupBox5.ResumeLayout(false);
			this.groupBox5.PerformLayout();
			this.TabBook.ResumeLayout(false);
			this.ResumeLayout(false);

        }

        #endregion

		private System.Windows.Forms.SaveFileDialog saveFileDialog1;
        private System.Windows.Forms.ContextMenuStrip UDIDContextMenu;
        private System.Windows.Forms.ToolStripMenuItem copyUDIDToClipboardToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem CheckThisUDIDAgainstAMobileprovMenuItem;
        private System.Windows.Forms.ToolStripMenuItem CheckUDIDInIPAMenuItem;
		private System.Windows.Forms.ToolStripSeparator toolStripMenuItem2;
        private System.Windows.Forms.ContextMenuStrip ProfilesMenu;
        private System.Windows.Forms.ToolStripMenuItem copySelectedToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem copyFullPathToProvisionToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem exportCertificateToolStripMenuItem;
		public System.Windows.Forms.TabPage DeploymentPage;
		private System.Windows.Forms.Button UninstallIPAButton;
		private System.Windows.Forms.Button BackupDocumentsButton;
		private System.Windows.Forms.Button CheckProvisionForUDIDButton;
		private System.Windows.Forms.Button CheckIPAForUDIDButton;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.TextBox ScanningUDIDEdit;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.ListView ConnectedDevicesList;
		private System.Windows.Forms.ColumnHeader columnHeader1;
		private System.Windows.Forms.ColumnHeader columnHeader2;
		private System.Windows.Forms.ColumnHeader DeviceTypeHeader;
		private System.Windows.Forms.Button FetchUDIDButton;
		private System.Windows.Forms.Button InstallIPAButton;
		public System.Windows.Forms.TabPage ResignPage;
		private System.Windows.Forms.GroupBox groupBox5;
		private System.Windows.Forms.CheckBox CBCompressModifiedFiles;
		private System.Windows.Forms.Button ResignButton;
		private System.Windows.Forms.GroupBox groupBox4;
		private System.Windows.Forms.Button InputIPABrowseButton;
		private System.Windows.Forms.TextBox IPAFilenameEdit;
		private System.Windows.Forms.GroupBox groupBox3;
		private System.Windows.Forms.TextBox BundleIDEdit;
		private System.Windows.Forms.Label label6;
		private System.Windows.Forms.TextBox DisplayNameEdit;
		private System.Windows.Forms.Label label5;
		private System.Windows.Forms.Button ImportInfoButton;
		private System.Windows.Forms.Button ExportInfoButton;
		private System.Windows.Forms.RadioButton RBReplaceInfoPList;
		private System.Windows.Forms.RadioButton RBModifyInfoPList;
		private System.Windows.Forms.RadioButton RBLeaveInfoPListAlone;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.RadioButton RBUseExplicitCert;
		private System.Windows.Forms.RadioButton RBSearchForMatchingCert;
		private System.Windows.Forms.Button CertificateBrowseButton;
		private System.Windows.Forms.TextBox CertificateEdit;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.RadioButton RBSpecifyMobileProvision;
		private System.Windows.Forms.RadioButton RBUseEmbeddedProvision;
		private System.Windows.Forms.Button MobileProvisionBrowseButton;
		private System.Windows.Forms.TextBox MobileProvisionEdit;
		public System.Windows.Forms.TabControl TabBook;



    }
}