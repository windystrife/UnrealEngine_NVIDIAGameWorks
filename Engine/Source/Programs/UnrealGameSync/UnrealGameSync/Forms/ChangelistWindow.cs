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
	public partial class ChangelistWindow : Form
	{
		public int ChangeNumber;

		public ChangelistWindow(int InChangeNumber)
		{
			InitializeComponent();

			ChangeNumber = InChangeNumber;
			ChangeNumberTextBox.Text = (ChangeNumber > 0)? ChangeNumber.ToString() : "";
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			int NewChangeNumber;
			if(int.TryParse(ChangeNumberTextBox.Text.Trim(), out NewChangeNumber))
			{
				int DeltaChangeNumber = NewChangeNumber - ChangeNumber;
				if((DeltaChangeNumber > -10000 && DeltaChangeNumber < 1000) || ChangeNumber <= 0 || MessageBox.Show(String.Format("Changelist {0} is {1} changes away from your current sync. Was that intended?", NewChangeNumber, Math.Abs(DeltaChangeNumber)), "Changelist is far away", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					ChangeNumber = NewChangeNumber;
					DialogResult = DialogResult.OK;
					Close();
				}
			}
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}
	}
}
