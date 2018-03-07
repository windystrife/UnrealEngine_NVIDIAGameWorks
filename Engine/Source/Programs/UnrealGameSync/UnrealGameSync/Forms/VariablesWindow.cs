// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
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
	public partial class VariablesWindow : Form
	{
		public delegate void InsertVariableDelegate(string Name);

		public event InsertVariableDelegate OnInsertVariable;

		public VariablesWindow(IReadOnlyDictionary<string, string> Variables)
		{
			InitializeComponent();

			ListViewGroup CurrentProjectGroup = new ListViewGroup("Current Project");
			MacrosList.Groups.Add(CurrentProjectGroup);

			ListViewGroup EnvironmentGroup = new ListViewGroup("Environment");
			MacrosList.Groups.Add(EnvironmentGroup);

			foreach(KeyValuePair<string, string> Pair in Variables)
			{
				ListViewItem Item = new ListViewItem(String.Format("$({0})", Pair.Key));
				Item.SubItems.Add(Pair.Value);
				Item.Group = CurrentProjectGroup;
				MacrosList.Items.Add(Item);
			}

			foreach(DictionaryEntry Entry in Environment.GetEnvironmentVariables())
			{
				string Key = Entry.Key.ToString();
				if(!Variables.ContainsKey(Key))
				{
					ListViewItem Item = new ListViewItem(String.Format("$({0})", Key));
					Item.SubItems.Add(Entry.Value.ToString());
					Item.Group = EnvironmentGroup;
					MacrosList.Items.Add(Item);
				}
			}
		}

		protected override bool ShowWithoutActivation
		{
			get { return true; }
		}

		private void OkButton_Click(object sender, EventArgs e)
		{
			DialogResult = System.Windows.Forms.DialogResult.OK;
			Close();
		}

		private void MacrosList_MouseDoubleClick(object Sender, MouseEventArgs Args)
		{
			if(Args.Button == MouseButtons.Left)
			{
				ListViewHitTestInfo HitTest = MacrosList.HitTest(Args.Location);
				if(HitTest.Item != null && OnInsertVariable != null)
				{
					OnInsertVariable(HitTest.Item.Text);
				}
			}
		}

		private void InsertButton_Click(object sender, EventArgs e)
		{
			if(MacrosList.SelectedItems.Count > 0 && OnInsertVariable != null)
			{
				OnInsertVariable(MacrosList.SelectedItems[0].Text);
			}
		}

		private void MacrosList_SelectedIndexChanged(object sender, EventArgs e)
		{
			InsertButton.Enabled = (MacrosList.SelectedItems.Count > 0);
		}
	}
}
