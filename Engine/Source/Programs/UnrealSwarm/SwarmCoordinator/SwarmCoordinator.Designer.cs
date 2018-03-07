// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace SwarmCoordinator
{
    partial class SwarmCoordinator
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose( bool disposing )
        {
            if( disposing && ( components != null ) )
            {
                components.Dispose();
            }
            base.Dispose( disposing );
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
			this.AgentGridView = new System.Windows.Forms.DataGridView();
			this.AgentName = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.GroupName = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.Version = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.State = new System.Windows.Forms.DataGridViewComboBoxColumn();
			this.IPAddress = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.LastPingTime = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.NumLocalCores = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.NumRemoteCores = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.RestartAllAgentsButton = new System.Windows.Forms.Button();
			this.RestartCoordinatorButton = new System.Windows.Forms.Button();
			this.RestartQAAgentsButton = new System.Windows.Forms.Button();
			( ( System.ComponentModel.ISupportInitialize )( this.AgentGridView ) ).BeginInit();
			this.SuspendLayout();
			// 
			// AgentGridView
			// 
			this.AgentGridView.AllowUserToAddRows = false;
			this.AgentGridView.AllowUserToDeleteRows = false;
			this.AgentGridView.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.AgentGridView.Columns.AddRange( new System.Windows.Forms.DataGridViewColumn[] {
            this.AgentName,
            this.GroupName,
            this.Version,
            this.State,
            this.IPAddress,
            this.LastPingTime,
            this.NumLocalCores,
            this.NumRemoteCores} );
			this.AgentGridView.Location = new System.Drawing.Point( 0, 0 );
			this.AgentGridView.Margin = new System.Windows.Forms.Padding( 0 );
			this.AgentGridView.Name = "AgentGridView";
			this.AgentGridView.RowHeadersWidth = 4;
			this.AgentGridView.Size = new System.Drawing.Size( 1091, 476 );
			this.AgentGridView.TabIndex = 0;
			// 
			// AgentName
			// 
			this.AgentName.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
			this.AgentName.HeaderText = "Name";
			this.AgentName.Name = "AgentName";
			this.AgentName.ReadOnly = true;
			this.AgentName.Width = 60;
			// 
			// GroupName
			// 
			this.GroupName.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
			this.GroupName.HeaderText = "Group Name";
			this.GroupName.Name = "GroupName";
			this.GroupName.ReadOnly = true;
			this.GroupName.Width = 102;
			// 
			// Version
			// 
			this.Version.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
			this.Version.HeaderText = "Agent Version";
			this.Version.Name = "Version";
			this.Version.ReadOnly = true;
			this.Version.Width = 123;
			// 
			// State
			// 
			this.State.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
			this.State.HeaderText = "State";
			this.State.Name = "State";
			this.State.ReadOnly = true;
			this.State.Width = 48;
			// 
			// IPAddress
			// 
			this.IPAddress.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
			this.IPAddress.HeaderText = "IP Address";
			this.IPAddress.Name = "IPAddress";
			this.IPAddress.ReadOnly = true;
			this.IPAddress.Width = 102;
			// 
			// LastPingTime
			// 
			this.LastPingTime.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
			this.LastPingTime.HeaderText = "Last Ping Time";
			this.LastPingTime.Name = "LastPingTime";
			this.LastPingTime.ReadOnly = true;
			this.LastPingTime.Width = 130;
			// 
			// NumLocalCores
			// 
			this.NumLocalCores.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
			this.NumLocalCores.HeaderText = "Cores for Local";
			this.NumLocalCores.Name = "NumLocalCores";
			this.NumLocalCores.ReadOnly = true;
			this.NumLocalCores.Width = 137;
			// 
			// NumRemoteCores
			// 
			this.NumRemoteCores.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.AllCells;
			this.NumRemoteCores.HeaderText = "Cores for Remote";
			this.NumRemoteCores.Name = "NumRemoteCores";
			this.NumRemoteCores.Width = 144;
			// 
			// RestartAllAgentsButton
			// 
			this.RestartAllAgentsButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.RestartAllAgentsButton.Location = new System.Drawing.Point( 1097, 50 );
			this.RestartAllAgentsButton.Name = "RestartAllAgentsButton";
			this.RestartAllAgentsButton.Size = new System.Drawing.Size( 155, 32 );
			this.RestartAllAgentsButton.TabIndex = 1;
			this.RestartAllAgentsButton.Text = "Restart All Agents";
			this.RestartAllAgentsButton.UseVisualStyleBackColor = true;
			this.RestartAllAgentsButton.Click += new System.EventHandler( this.RestartAllAgentsButton_Click );
			// 
			// RestartCoordinatorButton
			// 
			this.RestartCoordinatorButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.RestartCoordinatorButton.Location = new System.Drawing.Point( 1097, 88 );
			this.RestartCoordinatorButton.Name = "RestartCoordinatorButton";
			this.RestartCoordinatorButton.Size = new System.Drawing.Size( 155, 32 );
			this.RestartCoordinatorButton.TabIndex = 2;
			this.RestartCoordinatorButton.Text = "Restart Coordinator";
			this.RestartCoordinatorButton.UseVisualStyleBackColor = true;
			this.RestartCoordinatorButton.Click += new System.EventHandler( this.RestartCoordinatorButton_Click );
			// 
			// RestartQAAgentsButton
			// 
			this.RestartQAAgentsButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.RestartQAAgentsButton.Location = new System.Drawing.Point( 1097, 12 );
			this.RestartQAAgentsButton.Name = "RestartQAAgentsButton";
			this.RestartQAAgentsButton.Size = new System.Drawing.Size( 155, 32 );
			this.RestartQAAgentsButton.TabIndex = 3;
			this.RestartQAAgentsButton.Text = "Restart QA Agents";
			this.RestartQAAgentsButton.UseVisualStyleBackColor = true;
			this.RestartQAAgentsButton.Click += new System.EventHandler( this.RestartQAAgentsButton_Click );
			// 
			// SwarmCoordinator
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 14F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 1264, 476 );
			this.Controls.Add( this.RestartQAAgentsButton );
			this.Controls.Add( this.RestartCoordinatorButton );
			this.Controls.Add( this.RestartAllAgentsButton );
			this.Controls.Add( this.AgentGridView );
			this.Font = new System.Drawing.Font( "Consolas", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.Name = "SwarmCoordinator";
			this.RightToLeft = System.Windows.Forms.RightToLeft.No;
			this.Text = "Swarm Coordinator";
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler( this.SwarmCoordinator_Closing );
			( ( System.ComponentModel.ISupportInitialize )( this.AgentGridView ) ).EndInit();
			this.ResumeLayout( false );

        }

        #endregion

        private System.Windows.Forms.DataGridView AgentGridView;
		private System.Windows.Forms.Button RestartAllAgentsButton;
		private System.Windows.Forms.Button RestartCoordinatorButton;
		private System.Windows.Forms.DataGridViewTextBoxColumn AgentName;
		private System.Windows.Forms.DataGridViewTextBoxColumn GroupName;
		private System.Windows.Forms.DataGridViewTextBoxColumn Version;
		private System.Windows.Forms.DataGridViewComboBoxColumn State;
		private System.Windows.Forms.DataGridViewTextBoxColumn IPAddress;
		private System.Windows.Forms.DataGridViewTextBoxColumn LastPingTime;
		private System.Windows.Forms.DataGridViewTextBoxColumn NumLocalCores;
		private System.Windows.Forms.DataGridViewTextBoxColumn NumRemoteCores;
		private System.Windows.Forms.Button RestartQAAgentsButton;
    }
}

