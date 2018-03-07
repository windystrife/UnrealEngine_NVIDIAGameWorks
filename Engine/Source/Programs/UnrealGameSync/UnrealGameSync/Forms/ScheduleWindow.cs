// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

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
	public partial class ScheduleWindow : Form
	{
		public ScheduleWindow(bool InEnabled, LatestChangeType InChange, TimeSpan InTime)
		{
			InitializeComponent();

			EnableCheckBox.Checked = InEnabled;
			ChangeComboBox.SelectedIndex = (int)InChange;

			DateTime CurrentTime = DateTime.Now;
			TimePicker.Value = new DateTime(CurrentTime.Year, CurrentTime.Month, CurrentTime.Day, InTime.Hours, InTime.Minutes, InTime.Seconds);

			UpdateEnabledControls();
		}

		public void CopySettings(out bool OutEnabled, out LatestChangeType OutChange, out TimeSpan OutTime)
		{
			OutEnabled = EnableCheckBox.Checked;
			OutChange = (LatestChangeType)ChangeComboBox.SelectedIndex;
			OutTime = TimePicker.Value.TimeOfDay;
		}

		private void EnableCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledControls();
		}

		private void UpdateEnabledControls()
		{
			ChangeComboBox.Enabled = EnableCheckBox.Checked;
			TimePicker.Enabled = EnableCheckBox.Checked;
		}
	}
}
