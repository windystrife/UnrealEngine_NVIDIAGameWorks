namespace ImageValidator
{
    partial class Form1
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.folderBrowserDialog1 = new System.Windows.Forms.FolderBrowserDialog();
            this.imageListThumbnails = new System.Windows.Forms.ImageList(this.components);
            this.listView1 = new System.Windows.Forms.ListView();
            this.textBoxRefDir = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.openFileDialog1 = new System.Windows.Forms.OpenFileDialog();
            this.pictureBoxTest = new System.Windows.Forms.PictureBox();
            this.pictureBoxDiff = new System.Windows.Forms.PictureBox();
            this.pictureBoxRef = new System.Windows.Forms.PictureBox();
            this.buttonPickRefFolder = new System.Windows.Forms.Button();
            this.textBoxTestDir = new System.Windows.Forms.TextBox();
            this.buttonPickTestFolder = new System.Windows.Forms.Button();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.timer1 = new System.Windows.Forms.Timer(this.components);
            this.fileSystemWatcher1 = new System.IO.FileSystemWatcher();
            this.fileSystemWatcher2 = new System.IO.FileSystemWatcher();
            this.menuStrip1 = new System.Windows.Forms.MenuStrip();
            this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.newToolStripMenuNew = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            this.openToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.saveToolStripMenuSave = new System.Windows.Forms.ToolStripMenuItem();
            this.saveAsToolStripMenuSaveAs = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.quitToolStripMenuQuit = new System.Windows.Forms.ToolStripMenuItem();
            this.actionToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.MenuItemGenerateHTMLReport = new System.Windows.Forms.ToolStripMenuItem();
            this.generateHTMLReportWithThumbnailsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.helpToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.howToUseWithUnrealEngineToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator3 = new System.Windows.Forms.ToolStripSeparator();
            this.aboutToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.saveFileDialog1 = new System.Windows.Forms.SaveFileDialog();
            this.saveFileDialogExport = new System.Windows.Forms.SaveFileDialog();
            this.buttonRefresh = new System.Windows.Forms.Button();
            this.textBoxCountToFail = new ImageValidator.NumericTextBox();
            this.textBoxThreshold = new ImageValidator.NumericTextBox();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxTest)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxDiff)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxRef)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.fileSystemWatcher1)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.fileSystemWatcher2)).BeginInit();
            this.menuStrip1.SuspendLayout();
            this.SuspendLayout();
            // 
            // imageListThumbnails
            // 
            this.imageListThumbnails.ColorDepth = System.Windows.Forms.ColorDepth.Depth24Bit;
            this.imageListThumbnails.ImageSize = new System.Drawing.Size(64, 64);
            this.imageListThumbnails.TransparentColor = System.Drawing.Color.Transparent;
            // 
            // listView1
            // 
            this.listView1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.listView1.Location = new System.Drawing.Point(12, 138);
            this.listView1.MultiSelect = false;
            this.listView1.Name = "listView1";
            this.listView1.Size = new System.Drawing.Size(885, 321);
            this.listView1.TabIndex = 1;
            this.listView1.UseCompatibleStateImageBehavior = false;
            this.listView1.View = System.Windows.Forms.View.Details;
            this.listView1.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.listView1_ColumnClick);
            this.listView1.Click += new System.EventHandler(this.listView1_Click);
            // 
            // textBoxRefDir
            // 
            this.textBoxRefDir.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.textBoxRefDir.Location = new System.Drawing.Point(121, 44);
            this.textBoxRefDir.Name = "textBoxRefDir";
            this.textBoxRefDir.Size = new System.Drawing.Size(647, 20);
            this.textBoxRefDir.TabIndex = 2;
            this.textBoxRefDir.KeyDown += new System.Windows.Forms.KeyEventHandler(this.textBoxRefDir_KeyDown);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(26, 47);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(89, 13);
            this.label1.TabIndex = 3;
            this.label1.Text = "Reference Folder";
            this.label1.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // openFileDialog1
            // 
            this.openFileDialog1.DefaultExt = "IVxml";
            this.openFileDialog1.FileName = "openFileDialog1";
            this.openFileDialog1.Filter = "ImageValidator files|*.IVxml|all files|*.*";
            this.openFileDialog1.RestoreDirectory = true;
            // 
            // pictureBoxTest
            // 
            this.pictureBoxTest.BackColor = System.Drawing.Color.Black;
            this.pictureBoxTest.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.pictureBoxTest.Location = new System.Drawing.Point(12, 465);
            this.pictureBoxTest.Name = "pictureBoxTest";
            this.pictureBoxTest.Size = new System.Drawing.Size(143, 142);
            this.pictureBoxTest.TabIndex = 8;
            this.pictureBoxTest.TabStop = false;
            this.pictureBoxTest.Paint += new System.Windows.Forms.PaintEventHandler(this.pictureBoxTest_Paint);
            this.pictureBoxTest.MouseDown += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            this.pictureBoxTest.MouseMove += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            this.pictureBoxTest.MouseUp += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            this.pictureBoxTest.MouseWheel += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            // 
            // pictureBoxDiff
            // 
            this.pictureBoxDiff.BackColor = System.Drawing.Color.Black;
            this.pictureBoxDiff.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.pictureBoxDiff.Location = new System.Drawing.Point(161, 465);
            this.pictureBoxDiff.Name = "pictureBoxDiff";
            this.pictureBoxDiff.Size = new System.Drawing.Size(143, 142);
            this.pictureBoxDiff.TabIndex = 9;
            this.pictureBoxDiff.TabStop = false;
            this.pictureBoxDiff.Paint += new System.Windows.Forms.PaintEventHandler(this.pictureBoxDiff_Paint);
            this.pictureBoxDiff.MouseDown += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            this.pictureBoxDiff.MouseMove += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            this.pictureBoxDiff.MouseUp += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            this.pictureBoxDiff.MouseWheel += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            // 
            // pictureBoxRef
            // 
            this.pictureBoxRef.BackColor = System.Drawing.Color.Black;
            this.pictureBoxRef.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.pictureBoxRef.Location = new System.Drawing.Point(310, 465);
            this.pictureBoxRef.Name = "pictureBoxRef";
            this.pictureBoxRef.Size = new System.Drawing.Size(143, 142);
            this.pictureBoxRef.TabIndex = 10;
            this.pictureBoxRef.TabStop = false;
            this.pictureBoxRef.Paint += new System.Windows.Forms.PaintEventHandler(this.pictureBoxRef_Paint);
            this.pictureBoxRef.MouseDown += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            this.pictureBoxRef.MouseMove += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            this.pictureBoxRef.MouseUp += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            this.pictureBoxRef.MouseWheel += new System.Windows.Forms.MouseEventHandler(this.pictureBoxTest_MouseMove);
            // 
            // buttonPickRefFolder
            // 
            this.buttonPickRefFolder.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.buttonPickRefFolder.Location = new System.Drawing.Point(774, 44);
            this.buttonPickRefFolder.Name = "buttonPickRefFolder";
            this.buttonPickRefFolder.Size = new System.Drawing.Size(32, 20);
            this.buttonPickRefFolder.TabIndex = 11;
            this.buttonPickRefFolder.Text = "...";
            this.buttonPickRefFolder.UseVisualStyleBackColor = true;
            this.buttonPickRefFolder.Click += new System.EventHandler(this.buttonPickRefFolder_Click);
            // 
            // textBoxTestDir
            // 
            this.textBoxTestDir.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.textBoxTestDir.Location = new System.Drawing.Point(121, 70);
            this.textBoxTestDir.Name = "textBoxTestDir";
            this.textBoxTestDir.Size = new System.Drawing.Size(647, 20);
            this.textBoxTestDir.TabIndex = 5;
            this.textBoxTestDir.KeyDown += new System.Windows.Forms.KeyEventHandler(this.textBoxTestDir_KeyDown);
            // 
            // buttonPickTestFolder
            // 
            this.buttonPickTestFolder.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.buttonPickTestFolder.Location = new System.Drawing.Point(774, 70);
            this.buttonPickTestFolder.Name = "buttonPickTestFolder";
            this.buttonPickTestFolder.Size = new System.Drawing.Size(32, 20);
            this.buttonPickTestFolder.TabIndex = 7;
            this.buttonPickTestFolder.Text = "...";
            this.buttonPickTestFolder.UseVisualStyleBackColor = true;
            this.buttonPickTestFolder.Click += new System.EventHandler(this.buttonPickTestFolder_Click);
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(312, 106);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(86, 13);
            this.label3.TabIndex = 10;
            this.label3.Text = "pixel count to fail";
            this.label3.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(55, 73);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(60, 13);
            this.label4.TabIndex = 4;
            this.label4.Text = "Test Folder";
            this.label4.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(121, 106);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(81, 13);
            this.label2.TabIndex = 8;
            this.label2.Text = "Threshold (8Bit)";
            this.label2.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // timer1
            // 
            this.timer1.Tick += new System.EventHandler(this.timer1_Tick);
            // 
            // fileSystemWatcher1
            // 
            this.fileSystemWatcher1.EnableRaisingEvents = true;
            this.fileSystemWatcher1.Filter = "*.png";
            this.fileSystemWatcher1.IncludeSubdirectories = true;
            this.fileSystemWatcher1.SynchronizingObject = this;
            this.fileSystemWatcher1.Changed += new System.IO.FileSystemEventHandler(this.fileSystemWatcher1_Changed);
            this.fileSystemWatcher1.Created += new System.IO.FileSystemEventHandler(this.fileSystemWatcher1_Changed);
            this.fileSystemWatcher1.Deleted += new System.IO.FileSystemEventHandler(this.fileSystemWatcher1_Changed);
            this.fileSystemWatcher1.Renamed += new System.IO.RenamedEventHandler(this.fileSystemWatcher1_Renamed);
            // 
            // fileSystemWatcher2
            // 
            this.fileSystemWatcher2.EnableRaisingEvents = true;
            this.fileSystemWatcher2.Filter = "*.png";
            this.fileSystemWatcher2.IncludeSubdirectories = true;
            this.fileSystemWatcher2.SynchronizingObject = this;
            this.fileSystemWatcher2.Changed += new System.IO.FileSystemEventHandler(this.fileSystemWatcher1_Changed);
            this.fileSystemWatcher2.Created += new System.IO.FileSystemEventHandler(this.fileSystemWatcher1_Changed);
            this.fileSystemWatcher2.Deleted += new System.IO.FileSystemEventHandler(this.fileSystemWatcher1_Changed);
            this.fileSystemWatcher2.Renamed += new System.IO.RenamedEventHandler(this.fileSystemWatcher1_Renamed);
            // 
            // menuStrip1
            // 
            this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.actionToolStripMenuItem,
            this.helpToolStripMenuItem});
            this.menuStrip1.Location = new System.Drawing.Point(0, 0);
            this.menuStrip1.Name = "menuStrip1";
            this.menuStrip1.RenderMode = System.Windows.Forms.ToolStripRenderMode.System;
            this.menuStrip1.Size = new System.Drawing.Size(909, 24);
            this.menuStrip1.TabIndex = 13;
            this.menuStrip1.Text = "menuStrip1";
            // 
            // fileToolStripMenuItem
            // 
            this.fileToolStripMenuItem.BackColor = System.Drawing.SystemColors.ControlDark;
            this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.newToolStripMenuNew,
            this.toolStripSeparator2,
            this.openToolStripMenuItem,
            this.saveToolStripMenuSave,
            this.saveAsToolStripMenuSaveAs,
            this.toolStripSeparator1,
            this.quitToolStripMenuQuit});
            this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
            this.fileToolStripMenuItem.Size = new System.Drawing.Size(37, 20);
            this.fileToolStripMenuItem.Text = "File";
            // 
            // newToolStripMenuNew
            // 
            this.newToolStripMenuNew.Name = "newToolStripMenuNew";
            this.newToolStripMenuNew.Size = new System.Drawing.Size(123, 22);
            this.newToolStripMenuNew.Text = "New";
            this.newToolStripMenuNew.Click += new System.EventHandler(this.newToolStripMenuNew_Click);
            // 
            // toolStripSeparator2
            // 
            this.toolStripSeparator2.Name = "toolStripSeparator2";
            this.toolStripSeparator2.Size = new System.Drawing.Size(120, 6);
            // 
            // openToolStripMenuItem
            // 
            this.openToolStripMenuItem.Name = "openToolStripMenuItem";
            this.openToolStripMenuItem.Size = new System.Drawing.Size(123, 22);
            this.openToolStripMenuItem.Text = "Open...";
            this.openToolStripMenuItem.Click += new System.EventHandler(this.openToolStripMenuItem_Click);
            // 
            // saveToolStripMenuSave
            // 
            this.saveToolStripMenuSave.Name = "saveToolStripMenuSave";
            this.saveToolStripMenuSave.Size = new System.Drawing.Size(123, 22);
            this.saveToolStripMenuSave.Text = "Save";
            this.saveToolStripMenuSave.Click += new System.EventHandler(this.saveToolStripMenuSave_Click);
            // 
            // saveAsToolStripMenuSaveAs
            // 
            this.saveAsToolStripMenuSaveAs.Name = "saveAsToolStripMenuSaveAs";
            this.saveAsToolStripMenuSaveAs.Size = new System.Drawing.Size(123, 22);
            this.saveAsToolStripMenuSaveAs.Text = "Save As...";
            this.saveAsToolStripMenuSaveAs.Click += new System.EventHandler(this.saveAsToolStripMenuSaveAs_Click);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(120, 6);
            // 
            // quitToolStripMenuQuit
            // 
            this.quitToolStripMenuQuit.Name = "quitToolStripMenuQuit";
            this.quitToolStripMenuQuit.Size = new System.Drawing.Size(123, 22);
            this.quitToolStripMenuQuit.Text = "Quit";
            this.quitToolStripMenuQuit.Click += new System.EventHandler(this.quitToolStripMenuQuit_Click);
            // 
            // actionToolStripMenuItem
            // 
            this.actionToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.MenuItemGenerateHTMLReport,
            this.generateHTMLReportWithThumbnailsToolStripMenuItem});
            this.actionToolStripMenuItem.Name = "actionToolStripMenuItem";
            this.actionToolStripMenuItem.Size = new System.Drawing.Size(54, 20);
            this.actionToolStripMenuItem.Text = "Action";
            // 
            // MenuItemGenerateHTMLReport
            // 
            this.MenuItemGenerateHTMLReport.Name = "MenuItemGenerateHTMLReport";
            this.MenuItemGenerateHTMLReport.Size = new System.Drawing.Size(287, 22);
            this.MenuItemGenerateHTMLReport.Text = "Generate HTML Report";
            this.MenuItemGenerateHTMLReport.Click += new System.EventHandler(this.MenuItemGenerateHTMLReport_Click);
            // 
            // generateHTMLReportWithThumbnailsToolStripMenuItem
            // 
            this.generateHTMLReportWithThumbnailsToolStripMenuItem.Name = "generateHTMLReportWithThumbnailsToolStripMenuItem";
            this.generateHTMLReportWithThumbnailsToolStripMenuItem.Size = new System.Drawing.Size(287, 22);
            this.generateHTMLReportWithThumbnailsToolStripMenuItem.Text = "Generate HTML Report with Thumbnails";
            this.generateHTMLReportWithThumbnailsToolStripMenuItem.Click += new System.EventHandler(this.MenuItemGenerateHTMLReportWithT_Click);
            // 
            // helpToolStripMenuItem
            // 
            this.helpToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.howToUseWithUnrealEngineToolStripMenuItem,
            this.toolStripSeparator3,
            this.aboutToolStripMenuItem});
            this.helpToolStripMenuItem.Name = "helpToolStripMenuItem";
            this.helpToolStripMenuItem.Size = new System.Drawing.Size(44, 20);
            this.helpToolStripMenuItem.Text = "Help";
            // 
            // howToUseWithUnrealEngineToolStripMenuItem
            // 
            this.howToUseWithUnrealEngineToolStripMenuItem.Name = "howToUseWithUnrealEngineToolStripMenuItem";
            this.howToUseWithUnrealEngineToolStripMenuItem.Size = new System.Drawing.Size(238, 22);
            this.howToUseWithUnrealEngineToolStripMenuItem.Text = "How to use with UnrealEngine?";
            this.howToUseWithUnrealEngineToolStripMenuItem.Click += new System.EventHandler(this.howToUseWithUnrealEngineToolStripMenuItem_Click);
            // 
            // toolStripSeparator3
            // 
            this.toolStripSeparator3.Name = "toolStripSeparator3";
            this.toolStripSeparator3.Size = new System.Drawing.Size(235, 6);
            // 
            // aboutToolStripMenuItem
            // 
            this.aboutToolStripMenuItem.Name = "aboutToolStripMenuItem";
            this.aboutToolStripMenuItem.Size = new System.Drawing.Size(238, 22);
            this.aboutToolStripMenuItem.Text = "About";
            // 
            // saveFileDialog1
            // 
            this.saveFileDialog1.DefaultExt = "IVxml";
            this.saveFileDialog1.Filter = "ImageValidator files|*.IVxml|all files|*.*";
            this.saveFileDialog1.Title = "Save  ImageValidator Reference";
            // 
            // saveFileDialogExport
            // 
            this.saveFileDialogExport.DefaultExt = "html";
            this.saveFileDialogExport.Filter = "HTML files|*.html;*htm|All files|*.*";
            // 
            // Refresh
            // 
            this.buttonRefresh.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.buttonRefresh.FlatStyle = System.Windows.Forms.FlatStyle.System;
            this.buttonRefresh.Location = new System.Drawing.Point(812, 44);
            this.buttonRefresh.Name = "Refresh";
            this.buttonRefresh.Size = new System.Drawing.Size(85, 79);
            this.buttonRefresh.TabIndex = 14;
            this.buttonRefresh.Text = "Refresh";
            this.buttonRefresh.UseVisualStyleBackColor = true;
            this.buttonRefresh.Click += new System.EventHandler(this.Refresh_Click);
            // 
            // textBoxCountToFail
            // 
            this.textBoxCountToFail.AllowSpace = false;
            this.textBoxCountToFail.Location = new System.Drawing.Point(404, 103);
            this.textBoxCountToFail.Name = "textBoxCountToFail";
            this.textBoxCountToFail.Size = new System.Drawing.Size(49, 20);
            this.textBoxCountToFail.TabIndex = 9;
            this.textBoxCountToFail.Text = "0";
            this.textBoxCountToFail.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            this.textBoxCountToFail.TextChanged += new System.EventHandler(this.textBoxCountToFail_TextChanged);
            // 
            // textBoxThreshold
            // 
            this.textBoxThreshold.AllowSpace = false;
            this.textBoxThreshold.Location = new System.Drawing.Point(208, 103);
            this.textBoxThreshold.Name = "textBoxThreshold";
            this.textBoxThreshold.Size = new System.Drawing.Size(49, 20);
            this.textBoxThreshold.TabIndex = 7;
            this.textBoxThreshold.Text = "0";
            this.textBoxThreshold.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            this.textBoxThreshold.TextChanged += new System.EventHandler(this.textBoxThreshold_TextChanged);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(909, 643);
            this.Controls.Add(this.buttonRefresh);
            this.Controls.Add(this.buttonPickRefFolder);
            this.Controls.Add(this.pictureBoxRef);
            this.Controls.Add(this.textBoxTestDir);
            this.Controls.Add(this.pictureBoxDiff);
            this.Controls.Add(this.buttonPickTestFolder);
            this.Controls.Add(this.pictureBoxTest);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.listView1);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.textBoxCountToFail);
            this.Controls.Add(this.menuStrip1);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.textBoxRefDir);
            this.Controls.Add(this.textBoxThreshold);
            this.Controls.Add(this.label1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MinimumSize = new System.Drawing.Size(600, 500);
            this.Name = "Form1";
            this.Text = "WindowTitle";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.Resize += new System.EventHandler(this.Form1_Resize);
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxTest)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxDiff)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBoxRef)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.fileSystemWatcher1)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.fileSystemWatcher2)).EndInit();
            this.menuStrip1.ResumeLayout(false);
            this.menuStrip1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.FolderBrowserDialog folderBrowserDialog1;
        private System.Windows.Forms.ImageList imageListThumbnails;
        private System.Windows.Forms.ListView listView1;
        private System.Windows.Forms.TextBox textBoxRefDir;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.OpenFileDialog openFileDialog1;
        private System.Windows.Forms.PictureBox pictureBoxTest;
        private System.Windows.Forms.PictureBox pictureBoxDiff;
        private System.Windows.Forms.PictureBox pictureBoxRef;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private NumericTextBox textBoxThreshold;
        private NumericTextBox textBoxCountToFail;
        private System.Windows.Forms.Timer timer1;
        private System.IO.FileSystemWatcher fileSystemWatcher1;
        private System.IO.FileSystemWatcher fileSystemWatcher2;
        private System.Windows.Forms.MenuStrip menuStrip1;
        private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem newToolStripMenuNew;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
        private System.Windows.Forms.ToolStripMenuItem saveToolStripMenuSave;
        private System.Windows.Forms.ToolStripMenuItem saveAsToolStripMenuSaveAs;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripMenuItem quitToolStripMenuQuit;
        private System.Windows.Forms.TextBox textBoxTestDir;
        private System.Windows.Forms.Button buttonPickTestFolder;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.SaveFileDialog saveFileDialog1;
        private System.Windows.Forms.Button buttonPickRefFolder;
        private System.Windows.Forms.ToolStripMenuItem openToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem helpToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem howToUseWithUnrealEngineToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;
        private System.Windows.Forms.ToolStripMenuItem aboutToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem actionToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem MenuItemGenerateHTMLReport;
        private System.Windows.Forms.SaveFileDialog saveFileDialogExport;
        private System.Windows.Forms.ToolStripMenuItem generateHTMLReportWithThumbnailsToolStripMenuItem;
        private System.Windows.Forms.Button buttonRefresh;
    }
}

