// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Deployment.Application;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using Ionic.Zip;
using System.IO;

namespace UnrealGameSync
{
	public partial class DiagnosticsWindow : Form
	{
		string DataFolder;

		public DiagnosticsWindow(string InDataFolder, string InDiagnosticsText)
		{
			InitializeComponent();
			DataFolder = InDataFolder;
			DiagnosticsTextBox.Text = InDiagnosticsText.Replace("\n", "\r\n");
		}

		private void ViewLogsButton_Click(object sender, EventArgs e)
		{
			Process.Start("explorer.exe", DataFolder);
		}

		private void SaveButton_Click(object sender, EventArgs e)
		{
			SaveFileDialog Dialog = new SaveFileDialog();
			Dialog.Filter = "Zip Files (*.zip)|*.zip|AllFiles (*.*)|*.*";
			Dialog.InitialDirectory = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
			Dialog.FileName = Path.Combine(Dialog.InitialDirectory, "UGS-Diagnostics.zip");
			if(Dialog.ShowDialog() == DialogResult.OK)
			{
				string DiagnosticsFileName = Path.Combine(DataFolder, "Diagnostics.txt");
				try
				{
					File.WriteAllLines(DiagnosticsFileName, DiagnosticsTextBox.Lines);
				}
				catch(Exception Ex)
				{
					MessageBox.Show(String.Format("Couldn't write to '{0}'\n\n{1}", DiagnosticsFileName, Ex.ToString()));
					return;
				}

				string ZipFileName = Dialog.FileName;
				try
				{
					ZipFile Zip = new ZipFile();
					Zip.AddDirectory(DataFolder);
					Zip.Save(ZipFileName);
				}
				catch(Exception Ex)
				{
					MessageBox.Show(String.Format("Couldn't save '{0}'\n\n{1}", ZipFileName, Ex.ToString()));
					return;
				}
			}
		}
	}
}
