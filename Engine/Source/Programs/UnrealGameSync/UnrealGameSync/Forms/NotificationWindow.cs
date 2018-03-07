// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;

namespace UnrealGameSync
{
	public enum NotificationType
	{
		Info,
		Warning,
		Error,
	}

	public partial class NotificationWindow : Form
	{
		NotificationType Type;

		// Logo
		Rectangle LogoBounds;
		Bitmap LogoBitmap;

		// Caption
		Rectangle CaptionBounds;
		string CaptionText;
		Font CaptionFont;

		// Message
		Rectangle MessageBounds;
		string MessageText;

		// Close button
		Rectangle CloseButtonBounds;
		bool bHoverOverClose;
		bool bMouseDownOverClose;

		// Notifications
		public Action OnMoreInformation;
		public Action OnDismiss;

		public NotificationWindow(Image InLogo)
		{
			LogoBitmap = new Bitmap(InLogo);
			InitializeComponent();
		}

		public void Show(NotificationType InType, string InCaption, string InMessage)
		{
			Type = InType;
			CaptionText = InCaption;
			MessageText = InMessage;

			base.Show();

			CalculateBounds();

			Rectangle WorkingArea = Screen.PrimaryScreen.WorkingArea;
			Location = new Point(WorkingArea.Right - Size.Width - 16, WorkingArea.Bottom - Size.Height - 16);

			BringToFront();
			Invalidate();
		}

		protected override void Dispose(bool bDisposing)
		{
			if(bDisposing && LogoBitmap != null)
			{
				LogoBitmap.Dispose();
				LogoBitmap = null;
			}
			if(bDisposing && components != null)
			{
				components.Dispose();
			}
			base.Dispose(bDisposing);
		}

		protected override void OnClosed(EventArgs e)
		{
			base.OnClosed(e);

			if(LogoBitmap != null)
			{
				LogoBitmap.Dispose();
				LogoBitmap = null;
			}
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);
		}

		public void CalculateBounds()
		{
			int ReferenceSize8 = (int)(8 * Font.Height) / 16;
			using(Graphics Graphics = CreateGraphics())
			{
				int GutterW = ReferenceSize8 + ReferenceSize8 / 2;
				int GutterH = ReferenceSize8 + ReferenceSize8 / 2;

				// Get the close button size
				int CloseButtonSize = ReferenceSize8 * 2;

				// Space out the text
				int CaptionY = GutterH;
				int CaptionW = TextRenderer.MeasureText(Graphics, CaptionText, CaptionFont ?? Font, new Size(100 * ReferenceSize8, Font.Height), TextFormatFlags.Left | TextFormatFlags.Top | TextFormatFlags.SingleLine).Width;
				int CaptionH = (CaptionFont ?? Font).Height;

				int MessageY = CaptionY + CaptionH + ReferenceSize8 / 2;
				int MessageW = TextRenderer.MeasureText(Graphics, MessageText, Font, new Size(100 * ReferenceSize8, Font.Height), TextFormatFlags.Left | TextFormatFlags.Top | TextFormatFlags.SingleLine).Width;
				int MessageH = Font.Height;

				// Get the logo dimensions
				int LogoH = MessageY + MessageH - GutterH;
				int LogoW = (LogoBitmap == null)? 16 : (int)(LogoBitmap.Width * (float)LogoH / (float)LogoBitmap.Height);

				// Set the window size
				int TextX = GutterW + LogoW + ReferenceSize8 * 2;
				Size = new Size(TextX + MessageW + CloseButtonSize + GutterW, MessageY + MessageH + GutterH);

				// Set the bounds of the individual elements
				CloseButtonBounds = new Rectangle(Width - GutterW - CloseButtonSize, GutterH, CloseButtonSize, CloseButtonSize);
				LogoBounds = new Rectangle(GutterW + ReferenceSize8, (Height - LogoH) / 2, LogoW, LogoH);
				CaptionBounds = new Rectangle(TextX, CaptionY, CaptionW, CaptionH);
				MessageBounds = new Rectangle(TextX, MessageY, MessageW, MessageH);
			}
		}

		protected override void OnFontChanged(EventArgs e)
		{
			base.OnFontChanged(e);

			if(CaptionFont != null)
			{
				CaptionFont.Dispose();
			}
			CaptionFont = new Font(this.Font.Name, this.Font.Size * (12.0f / 9.0f), FontStyle.Regular);

			CalculateBounds();
			Invalidate();
		}

		protected override void OnFormClosed(FormClosedEventArgs e)
		{
			base.OnFormClosed(e);
		}

		protected override void OnResize(EventArgs e)
		{
			base.OnResize(e);

			CalculateBounds();
			Invalidate();
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			base.OnMouseMove(e);

			bool bNewHoverOverClose = CloseButtonBounds.Contains(e.Location);
			if(bHoverOverClose != bNewHoverOverClose)
			{
				bHoverOverClose = bNewHoverOverClose;
				Invalidate(CloseButtonBounds);
			}
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			base.OnMouseDown(e);

			if(e.Button == System.Windows.Forms.MouseButtons.Left)
			{
				bMouseDownOverClose = CloseButtonBounds.Contains(e.Location);
				Invalidate(CloseButtonBounds);
			}
		}

		protected override void OnMouseUp(MouseEventArgs e)
		{
			base.OnMouseUp(e);

			if(e.Button == System.Windows.Forms.MouseButtons.Left)
			{
				if(bMouseDownOverClose)
				{
					bMouseDownOverClose = false;
					Invalidate(CloseButtonBounds);
					Hide();
					if(OnDismiss != null)
					{
						OnDismiss();
					}
				}
				else
				{
					Hide();
					if(OnMoreInformation != null)
					{
						OnMoreInformation();
					}
				}
			}
		}

		protected override void OnPaintBackground(PaintEventArgs e)
		{
			base.OnPaintBackground(e);

			Color TintColor;
			switch(Type)
			{
				case NotificationType.Warning:
					TintColor = Color.FromArgb(240, 240, 220);
					break;
				case NotificationType.Error:
					TintColor = Color.FromArgb(240, 220, 220);
					break;
				default:
					TintColor = Color.FromArgb(220, 220, 240);
					break;
			}

			using(Brush BackgroundBrush = new LinearGradientBrush(new Point(0, 0), new Point(0, Height), Color.White, TintColor))
			{
				e.Graphics.FillRectangle(BackgroundBrush, ClientRectangle);
			}

			e.Graphics.DrawRectangle(Pens.Black, new Rectangle(0, 0, Width - 1, Height - 1));

			e.Graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
			e.Graphics.DrawImage(LogoBitmap, LogoBounds);
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

			// Draw the close button
			if(bHoverOverClose)
			{
				e.Graphics.FillRectangle(SystemBrushes.ActiveCaption, CloseButtonBounds);
			}
			else if(bMouseDownOverClose)
			{
				e.Graphics.FillRectangle(SystemBrushes.InactiveCaption, CloseButtonBounds);
			}
			if(bHoverOverClose || bMouseDownOverClose)
			{
				using(Pen BorderPen = new Pen(SystemColors.InactiveCaptionText, 1.0f))
				{
					e.Graphics.DrawRectangle(BorderPen, new Rectangle(CloseButtonBounds.X, CloseButtonBounds.Y, CloseButtonBounds.Width - 1, CloseButtonBounds.Height - 1));
				}
			}
			using(Pen CrossPen = new Pen(Brushes.Black, 2.0f))
			{
				float Offset = (CloseButtonBounds.Width - 1) / 4.0f;
				e.Graphics.DrawLine(CrossPen, CloseButtonBounds.Left + Offset, CloseButtonBounds.Top + Offset, CloseButtonBounds.Right - 1 - Offset, CloseButtonBounds.Bottom - 1 - Offset);
				e.Graphics.DrawLine(CrossPen, CloseButtonBounds.Left + Offset, CloseButtonBounds.Bottom - 1 - Offset, CloseButtonBounds.Right - 1 - Offset, CloseButtonBounds.Top + Offset);
			}

			// Draw the caption
			TextRenderer.DrawText(e.Graphics, CaptionText, CaptionFont, CaptionBounds, SystemColors.HotTrack, TextFormatFlags.Left);

			// Draw the message
			TextRenderer.DrawText(e.Graphics, MessageText, Font, MessageBounds, SystemColors.ControlText, TextFormatFlags.Left);
		}
	}
}
