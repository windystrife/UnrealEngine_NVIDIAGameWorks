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
using System.Windows.Forms.VisualStyles;

namespace UnrealGameSync
{
	partial class ModifyBuildStepsWindow : Form
	{
		List<string> TargetNames;
		List<BuildStep> Steps;
		HashSet<Guid> ProjectSteps;
		string BaseDirectory;
		ListViewItem.ListViewSubItem MouseDownSubItem = null;
		IReadOnlyDictionary<string, string> Variables;

		public ModifyBuildStepsWindow(List<string> InTargetNames, List<BuildStep> InSteps, HashSet<Guid> InProjectSteps, string InBaseDirectory, IReadOnlyDictionary<string, string> InVariables)
		{
			TargetNames = InTargetNames;
			Steps = InSteps;
			ProjectSteps = InProjectSteps;
			BaseDirectory = InBaseDirectory;
			Variables = InVariables;

			InitializeComponent();

			BuildStepList.Font = SystemFonts.IconTitleFont;
		}

		private void ModifyBuildStepsWindow_Load(object sender, EventArgs e)
		{
			foreach(BuildStep Task in Steps)
			{
				AddTask(Task);
			}
			UpdateEnabledButtons();
		}

		void AddTask(BuildStep Task)
		{
			ListViewItem Item = new ListViewItem(Task.Description);
			Item.Tag = Task;
			Item.SubItems.Add(new ListViewItem.ListViewSubItem());
			Item.SubItems.Add(new ListViewItem.ListViewSubItem());
			Item.SubItems.Add(new ListViewItem.ListViewSubItem());
			BuildStepList.Items.Add(Item);
		}

		private void NewStepButton_Click(object sender, EventArgs e)
		{
			BuildStep NewStep = new BuildStep(Guid.NewGuid(), BuildStepList.Items.Count, "Untitled Step", "Running Untitled Step...", 1, null, null, null, null, true);
			NewStep.Description = "Untitled Task";
			NewStep.EstimatedDuration = 1;

			BuildStepWindow NewStepWindow = new BuildStepWindow(NewStep, TargetNames, BaseDirectory, Variables);
			if(NewStepWindow.ShowDialog() == DialogResult.OK)
			{
				AddTask(NewStep);
			}
		}

		private void EditStepButton_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem Item in BuildStepList.SelectedItems)
			{
				BuildStepWindow EditStep = new BuildStepWindow((BuildStep)Item.Tag, TargetNames, BaseDirectory, Variables);
				EditStep.ShowDialog();
				Item.Text = ((BuildStep)Item.Tag).Description;
				break;
			}
		}

		private void RemoveStepButton_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem Item in BuildStepList.SelectedItems)
			{
				if(MessageBox.Show(String.Format("Remove the '{0}' step?", Item.Text), "Remove Step", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					BuildStepList.Items.Remove(Item);
				}
				break;
			}
		}

		private void CloseButton_Click(object sender, EventArgs e)
		{
			Steps.Clear();
			foreach(ListViewItem Item in BuildStepList.Items)
			{
				Steps.Add((BuildStep)Item.Tag);
			}
			Close();
		}

		private void UpdateEnabledButtons()
		{
			int SelectedIndex = (BuildStepList.SelectedIndices.Count > 0)? BuildStepList.SelectedIndices[0] : -1;

			bool bHasSelection = (SelectedIndex != -1);
			EditStepButton.Enabled = bHasSelection;
			RemoveStepButton.Enabled = bHasSelection && !ProjectSteps.Contains(((BuildStep)BuildStepList.SelectedItems[0].Tag).UniqueId);

			MoveUp.Enabled = (SelectedIndex >= 1);
			MoveDown.Enabled = (SelectedIndex >= 0 && SelectedIndex < BuildStepList.Items.Count - 1);
		}

		private void BuildStepList_SelectedIndexChanged(object sender, EventArgs e)
		{
			UpdateEnabledButtons();
		}

		private void BuildStepList_DrawColumnHeader(object sender, DrawListViewColumnHeaderEventArgs e)
		{
			e.DrawDefault = true;
		}

		private void BuildStepList_DrawSubItem(object sender, DrawListViewSubItemEventArgs e)
		{
			if(e.ColumnIndex == 0)
			{
				e.DrawDefault = true;
				return;
			}
			else
			{
				BuildStep Task = (BuildStep)e.Item.Tag;

				bool bEnabled;
				if(e.ColumnIndex == 1)
				{
					bEnabled = Task.bNormalSync;
				}
				else if(e.ColumnIndex == 2)
				{
					bEnabled = Task.bScheduledSync;
				}
				else
				{
					bEnabled = Task.bShowAsTool;
				}

				e.Graphics.FillRectangle(e.ItemState.HasFlag(ListViewItemStates.Selected)? SystemBrushes.Highlight : SystemBrushes.Window, e.Bounds);

				CheckBoxState State;
				if(bEnabled)
				{
					State = (MouseDownSubItem == e.SubItem)? CheckBoxState.CheckedPressed : CheckBoxState.CheckedNormal;
				}
				else
				{
					State = (MouseDownSubItem == e.SubItem)? CheckBoxState.UncheckedPressed : CheckBoxState.UncheckedNormal;
				}
				
				Size Size = CheckBoxRenderer.GetGlyphSize(e.Graphics, State);
				CheckBoxRenderer.DrawCheckBox(e.Graphics, new Point(e.Bounds.Left + (e.Bounds.Width - Size.Width) / 2, e.Bounds.Top + (e.Bounds.Height - Size.Height) / 2), State);
			}
		}

		private void BuildStepList_MouseDown(object sender, MouseEventArgs e)
		{
			ListViewHitTestInfo HitTest = BuildStepList.HitTest(e.X, e.Y);
			MouseDownSubItem = HitTest.SubItem;
			if(MouseDownSubItem != null)
			{
				BuildStepList.Invalidate(MouseDownSubItem.Bounds, true);
			}
		}

		private void BuildStepList_MouseUp(object sender, MouseEventArgs e)
		{
			ListViewHitTestInfo HitTest = BuildStepList.HitTest(e.X, e.Y);
			if(HitTest.Item != null && HitTest.SubItem != null)
			{
				int ColumnIndex = HitTest.Item.SubItems.IndexOf(HitTest.SubItem);
				if(ColumnIndex >= 1 && ColumnIndex <= 3)
				{
					BuildStep Task = (BuildStep)HitTest.Item.Tag;
					if(ColumnIndex == 1)
					{
						Task.bNormalSync ^= true;
					}
					else if(ColumnIndex == 2)
					{
						Task.bScheduledSync ^= true;
					}
					else
					{
						Task.bShowAsTool ^= true;
					}
					BuildStepList.Invalidate(HitTest.SubItem.Bounds);
				}
			}
			if(MouseDownSubItem != null)
			{
				BuildStepList.Invalidate(MouseDownSubItem.Bounds);
				MouseDownSubItem = null;
			}
		}

		private void MoveUp_Click(object sender, EventArgs e)
		{
			BuildStepList.BeginUpdate();
			foreach(ListViewItem Item in BuildStepList.SelectedItems)
			{
				int Index = Item.Index;
				if(Index > 0)
				{
					BuildStepList.Items.RemoveAt(Index);
					BuildStepList.Items.Insert(Index - 1, Item);
				}
				break;
			}
			BuildStepList.EndUpdate();
			UpdateEnabledButtons();
		}

		private void MoveDown_Click(object sender, EventArgs e)
		{
			BuildStepList.BeginUpdate();
			foreach(ListViewItem Item in BuildStepList.SelectedItems)
			{
				int Index = Item.Index;
				if(Index < BuildStepList.Items.Count - 1)
				{
					BuildStepList.Items.RemoveAt(Index);
					BuildStepList.Items.Insert(Index + 1, Item);
				}
				break;
			}
			BuildStepList.EndUpdate();
			UpdateEnabledButtons();
		}

		private void ModifyBuildStepsWindow_FormClosed(object sender, FormClosedEventArgs e)
		{
			Steps.Clear();
			foreach(ListViewItem Item in BuildStepList.Items)
			{
				BuildStep Step = (BuildStep)Item.Tag;
				Step.OrderIndex = Steps.Count;
				Steps.Add(Step);
			}
		}
	}
}
