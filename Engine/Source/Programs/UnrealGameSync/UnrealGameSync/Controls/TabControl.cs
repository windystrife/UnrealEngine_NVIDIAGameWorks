using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Drawing.Drawing2D;

namespace UnrealGameSync
{
	class TabControl : Control
	{
		const int TabPadding = 13;//18;
		const int TabCloseButtonWidth = 8 + 13;

		class TabData
		{
			public string Name;
			public object Data;
			public int MinX;
			public int Width;
			public Size TextSize;
			public Tuple<Color, float> Highlight;
		}

		class TabDragData
		{
			public TabData Tab;
			public int InitialIdx;
			public int MouseX;
			public int RelativeMouseX;
		}

		List<TabData> Tabs = new List<TabData>();
		int SelectedTabIdx;
		int HoverTabIdx;
		int HighlightTabIdx;
		TabDragData DragState;

		public delegate void OnTabChangedDelegate(object NewTabData);
		public event OnTabChangedDelegate OnTabChanged;

		public delegate void OnNewTabClickDelegate(Point Location, MouseButtons Buttons);
		public event OnNewTabClickDelegate OnNewTabClick;

		public delegate void OnTabClickedDelegate(object TabData, Point Location, MouseButtons Buttons);
		public event OnTabClickedDelegate OnTabClicked;

		public delegate bool OnTabClosingDelegate(object TabData);
		public event OnTabClosingDelegate OnTabClosing;

		public delegate void OnTabClosedDelegate(object TabData);
		public event OnTabClosedDelegate OnTabClosed;

		public delegate void OnTabReorderDelegate();
		public event OnTabReorderDelegate OnTabReorder;

		public delegate void OnButtonClickDelegate(int ButtonIdx, Point Location, MouseButtons Buttons);
		public event OnButtonClickDelegate OnButtonClick;

		public TabControl()
		{
			DoubleBuffered = true;
			Tabs.Add(new TabData{ Name = "+" });
			SelectedTabIdx = -1;
			HoverTabIdx = -1;
			HighlightTabIdx = -1;
		}

		public void LockHover()
		{
			HighlightTabIdx = HoverTabIdx;
		}

		public void UnlockHover()
		{
			HighlightTabIdx = -1;
			Invalidate();
		}

		public int FindTabIndex(object Data)
		{
			int Result = -1;
			for(int TabIdx = 0; TabIdx < Tabs.Count - 1; TabIdx++)
			{
				if(Tabs[TabIdx].Data == Data)
				{
					Result = TabIdx;
					break;
				}
			}
			return Result;
		}

		public void SetHighlight(int TabIdx, Tuple<Color, float> Highlight)
		{
			Tuple<Color, float> CurrentHighlight = Tabs[TabIdx].Highlight;
			if(Highlight == null || CurrentHighlight == null)
			{
				if(Highlight != CurrentHighlight)
				{
					Tabs[TabIdx].Highlight = Highlight;
					Invalidate();
				}
			}
			else
			{
				if(Highlight.Item1 != CurrentHighlight.Item1 || Highlight.Item2 != CurrentHighlight.Item2)
				{
					Tabs[TabIdx].Highlight = Highlight;
					Invalidate();
				}
			}
		}

		public int InsertTab(int InsertIdx, string Name, object Data)
		{
			int Idx = Tabs.Count - 1;
			if(InsertIdx >= 0 && InsertIdx < Tabs.Count - 1)
			{
				Idx = InsertIdx;
			}
			if(SelectedTabIdx >= Idx)
			{
				SelectedTabIdx++;
			}

			Tabs.Insert(Idx, new TabData{ Name = Name, Data = Data });
			LayoutTabs();
			Invalidate();
			return Idx;
		}

		public int GetTabCount()
		{
			return Tabs.Count - 1;
		}

		public void SetTabName(int TabIdx, string Name)
		{
			Tabs[TabIdx].Name = Name;
			LayoutTabs();
			Invalidate();
		}

		public object GetTabData(int TabIdx)
		{
			return Tabs[TabIdx].Data;
		}

		public int GetSelectedTabIndex()
		{
			return SelectedTabIdx;
		}

		public object GetSelectedTabData()
		{
			return Tabs[SelectedTabIdx].Data;
		}

		public void SelectTab(int TabIdx)
		{
			if(TabIdx != SelectedTabIdx)
			{
				ForceSelectTab(TabIdx);
			}
		}

		private void ForceSelectTab(int TabIdx)
		{
			SelectedTabIdx = TabIdx;
			LayoutTabs();
			Invalidate();

			if(OnTabChanged != null)
			{
				if(SelectedTabIdx == -1)
				{
					OnTabChanged(null);
				}
				else
				{
					OnTabChanged(Tabs[SelectedTabIdx].Data);
				}
			}
		}

		public void RemoveTab(int TabIdx)
		{
			object TabData = Tabs[TabIdx].Data;
			if(OnTabClosing == null || OnTabClosing(TabData))
			{
				Tabs.RemoveAt(TabIdx);

				if(HoverTabIdx == TabIdx)
				{
					HoverTabIdx = -1;
				}

				if(HighlightTabIdx == TabIdx)
				{
					HighlightTabIdx = -1;
				}

				if(SelectedTabIdx == TabIdx)
				{
					if(SelectedTabIdx < Tabs.Count - 1)
					{
						ForceSelectTab(SelectedTabIdx);
					}
					else
					{
						ForceSelectTab(SelectedTabIdx - 1);
					}
				}
				else
				{
					if(SelectedTabIdx > TabIdx)
					{
						SelectedTabIdx--;
					}
				}

				LayoutTabs();
				Invalidate();

				if(OnTabClosed != null)
				{
					OnTabClosed(TabData);
				}
			}
		}

		void LayoutTabs()
		{
			using(Graphics Graphics = CreateGraphics())
			{
				for(int Idx = 0; Idx < Tabs.Count; Idx++)
				{
					TabData Tab = Tabs[Idx];
					Tab.TextSize = TextRenderer.MeasureText(Graphics, Tab.Name, Font, new Size(int.MaxValue, int.MaxValue), TextFormatFlags.NoPadding);
					Tab.Width = TabPadding + Tab.TextSize.Width + TabPadding;
					if(Idx == SelectedTabIdx)
					{
						Tab.Width += TabCloseButtonWidth;
					}
				}

				int LeftX = 0;
				for(int Idx = 0; Idx < Tabs.Count; Idx++)
				{
					TabData Tab = Tabs[Idx];
					Tab.MinX = LeftX;
					LeftX += Tab.Width;
				}

				int RightX = Width - 1;
				if(LeftX > RightX)
				{
					int UsedWidth = Tabs.Take(Tabs.Count - 1).Sum(x => x.Width);
					int RemainingWidth = RightX + 1 - Tabs[Tabs.Count - 1].Width;
					if(SelectedTabIdx != -1)
					{
						UsedWidth -= Tabs[SelectedTabIdx].Width;
						RemainingWidth -= Tabs[SelectedTabIdx].Width;
					}

					int NewX = 0;
					for(int Idx = 0; Idx < Tabs.Count; Idx++)
					{
						Tabs[Idx].MinX = NewX;
						int PrevWidth = Tabs[Idx].Width;
						if(Idx != SelectedTabIdx && Idx != Tabs.Count - 1)
						{
							Tabs[Idx].Width = Math.Max((Tabs[Idx].Width * RemainingWidth) / UsedWidth, TabPadding * 3);
						}
						Tabs[Idx].TextSize.Width -= PrevWidth - Tabs[Idx].Width;
						NewX += Tabs[Idx].Width;
					}
				}

				if(DragState != null)
				{
					int MinOffset = -DragState.Tab.MinX;
					int MaxOffset = Tabs[Tabs.Count - 1].MinX - DragState.Tab.Width - DragState.Tab.MinX;

					int Offset = (DragState.MouseX - DragState.RelativeMouseX) - DragState.Tab.MinX;
					Offset = Math.Max(Offset, MinOffset);
					Offset = Math.Min(Offset, MaxOffset);

					DragState.Tab.MinX += Offset;
				}
			}
		}

		protected override void OnResize(EventArgs e)
		{
			base.OnResize(e);

			LayoutTabs();
			Invalidate();
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			base.OnMouseDown(e);

			OnMouseMove(e);

			if(HoverTabIdx != -1)
			{
				if(e.Button == MouseButtons.Left)
				{
					if(HoverTabIdx == SelectedTabIdx)
					{
						if(e.Location.X > Tabs[HoverTabIdx].MinX + Tabs[HoverTabIdx].Width - TabCloseButtonWidth)
						{
							RemoveTab(HoverTabIdx);
						}
						else
						{
							DragState = new TabDragData{ Tab = Tabs[SelectedTabIdx], InitialIdx = SelectedTabIdx, MouseX = e.Location.X, RelativeMouseX = e.Location.X - Tabs[SelectedTabIdx].MinX };
						}
					}
					else
					{
						if(HoverTabIdx > Tabs.Count - 1)
						{
							OnButtonClick(HoverTabIdx - Tabs.Count, e.Location, e.Button);
						}
						else if(HoverTabIdx == Tabs.Count - 1)
						{
							OnNewTabClick(e.Location, e.Button);
						}
						else
						{
							SelectTab(HoverTabIdx);
						}
					}
				}
				else if(e.Button == MouseButtons.Middle)
				{
					if(HoverTabIdx < Tabs.Count - 1)
					{
						RemoveTab(HoverTabIdx);
					}
				}
			}

			if(OnTabClicked != null)
			{
				object TabData = (HoverTabIdx == -1)? null : Tabs[HoverTabIdx].Data;
				OnTabClicked(TabData, e.Location, e.Button);
			}
		}

		protected override void OnMouseUp(MouseEventArgs e)
		{
			base.OnMouseUp(e);

			if(DragState != null)
			{
				if(OnTabReorder != null)
				{
					OnTabReorder();
				}

				DragState = null;

				LayoutTabs();
				Invalidate();
			}
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			base.OnMouseMove(e);

			if(DragState != null)
			{
				DragState.MouseX = e.Location.X;

				Tabs.Remove(DragState.Tab);

				int TotalWidth = 0;
				for(int InsertIdx = 0;;InsertIdx++)
				{
					if(InsertIdx == Tabs.Count - 1 || DragState.MouseX - DragState.RelativeMouseX < TotalWidth + Tabs[InsertIdx].Width / 2)
					{
						HoverTabIdx = SelectedTabIdx = InsertIdx;
						Tabs.Insert(InsertIdx, DragState.Tab);
						break;
					}
					TotalWidth += Tabs[InsertIdx].Width;
				}

				LayoutTabs();
				Invalidate();
			}
			else
			{
				int NewHoverTabIdx = -1;
				if(ClientRectangle.Contains(e.Location))
				{
					for(int Idx = 0; Idx < Tabs.Count; Idx++)
					{
						if(e.Location.X > Tabs[Idx].MinX && e.Location.X < Tabs[Idx].MinX + Tabs[Idx].Width)
						{
							NewHoverTabIdx = Idx;
							break;
						}
					}
				}

				if(HoverTabIdx != NewHoverTabIdx)
				{
					HoverTabIdx = NewHoverTabIdx;
					LayoutTabs();
					Invalidate();

					if(HoverTabIdx != -1)
					{
						Capture = true;
					}
					else
					{
						Capture = false;
					}
				}
			}
		}

		protected override void OnMouseCaptureChanged(EventArgs e)
		{
			base.OnMouseCaptureChanged(e);

			if(HoverTabIdx != -1)
			{
				HoverTabIdx = -1;
				Invalidate();
			}
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			LayoutTabs();

			using(SolidBrush HoverBrush = new SolidBrush(Color.FromArgb(192, 192, 192)))
			{
				for(int Idx = 0; Idx < Tabs.Count; Idx++)
				{
					TabData Tab = Tabs[Idx];
					if(Idx == HoverTabIdx || Idx == HighlightTabIdx)
					{
						DrawBackground(e.Graphics, Tab, SystemBrushes.Window, HoverBrush, Tab.Highlight);
					}
					else
					{
						DrawBackground(e.Graphics, Tab, SystemBrushes.Control, SystemBrushes.Control, Tab.Highlight);
					}
				}
			}

			using(Pen SeparatorPen = new Pen(Color.FromArgb(192, SystemColors.ControlDarkDark)))
			{
				for(int Idx = 0; Idx < Tabs.Count; Idx++)
				{
					TabData Tab = Tabs[Idx];
					if((Idx > 0 && Idx < Tabs.Count - 1) || Idx >= Tabs.Count || Idx == SelectedTabIdx || Idx == HoverTabIdx || Idx == HighlightTabIdx || Tabs.Count == 1)
					{
						e.Graphics.DrawLine(SeparatorPen, Tab.MinX, 0, Tab.MinX, ClientSize.Height - 2);
					}
					if(Idx < Tabs.Count - 1 || Idx >= Tabs.Count || Idx == HoverTabIdx || Idx == HighlightTabIdx || Tabs.Count == 1)
					{
						e.Graphics.DrawLine(SeparatorPen, Tab.MinX + Tab.Width, 0, Tab.MinX + Tab.Width, ClientSize.Height - 2);
					}
					if(Idx == HoverTabIdx || Idx == HighlightTabIdx || Idx >= Tabs.Count)
					{
						e.Graphics.DrawLine(SeparatorPen, Tab.MinX, 0, Tab.MinX + Tab.Width, 0);
					}
				}
			}

			for(int Idx = 0; Idx < Tabs.Count; Idx++)
			{
				if(Idx != SelectedTabIdx)
				{
					DrawText(e.Graphics, Tabs[Idx]);
				}
			}


			if(SelectedTabIdx != -1)
			{
				TabData SelectedTab = Tabs[SelectedTabIdx];

				using(SolidBrush SelectedBrush = new SolidBrush(Color.FromArgb(0, 136, 204)))
				{
					DrawBackground(e.Graphics, SelectedTab, SystemBrushes.Window, SelectedBrush, SelectedTab.Highlight);
				}

				DrawText(e.Graphics, SelectedTab);
	
				// Draw the close button
				SmoothingMode PrevSmoothingMode = e.Graphics.SmoothingMode;
				e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

				int CloseMidX = SelectedTab.MinX + SelectedTab.Width - (TabPadding + TabCloseButtonWidth) / 2;// + (13 / 2);
				int CloseMidY = (ClientSize.Height - 4) / 2;

				Rectangle CloseButton = new Rectangle(CloseMidX - (13 / 2), CloseMidY - (13 / 2), 13, 13);
				e.Graphics.FillEllipse(SystemBrushes.ControlDark, CloseButton);

				using(Pen CrossPen = new Pen(SystemBrushes.Window, 2.0f))
				{
					const float Indent = 3.5f;
					e.Graphics.DrawLine(CrossPen, CloseButton.Left + Indent, CloseButton.Top + Indent, CloseButton.Right - Indent, CloseButton.Bottom - Indent);
					e.Graphics.DrawLine(CrossPen, CloseButton.Left + Indent, CloseButton.Bottom - Indent, CloseButton.Right - Indent, CloseButton.Top + Indent);
				}
				e.Graphics.SmoothingMode = PrevSmoothingMode;

				// Border
				e.Graphics.DrawLine(SystemPens.ControlDark, SelectedTab.MinX, 0, SelectedTab.MinX + SelectedTab.Width, 0);
				e.Graphics.DrawLine(SystemPens.ControlDark, SelectedTab.MinX, 0, SelectedTab.MinX, ClientSize.Height - 2);
				e.Graphics.DrawLine(SystemPens.ControlDark, SelectedTab.MinX + SelectedTab.Width, 0, SelectedTab.MinX + SelectedTab.Width, ClientSize.Height - 2);
			}

			e.Graphics.DrawLine(SystemPens.ControlDarkDark, 0, ClientSize.Height - 2,  ClientSize.Width, ClientSize.Height - 2);
			e.Graphics.DrawLine(SystemPens.ControlLightLight, 0, ClientSize.Height - 1, ClientSize.Width, ClientSize.Height - 1);
		}

		void DrawBackground(Graphics Graphics, TabData Tab, Brush BackgroundBrush, Brush StripeBrush, Tuple<Color, float> Highlight)
		{
			Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;
			Graphics.FillRectangle(BackgroundBrush, Tab.MinX + 1, 1, Tab.Width - 1, ClientSize.Height - 3);
			Graphics.FillRectangle(StripeBrush, Tab.MinX + 1, ClientSize.Height - 5, Tab.Width - 1, 3);

			if(Highlight != null && Highlight.Item2 > 0.0f)
			{
				using(SolidBrush Brush = new SolidBrush(Highlight.Item1))
				{
					Graphics.FillRectangle(Brush, Tab.MinX + 1, ClientSize.Height - 5, (int)((Tab.Width - 2) * Highlight.Item2), 3);
				}
			}
		}

		void DrawText(Graphics Graphics, TabData Tab)
		{
			TextRenderer.DrawText(Graphics, Tab.Name, Font, new Rectangle(Tab.MinX + TabPadding, 0, Tab.TextSize.Width, ClientSize.Height - 3), Color.Black, TextFormatFlags.NoPadding | TextFormatFlags.EndEllipsis | TextFormatFlags.VerticalCenter);
		}
	}
}
