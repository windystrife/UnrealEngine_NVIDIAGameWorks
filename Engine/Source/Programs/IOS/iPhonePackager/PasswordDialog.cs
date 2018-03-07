// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace iPhonePackager
{
	public partial class PasswordDialog : Form
	{
		string PasswordResult = null;

		public PasswordDialog()
		{
			InitializeComponent();
		}

		public static bool RequestPassword(out string Password)
		{
			PasswordDialog Dlg = new PasswordDialog();
			Dlg.ShowDialog();

			Password = Dlg.PasswordResult;
			return Password != null;
		}

		private void OKButton_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.OK;
			PasswordResult = PasswordEntryBox.Text;
			Close();
		}
	}
}
