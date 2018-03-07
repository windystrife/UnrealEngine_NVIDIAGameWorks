// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
    public class BuildListControl : ListView 
	{
		[StructLayout(LayoutKind.Sequential)]
		public class SCROLLINFO
		{
			public int cbSize;
			public int fMask;
			public int nMin;
			public int nMax;
			public int nPage;
			public int nPos;
			public int nTrackPos;
		}

		const int WM_VSCROLL = 0x115;
		const int WM_MOUSEWHEEL = 0x020A;

		const int SB_HORZ = 0;
		const int SB_VERT = 1;

		const int SIF_RANGE = 0x0001;
		const int SIF_PAGE = 0x0002;
		const int SIF_POS = 0x0004;
		const int SIF_DISABLENOSCROLL = 0x0008;
		const int SIF_TRACKPOS = 0x0010;
		const int SIF_ALL = (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS);        

		const int LVM_GETTOPINDEX = 0x1000 + 39;
		const int LVM_GETCOUNTPERPAGE = 0x1000 + 40;

		[DllImport("user32.dll")]
		private static extern int GetScrollInfo(IntPtr hwnd, int fnBar, SCROLLINFO lpsi);

		[DllImport("user32.dll")]
		private static extern int SetScrollInfo(IntPtr hwnd, int fnBar, SCROLLINFO lpsi, int Redraw);

		[DllImport("user32.dll", CharSet = CharSet.Auto)]
		private static extern IntPtr SendMessage(IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam);

		public delegate void OnScrollDelegate();

		public event OnScrollDelegate OnScroll;

		public BuildListControl()
		{
			Font = SystemFonts.IconTitleFont;
		}

		public int GetFirstVisibleIndex()
		{
			return SendMessage(Handle, LVM_GETTOPINDEX, IntPtr.Zero, IntPtr.Zero).ToInt32();
		}

		public int GetVisibleItemsPerPage()
		{
			return SendMessage(Handle, LVM_GETCOUNTPERPAGE, IntPtr.Zero, IntPtr.Zero).ToInt32();
		}

		public bool GetScrollPosition(out int ScrollY)
		{
			SCROLLINFO ScrollInfo = new SCROLLINFO();
			ScrollInfo.cbSize = Marshal.SizeOf(ScrollInfo);
			ScrollInfo.fMask = SIF_ALL;
			if(GetScrollInfo(Handle, SB_VERT, ScrollInfo) == 0)
			{
				ScrollY = 0;
				return false;
			}
			else
			{
				ScrollY = ScrollInfo.nPos;
				return true;
			}
		}

		public void SetScrollPosition(int ScrollY)
		{
			SCROLLINFO ScrollInfo = new SCROLLINFO();
			ScrollInfo.cbSize = Marshal.SizeOf(ScrollInfo);
			ScrollInfo.nPos = ScrollY;
			ScrollInfo.fMask = SIF_POS;
			SetScrollInfo(Handle, SB_VERT, ScrollInfo, 0);
		}

		public bool IsScrolledToLastPage()
		{
			SCROLLINFO ScrollInfo = new SCROLLINFO();
			ScrollInfo.cbSize = Marshal.SizeOf(ScrollInfo);
			ScrollInfo.fMask = SIF_ALL;
			if(GetScrollInfo(Handle, SB_VERT, ScrollInfo) != 0 && ScrollInfo.nPos >= ScrollInfo.nMax - ScrollInfo.nPage)
			{
				return true;
			}
			return false;
		}

		protected override void WndProc(ref Message Message)
		{
			base.WndProc(ref Message);

			switch(Message.Msg)
			{
				case WM_VSCROLL:
				case WM_MOUSEWHEEL:
					if(OnScroll != null)
					{
						OnScroll();
					}
					break;
			}
		}

		[Browsable(false)]
		[DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
		public override Font Font
		{
			get { return base.Font; }
			set { base.Font = value; }
		}
	}
}
