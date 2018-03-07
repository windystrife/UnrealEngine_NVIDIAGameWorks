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
using System.IO;

namespace UnrealGameSync
{
	public class LogControl : UserControl
	{
		[Flags]
		public enum ScrollInfoMask : uint
		{
			SIF_RANGE = 0x1,
			SIF_PAGE = 0x2,
			SIF_POS = 0x4,
			SIF_DISABLENOSCROLL = 0x8,
			SIF_TRACKPOS = 0x10,
			SIF_ALL = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS,
		}

		[StructLayout(LayoutKind.Sequential)]
		class SCROLLINFO 
		{
			public int cbSize;
			public ScrollInfoMask fMask;
			public int nMin;
			public int nMax;
			public int nPage;
			public int nPos;
			public int nTrackPos;
		}

		enum ScrollBarType : int
		{
			SB_HORZ = 0,
			SB_VERT = 1,
			SB_CONTROL = 2,
			SB_BOTH = 3,
		}

		enum ScrollBarArrows : uint
		{
			ESB_ENABLE_BOTH = 0,
		}

		[DllImport("user32.dll")]
		static extern bool EnableScrollBar(IntPtr hWnd, ScrollBarType wSBflags, ScrollBarArrows wArrows);

		[DllImport("user32.dll")]
		static extern int SetScrollInfo(IntPtr hwnd, ScrollBarType fnBar, SCROLLINFO lpsi, bool fRedraw);

		[DllImport("user32.dll")]
		static extern int GetScrollInfo(IntPtr hwnd, ScrollBarType fnBar, SCROLLINFO lpsi);

		[DllImport("user32.dll", CharSet = CharSet.Auto)]
		static extern IntPtr SendMessage(IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam);

		struct TextLocation
		{
			public readonly int LineIdx;
			public readonly int ColumnIdx;

			public TextLocation(int InLineIdx, int InColumnIdx)
			{
				LineIdx = InLineIdx;
				ColumnIdx = InColumnIdx;
			}

			public override string ToString()
			{
				return String.Format("Ln {0}, Col {1}", LineIdx, ColumnIdx);
			}
		}

		class TextSelection
		{
			public TextLocation Start;
			public TextLocation End;

			public TextSelection(TextLocation InStart, TextLocation InEnd)
			{
				Start = InStart;
				End = InEnd;
			}

			public bool IsEmpty()
			{
				return Start.LineIdx == End.LineIdx && Start.ColumnIdx == End.ColumnIdx;
			}

			public override string ToString()
			{
				return String.Format("{0} - {1}", Start, End);
			}
		}

		List<string> Lines = new List<string>();
		int MaxLineLength;

		int ScrollLine;
		int ScrollLinesPerPage;
		bool bTrackingScroll;

		int ScrollColumn;
		int ScrollColumnsPerPage;

		bool bIsSelecting;
		TextSelection Selection;

		Size FontSize;

		Timer SelectionScrollTimer;
		int AutoScrollRate = 0;

		Timer UpdateTimer;
		ConcurrentQueue<string> QueuedLines = new ConcurrentQueue<string>();
		FileStream LogFileStream;

		public LogControl()
		{
			BackColor = Color.White;
			ForeColor = Color.FromArgb(255, 32, 32, 32);
			
			DoubleBuffered = true;

			ContextMenuStrip = new ContextMenuStrip();
			ContextMenuStrip.Items.Add("Copy", null, new EventHandler(ContextMenu_CopySelection));
			ContextMenuStrip.Items.Add("-");
			ContextMenuStrip.Items.Add("Select All", null, new EventHandler(ContextMenu_SelectAll));

			SetStyle(ControlStyles.Selectable, true);
		}

		protected override void OnCreateControl()
		{
 			base.OnCreateControl();

			EnableScrollBar(Handle, ScrollBarType.SB_BOTH, ScrollBarArrows.ESB_ENABLE_BOTH);

			Cursor = Cursors.IBeam;

			UpdateTimer = new Timer();
			UpdateTimer.Interval = 200;
			UpdateTimer.Tick += (a, b) => Tick();
			UpdateTimer.Enabled = true;

			SelectionScrollTimer = new Timer();
			SelectionScrollTimer.Interval = 200;
			SelectionScrollTimer.Tick += new EventHandler(SelectionScrollTimer_TimerElapsed);

			Clear();

			UpdateFontMetrics();
			UpdateScrollBarPageSize();
		}

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			if(UpdateTimer != null)
			{
				UpdateTimer.Dispose();
				UpdateTimer = null;
			}

			CloseFile();
		}

		public bool OpenFile(string NewLogFileName)
		{
			CloseFile();
			Clear();
			try
			{
				LogFileStream = File.Open(NewLogFileName, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.Read);
			}
			catch(Exception)
			{
				return false;
			}

			LogFileStream.Seek(0, SeekOrigin.Begin);

			string Text = new StreamReader(LogFileStream).ReadToEnd().TrimEnd('\r', '\n');
			if(Text.Length > 0)
			{
				AddLinesInternal(Text.Split('\n').Select(x => x + "\n").ToList());
			}

			Invalidate();
			return true;
		}

		public void CloseFile()
		{
			if(LogFileStream != null)
			{
				LogFileStream.Dispose();
				LogFileStream = null;
			}
		}

		public void Clear()
		{
			Lines.Clear();
			MaxLineLength = 0;

			ScrollLine = 0;
			ScrollColumn = 0;
			bIsSelecting = false;
			Selection = null;

			SCROLLINFO ScrollInfo = new SCROLLINFO();
			ScrollInfo.cbSize = Marshal.SizeOf(ScrollInfo);
			ScrollInfo.fMask = ScrollInfoMask.SIF_RANGE | ScrollInfoMask.SIF_PAGE | ScrollInfoMask.SIF_POS;
			SetScrollInfo(Handle, ScrollBarType.SB_HORZ, ScrollInfo, true);
			SetScrollInfo(Handle, ScrollBarType.SB_VERT, ScrollInfo, true);

			QueuedLines = new ConcurrentQueue<string>();
			if(LogFileStream != null)
			{
				LogFileStream.SetLength(0);
			}
		}

		public void ScrollToEnd()
		{
			ScrollWindow(Lines.Count);
			Invalidate();
		}

		public void AppendLine(string Line)
		{
			QueuedLines.Enqueue(Line);
		}

		private void Tick()
		{
			if(!Focused || !Capture)
			{
				List<string> NewLines = new List<string>();
				for(;;)
				{
					string NextLine;
					if(!QueuedLines.TryDequeue(out NextLine))
					{
						break;
					}
					NewLines.Add(NextLine.TrimEnd('\r', '\n') + "\n");
				}
				if(NewLines.Count > 0)
				{
					StringBuilder TextToAppendBuilder = new StringBuilder();
					foreach(string NewLine in NewLines)
					{
						TextToAppendBuilder.AppendLine(NewLine.TrimEnd('\n'));
					}

					string TextToAppend = TextToAppendBuilder.ToString();
					if(LogFileStream != null)
					{
						byte[] Data = Encoding.UTF8.GetBytes(TextToAppend);
						try
						{
							LogFileStream.Write(Data, 0, Data.Length);
							LogFileStream.Flush();
						}
						catch(Exception Ex)
						{
							TextToAppend += String.Format("Failed to write to log file ({0}): {1}\n", LogFileStream.Name, Ex.ToString());
						}
					}

					AddLinesInternal(NewLines);
				}
			}
		}

		private void AddLinesInternal(List<string> NewLines)
		{
			Lines.AddRange(NewLines);

			// Figure out if we're tracking the last line
			bool bIsTrackingLastLine = IsTrackingLastLine();

			int NewMaxLineLength = Math.Max(MaxLineLength, NewLines.Max(x => x.Length));
			if(NewMaxLineLength > MaxLineLength)
			{
				MaxLineLength = NewMaxLineLength;

				SCROLLINFO HorizontalScroll = new SCROLLINFO();
				HorizontalScroll.cbSize = Marshal.SizeOf(HorizontalScroll);
				HorizontalScroll.fMask = ScrollInfoMask.SIF_PAGE | ScrollInfoMask.SIF_RANGE;
				HorizontalScroll.nMin = 0;
				HorizontalScroll.nMax = NewMaxLineLength;
				HorizontalScroll.nPage = ScrollColumnsPerPage;
				SetScrollInfo(Handle, ScrollBarType.SB_HORZ, HorizontalScroll, true);
			}

			SCROLLINFO VerticalScroll = new SCROLLINFO();
			VerticalScroll.cbSize = Marshal.SizeOf(VerticalScroll);
			VerticalScroll.fMask = ScrollInfoMask.SIF_POS | ScrollInfoMask.SIF_RANGE | ScrollInfoMask.SIF_PAGE | ScrollInfoMask.SIF_TRACKPOS;
			GetScrollInfo(Handle, ScrollBarType.SB_VERT, VerticalScroll);

			if(bTrackingScroll)
			{
				UpdateVerticalScrollPosition(VerticalScroll.nTrackPos, ref VerticalScroll);
			}
			else
			{
				VerticalScroll.fMask = ScrollInfoMask.SIF_RANGE | ScrollInfoMask.SIF_PAGE;
				VerticalScroll.nMin = 0;
				if(bIsTrackingLastLine)
				{
					VerticalScroll.fMask |= ScrollInfoMask.SIF_POS;
					VerticalScroll.nPos = Math.Max(Lines.Count - 1 - ScrollLinesPerPage + 1, 0);
					ScrollLine = Math.Max(Lines.Count - 1 - ScrollLinesPerPage + 1, 0);
				}
				VerticalScroll.nMax = Math.Max(Lines.Count - 1, 1);
				VerticalScroll.nPage = ScrollLinesPerPage;
				SetScrollInfo(Handle, ScrollBarType.SB_VERT, VerticalScroll, true);
			}

			Invalidate();
		}

		private SCROLLINFO GetScrollInfo(ScrollBarType Type, ScrollInfoMask Mask)
		{
			SCROLLINFO ScrollInfo = new SCROLLINFO();
			ScrollInfo.cbSize = Marshal.SizeOf(VerticalScroll);
			ScrollInfo.fMask = Mask;
			GetScrollInfo(Handle, ScrollBarType.SB_VERT, ScrollInfo);
			return ScrollInfo;
		}

		private bool IsTrackingLastLine()
		{
			SCROLLINFO VerticalScroll = new SCROLLINFO();
			VerticalScroll.cbSize = Marshal.SizeOf(VerticalScroll);
			VerticalScroll.fMask = ScrollInfoMask.SIF_POS | ScrollInfoMask.SIF_RANGE | ScrollInfoMask.SIF_PAGE | ScrollInfoMask.SIF_TRACKPOS;
			GetScrollInfo(Handle, ScrollBarType.SB_VERT, VerticalScroll);

			return (VerticalScroll.nPos >= VerticalScroll.nMax - ScrollLinesPerPage + 1);
		}

		private void CopySelection()
		{
			if(Lines.Count > 0)
			{
				StringBuilder SelectedText = new StringBuilder();
				if(Selection == null || Selection.IsEmpty())
				{
					foreach(string Line in Lines)
					{
						SelectedText.Append(Line);
					}
				}
				else
				{
					for(int LineIdx = Math.Min(Selection.Start.LineIdx, Selection.End.LineIdx); LineIdx <= Math.Max(Selection.Start.LineIdx, Selection.End.LineIdx); LineIdx++)
					{
						int MinIdx, MaxIdx;
						ClipSelectionToLine(LineIdx, out MinIdx, out MaxIdx);
						SelectedText.Append(Lines[LineIdx], MinIdx, MaxIdx - MinIdx);
					}
				}
				Clipboard.SetText(SelectedText.ToString().Replace("\n", "\r\n"));
			}
		}

		protected void ContextMenu_CopySelection(object sender, EventArgs e)
		{
			CopySelection();
		}

		protected void ContextMenu_SelectAll(object sender, EventArgs e)
		{
			SelectAll();
		}

		public void SelectAll()
		{
			if(Lines.Count > 0)
			{
				Selection = new TextSelection(new TextLocation(0, 0), new TextLocation(Lines.Count - 1, Lines[Lines.Count - 1].Length));
				Invalidate();
			}
		}

		TextLocation PointToTextLocation(Point ClientPoint)
		{
			if(Lines.Count == 0)
			{
				return new TextLocation(0, 0);
			}
			else
			{
				int UnclippedLineIdx = (ClientPoint.Y / FontSize.Height) + ScrollLine;
				if(UnclippedLineIdx < 0)
				{
					return new TextLocation(0, 0);
				}
				else if(UnclippedLineIdx >= Lines.Count)
				{
					return new TextLocation(Lines.Count - 1, Lines[Lines.Count - 1].Length);
				}
				else
				{
					return new TextLocation(UnclippedLineIdx, Math.Max(0, Math.Min((ClientPoint.X / FontSize.Width) + ScrollColumn, Lines[UnclippedLineIdx].Length)));
				}
			}
		}

		protected override void OnResize(EventArgs e)
		{
			base.OnResize(e);

			UpdateScrollBarPageSize();

			int NewScrollLine = ScrollLine;
			NewScrollLine = Math.Min(NewScrollLine, Lines.Count - ScrollLinesPerPage);
			NewScrollLine = Math.Max(NewScrollLine, 0);

			if(ScrollLine != NewScrollLine)
			{
				ScrollLine = NewScrollLine;
				Invalidate();
			}
		}

		void UpdateVerticalScrollBarPageSize()
		{
			// Calculate the number of lines per page
			ScrollLinesPerPage = ClientRectangle.Height / FontSize.Height;

			// Update the vertical scroll bar
			SCROLLINFO VerticalScroll = new SCROLLINFO();
			VerticalScroll.cbSize = Marshal.SizeOf(VerticalScroll);
			VerticalScroll.fMask = ScrollInfoMask.SIF_PAGE;
			VerticalScroll.nPage = ScrollLinesPerPage;
			SetScrollInfo(Handle, ScrollBarType.SB_VERT, VerticalScroll, true);
		}

		void UpdateScrollBarPageSize()
		{
			// Update the vertical scroll bar, so we know what fits
			UpdateVerticalScrollBarPageSize();

			// This may have modified the client area. Now calculate the number of columns per page.
			ScrollColumnsPerPage = ClientRectangle.Width / FontSize.Width;

			// And update the horizontal scroll bar
			SCROLLINFO HorizontalScroll = new SCROLLINFO();
			HorizontalScroll.cbSize = Marshal.SizeOf(HorizontalScroll);
			HorizontalScroll.fMask = ScrollInfoMask.SIF_PAGE;
			HorizontalScroll.nPage = ScrollColumnsPerPage;
			SetScrollInfo(Handle, ScrollBarType.SB_HORZ, HorizontalScroll, true);

			// Now that we know whether we have a horizontal scroll bar or not, calculate the vertical scroll again
			UpdateVerticalScrollBarPageSize();
		}

		protected override void OnFontChanged(EventArgs e)
		{
			base.OnFontChanged(e);

			UpdateFontMetrics();
		}

		private void UpdateFontMetrics()
		{
			using(Graphics Graphics = CreateGraphics())
			{
				FontSize = TextRenderer.MeasureText(Graphics, "A", Font, new Size(int.MaxValue, int.MaxValue), TextFormatFlags.NoPadding);
			}
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			base.OnMouseDown(e);

			if(e.Button == MouseButtons.Left)
			{
				bIsSelecting = true;

				TextLocation Location = PointToTextLocation(e.Location);
				Selection = new TextSelection(Location, Location);

				SelectionScrollTimer.Start();
				AutoScrollRate = 0;

				Capture = true;
				Invalidate();
			}
		}

		protected override void OnMouseCaptureChanged(EventArgs e)
		{
			base.OnMouseCaptureChanged(e);

			EndSelection();
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			base.OnMouseMove(e);

			if(bIsSelecting)
			{
				TextLocation NewSelectionEnd = PointToTextLocation(e.Location);
				if(NewSelectionEnd.LineIdx != Selection.End.LineIdx || NewSelectionEnd.ColumnIdx != Selection.End.ColumnIdx)
				{
					Selection.End = NewSelectionEnd;
					Invalidate();
				}

				AutoScrollRate = ((e.Y < 0)? e.Y : Math.Max(e.Y - ClientSize.Height, 0)) / FontSize.Height;
			}
		}

		protected override void OnMouseUp(MouseEventArgs e)
		{
			base.OnMouseUp(e);

			EndSelection();
			Capture = false;

			if(e.Button == MouseButtons.Right)
			{
				ContextMenuStrip.Show(this, e.Location);
			}
		}

		protected override void OnMouseWheel(MouseEventArgs e)
		{
			base.OnMouseWheel(e);

			ScrollWindow(-e.Delta / 60);

			Invalidate();
		}

		protected override bool IsInputKey(Keys Key)
		{
			switch(Key & ~Keys.Modifiers)
			{
				case Keys.Up:
				case Keys.Down:
					return true;
				default:
					return base.IsInputKey(Key);
			}
		}

		protected override void OnKeyDown(KeyEventArgs e)
		{
			switch(e.KeyCode & ~Keys.Modifiers)
			{
				case Keys.Up:
					ScrollWindow(-1);
					Invalidate();
					break;
				case Keys.Down:
					ScrollWindow(+1);
					Invalidate();
					break;
				case Keys.PageUp:
					ScrollWindow(-ScrollLinesPerPage);
					Invalidate();
					break;
				case Keys.PageDown:
					ScrollWindow(+ScrollLinesPerPage);
					Invalidate();
					break;
				case Keys.Home:
					ScrollWindow(-Lines.Count);
					Invalidate();
					break;
				case Keys.End:
					ScrollWindow(+Lines.Count);
					Invalidate();
					break;
				case Keys.A:
					if(e.Control)
					{
						SelectAll();
					}
					break;
				case Keys.C:
					if(e.Control)
					{
						CopySelection();
					}
					break;
			}
		}

		void EndSelection()
		{
			if(bIsSelecting)
			{
				if(Selection != null && Selection.IsEmpty())
				{
					Selection = null;
				}
				SelectionScrollTimer.Stop();
				bIsSelecting = false;
			}
		}

		protected override void OnScroll(ScrollEventArgs se)
		{
			base.OnScroll(se);

			if(se.ScrollOrientation == ScrollOrientation.HorizontalScroll)
			{
				// Get the current scroll position
				SCROLLINFO ScrollInfo = new SCROLLINFO();
				ScrollInfo.cbSize = Marshal.SizeOf(ScrollInfo);
				ScrollInfo.fMask = ScrollInfoMask.SIF_ALL;
				GetScrollInfo(Handle, ScrollBarType.SB_HORZ, ScrollInfo);

				// Get the new scroll position
				int TargetScrollPos = ScrollInfo.nPos;
				switch(se.Type)
				{
					case ScrollEventType.SmallDecrement:
						TargetScrollPos = ScrollInfo.nPos - 1;
						break;
					case ScrollEventType.SmallIncrement:
						TargetScrollPos = ScrollInfo.nPos + 1;
						break;
					case ScrollEventType.LargeDecrement:
						TargetScrollPos = ScrollInfo.nPos - ScrollInfo.nPage;
						break;
					case ScrollEventType.LargeIncrement:
						TargetScrollPos = ScrollInfo.nPos + ScrollInfo.nPage;
						break;
					case ScrollEventType.ThumbPosition:
						TargetScrollPos = ScrollInfo.nTrackPos;
						break;
					case ScrollEventType.ThumbTrack:
						TargetScrollPos = ScrollInfo.nTrackPos;
						break;
				}
				ScrollColumn = Math.Max(0, Math.Min(TargetScrollPos, ScrollInfo.nMax - ScrollInfo.nPage + 1));

				// Update the scroll bar if we're not tracking
				if(se.Type != ScrollEventType.ThumbTrack)
				{
					ScrollInfo.fMask = ScrollInfoMask.SIF_POS;
					ScrollInfo.nPos = ScrollColumn;
					SetScrollInfo(Handle, ScrollBarType.SB_HORZ, ScrollInfo, true);
				}

				Invalidate();
			}
			else if(se.ScrollOrientation == ScrollOrientation.VerticalScroll)
			{
				// Get the current scroll position
				SCROLLINFO ScrollInfo = new SCROLLINFO();
				ScrollInfo.cbSize = Marshal.SizeOf(ScrollInfo);
				ScrollInfo.fMask = ScrollInfoMask.SIF_ALL;
				GetScrollInfo(Handle, ScrollBarType.SB_VERT, ScrollInfo);

				// Get the new scroll position
				int TargetScrollPos = ScrollInfo.nPos;
				switch(se.Type)
				{
					case ScrollEventType.SmallDecrement:
						TargetScrollPos = ScrollInfo.nPos - 1;
						break;
					case ScrollEventType.SmallIncrement:
						TargetScrollPos = ScrollInfo.nPos + 1;
						break;
					case ScrollEventType.LargeDecrement:
						TargetScrollPos = ScrollInfo.nPos - ScrollLinesPerPage;
						break;
					case ScrollEventType.LargeIncrement:
						TargetScrollPos = ScrollInfo.nPos + ScrollLinesPerPage;
						break;
					case ScrollEventType.ThumbPosition:
						TargetScrollPos = ScrollInfo.nTrackPos;
						break;
					case ScrollEventType.ThumbTrack:
						TargetScrollPos = ScrollInfo.nTrackPos;
						break;
				}

				// Try to move to the new scroll position
				UpdateVerticalScrollPosition(TargetScrollPos, ref ScrollInfo);

				// Update the new range as well
				if(se.Type == ScrollEventType.ThumbTrack)
				{
					bTrackingScroll = true;
				}
				else
				{
					// If we've just finished tracking, allow updating the range
					ScrollInfo.nPos = ScrollLine;
					if(bTrackingScroll)
					{
						ScrollInfo.fMask |= ScrollInfoMask.SIF_RANGE;
						ScrollInfo.nMax = Math.Max(Lines.Count, 1) - 1;
						bTrackingScroll = false;
					}
					SetScrollInfo(Handle, ScrollBarType.SB_VERT, ScrollInfo, true);
				}

				Invalidate();
			}
		}

		void UpdateVerticalScrollPosition(int TargetScrollPos, ref SCROLLINFO ScrollInfo)
		{
			// Scale it up to the number of lines. We don't adjust the scroll max while tracking so we can easily scroll to the end without more stuff being added.
			float Ratio = Math.Max(Math.Min((float)TargetScrollPos / (float)(ScrollInfo.nMax - ScrollLinesPerPage + 1), 1.0f), 0.0f);

			// Calculate the new scroll line, rounding to the nearest
			ScrollLine = (int)((((Math.Max(Lines.Count, 1) - 1) - ScrollLinesPerPage + 1) * Ratio) + 0.5f);
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			int TextX = -ScrollColumn * FontSize.Width;
			for(int Idx = ScrollLine; Idx < ScrollLine + ScrollLinesPerPage + 1 && Idx < Lines.Count; Idx++)
			{
				int SelectMinIdx;
				int SelectMaxIdx;
				ClipSelectionToLine(Idx, out SelectMinIdx, out SelectMaxIdx);

				int TextY = (Idx - ScrollLine) * FontSize.Height;
				if(SelectMinIdx > 0)
				{
					TextRenderer.DrawText(e.Graphics, Lines[Idx].Substring(0, SelectMinIdx), Font, new Point(TextX, TextY), Color.Black, TextFormatFlags.NoPadding);
				}
				if(SelectMinIdx < SelectMaxIdx)
				{
					e.Graphics.FillRectangle(SystemBrushes.Highlight, TextX + (SelectMinIdx * FontSize.Width), TextY, (SelectMaxIdx - SelectMinIdx) * FontSize.Width, FontSize.Height);
					TextRenderer.DrawText(e.Graphics, Lines[Idx].Substring(SelectMinIdx, SelectMaxIdx - SelectMinIdx), Font, new Point(TextX + (SelectMinIdx * FontSize.Width), TextY), SystemColors.HighlightText, TextFormatFlags.NoPadding);
				}
				if(SelectMaxIdx < Lines[Idx].Length)
				{
					TextRenderer.DrawText(e.Graphics, Lines[Idx].Substring(SelectMaxIdx), Font, new Point(TextX + (SelectMaxIdx * FontSize.Width), TextY), Color.Black, TextFormatFlags.NoPadding);
				}
			}
		}

		void ScrollWindow(int ScrollDelta)
		{
			SCROLLINFO VerticalScroll = new SCROLLINFO();
			VerticalScroll.cbSize = Marshal.SizeOf(VerticalScroll);
			VerticalScroll.fMask = ScrollInfoMask.SIF_ALL;
			GetScrollInfo(Handle, ScrollBarType.SB_VERT, VerticalScroll);

			VerticalScroll.nPos = Math.Min(Math.Max(VerticalScroll.nPos + ScrollDelta, 0), VerticalScroll.nMax - VerticalScroll.nPage + 1);
			VerticalScroll.fMask = ScrollInfoMask.SIF_POS;
			SetScrollInfo(Handle, ScrollBarType.SB_VERT, VerticalScroll, true);

			ScrollLine = VerticalScroll.nPos;
		}

		protected void SelectionScrollTimer_TimerElapsed(object Sender, EventArgs Args)
		{
			if(AutoScrollRate != 0 && Selection != null)
			{
				ScrollWindow(AutoScrollRate);

				SCROLLINFO VerticalScroll = new SCROLLINFO();
				VerticalScroll.cbSize = Marshal.SizeOf(VerticalScroll);
				VerticalScroll.fMask = ScrollInfoMask.SIF_ALL;
				GetScrollInfo(Handle, ScrollBarType.SB_VERT, VerticalScroll);

				if(AutoScrollRate < 0)
				{
					if(Selection.End.LineIdx > VerticalScroll.nPos)
					{
						Selection.End = new TextLocation(VerticalScroll.nPos, 0);
					}
				}
				else
				{
					if(Selection.End.LineIdx < VerticalScroll.nPos + VerticalScroll.nPage)
					{
						int LineIdx = Math.Min(VerticalScroll.nPos + VerticalScroll.nPage, Lines.Count - 1);
						Selection.End = new TextLocation(LineIdx, Lines[LineIdx].Length);
					}
				}

				Invalidate();
			}
		}

		protected void ClipSelectionToLine(int LineIdx, out int SelectMinIdx, out int SelectMaxIdx)
		{
			if(Selection == null)
			{
				SelectMinIdx = 0;
				SelectMaxIdx = 0;
			}
			else if(Selection.Start.LineIdx < Selection.End.LineIdx)
			{
				if(LineIdx < Selection.Start.LineIdx)
				{
					SelectMinIdx = Lines[LineIdx].Length;
					SelectMaxIdx = Lines[LineIdx].Length;
				}
				else if(LineIdx == Selection.Start.LineIdx)
				{
					SelectMinIdx = Selection.Start.ColumnIdx;
					SelectMaxIdx = Lines[LineIdx].Length;
				}
				else if(LineIdx < Selection.End.LineIdx)
				{
					SelectMinIdx = 0;
					SelectMaxIdx = Lines[LineIdx].Length;
				}
				else if(LineIdx == Selection.End.LineIdx)
				{
					SelectMinIdx = 0;
					SelectMaxIdx = Selection.End.ColumnIdx;
				}
				else
				{
					SelectMinIdx = 0;
					SelectMaxIdx = 0;
				}
			}
			else if(Selection.Start.LineIdx > Selection.End.LineIdx)
			{
				if(LineIdx < Selection.End.LineIdx)
				{
					SelectMinIdx = Lines[LineIdx].Length;
					SelectMaxIdx = Lines[LineIdx].Length;
				}
				else if(LineIdx == Selection.End.LineIdx)
				{
					SelectMinIdx = Selection.End.ColumnIdx;
					SelectMaxIdx = Lines[LineIdx].Length;
				}
				else if(LineIdx < Selection.Start.LineIdx)
				{
					SelectMinIdx = 0;
					SelectMaxIdx = Lines[LineIdx].Length;
				}
				else if(LineIdx == Selection.Start.LineIdx)
				{
					SelectMinIdx = 0;
					SelectMaxIdx = Selection.Start.ColumnIdx;
				}
				else
				{
					SelectMinIdx = 0;
					SelectMaxIdx = 0;
				}
			}
			else
			{
				if(LineIdx == Selection.Start.LineIdx)
				{
					SelectMinIdx = Math.Min(Selection.Start.ColumnIdx, Selection.End.ColumnIdx);
					SelectMaxIdx = Math.Max(Selection.Start.ColumnIdx, Selection.End.ColumnIdx);
				}
				else
				{
					SelectMinIdx = 0;
					SelectMaxIdx = 0;
				}
			}
		}
	}

	public class LogControlTextWriter : LineBasedTextWriter
	{
		LogControl LogControl;

		public LogControlTextWriter(LogControl InLogControl)
		{
			LogControl = InLogControl;
		}

		protected override void FlushLine(string Line)
		{
			LogControl.AppendLine(Line);
		}
	}
}
