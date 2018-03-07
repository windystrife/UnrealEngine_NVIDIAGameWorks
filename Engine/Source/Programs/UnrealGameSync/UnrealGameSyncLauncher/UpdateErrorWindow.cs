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

namespace UnrealGameSyncLauncher
{
	public partial class UpdateErrorWindow : Form
	{
		public UpdateErrorWindow(string Text)
		{
			InitializeComponent();

			UpdateLog.Text = Text;
			UpdateLog.Select(Text.Length, 0);
		}
	}
}
