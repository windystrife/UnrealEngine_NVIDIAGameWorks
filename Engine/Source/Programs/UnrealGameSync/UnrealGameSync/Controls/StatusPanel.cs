// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	abstract class StatusElement
	{
		public Cursor Cursor = Cursors.Arrow;
		public bool bMouseOver;
		public bool bMouseDown;
		public Rectangle Bounds;

		public static Font FindOrAddFont(FontStyle Style, Dictionary<FontStyle, Font> Cache)
		{
			Font Result;
			if(!Cache.TryGetValue(Style, out Result))
			{
				Result = new Font(Cache[FontStyle.Regular], Style);
				Cache[Style] = Result;
			}
			return Result;
		}

		public Point Layout(Graphics Graphics, Point Location, Dictionary<FontStyle, Font> FontCache)
		{
			Size Size = Measure(Graphics, FontCache);
			Bounds = new Rectangle(Location.X, Location.Y - (Size.Height + 1) / 2, Size.Width, Size.Height);
			return new Point(Location.X + Size.Width, Location.Y);
		}

		public abstract Size Measure(Graphics Graphics, Dictionary<FontStyle, Font> FontCache);
		public abstract void Draw(Graphics Grahpics, Dictionary<FontStyle, Font> FontCache);

		public virtual void OnClick(Point Location)
		{
		}
	}

	class IconStatusElement : StatusElement
	{
		Image Icon;

		public IconStatusElement(Image InIcon)
		{
			Icon = InIcon;
		}

		public override Size Measure(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			return Icon.Size;
		}

		public override void Draw(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			Graphics.DrawImage(Icon, Bounds.Location);
		}
	}

	class IconStripStatusElement : StatusElement
	{
		Image Strip;
		Size IconSize;
		int Index;

		public IconStripStatusElement(Image InStrip, Size InIconSize, int InIndex)
		{
			Strip = InStrip;
			IconSize = InIconSize;
			Index = InIndex;
		}

		public override Size Measure(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			return IconSize;
		}

		public override void Draw(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			Graphics.DrawImage(Strip, Bounds, new Rectangle(IconSize.Width * Index, 0, IconSize.Width, IconSize.Height), GraphicsUnit.Pixel);
		}
	}

	class TextStatusElement : StatusElement
	{
		string Text;
		FontStyle Style;

		public TextStatusElement(string InText, FontStyle InStyle)
		{
			Text = InText;
			Style = InStyle;
		}

		public override Size Measure(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			return TextRenderer.MeasureText(Graphics, Text, FindOrAddFont(Style, FontCache), new Size(int.MaxValue, int.MaxValue), TextFormatFlags.NoPadding);
		}

		public override void Draw(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			TextRenderer.DrawText(Graphics, Text, FindOrAddFont(Style, FontCache), Bounds.Location, SystemColors.ControlText, TextFormatFlags.NoPadding);
		}
	}

	class LinkStatusElement : StatusElement
	{
		string Text;
		FontStyle Style;
		Action<Point, Rectangle> LinkAction;

		public LinkStatusElement(string InText, FontStyle InStyle, Action<Point, Rectangle> InLinkAction)
		{
			Text = InText;
			Style = InStyle;
			LinkAction = InLinkAction;
			Cursor = Cursors.Hand;
		}

		public override void OnClick(Point Location)
		{
			LinkAction(Location, Bounds);
		}

		public override Size Measure(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			return TextRenderer.MeasureText(Graphics, Text, FindOrAddFont(Style, FontCache), new Size(int.MaxValue, int.MaxValue), TextFormatFlags.NoPadding);
		}

		public override void Draw(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			Color TextColor = SystemColors.HotTrack;
			if(bMouseDown)
			{
				TextColor = Color.FromArgb(TextColor.B / 2, TextColor.G / 2, TextColor.R);
			}
			else if(bMouseOver)
			{
				TextColor = Color.FromArgb(TextColor.B, TextColor.G, TextColor.R);
			}
			TextRenderer.DrawText(Graphics, Text, FindOrAddFont(Style, FontCache), Bounds.Location, TextColor, TextFormatFlags.NoPadding);
		}
	}

	class ProgressBarStatusElement : StatusElement
	{
		float Progress;

		public ProgressBarStatusElement(float InProgress)
		{
			Progress = InProgress;
		}

		public override Size Measure(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			int Height = (int)(FindOrAddFont(FontStyle.Regular, FontCache).Height * 0.9f);
			return new Size(Height * 16, Height);
		}

		public override void Draw(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;

			Graphics.DrawRectangle(Pens.Black, Bounds.Left, Bounds.Top, Bounds.Width - 1, Bounds.Height - 1);
			Graphics.FillRectangle(Brushes.White, Bounds.Left + 1, Bounds.Top + 1, Bounds.Width - 2, Bounds.Height - 2);

			int ProgressX = Bounds.Left + 2 + (int)((Bounds.Width - 4) * Progress);
			using(Brush ProgressBarBrush = new SolidBrush(Color.FromArgb(112, 146, 190)))
			{
				Graphics.FillRectangle(ProgressBarBrush, Bounds.Left + 2, Bounds.Y + 2, ProgressX - (Bounds.Left + 2), Bounds.Height - 4);
			}
		}
	}

	class StatusLine
	{
		List<StatusElement> Elements = new List<StatusElement>();

		public StatusLine()
		{
			LineHeight = 1.0f;
		}

		public float LineHeight
		{
			get;
			set;
		}

		public Rectangle Bounds
		{
			get;
			private set;
		}

		public void AddIcon(Image InIcon)
		{
			Elements.Add(new IconStatusElement(InIcon));
		}

		public void AddIcon(Image InStrip, Size InIconSize, int InIndex)
		{
			Elements.Add(new IconStripStatusElement(InStrip, InIconSize, InIndex));
		}

		public void AddText(string InText, FontStyle InStyle = FontStyle.Regular)
		{
			Elements.Add(new TextStatusElement(InText, InStyle));
		}

		public void AddLink(string InText, FontStyle InStyle, Action InLinkAction)
		{
			Elements.Add(new LinkStatusElement(InText, InStyle, (P, R) => { InLinkAction(); }));
		}

		public void AddLink(string InText, FontStyle InStyle, Action<Point, Rectangle> InLinkAction)
		{
			Elements.Add(new LinkStatusElement(InText, InStyle, InLinkAction));
		}

		public void AddProgressBar(float Progress)
		{
			Elements.Add(new ProgressBarStatusElement(Progress));
		}

		public bool HitTest(Point Location, out StatusElement OutElement)
		{
			OutElement = null;
			if(Bounds.Contains(Location))
			{
				foreach(StatusElement Element in Elements)
				{
					if(Element.Bounds.Contains(Location))
					{
						OutElement = Element;
						return true;
					}
				}
			}
			return false;
		}

		public void Layout(Graphics Graphics, Point Location, Dictionary<FontStyle, Font> FontCache)
		{
			Point NextLocation = Location;
			foreach(StatusElement Element in Elements)
			{
				NextLocation = Element.Layout(Graphics, NextLocation, FontCache);
				Bounds = Bounds.IsEmpty? Element.Bounds : Rectangle.Union(Bounds, Element.Bounds);
			}
		}

		public void Draw(Graphics Graphics, Dictionary<FontStyle, Font> FontCache)
		{
			foreach(StatusElement Element in Elements)
			{
				Element.Draw(Graphics, FontCache);
			}
		}
	}

	class StatusPanel : Panel
	{
		const float LineSpacing = 1.35f;

		Image ProjectLogo;
		Rectangle ProjectLogoBounds;
		Dictionary<FontStyle, Font> FontCache = new Dictionary<FontStyle,Font>();
		List<StatusLine> Lines = new List<StatusLine>();
		Point? MouseOverLocation;
		StatusElement MouseOverElement;
		Point? MouseDownLocation;
		StatusElement MouseDownElement;
		int ContentWidth = 400;
		int SuspendDisplayCount;

		public StatusPanel()
		{
			DoubleBuffered = true;
		}

		public void SuspendDisplay()
		{
			SuspendDisplayCount++;
		}

		public void ResumeDisplay()
		{
			SuspendDisplayCount--;
		}

		public void SetContentWidth(int NewContentWidth)
		{
			if(ContentWidth != NewContentWidth)
			{
				ContentWidth = NewContentWidth;
				LayoutElements();
				Invalidate();
			}
		}

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			if(disposing)
			{
				if(ProjectLogo != null)
				{
					ProjectLogo.Dispose();
					ProjectLogo = null;
				}
				ResetFontCache();
			}
		}

		private void ResetFontCache()
		{
			// Dispose of all the fonts we have at the moment
			foreach(KeyValuePair<FontStyle, Font> FontPair in FontCache)
			{
				if(FontPair.Key != FontStyle.Regular)
				{
					FontPair.Value.Dispose();
				}
			}

			// Repopulate with the basic font
			FontCache.Clear();
			FontCache.Add(FontStyle.Regular, Font);
		}

		public void SetProjectLogo(Image NewProjectLogo)
		{
			if(ProjectLogo != null)
			{
				ProjectLogo.Dispose();
			}
			ProjectLogo = NewProjectLogo;
			Invalidate();
		}

		public void Clear()
		{
			InvalidateElements();
			Lines.Clear();
		}

		public void Set(IEnumerable<StatusLine> NewLines)
		{
			InvalidateElements();
			Lines.Clear();
			Lines.AddRange(NewLines);
			LayoutElements();
			InvalidateElements();

			MouseOverElement = null;
			MouseDownElement = null;
			SetMouseOverLocation(MouseOverLocation);
			SetMouseDownLocation(MouseDownLocation);
		}

		protected void InvalidateElements()
		{
			foreach(StatusLine Line in Lines)
			{
				Invalidate(Line.Bounds);
			}
		}

		protected bool HitTest(Point Location, out StatusElement OutElement)
		{
			OutElement = null;
			foreach(StatusLine Line in Lines)
			{
				if(Line.HitTest(Location, out OutElement))
				{
					return true;
				}
			}
			return false;
		}

		protected override void OnFontChanged(EventArgs e)
		{
			base.OnFontChanged(e);

			ResetFontCache();
			LayoutElements();
		}

		protected void LayoutElements()
		{
			using(Graphics Graphics = CreateGraphics())
			{
				LayoutElements(Graphics);
			}
		}

		protected void LayoutElements(Graphics Graphics)
		{
			// Get the logo size
			Image DrawProjectLogo = ProjectLogo ?? Properties.Resources.DefaultProjectLogo;
			float LogoScale = Math.Min((float)Height / DrawProjectLogo.Height, 1.0f);
			int LogoWidth = (int)(DrawProjectLogo.Width * LogoScale);
			int LogoHeight = (int)(DrawProjectLogo.Height * LogoScale);

			// Figure out where the split between content and the logo is going to be
			int DividerX = ((Width - LogoWidth - ContentWidth) / 2) + LogoWidth;

			// Set the logo rectangle
			ProjectLogoBounds = new Rectangle(DividerX - LogoWidth, (Height - LogoHeight) / 2, LogoWidth, LogoHeight);

			// Measure up all the line height
			float TotalLineHeight = Lines.Sum(x => x.LineHeight);

			// Space out all the lines
			float LineY = (Height - TotalLineHeight * (int)(Font.Height * LineSpacing)) / 2;
			foreach(StatusLine Line in Lines)
			{
				LineY += (int)(Font.Height * LineSpacing * Line.LineHeight * 0.5f);
				Line.Layout(Graphics, new Point(DividerX + 5, (int)LineY), FontCache);
				LineY += (int)(Font.Height * LineSpacing * Line.LineHeight * 0.5f);
			}
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			SetMouseOverLocation(e.Location);
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			if(e.Button == MouseButtons.Left)
			{
				SetMouseDownLocation(e.Location);
			}
		}

		protected override void OnMouseUp(MouseEventArgs e)
		{
			if(MouseDownElement != null && MouseOverElement == MouseDownElement)
			{
				MouseDownElement.OnClick(e.Location);
			}
			SetMouseDownLocation(null);
		}

		protected override void OnMouseCaptureChanged(EventArgs e)
		{
			base.OnMouseCaptureChanged(e);

			if(!Capture)
			{
				SetMouseDownLocation(null);
				SetMouseOverLocation(null);
			}
		}

		protected override void OnLeave(EventArgs e)
		{
			base.OnLeave(e);

			SetMouseDownLocation(null);
			SetMouseOverLocation(null);
		}

		protected override void OnResize(EventArgs eventargs)
		{
			base.OnResize(eventargs);

			LayoutElements();
			Invalidate();
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			if(SuspendDisplayCount == 0)
			{
				e.Graphics.DrawImage(ProjectLogo ?? Properties.Resources.DefaultProjectLogo, ProjectLogoBounds);

				e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;

				foreach(StatusLine Line in Lines)
				{
					Line.Draw(e.Graphics, FontCache);
				}
			}
		}

		protected void SetMouseOverLocation(Point? NewMouseOverLocation)
		{
			MouseOverLocation = NewMouseOverLocation;

			StatusElement NewMouseOverElement = null;
			if(MouseOverLocation.HasValue)
			{
				HitTest(MouseOverLocation.Value, out NewMouseOverElement);
			}

			if(NewMouseOverElement != MouseOverElement)
			{
				if(MouseOverElement != null)
				{
					MouseOverElement.bMouseOver = false;
					Cursor = Cursors.Arrow;
					Invalidate(MouseOverElement.Bounds);
				}

				MouseOverElement = NewMouseOverElement;

				if(MouseOverElement != null)
				{
					MouseOverElement.bMouseOver = true;
					Cursor = MouseOverElement.Cursor;
					Invalidate(MouseOverElement.Bounds);
				}
			}

			Capture = (MouseDownElement != null || MouseOverElement != null);
		}

		protected void SetMouseDownLocation(Point? NewMouseDownLocation)
		{
			MouseDownLocation = NewMouseDownLocation;

			StatusElement NewMouseDownElement = null;
			if(MouseDownLocation.HasValue)
			{
				HitTest(MouseDownLocation.Value, out NewMouseDownElement);
			}

			if(NewMouseDownElement != MouseDownElement)
			{
				if(MouseDownElement != null)
				{
					MouseDownElement.bMouseDown = false;
					Invalidate(MouseDownElement.Bounds);
				}

				MouseDownElement = NewMouseDownElement;

				if(MouseDownElement != null)
				{
					MouseDownElement.bMouseDown = true;
					Invalidate(MouseDownElement.Bounds);
				}
			}

			Capture = (MouseDownElement != null || MouseOverElement != null);
		}
	}
}
