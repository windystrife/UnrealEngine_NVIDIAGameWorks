// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.ComponentModel;
using System.Drawing.Design;
using System.Collections.Specialized;

namespace MemoryProfiler2
{
	/// <summary> 
	/// Represents a Windows list view control, which displays a collection of items that can be displayed using one of four different views.
	/// This version adds following extensions:
	///		Tooltips for column headers
	///		Ability to set the sort arrow to a particular column
	/// </summary>
	public class ListViewEx : System.Windows.Forms.ListView
	{
		/// <summary> Instance of list view header class. </summary>
		FListViewHeader ListViewHeader;

		/// <summary> Collection of string used later as a text for tooltip in column header. </summary>
		StringCollection _ColumnsTooltips = new StringCollection();

		protected override void OnHandleCreated( EventArgs e )
		{
			// Create a new ListViewHeader object
			ListViewHeader = new FListViewHeader( this );
			base.OnHandleCreated( e );
		}

		/// <summary> Sets the sort arrow to a particular column in the list view. </summary>
		/// <param name="ColumnToShowArrow"> Index of the column where the sort arrow will be displayed </param>
		public void SetSortArrow( int ColumnToShowArrow, bool bSortModeAscending )
		{
			if( ListViewHeader == null )
			{
				return;
			}

			IntPtr ColumnHeader = ListViewHeader.Handle;

			for( int ColumnIndex = 0; ColumnIndex < Columns.Count; ColumnIndex++ )
			{
				IntPtr ColumnPtr = new IntPtr( ColumnIndex );
				ListViewWin32.HDITEM ListViewColumn = new ListViewWin32.HDITEM();
				ListViewColumn.mask = ListViewWin32.HDI_FORMAT;
				ListViewWin32.SendMessageHeaderItem( ColumnHeader, ListViewWin32.HDM_GETITEM, ColumnPtr, ref ListViewColumn );

				bool bIsSortArrowDown = ( ( ListViewColumn.fmt & ListViewWin32.HDF_SORTDOWN ) == ListViewWin32.HDF_SORTDOWN );
				bool bIsSortArrowUp = ( ( ListViewColumn.fmt & ListViewWin32.HDF_SORTUP ) == ListViewWin32.HDF_SORTUP );

				// Change the sort arrow to opposite direction.
				if( ColumnToShowArrow == ColumnIndex )
				{
					if( bSortModeAscending )
					{
						ListViewColumn.fmt &= ~ListViewWin32.HDF_SORTDOWN;
						ListViewColumn.fmt |= ListViewWin32.HDF_SORTUP;
					}
					else
					{
						ListViewColumn.fmt &= ~ListViewWin32.HDF_SORTUP;
						ListViewColumn.fmt |= ListViewWin32.HDF_SORTDOWN;
					}
				}
				else
				{
					ListViewColumn.fmt &= ~ListViewWin32.HDF_SORTDOWN & ~ListViewWin32.HDF_SORTUP;
				}

				ListViewWin32.SendMessageHeaderItem( ColumnHeader, ListViewWin32.HDM_SETITEM, ColumnPtr, ref ListViewColumn );
			}
		}

		[Editor( "System.Windows.Forms.Design.StringCollectionEditor, System.Design, Version=2.0.0.0, Culture=neutral, PublicKeyToken=b03f5f7f11d50a3a", typeof( UITypeEditor ) )]
		[MergableProperty( false )]
		[DesignerSerializationVisibility( DesignerSerializationVisibility.Content )]
		[Localizable( true )]
		[
			Category( "Behavior" ),
			Description( "Strings for tooltips on the header columns. Number of items in this list must match number of columns" )
		]
		/// <summary> Strings for tooltips on the header columns. Number of items in this list must match number of columns. </summary>
		public StringCollection ColumnsTooltips
		{
			get
			{
				return _ColumnsTooltips;
			}
		}
	}

	/// <summary> 
	/// Class used to bypass default message processing of the header control used by the list-view control. 
	/// This version adds following extensions:
	///		Tooltips for column headers
	/// </summary>
	internal class FListViewHeader : NativeWindow
	{
		/// <summary> Reference to the parent list view control. </summary>
		ListViewEx Parent;

		/// <summary> Tooltip used by the header control. </summary>
		ToolTip HeaderToolTip = new ToolTip();

		/// <summary> Index of the header column currently pointed by the mouse. </summary>
		int CurrentColumnIndex = -1;

		/// <summary> Index of the header column previously pointed by the mouse. </summary>
		int LastColumnIndex = -1;

		/// <summary> Whether to show tooltips. </summary>
		bool bAllowTooltips = false;

		/// <summary> Default constructor. </summary>
		public FListViewHeader( ListViewEx InParent )
		{
			Parent = InParent;
			// Get the header control window.
			IntPtr HeaderWindow = ListViewWin32.SendMessage( Parent.Handle, ListViewWin32.LVM_GETHEADER, IntPtr.Zero, IntPtr.Zero );
			AssignHandle( HeaderWindow );

			// Only allow tooltips if data is valid.
			bAllowTooltips = Parent.ColumnsTooltips.Count == Parent.Columns.Count;

			// Assign tooltip to the parent control.
			HeaderToolTip.SetToolTip( Parent, "ListViewEx" );
		}

		/// <summary> Overridden window message processing. </summary>
		protected override void WndProc( ref Message Msg )
		{
			if( Msg.Msg == ListViewWin32.WM_MOUSEMOVE )
			{
				int PosX = ListViewWin32.GET_X_LPARAM( ( uint )Msg.LParam );
				int PosY = ListViewWin32.GET_Y_LPARAM( ( uint )Msg.LParam );

				int ColumnStart = 0;
				int ColumnEnd = 0;
				CurrentColumnIndex = -1;

				// Find on which column we should show the tooltip.
				for( int ColumnIndex = 0; ColumnIndex < Parent.Columns.Count; ColumnIndex++ )
				{
					int ColumnWidth = Parent.Columns[ ColumnIndex ].Width;
					ColumnEnd = ColumnStart + ColumnWidth;

					if( PosX >= ColumnStart && PosX < ColumnEnd )
					{
						CurrentColumnIndex = ColumnIndex;
						break;
					}

					ColumnStart = ColumnEnd;
				}

				if( CurrentColumnIndex == -1 )
				{
					HeaderToolTip.Hide( Parent );
				}
				else if( CurrentColumnIndex != LastColumnIndex )
				{
					ListViewWin32.TRACKMOUSEEVENT trackMouseEvent = new ListViewWin32.TRACKMOUSEEVENT(ListViewWin32.ETrackMouseEvent.TME_HOVER, Handle, ListViewWin32.HOVER_DEFAULT );
					ListViewWin32.TrackMouseEvent( ref trackMouseEvent );

					HeaderToolTip.Hide( Parent );
				}

				LastColumnIndex = CurrentColumnIndex;
			}
			else if( Msg.Msg == ListViewWin32.WM_MOUSELEAVE )
			{
				HeaderToolTip.Hide( Parent );
				LastColumnIndex = -1;
			}
			else if( Msg.Msg == ListViewWin32.WM_MOUSEHOVER )
			{
				if( bAllowTooltips && CurrentColumnIndex != -1 )
				{
					int PosX = ListViewWin32.GET_X_LPARAM( ( uint )Msg.LParam );
					int PosY = ListViewWin32.GET_Y_LPARAM( ( uint )Msg.LParam );

					// Show tooltip.
					HeaderToolTip.Show( Parent.ColumnsTooltips[ CurrentColumnIndex ], Parent, PosX, PosY + 32 );
				}
			}
			// Default processing.
			base.WndProc( ref Msg );
		}
	}

	/// <summary> Helper class with native Win32 functions, constants and structures. </summary>
	public class ListViewWin32
	{
		[DllImport( "user32.dll", EntryPoint = "SendMessage" )]
		public static extern IntPtr SendMessage( IntPtr hwnd, UInt32 wMsg, IntPtr wParam, IntPtr lParam );

		[DllImport( "user32.dll" )]
		[return: MarshalAs( UnmanagedType.Bool )]
		public static extern bool GetWindowRect( HandleRef hWnd, out RECT lpRect );

		[DllImport( "user32.dll", SetLastError = true )]
		[return: MarshalAs( UnmanagedType.Bool )]
		public static extern bool TrackMouseEvent( ref TRACKMOUSEEVENT lpEventTrack );

		[DllImport( "kernel32.dll", SetLastError = true )]
		public static extern int GetLastError();

		[System.Runtime.InteropServices.DllImport( "user32.dll", EntryPoint = "SendMessage" )]
		public static extern IntPtr SendMessageHeaderItem( IntPtr hWnd, UInt32 Msg, IntPtr wParam, ref HDITEM PtrHDITEM );

		/// <summary> Posted to a window when the cursor move. </summary>
		public const UInt32 WM_MOUSEMOVE = 0x200;

		/// <summary> Posted to a window when the cursor leaves the client area of the window. </summary>
		public const UInt32 WM_MOUSELEAVE = 0x2a3;

		/// <summary> Posted to a window when the cursor hovers over the client area of the window for the period of time specified in a prior call to TrackMouseEvent. </summary>
		public const UInt32 WM_MOUSEHOVER = 0x02A1;

		/// <summary> The system default hover time-out. </summary>
		public const UInt32 HOVER_DEFAULT = 0xFFFFFFFF;

		public const UInt32 LVM_FIRST = 0x1000;

		/// <summary> Gets the handle to the header control used by the list-view control. </summary>
		public const UInt32 LVM_GETHEADER = ( LVM_FIRST + 31 );

		/// <summary> The fmt member is valid. </summary>
		public const UInt32 HDI_FORMAT = 0x4;

		/// <summary> 
		/// Draws an up-arrow on this item. 
		/// This is typically used to indicate that information in the current window is sorted on this column in ascending order. 
		/// This flag cannot be combined with HDF_IMAGE or HDF_BITMAP. 
		/// </summary>
		public const Int32 HDF_SORTUP = 0x400;

		/// <summary> 
		/// Draws a down-arrow on this item. 
		/// This is typically used to indicate that information in the current window is sorted on this column in descending order. 
		/// This flag cannot be combined with HDF_IMAGE or HDF_BITMAP.
		/// </summary>
		public const Int32 HDF_SORTDOWN = 0x200;

		/// <summary> Gets information about an item in a header control. </summary>
		public const UInt32 HDM_GETITEM = 0x120b;

		/// <summary> Sets the attributes of the specified item in a header control.. </summary>
		public const UInt32 HDM_SETITEM = 0x120c;

		[StructLayout( LayoutKind.Sequential )]
		/// <summary> The RECT structure defines the coordinates of the upper-left and lower-right corners of a rectangle. </summary>
		public struct RECT
		{
			/// <summary> X position of upper-left corner. </summary>
			public int Left;
			/// <summary> Y position of upper-left corner. </summary>
			public int Top;
			/// <summary> X position of lower-right corner. </summary>
			public int Right;
			/// <summary> Y position of lower-right corner. </summary>
			public int Bottom;
		}

		/// <summary> Used by the TrackMouseEvent function to track when the mouse pointer leaves a window or hovers over a window for a specified amount of time. </summary>
		[StructLayout( LayoutKind.Sequential )]
		public struct TRACKMOUSEEVENT
		{
			/// <summary> The size of the TRACKMOUSEEVENT structure, in bytes. </summary>
			public UInt32 cbSize;

			/// <summary> The services requested. This member can be a combination of the following values. </summary>
			public UInt32 dwFlags;

			/// <summary> A handle to the window to track. </summary>
			public IntPtr hwndTrack;

			/// <summary> The hover time-out (if TME_HOVER was specified in dwFlags), in milliseconds. Can be HOVER_DEFAULT, which means to use the system default hover time-out. </summary>
			public UInt32 dwHoverTime;

			/// <summary> Constructor. </summary>
			public TRACKMOUSEEVENT( ETrackMouseEvent InFlags, IntPtr InHwnd, UInt32 InHoverTime )
			{
				cbSize = ( uint )Marshal.SizeOf( typeof( TRACKMOUSEEVENT ) );
				dwFlags = ( uint )InFlags;
				hwndTrack = InHwnd;
				dwHoverTime = InHoverTime;
			}
		}

		/// <summary> The services requested. This member can be a combination of the following values. </summary>
		[Flags]
		public enum ETrackMouseEvent : uint
		{
			/// <summary>
			/// The caller wants to cancel a prior tracking request. The caller should also specify the type of tracking that it wants to cancel. 
			/// For example, to cancel hover tracking, the caller must pass the TME_CANCEL and TME_HOVER flags.
			/// </summary>
			TME_CANCEL = 0x80000000,

			/// <summary>
			/// The caller wants hover notification. Notification is delivered as a WM_MOUSEHOVER message.
			/// If the caller requests hover tracking while hover tracking is already active, the hover timer will be reset.
			/// This flag is ignored if the mouse pointer is not over the specified window or area.
			/// </summary>
			TME_HOVER = 0x00000001,

			/// <summary>
			/// The caller wants leave notification. Notification is delivered as a WM_MOUSELEAVE message. 
			/// If the mouse is not over the specified window or area, a leave notification is generated immediately and no further tracking is performed.
			/// </summary>
			TME_LEAVE = 0x00000002,

			/// <summary>
			/// The caller wants hover and leave notification for the non client areas. Notification is delivered as WM_NCMOUSEHOVER and WM_NCMOUSELEAVE messages.
			/// </summary>
			TME_NONCLIENT = 0x00000010,

			/// <summary>
			/// The function fills in the structure instead of treating it as a tracking request. 
			/// The structure is filled such that had that structure been passed to TrackMouseEvent, it would generate the current tracking. 
			/// The only anomaly is that the hover time-out returned is always the actual time-out and not HOVER_DEFAULT, 
			/// if HOVER_DEFAULT was specified during the original TrackMouseEvent request. 
			/// </summary>
			TME_QUERY = 0x40000000,
		}

		[StructLayout( LayoutKind.Sequential )]
		/// <summary> Contains information about an item in a header control. This structure supersedes the HD_ITEM structure. </summary>
		public struct HDITEM
		{
			/// <summary> Flags indicating which other structure members contain valid data or must be filled in. </summary>
			public UInt32 mask;
			/// <summary> The width or height of the item. </summary>
			public Int32 cx;
			/// <summary> A pointer to an item string. </summary>
			public IntPtr pszText;
			/// <summary> A handle to the item bitmap. </summary>
			public IntPtr hbm;
			/// <summary> The length of the item string, in TCHARs. </summary>
			public Int32 cchTextMax;
			/// <summary> Flags that specify the item's format. </summary>
			public Int32 fmt;
			/// <summary> Application-defined item data. </summary>
			public IntPtr lParam;
			/// <summary> The zero-based index of an image within the image list. </summary>
			public Int32 iImage;
			/// <summary> The order in which the item appears within the header control, from left to right. </summary>
			public Int32 iOrder;
		}

		/// <summary> Retrieves the signed x-coordinate from the specified LPARAM value. </summary>
		public static int GET_X_LPARAM( uint Param )
		{
			return ( int )( Param & 0xffff );
		}

		/// <summary> Retrieves the signed y-coordinate from the specified LPARAM value. </summary>
		public static int GET_Y_LPARAM( uint Param )
		{
			return ( int )( ( Param >> 16 ) & 0xffff );
		}
	}
}