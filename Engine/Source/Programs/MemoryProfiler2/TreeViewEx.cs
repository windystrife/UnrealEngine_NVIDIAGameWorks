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
	/// Represents a Windows tree view control, which displays a collection of items in a tree.
	/// This version adds following extensions:
	///		Double buffered rendering
	/// </summary>
	public class TreeViewEx : System.Windows.Forms.TreeView
	{
		protected override void OnHandleCreated( EventArgs e )
		{
			TreeViewWin32.SendMessage( this.Handle, TreeViewWin32.TVM_SETEXTENDEDSTYLE, (IntPtr)TreeViewWin32.TVS_EX_DOUBLEBUFFER, (IntPtr)TreeViewWin32.TVS_EX_DOUBLEBUFFER );
			base.OnHandleCreated( e );
		}

		public void SetCheckBoxImageState(TreeNode InTreeNode, UInt32 InImageStateIndex)
		{
			var TreeViewItem = new TreeViewWin32.TVITEM();
			TreeViewItem.hItem = InTreeNode.Handle;
			TreeViewItem.mask = TreeViewWin32.TVIF_STATE;
			TreeViewItem.stateMask = TreeViewWin32.TVIS_STATEIMAGEMASK;
			TreeViewItem.state = InImageStateIndex << 12; // see INDEXTOSTATEIMAGEMASK
			TreeViewWin32.SendMessage( this.Handle, TreeViewWin32.TVM_SETITEM, IntPtr.Zero, ref TreeViewItem );
		}
	}

	/// <summary> Helper class with native Win32 functions, constants and structures. </summary>
	public class TreeViewWin32
	{
		[StructLayout(LayoutKind.Sequential, Pack = 8, CharSet = CharSet.Auto)]
		public struct TVITEM
		{
			public UInt32 mask;
			public IntPtr hItem;
			public UInt32 state;
			public UInt32 stateMask;
			[MarshalAs(UnmanagedType.LPTStr)]
			public string lpszText;
			public int cchTextMax;
			public int iImage;
			public int iSelectedImage;
			public int cChildren;
			public IntPtr lParam;
		}

		[DllImport( "user32.dll", EntryPoint = "SendMessage" )]
		public static extern IntPtr SendMessage( IntPtr hwnd, UInt32 wMsg, IntPtr wParam, IntPtr lParam );

		[DllImport( "user32.dll", EntryPoint = "SendMessage", CharSet = CharSet.Auto )]
		public static extern IntPtr SendMessage( IntPtr hWnd, UInt32 Msg, IntPtr wParam, ref TVITEM lParam );

		public const UInt32 TV_FIRST = 0x1100;

		/// <summary> Informs the tree-view control to set extended styles. </summary>
		public const UInt32 TVM_SETEXTENDEDSTYLE = TV_FIRST + 44;

		/// <summary> Sets some or all of a tree-view item's attributes. </summary>
		public const UInt32 TVM_SETITEM = TV_FIRST + 63;

		/// <summary> Specifies how the background is erased or filled. </summary>
		public const UInt32 TVS_EX_DOUBLEBUFFER = 0x0004;

		/// <summary> The state and stateMask members are valid. </summary>
		public const UInt32 TVIF_STATE = 0x8;

		/// <summary> Mask for the bits used to specify the item's state image index. </summary>
		public const UInt32 TVIS_STATEIMAGEMASK = 0xF000;
	}
}