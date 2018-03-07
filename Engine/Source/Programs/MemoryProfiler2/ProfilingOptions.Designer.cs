/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

namespace MemoryProfiler2
{
    partial class ProfilingOptions
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
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.LevelStreamEndSnapshotsCheckBox = new System.Windows.Forms.CheckBox();
			this.LevelStreamStartSnapshotsCheckBox = new System.Windows.Forms.CheckBox();
			this.LoadMapMidSnapshotsCheckBox = new System.Windows.Forms.CheckBox();
			this.LoadMapEndSnapshotsCheckBox = new System.Windows.Forms.CheckBox();
			this.GCEndSnapshotsCheckBox = new System.Windows.Forms.CheckBox();
			this.LoadMapStartSnapshotsCheckBox = new System.Windows.Forms.CheckBox();
			this.GCStartSnapshotsCheckBox = new System.Windows.Forms.CheckBox();
			this.btnOK = new System.Windows.Forms.Button();
			this.btnCancel = new System.Windows.Forms.Button();
			this.GenerateSizeGraphsCheckBox = new System.Windows.Forms.CheckBox();
			this.btnSetDefaults = new System.Windows.Forms.Button();
			this.KeepLifecyclesCheckBox = new System.Windows.Forms.CheckBox();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.label3 = new System.Windows.Forms.Label();
			this.label4 = new System.Windows.Forms.Label();
			this.label5 = new System.Windows.Forms.Label();
			this.label6 = new System.Windows.Forms.Label();
			this.label7 = new System.Windows.Forms.Label();
			this.label8 = new System.Windows.Forms.Label();
			this.label9 = new System.Windows.Forms.Label();
			this.label10 = new System.Windows.Forms.Label();
			this.label12 = new System.Windows.Forms.Label();
			this.label13 = new System.Windows.Forms.Label();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// groupBox1
			// 
			this.groupBox1.Controls.Add(this.LevelStreamEndSnapshotsCheckBox);
			this.groupBox1.Controls.Add(this.LevelStreamStartSnapshotsCheckBox);
			this.groupBox1.Controls.Add(this.LoadMapMidSnapshotsCheckBox);
			this.groupBox1.Controls.Add(this.LoadMapEndSnapshotsCheckBox);
			this.groupBox1.Controls.Add(this.GCEndSnapshotsCheckBox);
			this.groupBox1.Controls.Add(this.LoadMapStartSnapshotsCheckBox);
			this.groupBox1.Controls.Add(this.GCStartSnapshotsCheckBox);
			this.groupBox1.Location = new System.Drawing.Point(12, 9);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(165, 196);
			this.groupBox1.TabIndex = 4;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Enabled Snapshot Types";
			// 
			// LevelStreamEndSnapshotsCheckBox
			// 
			this.LevelStreamEndSnapshotsCheckBox.AutoSize = true;
			this.LevelStreamEndSnapshotsCheckBox.Checked = true;
			this.LevelStreamEndSnapshotsCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.LevelStreamEndSnapshotsCheckBox.Location = new System.Drawing.Point(6, 171);
			this.LevelStreamEndSnapshotsCheckBox.Name = "LevelStreamEndSnapshotsCheckBox";
			this.LevelStreamEndSnapshotsCheckBox.Size = new System.Drawing.Size(107, 17);
			this.LevelStreamEndSnapshotsCheckBox.TabIndex = 6;
			this.LevelStreamEndSnapshotsCheckBox.Text = "LevelStream End";
			this.LevelStreamEndSnapshotsCheckBox.UseVisualStyleBackColor = true;
			// 
			// LevelStreamStartSnapshotsCheckBox
			// 
			this.LevelStreamStartSnapshotsCheckBox.AutoSize = true;
			this.LevelStreamStartSnapshotsCheckBox.Checked = true;
			this.LevelStreamStartSnapshotsCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.LevelStreamStartSnapshotsCheckBox.Location = new System.Drawing.Point(6, 148);
			this.LevelStreamStartSnapshotsCheckBox.Name = "LevelStreamStartSnapshotsCheckBox";
			this.LevelStreamStartSnapshotsCheckBox.Size = new System.Drawing.Size(110, 17);
			this.LevelStreamStartSnapshotsCheckBox.TabIndex = 5;
			this.LevelStreamStartSnapshotsCheckBox.Text = "LevelStream Start";
			this.LevelStreamStartSnapshotsCheckBox.UseVisualStyleBackColor = true;
			// 
			// LoadMapMidSnapshotsCheckBox
			// 
			this.LoadMapMidSnapshotsCheckBox.AutoSize = true;
			this.LoadMapMidSnapshotsCheckBox.Checked = true;
			this.LoadMapMidSnapshotsCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.LoadMapMidSnapshotsCheckBox.Location = new System.Drawing.Point(6, 102);
			this.LoadMapMidSnapshotsCheckBox.Name = "LoadMapMidSnapshotsCheckBox";
			this.LoadMapMidSnapshotsCheckBox.Size = new System.Drawing.Size(91, 17);
			this.LoadMapMidSnapshotsCheckBox.TabIndex = 3;
			this.LoadMapMidSnapshotsCheckBox.Text = "LoadMap Mid";
			this.LoadMapMidSnapshotsCheckBox.UseVisualStyleBackColor = true;
			// 
			// LoadMapEndSnapshotsCheckBox
			// 
			this.LoadMapEndSnapshotsCheckBox.AutoSize = true;
			this.LoadMapEndSnapshotsCheckBox.Checked = true;
			this.LoadMapEndSnapshotsCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.LoadMapEndSnapshotsCheckBox.Location = new System.Drawing.Point(6, 125);
			this.LoadMapEndSnapshotsCheckBox.Name = "LoadMapEndSnapshotsCheckBox";
			this.LoadMapEndSnapshotsCheckBox.Size = new System.Drawing.Size(93, 17);
			this.LoadMapEndSnapshotsCheckBox.TabIndex = 4;
			this.LoadMapEndSnapshotsCheckBox.Text = "LoadMap End";
			this.LoadMapEndSnapshotsCheckBox.UseVisualStyleBackColor = true;
			// 
			// GCEndSnapshotsCheckBox
			// 
			this.GCEndSnapshotsCheckBox.AutoSize = true;
			this.GCEndSnapshotsCheckBox.Checked = true;
			this.GCEndSnapshotsCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.GCEndSnapshotsCheckBox.Location = new System.Drawing.Point(6, 56);
			this.GCEndSnapshotsCheckBox.Name = "GCEndSnapshotsCheckBox";
			this.GCEndSnapshotsCheckBox.Size = new System.Drawing.Size(63, 17);
			this.GCEndSnapshotsCheckBox.TabIndex = 1;
			this.GCEndSnapshotsCheckBox.Text = "GC End";
			this.GCEndSnapshotsCheckBox.UseVisualStyleBackColor = true;
			// 
			// LoadMapStartSnapshotsCheckBox
			// 
			this.LoadMapStartSnapshotsCheckBox.AutoSize = true;
			this.LoadMapStartSnapshotsCheckBox.Checked = true;
			this.LoadMapStartSnapshotsCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.LoadMapStartSnapshotsCheckBox.Location = new System.Drawing.Point(6, 79);
			this.LoadMapStartSnapshotsCheckBox.Name = "LoadMapStartSnapshotsCheckBox";
			this.LoadMapStartSnapshotsCheckBox.Size = new System.Drawing.Size(96, 17);
			this.LoadMapStartSnapshotsCheckBox.TabIndex = 2;
			this.LoadMapStartSnapshotsCheckBox.Text = "LoadMap Start";
			this.LoadMapStartSnapshotsCheckBox.UseVisualStyleBackColor = true;
			// 
			// GCStartSnapshotsCheckBox
			// 
			this.GCStartSnapshotsCheckBox.AutoSize = true;
			this.GCStartSnapshotsCheckBox.Checked = true;
			this.GCStartSnapshotsCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.GCStartSnapshotsCheckBox.Location = new System.Drawing.Point(6, 33);
			this.GCStartSnapshotsCheckBox.Name = "GCStartSnapshotsCheckBox";
			this.GCStartSnapshotsCheckBox.Size = new System.Drawing.Size(66, 17);
			this.GCStartSnapshotsCheckBox.TabIndex = 0;
			this.GCStartSnapshotsCheckBox.Text = "GC Start";
			this.GCStartSnapshotsCheckBox.UseVisualStyleBackColor = true;
			// 
			// btnOK
			// 
			this.btnOK.Location = new System.Drawing.Point(111, 266);
			this.btnOK.Name = "btnOK";
			this.btnOK.Size = new System.Drawing.Size(75, 28);
			this.btnOK.TabIndex = 9;
			this.btnOK.Text = "OK";
			this.btnOK.UseVisualStyleBackColor = true;
			this.btnOK.Click += new System.EventHandler(this.btnOK_Click);
			// 
			// btnCancel
			// 
			this.btnCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.btnCancel.Location = new System.Drawing.Point(202, 266);
			this.btnCancel.Name = "btnCancel";
			this.btnCancel.Size = new System.Drawing.Size(75, 28);
			this.btnCancel.TabIndex = 10;
			this.btnCancel.Text = "Cancel";
			this.btnCancel.UseVisualStyleBackColor = true;
			this.btnCancel.Click += new System.EventHandler(this.btnCancel_Click);
			// 
			// GenerateSizeGraphsCheckBox
			// 
			this.GenerateSizeGraphsCheckBox.AutoSize = true;
			this.GenerateSizeGraphsCheckBox.Checked = true;
			this.GenerateSizeGraphsCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.GenerateSizeGraphsCheckBox.Location = new System.Drawing.Point(18, 211);
			this.GenerateSizeGraphsCheckBox.Name = "GenerateSizeGraphsCheckBox";
			this.GenerateSizeGraphsCheckBox.Size = new System.Drawing.Size(159, 17);
			this.GenerateSizeGraphsCheckBox.TabIndex = 7;
			this.GenerateSizeGraphsCheckBox.Text = "Generate Callstack Histories";
			this.GenerateSizeGraphsCheckBox.UseVisualStyleBackColor = true;
			// 
			// btnSetDefaults
			// 
			this.btnSetDefaults.Location = new System.Drawing.Point(294, 266);
			this.btnSetDefaults.Name = "btnSetDefaults";
			this.btnSetDefaults.Size = new System.Drawing.Size(75, 28);
			this.btnSetDefaults.TabIndex = 11;
			this.btnSetDefaults.Text = "Set Defaults";
			this.btnSetDefaults.UseVisualStyleBackColor = true;
			this.btnSetDefaults.Click += new System.EventHandler(this.btnSetDefaults_Click);
			// 
			// KeepLifecyclesCheckBox
			// 
			this.KeepLifecyclesCheckBox.AutoSize = true;
			this.KeepLifecyclesCheckBox.Checked = true;
			this.KeepLifecyclesCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
			this.KeepLifecyclesCheckBox.Location = new System.Drawing.Point(18, 234);
			this.KeepLifecyclesCheckBox.Name = "KeepLifecyclesCheckBox";
			this.KeepLifecyclesCheckBox.Size = new System.Drawing.Size(128, 17);
			this.KeepLifecyclesCheckBox.TabIndex = 8;
			this.KeepLifecyclesCheckBox.Text = "Retain Lifecycle Data";
			this.KeepLifecyclesCheckBox.UseVisualStyleBackColor = true;
			// 
			// label1
			// 
			this.label1.Location = new System.Drawing.Point(183, 9);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(82, 30);
			this.label1.TabIndex = 10;
			this.label1.Text = "Typical Memory Saving";
			this.label1.TextAlign = System.Drawing.ContentAlignment.TopCenter;
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(208, 43);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(27, 13);
			this.label2.TabIndex = 11;
			this.label2.Text = "10%";
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(208, 65);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(27, 13);
			this.label3.TabIndex = 12;
			this.label3.Text = "10%";
			// 
			// label4
			// 
			this.label4.Location = new System.Drawing.Point(208, 89);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(27, 13);
			this.label4.TabIndex = 13;
			this.label4.Text = "1%";
			this.label4.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
			// 
			// label5
			// 
			this.label5.AutoSize = true;
			this.label5.Location = new System.Drawing.Point(214, 112);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(21, 13);
			this.label5.TabIndex = 14;
			this.label5.Text = "1%";
			// 
			// label6
			// 
			this.label6.AutoSize = true;
			this.label6.Location = new System.Drawing.Point(208, 212);
			this.label6.Name = "label6";
			this.label6.Size = new System.Drawing.Size(27, 13);
			this.label6.TabIndex = 15;
			this.label6.Text = "30%";
			// 
			// label7
			// 
			this.label7.AutoSize = true;
			this.label7.Location = new System.Drawing.Point(208, 235);
			this.label7.Name = "label7";
			this.label7.Size = new System.Drawing.Size(27, 13);
			this.label7.TabIndex = 16;
			this.label7.Text = "30%";
			// 
			// label8
			// 
			this.label8.Location = new System.Drawing.Point(291, 9);
			this.label8.Name = "label8";
			this.label8.Size = new System.Drawing.Size(107, 17);
			this.label8.TabIndex = 17;
			this.label8.Text = "Dependent Features";
			this.label8.TextAlign = System.Drawing.ContentAlignment.TopCenter;
			// 
			// label9
			// 
			this.label9.AutoSize = true;
			this.label9.Location = new System.Drawing.Point(262, 235);
			this.label9.Name = "label9";
			this.label9.Size = new System.Drawing.Size(189, 13);
			this.label9.TabIndex = 18;
			this.label9.Text = "Memory Bitmap, Short-lived Allocations";
			// 
			// label10
			// 
			this.label10.AutoSize = true;
			this.label10.Location = new System.Drawing.Point(262, 212);
			this.label10.Name = "label10";
			this.label10.Size = new System.Drawing.Size(65, 13);
			this.label10.TabIndex = 19;
			this.label10.Text = "View History";
			// 
			// label12
			// 
			this.label12.Location = new System.Drawing.Point(262, 42);
			this.label12.Name = "label12";
			this.label12.Size = new System.Drawing.Size(202, 40);
			this.label12.TabIndex = 21;
			this.label12.Text = "Having at least one GC snapshot type enabled will make Short-lived Allocation ana" +
    "lysis much faster.";
			// 
			// label13
			// 
			this.label13.AutoSize = true;
			this.label13.Location = new System.Drawing.Point(214, 135);
			this.label13.Name = "label13";
			this.label13.Size = new System.Drawing.Size(21, 13);
			this.label13.TabIndex = 22;
			this.label13.Text = "1%";
			// 
			// ProfilingOptions
			// 
			this.AcceptButton = this.btnOK;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.btnCancel;
			this.ClientSize = new System.Drawing.Size(472, 304);
			this.Controls.Add(this.label13);
			this.Controls.Add(this.label12);
			this.Controls.Add(this.label10);
			this.Controls.Add(this.label9);
			this.Controls.Add(this.label8);
			this.Controls.Add(this.label7);
			this.Controls.Add(this.label6);
			this.Controls.Add(this.label5);
			this.Controls.Add(this.label4);
			this.Controls.Add(this.label3);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.KeepLifecyclesCheckBox);
			this.Controls.Add(this.btnSetDefaults);
			this.Controls.Add(this.GenerateSizeGraphsCheckBox);
			this.Controls.Add(this.btnCancel);
			this.Controls.Add(this.btnOK);
			this.Controls.Add(this.groupBox1);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ProfilingOptions";
			this.Text = "Profiling Options";
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.Button btnOK;
        private System.Windows.Forms.Button btnCancel;
        private System.Windows.Forms.Button btnSetDefaults;
        public System.Windows.Forms.CheckBox LoadMapEndSnapshotsCheckBox;
        public System.Windows.Forms.CheckBox GCEndSnapshotsCheckBox;
        public System.Windows.Forms.CheckBox LoadMapStartSnapshotsCheckBox;
        public System.Windows.Forms.CheckBox GCStartSnapshotsCheckBox;
        public System.Windows.Forms.CheckBox GenerateSizeGraphsCheckBox;
        public System.Windows.Forms.CheckBox KeepLifecyclesCheckBox;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.Label label7;
        private System.Windows.Forms.Label label8;
        private System.Windows.Forms.Label label9;
		private System.Windows.Forms.Label label10;
        private System.Windows.Forms.Label label12;
        private System.Windows.Forms.Label label13;
        public System.Windows.Forms.CheckBox LevelStreamEndSnapshotsCheckBox;
        public System.Windows.Forms.CheckBox LevelStreamStartSnapshotsCheckBox;
        public System.Windows.Forms.CheckBox LoadMapMidSnapshotsCheckBox;

    }
}