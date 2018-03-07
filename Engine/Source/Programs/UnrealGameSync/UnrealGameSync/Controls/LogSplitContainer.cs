// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;

namespace UnrealGameSync
{
	public class LogSplitContainer : SplitContainer
	{
		const int WM_SETCURSOR = 0x0020;

		bool bHoverOverClose;
		bool bMouseDownOverClose;
		bool bLogVisible = true;
		bool bSplitterMoved = false;
		bool bChangingLogVisibility = false;

		const int CaptionPadding = 4;

		int LogHeight = 200;

		public event Action<bool> OnVisibilityChanged;

		[Browsable(true)]
		public string Caption
		{
			get;
			set;
		}

		public LogSplitContainer()
		{
			DoubleBuffered = true;

			Caption = "Log";
			Orientation = System.Windows.Forms.Orientation.Horizontal;

			SplitterMoved += OnSplitterMoved;
		}

		protected Font CaptionFont
		{
			get { return SystemFonts.IconTitleFont; }
		}

		protected override void OnCreateControl()
		{
			base.OnCreateControl();

			UpdateMetrics();
		}

		protected override void OnKeyUp(KeyEventArgs e)
		{
			// Base implementation directly repaints the splitter control in response to this message, which bypasses our custom paint handler. Invalidate the window to get it drawn again.
			SuspendLayout();
			base.OnKeyUp(e);
			ResumeLayout();
			Invalidate();
		}

		public void OnSplitterMoved(object Obj, EventArgs Args)
		{
			bSplitterMoved = true;

			if(bLogVisible && !bChangingLogVisibility)
			{
				LogHeight = Height - (SplitterDistance + SplitterWidth);
			}

			Invalidate();
		}

		public void SetLogVisibility(bool bVisible)
		{
			if(bVisible != bLogVisible)
			{
				SuspendLayout();

				bLogVisible = bVisible;
				bChangingLogVisibility = true;
				UpdateMetrics();
				if(bLogVisible)
				{
					SplitterDistance = Math.Max(Height - LogHeight - SplitterWidth, Panel1MinSize);
				}
				else
				{
					SplitterDistance = Height - SplitterWidth;
				}

				bChangingLogVisibility = false;

				ResumeLayout();

				if(OnVisibilityChanged != null)
				{
					OnVisibilityChanged(bVisible);
				}
			}
		}

		private void UpdateMetrics()
		{
			if(bLogVisible)
			{
				IsSplitterFixed = false;
				SplitterWidth = CaptionFont.Height + (CaptionPadding * 2) + 4;
				Panel2MinSize = 50;
				Panel2.Show();
			}
			else
			{
				IsSplitterFixed = true;
				Panel2MinSize = 0;
				SplitterWidth = CaptionFont.Height + CaptionPadding + 4;
				Panel2.Hide();
			}
		}

		protected override void OnSizeChanged(EventArgs e)
		{
			base.OnSizeChanged(e);

			// Check it's not minimized; if our minimum size is not being respected, we can't resize properly
			if(Height >= SplitterWidth)
			{
				UpdateMetrics();

				if(!bLogVisible)
				{
					SplitterDistance = Height - SplitterWidth;
				}
			}
		}

		protected override void OnLayout(LayoutEventArgs e)
		{
			base.OnLayout(e);

			if(bLogVisible && !bChangingLogVisibility)
			{
				LogHeight = Height - (SplitterDistance + SplitterWidth);
			}
		}

		public bool IsLogVisible()
		{
			return bLogVisible;
		}

		protected override void OnFontChanged(EventArgs e)
		{
			base.OnFontChanged(e);

			UpdateMetrics();
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			base.OnMouseMove(e);

			bool bNewHoverOverClose = CloseButtonRectangle.Contains(e.Location);
			if(bHoverOverClose != bNewHoverOverClose)
			{
				bHoverOverClose = bNewHoverOverClose;
				Invalidate();
			}
		}

		protected override void OnMouseLeave(EventArgs e)
		{
			base.OnMouseLeave(e);

			if(bHoverOverClose)
			{
				bHoverOverClose = false;
				bMouseDownOverClose = false;
				Invalidate();
			}
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			bSplitterMoved = false;

			if(CloseButtonRectangle.Contains(e.Location))
			{
				bMouseDownOverClose = bHoverOverClose;
			}
			else
			{
				base.OnMouseDown(e);
			}
		}

		protected override void OnMouseUp(MouseEventArgs e)
		{
			bMouseDownOverClose = false;

			base.OnMouseUp(e);

			if(e.Button.HasFlag(MouseButtons.Left) && !bSplitterMoved)
			{
				SetLogVisibility(!IsLogVisible());
			}
		}

		protected override bool ShowFocusCues
		{
			get { return false; }
		}

		protected override void OnResize(EventArgs e)
		{
			base.OnResize(e);

			Invalidate(false);
		}

		protected override void OnPaintBackground(PaintEventArgs e)
		{
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			int CaptionMinY = SplitterDistance + CaptionPadding;
			int CaptionMaxY = SplitterDistance + SplitterWidth;
			int ButtonSize = SplitterWidth - CaptionPadding - 2 - 4;
			if(IsLogVisible())
			{
				CaptionMaxY -= CaptionPadding;
				ButtonSize -= CaptionPadding;
			}

			using(Pen CaptionBorderPen = new Pen(SystemColors.ControlDark, 1.0f))
			{
				e.Graphics.DrawRectangle(CaptionBorderPen, 0, CaptionMinY, ClientRectangle.Width - 1, CaptionMaxY - CaptionMinY - 1);
			}

			using(Brush BackgroundBrush = new SolidBrush(BackColor))
			{
				e.Graphics.FillRectangle(BackgroundBrush, 0, 0, ClientRectangle.Width, CaptionMinY);
				e.Graphics.FillRectangle(BackgroundBrush, 0, CaptionMaxY, ClientRectangle.Width, Height - CaptionMaxY);
				e.Graphics.FillRectangle(BackgroundBrush, 1, CaptionMinY + 1, ClientRectangle.Width - 2, CaptionMaxY - CaptionMinY - 2);
			}

			int CrossX = ClientRectangle.Right - (ButtonSize / 2) - 4;
			int CrossY = (CaptionMinY + CaptionMaxY) / 2;
			if(bHoverOverClose)
			{
				e.Graphics.FillRectangle(SystemBrushes.ActiveCaption, CrossX - ButtonSize / 2, CrossY - ButtonSize / 2, ButtonSize, ButtonSize);
			}
			else if(bMouseDownOverClose)
			{
				e.Graphics.FillRectangle(SystemBrushes.InactiveCaption, CrossX - ButtonSize / 2, CrossY - ButtonSize / 2, ButtonSize - 2, ButtonSize - 2);
			}
			if(bHoverOverClose || bMouseDownOverClose)
			{
				using(Pen BorderPen = new Pen(SystemColors.InactiveCaptionText, 1.0f))
				{
					e.Graphics.DrawRectangle(BorderPen, CrossX - ButtonSize / 2, CrossY - ButtonSize / 2, ButtonSize, ButtonSize);
				}
			}
			if(SplitterDistance >= Height - SplitterWidth)
			{
				e.Graphics.DrawImage(Properties.Resources.Log, new Rectangle(CrossX - (ButtonSize / 2) - 1, CrossY - (ButtonSize / 2) - 1, ButtonSize + 2, ButtonSize + 2), new Rectangle(16, 0, 16, 16), GraphicsUnit.Pixel);
			}
			else
			{
				e.Graphics.DrawImage(Properties.Resources.Log, new Rectangle(CrossX - (ButtonSize / 2) - 1, CrossY - (ButtonSize / 2) - 1, ButtonSize + 2, ButtonSize + 2), new Rectangle(0, 0, 16, 16), GraphicsUnit.Pixel);
			}

			TextRenderer.DrawText(e.Graphics, Caption, CaptionFont, new Rectangle(2, CaptionMinY, ClientRectangle.Width - 20, CaptionMaxY - CaptionMinY), SystemColors.ControlText, TextFormatFlags.Left | TextFormatFlags.VerticalCenter);
		}

		protected override void WndProc(ref Message Message)
		{
			base.WndProc(ref Message);

			switch (Message.Msg)
			{
				case WM_SETCURSOR:
					if(RectangleToScreen(CloseButtonRectangle).Contains(Cursor.Position))
					{
						Cursor.Current = Cursors.Default;
						Message.Result = new IntPtr(1);
					}
					return;
			}
		}

		Rectangle CloseButtonRectangle
		{
			get { return new Rectangle(ClientRectangle.Right - 20, SplitterDistance, 20, SplitterWidth); }
		}
	}
}
