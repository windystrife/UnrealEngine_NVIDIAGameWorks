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
	partial class SyncFilter : Form
	{
		Dictionary<Guid, WorkspaceSyncCategory> UniqueIdToCategory;
		public string[] GlobalView;
		public Guid[] GlobalExcludedCategories;
		public string[] WorkspaceView;
		public Guid[] WorkspaceExcludedCategories;

		public SyncFilter(Dictionary<Guid, WorkspaceSyncCategory> InUniqueIdToCategory, string[] InGlobalView, Guid[] InGlobalExcludedCategories, string[] InWorkspaceView, Guid[] InWorkspaceExcludedCategories)
		{
			InitializeComponent();

			UniqueIdToCategory = InUniqueIdToCategory;
			GlobalExcludedCategories = InGlobalExcludedCategories;
			GlobalView = InGlobalView;
			WorkspaceExcludedCategories = InWorkspaceExcludedCategories;
			WorkspaceView = InWorkspaceView;

			GlobalControl.SetView(GlobalView);
			SetExcludedCategories(GlobalControl.CategoriesCheckList, UniqueIdToCategory, GlobalExcludedCategories);
			WorkspaceControl.SetView(WorkspaceView);
			SetExcludedCategories(WorkspaceControl.CategoriesCheckList, UniqueIdToCategory, WorkspaceExcludedCategories);
		}

		private static void SetExcludedCategories(CheckedListBox ListBox, Dictionary<Guid, WorkspaceSyncCategory> UniqueIdToFilter, Guid[] ExcludedCategories)
		{
			foreach(WorkspaceSyncCategory Filter in UniqueIdToFilter.Values)
			{
				CheckState State = CheckState.Checked;
				if(ExcludedCategories.Contains(Filter.UniqueId))
				{
					State = CheckState.Unchecked;
				}
				ListBox.Items.Add(Filter, State);
			}
		}

		private static Guid[] GetExcludedCategories(CheckedListBox ListBox, Guid[] PreviousExcludedCategories)
		{
			HashSet<Guid> ExcludedCategories = new HashSet<Guid>(PreviousExcludedCategories);
			for(int Idx = 0; Idx < ListBox.Items.Count; Idx++)
			{
				Guid UniqueId = ((WorkspaceSyncCategory)ListBox.Items[Idx]).UniqueId;
				if(ListBox.GetItemCheckState(Idx) == CheckState.Unchecked)
				{
					ExcludedCategories.Add(UniqueId);
				}
				else
				{
					ExcludedCategories.Remove(UniqueId);
				}
			}
			return ExcludedCategories.ToArray();
		}

		private static string[] GetView(TextBox FilterText)
		{
			List<string> NewLines = new List<string>(FilterText.Lines);
			while (NewLines.Count > 0 && NewLines.Last().Trim().Length == 0)
			{
				NewLines.RemoveAt(NewLines.Count - 1);
			}
			return NewLines.Count > 0 ? FilterText.Lines : new string[0];
		}

		private void OkButton_Click(object sender, EventArgs e)
		{
			string[] NewGlobalView = GlobalControl.GetView();
			string[] NewWorkspaceView = WorkspaceControl.GetView();

			if(NewGlobalView.Any(x => x.Contains("//")) || NewWorkspaceView.Any(x => x.Contains("//")))
			{
				if(MessageBox.Show(this, "Custom views should be relative to the stream root (eg. -/Engine/...).\r\n\r\nFull depot paths (eg. //depot/...) will not match any files.\r\n\r\nAre you sure you want to continue?", "Invalid view", MessageBoxButtons.OKCancel) != System.Windows.Forms.DialogResult.OK)
				{
					return;
				}
			}
		
			GlobalView = NewGlobalView;
			GlobalExcludedCategories = GetExcludedCategories(GlobalControl.CategoriesCheckList, GlobalExcludedCategories);
			WorkspaceView = NewWorkspaceView;
			WorkspaceExcludedCategories = GetExcludedCategories(WorkspaceControl.CategoriesCheckList, WorkspaceExcludedCategories);

			DialogResult = DialogResult.OK;
		}

		private void CancButton_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
		}

		private void ShowCombinedView_Click(object sender, EventArgs e)
		{
			string[] Filter = UserSettings.GetCombinedSyncFilter(UniqueIdToCategory, GlobalControl.GetView(), GetExcludedCategories(GlobalControl.CategoriesCheckList, GlobalExcludedCategories), WorkspaceControl.GetView(), GetExcludedCategories(WorkspaceControl.CategoriesCheckList, WorkspaceExcludedCategories));
			if(Filter.Length == 0)
			{
				Filter = new string[]{ "All files will be synced." };
			}
			MessageBox.Show(String.Join("\r\n", Filter), "Combined View");
		}
	}
}
