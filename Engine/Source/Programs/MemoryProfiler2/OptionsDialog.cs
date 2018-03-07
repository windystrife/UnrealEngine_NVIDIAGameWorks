// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace MemoryProfiler2
{
	public partial class OptionsDialog : Form
	{
		private MainWindow Main = null;

		public OptionsDialog( MainWindow InMain, SettableOptions Options )
		{
			Main = InMain;

			InitializeComponent();

			OptionsGrid.SelectedObject = Options;
		}

		private void OptionsDialogClosing( object sender, FormClosingEventArgs e )
		{
			Main.Options = ( SettableOptions )OptionsGrid.SelectedObject;
		}
	}
}
