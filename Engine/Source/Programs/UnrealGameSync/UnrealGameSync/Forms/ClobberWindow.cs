// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	public partial class ClobberWindow : Form
	{
		Dictionary<string, bool> FilesToClobber;

		public ClobberWindow(Dictionary<string, bool> InFilesToClobber)
		{
			InitializeComponent();

			FilesToClobber = InFilesToClobber;

			foreach(KeyValuePair<string, bool> FileToClobber in FilesToClobber)
			{
				ListViewItem Item = new ListViewItem(Path.GetFileName(FileToClobber.Key));
				Item.Tag = FileToClobber.Key;
				Item.Checked = FileToClobber.Value;
				Item.SubItems.Add(Path.GetDirectoryName(FileToClobber.Key));
				FileList.Items.Add(Item);
			}
		}

		private void UncheckAll_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem Item in FileList.Items)
			{
				Item.Checked = false;
			}
		}

		private void ContinueButton_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem Item in FileList.Items)
			{
				FilesToClobber[(string)Item.Tag] = Item.Checked;
			}
		}
	}
}
