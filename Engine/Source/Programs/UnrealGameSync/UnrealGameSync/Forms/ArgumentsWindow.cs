// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;

namespace UnrealGameSync
{
	public partial class ArgumentsWindow : Form
	{
		const int LVM_FIRST = 0x1000;
		const int LVM_EDITLABELW = (LVM_FIRST + 118);

		const int EM_SETSEL = 0xb1;

		[DllImport("user32.dll", CharSet = CharSet.Auto)]
		static extern IntPtr SendMessage(IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam);
		
		[DllImport("user32.dll")]
		static extern bool SetWindowText(IntPtr hWnd, string text);

		public ArgumentsWindow(List<Tuple<string, bool>> Arguments)
		{
			InitializeComponent();
			ActiveControl = ArgumentsList;

			ArgumentsList.Font = SystemFonts.IconTitleFont;

			ArgumentsList.Items.Clear();

			foreach(Tuple<string, bool> Argument in Arguments)
			{
				ListViewItem Item = new ListViewItem(Argument.Item1);
				Item.Checked = Argument.Item2;
				ArgumentsList.Items.Add(Item);
			}

			ListViewItem AddAnotherItem = new ListViewItem("Click to add an item...", 0);
			ArgumentsList.Items.Add(AddAnotherItem);
		}

		public List<Tuple<string, bool>> GetItems()
		{
			List<Tuple<string, bool>> Items = new List<Tuple<string,bool>>();
			for(int Idx = 0; Idx < ArgumentsList.Items.Count - 1; Idx++)
			{
				ListViewItem Item = ArgumentsList.Items[Idx];
				Items.Add(new Tuple<string,bool>(Item.Text, Item.Checked));
			}
			return Items;
		}

		private void ClearButton_Click(object sender, EventArgs e)
		{
//			ArgumentsTextBox.Text = "";
			DialogResult = System.Windows.Forms.DialogResult.OK;
			Close();
		}

		private void ArgumentsList_AfterLabelEdit(object sender, LabelEditEventArgs e)
		{
			if((e.Label == null && ArgumentsList.Items[e.Item].Text.Length == 0) || (e.Label != null && e.Label.Trim().Length == 0))
			{
				e.CancelEdit = true;
				ArgumentsList.Items.RemoveAt(e.Item);
			}
		}

		private void ArgumentsList_BeforeLabelEdit(object sender, LabelEditEventArgs e)
		{
			if(e.Item == ArgumentsList.Items.Count - 1)
			{
				e.CancelEdit = true;
			}
		}

		private void ArgumentsList_MouseClick(object sender, MouseEventArgs e)
		{
			ListViewHitTestInfo Info = ArgumentsList.HitTest(e.Location);
			if(Info.Item.Index == ArgumentsList.Items.Count - 1)
			{
				ListViewItem NewItem = new ListViewItem();
				NewItem.Checked = true;
				NewItem = ArgumentsList.Items.Insert(ArgumentsList.Items.Count - 1, NewItem);
				NewItem.BeginEdit();
			}
			else
			{
				using(Graphics Graphics = ArgumentsList.CreateGraphics())
				{
					int LabelOffset = e.X - CheckBoxPadding - CheckBoxRenderer.GetGlyphSize(Graphics, CheckBoxState.CheckedNormal).Width - CheckBoxPadding;
					if(LabelOffset >= 0 && LabelOffset < TextRenderer.MeasureText(Info.Item.Text, ArgumentsList.Font).Width)
					{
						Info.Item.BeginEdit();
					}
				}
			}
		}

		const int CheckBoxPadding = 5;

		private void ArgumentsList_DrawItem(object sender, DrawListViewItemEventArgs e)
		{
			e.DrawBackground();
			if(e.ItemIndex < ArgumentsList.Items.Count - 1)
			{
				CheckBoxState State = e.Item.Checked? CheckBoxState.CheckedNormal : CheckBoxState.UncheckedNormal;
				Size CheckSize = CheckBoxRenderer.GetGlyphSize(e.Graphics, State);
				CheckBoxRenderer.DrawCheckBox(e.Graphics, new Point(e.Bounds.Left + 4, e.Bounds.Top + (e.Bounds.Height - CheckSize.Height) / 2), State);
				DrawItemLabel(e.Graphics, SystemColors.WindowText, e.Item);
			}
			else
			{
				DrawItemLabel(e.Graphics, SystemColors.GrayText, e.Item);
			}
		}

		private void DrawItemLabel(Graphics Graphics, Color NormalColor, ListViewItem Item)
		{
			Rectangle LabelRect = GetLabelRectangle(Graphics, Item);
			if(Item.Selected)
			{
				Graphics.FillRectangle(SystemBrushes.Highlight, LabelRect);
				TextRenderer.DrawText(Graphics, Item.Text, ArgumentsList.Font, LabelRect, SystemColors.HighlightText, SystemColors.Highlight, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine);
			}
			else
			{
				TextRenderer.DrawText(Graphics, Item.Text, ArgumentsList.Font, LabelRect, NormalColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine);
			}
		}

		private Rectangle GetLabelRectangle(Graphics Graphics, ListViewItem Item)
		{
			CheckBoxState State = Item.Checked? CheckBoxState.CheckedNormal : CheckBoxState.UncheckedNormal;
			Size CheckSize = CheckBoxRenderer.GetGlyphSize(Graphics, State);
			Size LabelSize = TextRenderer.MeasureText(Item.Text, ArgumentsList.Font);

			int LabelIndent = CheckBoxPadding + CheckSize.Width + CheckBoxPadding;
			return new Rectangle(Item.Bounds.Left + LabelIndent, Item.Bounds.Top, LabelSize.Width, Item.Bounds.Height);
		}

		private void ArgumentsList_KeyUp(object sender, KeyEventArgs e)
		{
			if(e.KeyCode == Keys.Delete && ArgumentsList.SelectedIndices.Count == 1)
			{
				int Index = ArgumentsList.SelectedIndices[0];
				ArgumentsList.Items.RemoveAt(Index);
				if(Index < ArgumentsList.Items.Count - 1)
				{
					ArgumentsList.Items[Index].Selected = true;
				}
			}
		}

		private void ArgumentsList_KeyPress(object sender, KeyPressEventArgs e)
		{
			if(ArgumentsList.SelectedItems.Count == 1 && !Char.IsControl(e.KeyChar))
			{
				ListViewItem Item = ArgumentsList.SelectedItems[0];
				if(Item.Index == ArgumentsList.Items.Count - 1)
				{
					Item = new ListViewItem();
					Item.Checked = true;
					Item = ArgumentsList.Items.Insert(ArgumentsList.Items.Count - 1, Item);
				}

				IntPtr NewHandle = SendMessage(ArgumentsList.Handle, LVM_EDITLABELW, new IntPtr(Item.Index), IntPtr.Zero);
				SetWindowText(NewHandle, e.KeyChar.ToString());
				SendMessage(NewHandle, EM_SETSEL, new IntPtr(1), new IntPtr(1));

				e.Handled = true;
			}
		}

		private void MoveUpButton_Click(object sender, EventArgs e)
		{
			if(ArgumentsList.SelectedIndices.Count == 1)
			{
				int Index = ArgumentsList.SelectedIndices[0];
				if(Index > 0)
				{
					ListViewItem Item = ArgumentsList.Items[Index];
					ArgumentsList.Items.RemoveAt(Index);
					ArgumentsList.Items.Insert(Index - 1, Item);
				}
			}
		}

		private void MoveDownButton_Click(object sender, EventArgs e)
		{
			if(ArgumentsList.SelectedIndices.Count == 1)
			{
				int Index = ArgumentsList.SelectedIndices[0];
				if(Index < ArgumentsList.Items.Count - 2)
				{
					ListViewItem Item = ArgumentsList.Items[Index];
					ArgumentsList.Items.RemoveAt(Index);
					ArgumentsList.Items.Insert(Index + 1, Item);
				}
			}
		}

		private void ArgumentsList_SelectedIndexChanged(object sender, EventArgs e)
		{
			UpdateMoveButtons();
		}

		private void UpdateMoveButtons()
		{
			MoveUpButton.Enabled = (ArgumentsList.SelectedIndices.Count == 1 && ArgumentsList.SelectedIndices[0] > 0);
			MoveDownButton.Enabled = (ArgumentsList.SelectedIndices.Count == 1 && ArgumentsList.SelectedIndices[0] < ArgumentsList.Items.Count - 2);
		}
	}
}
