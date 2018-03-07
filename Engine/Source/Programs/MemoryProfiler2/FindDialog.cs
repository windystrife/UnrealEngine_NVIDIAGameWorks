// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace MemoryProfiler2
{
	public partial class FindDialog : Form
	{
		public FindDialog( string SearchText, bool bIsRegex, bool bMatchCase )
		{
			InitializeComponent();

			SearchTextBox.Text = SearchText;
			RegexCheckButton.Checked = bIsRegex;
			MatchCaseCheckButton.Checked = bMatchCase;
		}

		private void FindNextButton_Click( object sender, EventArgs e )
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void CancelButton_Click( object sender, EventArgs e )
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void FindAllButton_Click( object sender, EventArgs e )
		{
			DialogResult = DialogResult.Yes;
			Close();
		}
	}
}