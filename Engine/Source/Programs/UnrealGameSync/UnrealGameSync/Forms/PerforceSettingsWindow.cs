using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class PerforceSettingsWindow : Form
	{
		UserSettings Settings;

		public PerforceSettingsWindow(UserSettings Settings)
		{
			this.Settings = Settings;
			InitializeComponent();
		}

		private void PerforceSettingsWindow_Load(object sender, EventArgs e)
		{
			PerforceSyncOptions SyncOptions = Settings.SyncOptions;
			NumRetriesTextBox.Text = (SyncOptions.NumRetries > 0)? SyncOptions.NumRetries.ToString() : "";
			NumThreadsTextBox.Text = (SyncOptions.NumThreads > 0)? SyncOptions.NumThreads.ToString() : "";
			TcpBufferSizeText.Text = (SyncOptions.TcpBufferSize > 0)? SyncOptions.TcpBufferSize.ToString() : "";
		}

		private void OkButton_Click(object sender, EventArgs e)
		{
			int NewNumRetries = 0;
			if(NumRetriesTextBox.Text.Length > 0 && !int.TryParse(NumRetriesTextBox.Text, out NewNumRetries))
			{
				MessageBox.Show("Invalid value for number of retries");
				return;
			}

			int NewNumThreads = 0;
			if(NumThreadsTextBox.Text.Length > 0 && !int.TryParse(NumThreadsTextBox.Text, out NewNumThreads))
			{
				MessageBox.Show("Invalid value for number of threads");
				return;
			}

			int NewTcpBufferSize = 0;
			if(TcpBufferSizeText.Text.Length > 0 && !int.TryParse(TcpBufferSizeText.Text, out NewTcpBufferSize))
			{
				MessageBox.Show("Invalid value for TCP buffer size");
				return;
			}

			Settings.SyncOptions.NumRetries = NewNumRetries;
			Settings.SyncOptions.NumThreads = NewNumThreads;
			Settings.SyncOptions.TcpBufferSize = NewTcpBufferSize;
			Settings.Save();

			DialogResult = System.Windows.Forms.DialogResult.OK;
			Close();
		}

		private void CancButton_Click(object sender, EventArgs e)
		{
			DialogResult = System.Windows.Forms.DialogResult.Cancel;
			Close();
		}
	}
}
