// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Diagnostics;

namespace MemoryProfiler2
{
	/// <summary> 
	///	Memory bitmap parser
	/// Helper class used to parse memory profiler snapshots and displaying them in the graphical form.
	/// </summary>
	public static class FMemoryBitmapParser
	{
		/// <summary> Left margin for the memory bitmap image. </summary>
		private const int MEMORY_BITMAP_LEFT_MARGIN = 70;

		/// <summary> Bitmap which contains image of the memory bitmap. </summary>
		private static Bitmap MemoryBitmap;

		/// <summary> Amount of memory allocated for the current view of memory bitmap. </summary>
		private static long AllocatedMemorySize;

		/// <summary> Indicates how many bytes are displayed as a single pixel. </summary>
		private static int BytesPerPixel = 1;

		/// <summary> Selected pointer range types. </summary>
		enum ESelectionTypes
		{
			/// <summary> Selected pointer begins before the currently visible memory range. </summary>
			ST_PointerBeforeBase,

			/// <summary> Selected pointer is in range of the currently visible memory range. </summary>
			ST_PointerInRange,

			/// <summary> Selected pointer is outside range of the currently visible memory range. </summary>
			ST_PointerOutRange,

			/// <summary> Selected pointer ends after the currently visible memory range. </summary>
			ST_PointerAfterTop,
		}

		/// <summary> Start position of the currently selected memory region. </summary>
		private static Point SelectedMemoryStart;

		/// <summary> End position of the currently selected memory region. </summary>
		private static Point SelectedMemoryEnd;

		/// <summary> Address of the first allocated pointer. </summary>
		private static ulong MemoryBase;
		
		/// <summary> Size of the allocated memory for all memory pools. </summary>
		private static ulong MemorySize;


		/// <summary> True if we are selecting a memory region to zoom. </summary>
		private static bool bZoomSelectionInProgress;

		/// <summary> Start position of the currently selected memory region. </summary>
		private static int ZoomSelectionStartY;

		/// <summary> Start position of the currently selected memory region. </summary>
		private static int ZoomSelectionEndY;

		/// <summary> True if we are in a zoomed memory region. </summary>
		private static bool bZoomSelectionActive;

		/// <summary> Address of the first allocated pointer for currently zoomed memory region. </summary>
		private static ulong ZoomSelectionMemoryBase;

		/// <summary> Size of the allocated memory for for currently zoomed memory region. </summary>
		private static ulong ZoomSelectionMemorySize;

		/// <summary> Array of all zoom levels. </summary>
		private static List<FMemorySpace> ZoomLevels = new List<FMemorySpace>();


		private static Pen HeatPen = new Pen( Color.FromArgb( 10, 255, 255, 255 ) );

		private static Brush CheckerBWBrush4x4 = new HatchBrush( HatchStyle.LargeCheckerBoard, Color.FromArgb( 160, Color.Plum ), Color.FromArgb( 160, Color.SkyBlue ) );
		private static Brush CheckerBWBrush2x2 = new HatchBrush( HatchStyle.SmallCheckerBoard, Color.FromArgb( 192, Color.Plum ), Color.FromArgb( 192, Color.SkyBlue ) );
		private static Brush CheckerBWBrush1x1 = new HatchBrush( HatchStyle.Percent50, Color.FromArgb( 224, Color.Plum ), Color.FromArgb( 224, Color.SkyBlue ) );

		private static Pen OutlinePen1 = new Pen( Color.FromArgb( 128, Color.White ) );
		private static Pen OutlinePen2 = new Pen( Color.FromArgb( 192, Color.Black ) );

		/// <summary> Pen with 1x1 checker used to draw selected memory region. </summary>
		private static Pen CheckerPen1x1 = new Pen( CheckerBWBrush1x1 );

		/// <summary> Pen with 2x2 checker used to draw selected memory region. </summary>
		private static Pen CheckerPen2x2 = new Pen( CheckerBWBrush2x2 );

		/// <summary> Pen with 4x4 checker used to draw selected memory region. </summary>
		private static Pen CheckerPen4x4 = new Pen( CheckerBWBrush4x4 );

		private static Pen SelectedSolidPen = new Pen( new SolidBrush( Color.LightSteelBlue ) );

		/// <summary> Pen used to draw the selected memory region. Size of the checker is based on the selection height. </summary>
		private static Pen SelectedMemoryPen
		{
			get
			{
				int SelectionHeight = Math.Abs( SelectedMemoryStart.Y - SelectedMemoryEnd.Y );

				if( SelectionHeight < 8 )
				{
					return SelectedSolidPen/*CheckerPen1x1*/;
				}
				else if( SelectionHeight < 16 )
				{
					return CheckerPen2x2;
				}

				return CheckerPen4x4;
			}
		}

		/// <summary> Pen used to draw the zoom selection. Size of the checker is based on the selection height. </summary>
		private static Pen ZoomMemoryPen
		{
			get
			{
				int SelectionHeight = Math.Abs( ZoomSelectionEndY - ZoomSelectionStartY );

				if( SelectionHeight < 8 )
				{
					return CheckerPen1x1;
				}
				else if( SelectionHeight < 16 )
				{
					return CheckerPen2x2;
				}

				return CheckerPen4x4;
			}
		}

		/// <summary>
		/// Note that the magenta area at the bottom of the graph is where the 
		/// memory space ends. Because I chose to force a whole number of bytes per 
		/// pixel in the bitmap, there will almost always be a few pixels at the end 
		/// of the bitmap that fall outside the selected memory range. These are 
		/// coloured magenta.
		///  </summary>
		private static Pen OutOfSpacePen = new Pen( Color.DarkMagenta );

		/// <summary> Reference to the main memory profiler window. </summary>
		private static MainWindow OwnerWindow;

		/// <summary> Sets reference to the main memory profiler window. </summary>
		public static void SetProfilerWindow( MainWindow InMainWindow )
		{
			OwnerWindow = InMainWindow;
		}

		public static Bitmap ParseSnapshot( int BitmapWidth, int BitmapHeight, ulong SnapshotStreamIndex, ulong DiffbaseSnapshotStreamIndex, string FilterText )
		{
			// Progress bar.
			OwnerWindow.ToolStripProgressBar.Value = 0;
			OwnerWindow.ToolStripProgressBar.Visible = true;

			AllocatedMemorySize = 0;

			Bitmap MyBitmap = new Bitmap( BitmapWidth, BitmapHeight );
			Graphics MyGraphics = Graphics.FromImage( MyBitmap );

			int PixelCount = BitmapWidth * BitmapHeight;
			BytesPerPixel = ( int )Math.Ceiling( ( double )MemorySize / ( double )PixelCount );
			long BytesPerLine = ( long )BytesPerPixel * ( long )BitmapWidth;

			ulong MemoryTop = MemoryBase + MemorySize;

			// JarekS@TODO Multithreading
			if( OwnerWindow.MemoryBitmapHeatMapButton.Checked )
			{
				// clear bitmap to black
				MyGraphics.FillRectangle( Brushes.Black, 0, 0, BitmapWidth, BitmapHeight );

				if( DiffbaseSnapshotStreamIndex == FStreamInfo.INVALID_STREAM_INDEX )
				{
					// if there is no base snapshot, start at the beginning of the profile
					DiffbaseSnapshotStreamIndex = 0;
				}

				using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FMemoryBitmapParser.ParseSnapshot.HeatMap" ) )
				{
					long ProgressInterval = FStreamInfo.GlobalInstance.CallStackArray.Count / 20;
					long NextProgressUpdate = ProgressInterval;
					int CallStackCurrent = 0;

					foreach( FCallStack CallStack in FStreamInfo.GlobalInstance.CallStackArray )
					{
						// Update progress bar.
						if( CallStackCurrent >= NextProgressUpdate )
						{
							OwnerWindow.ToolStripProgressBar.PerformStep();
							NextProgressUpdate += ProgressInterval;
							Debug.WriteLine( "FMemoryBitmapParser.ParseSnapshot.HeatMap " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
						}
						CallStackCurrent++;

						if( CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, OwnerWindow.IsFilteringIn(), OwnerWindow.SelectedMemoryPool ) )
						{
							foreach( FAllocationLifecycle AllocLifecycle in CallStack.CompleteLifecycles )
							{
								if( AllocLifecycle.AllocEvent.StreamIndex < SnapshotStreamIndex )
								{
									if( AllocLifecycle.AllocEvent.StreamIndex > DiffbaseSnapshotStreamIndex
										&& AllocLifecycle.AllocEvent.Pointer + ( ulong )AllocLifecycle.AllocEvent.Size > MemoryBase
										&& AllocLifecycle.AllocEvent.Pointer < MemoryTop )
									{
										FillMemoryArea( MyGraphics, HeatPen, BitmapWidth, BytesPerLine, BytesPerPixel, AllocLifecycle.AllocEvent.Pointer - MemoryBase, AllocLifecycle.AllocEvent.Size );
									}

									if( AllocLifecycle.ReallocsEvents != null )
									{
										for( int i = 0; i < AllocLifecycle.ReallocsEvents.Count; i++ )
										{
											FReallocationEvent ReallocEvent = AllocLifecycle.ReallocsEvents[ i ];
											if( ReallocEvent.StreamIndex < SnapshotStreamIndex )
											{
												if( ReallocEvent.StreamIndex > DiffbaseSnapshotStreamIndex
													&& ReallocEvent.NewPointer + ( ulong )ReallocEvent.NewSize > MemoryBase
													&& ReallocEvent.NewPointer < MemoryTop )
												{
													FillMemoryArea( MyGraphics, HeatPen, BitmapWidth, BytesPerLine, BytesPerPixel, ReallocEvent.NewPointer - MemoryBase, ReallocEvent.NewSize );
												}
											}
											else
											{
												break;
											}
										}
									}
								}
							}
						}
					}
				}
			}
			else
			{
				bool bDiffAllocs = DiffbaseSnapshotStreamIndex != FStreamInfo.INVALID_STREAM_INDEX;
				long ProgressIntervalCallstack = FStreamInfo.GlobalInstance.CallStackArray.Count / ( bDiffAllocs ? 5 : 10 );
				long ProgressIntervalGroup = OwnerWindow.Options.ClassGroups.Count / 10;
				long NextProgressUpdate = ProgressIntervalCallstack;
				int CallStackCurrent = 0;

				using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FMemoryBitmapParser.ParseSnapshot.AllAllocations" ) )
				{
					// render all allocations in black
					foreach( FCallStack CallStack in FStreamInfo.GlobalInstance.CallStackArray )
					{
						// Update progress bar.
						if( CallStackCurrent >= NextProgressUpdate )
						{
							OwnerWindow.ToolStripProgressBar.PerformStep();
							NextProgressUpdate += ProgressIntervalCallstack;
							Debug.WriteLine( "FMemoryBitmapParser.ParseSnapshot " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
						}
						CallStackCurrent++;

						if( CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, OwnerWindow.IsFilteringIn(), OwnerWindow.SelectedMemoryPool ) )
						{
							// Process complete life cycles.
							foreach( FAllocationLifecycle AllocLifecycle in CallStack.CompleteLifecycles )
							{
								ProcessLifecycle( Pens.Black, BitmapWidth, SnapshotStreamIndex, MyGraphics, BytesPerLine, AllocLifecycle, 1 );
							}
							// Process incomplete life cycles.
							foreach( KeyValuePair<ulong, FAllocationLifecycle> AllocLifecycle in CallStack.IncompleteLifecycles )
							{
								ProcessLifecycle( Pens.Black, BitmapWidth, SnapshotStreamIndex, MyGraphics, BytesPerLine, AllocLifecycle.Value, 1 );
							}
						}
					}
				}

				using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FMemoryBitmapParser.ParseSnapshot.HistogramAllocations" ) )
				{
					int CurrentGroup = 0;
					long NextGroupProgressUpdate = ProgressIntervalGroup;
					// render histogram allocations in colour
					foreach( ClassGroup CallStackGroup in OwnerWindow.Options.ClassGroups )
					{
						// Update progress bar.
						if( CurrentGroup >= NextGroupProgressUpdate )
						{
							OwnerWindow.ToolStripProgressBar.PerformStep();
							NextGroupProgressUpdate += ProgressIntervalGroup;
							Debug.WriteLine( "FMemoryBitmapParser.ParseSnapshot " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
						}
						CurrentGroup++;

						Pen GroupPen = new Pen( CallStackGroup.Color );

						foreach( CallStackPattern CallStackPatternIt in CallStackGroup.CallStackPatterns )
						{
							foreach( FCallStack CallStack in CallStackPatternIt.GetCallStacks() )
							{
								if( CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, OwnerWindow.IsFilteringIn(), OwnerWindow.SelectedMemoryPool ) )
								{
									foreach( FAllocationLifecycle AllocLifecycle in CallStack.CompleteLifecycles )
									{
										ProcessLifecycle( GroupPen, BitmapWidth, SnapshotStreamIndex, MyGraphics, BytesPerLine, AllocLifecycle, 0 );
									}

									foreach (KeyValuePair<ulong, FAllocationLifecycle> AllocLifecycle in CallStack.IncompleteLifecycles)
									{
										ProcessLifecycle( GroupPen, BitmapWidth, SnapshotStreamIndex, MyGraphics, BytesPerLine, AllocLifecycle.Value, 0 );
									}
								}
							}
						}

						GroupPen.Dispose();
					}
				}

				using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FMemoryBitmapParser.ParseSnapshot.DiffAllocations" ) )
				{
					CallStackCurrent = 0;
					NextProgressUpdate = ProgressIntervalCallstack;

					// render white where allocation exists in diff-base snapshot
					if( DiffbaseSnapshotStreamIndex != FStreamInfo.INVALID_STREAM_INDEX )
					{
						foreach( FCallStack CallStack in FStreamInfo.GlobalInstance.CallStackArray )
						{
							// Update progress bar.
							if( CallStackCurrent >= NextProgressUpdate )
							{
								OwnerWindow.ToolStripProgressBar.PerformStep();
								NextProgressUpdate += ProgressIntervalCallstack;
								Debug.WriteLine( "FMemoryBitmapParser.ParseSnapshot " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
							}
							CallStackCurrent++;

							if( CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, OwnerWindow.IsFilteringIn(), OwnerWindow.SelectedMemoryPool ) )
							{
								foreach( FAllocationLifecycle AllocLifecycle in CallStack.CompleteLifecycles )
								{
									ProcessLifecycle( Pens.AliceBlue, BitmapWidth, DiffbaseSnapshotStreamIndex, MyGraphics, BytesPerLine, AllocLifecycle, -1 );
								}

								foreach( KeyValuePair<ulong, FAllocationLifecycle> AllocLifecycle in CallStack.IncompleteLifecycles )
								{
									ProcessLifecycle( Pens.AliceBlue, BitmapWidth, DiffbaseSnapshotStreamIndex, MyGraphics, BytesPerLine, AllocLifecycle.Value, -1 );
								}
							}
						}
					}
				}
			}

			// shade space at bottom of image to mark end of memory space.
			ulong MemoryTopPointer = MemorySize;
			int StartX = ( int )( MemoryTopPointer % ( uint )BytesPerLine / ( uint )BytesPerPixel );
			int StartY = ( int )( MemoryTopPointer / ( uint )BytesPerLine );
			MyGraphics.DrawLine( OutOfSpacePen, StartX, StartY, BitmapWidth - 1, StartY );
			if( StartY + 1 < BitmapHeight )
			{
				MyGraphics.FillRectangle( OutOfSpacePen.Brush, 0, StartY + 1, BitmapWidth, BitmapHeight - ( StartY + 1 ) );
			}

			MyGraphics.Dispose();

			OwnerWindow.ToolStripProgressBar.Visible = false;

			return MyBitmap;
		}

		private static void ProcessLifecycle( Pen AreaPen, int BitmapWidth, ulong SnapshotStreamIndex, Graphics MyGraphics, long BytesPerLine, FAllocationLifecycle AllocLifecycle, int MemorySizeAdjustment )
		{
			uint Size = 0;
			ulong Pointer = AllocLifecycle.GetPointerAtStreamIndex( SnapshotStreamIndex, out Size );

			if( Pointer != 0 && Pointer < MemoryBase + MemorySize && Pointer + Size >= MemoryBase )
			{
				if( Pointer < MemoryBase )
				{
					Size -= ( uint )( MemoryBase - Pointer );
					Pointer = MemoryBase;
				}

				if( Pointer + Size > MemoryBase + MemorySize )
				{
					Size = ( uint )( MemoryBase + MemorySize - Pointer );
				}

				FillMemoryArea( MyGraphics, AreaPen, BitmapWidth, BytesPerLine, BytesPerPixel, Pointer - MemoryBase, ( int )Size );

				AllocatedMemorySize += MemorySizeAdjustment * Size;
			}
		}

		public static ulong GetPointerFromPixel( int BitmapWidth, int BytesPerPixel, int X, int Y )
		{
			long BytesPerLine = ( long )BytesPerPixel * ( long )BitmapWidth;
			Debug.Assert( BytesPerLine > 0 );

			return MemoryBase + ( ulong )Y * ( ulong )BytesPerLine + ( ulong )X * ( ulong )BytesPerPixel;
		}

		// Not used.
		public static Point GetPixelFromPointer( int BitmapWidth, int BitmapHeight, int BytesPerPixel, ulong Pointer )
		{
			long BytesPerLine = ( long )BytesPerPixel * ( long )BitmapWidth;

			bool bPointerBeforeBase = Pointer < MemoryBase;

			Pointer = Pointer < MemoryBase ? 0 : Pointer - MemoryBase;

			Point Calculated = new Point( ( int )( Pointer % ( ulong )BytesPerLine / ( ulong )BytesPerPixel ), ( int )( Pointer / ( ulong )BytesPerLine ) );

			Calculated.Y = Math.Min( Calculated.Y, BitmapHeight-1 );

			return Calculated;
		}

		public static void GetPixelRangeFromPointer( int BitmapWidth, int BitmapHeight, int BytesPerPixel, ulong Pointer, ulong PointerSize )
		{
			ulong BytesPerLine = ( ulong )BytesPerPixel * ( ulong )BitmapWidth;
			ulong MemoryTop = MemoryBase + MemorySize;

			ulong PointerBase = Pointer - MemoryBase;
			ulong PointerTop = Pointer + PointerSize - MemoryBase;

			bool bPointerBeforeBase = Pointer < MemoryBase;
			bool bPointerAfterTop = Pointer + PointerSize > MemoryTop;

			SelectedMemoryStart = new Point( ( int )( PointerBase % BytesPerLine / ( ulong )BytesPerPixel ), ( int )( PointerBase / BytesPerLine ) );
			SelectedMemoryStart.X += MEMORY_BITMAP_LEFT_MARGIN;

			SelectedMemoryEnd = new Point( ( int )( PointerTop % BytesPerLine / ( ulong )BytesPerPixel ), ( int )( PointerTop / BytesPerLine ) );
			SelectedMemoryEnd.X += MEMORY_BITMAP_LEFT_MARGIN;

			if( bPointerBeforeBase )
			{
				SelectedMemoryStart.Y = 0;
			}

			if( bPointerAfterTop )
			{
				SelectedMemoryEnd.Y = BitmapHeight - 1;
			}

			OwnerWindow.MemoryBitmapResetSelectionButton.Enabled = true;
		}

		private static void FillMemoryArea( Graphics graphics, Pen pen, int BitmapWidth, long BytesPerLine, int BytesPerPixel, ulong Pointer, long Size )
		{
			int StartX = ( int )( Pointer % ( ulong )BytesPerLine / ( ulong )BytesPerPixel );
			int StartY = ( int )( Pointer / ( ulong )BytesPerLine );
			int EndX = ( int )( ( Pointer + ( ulong )Size ) % ( ulong )BytesPerLine / ( ulong )BytesPerPixel );
			int EndY = ( int )( ( Pointer + ( ulong )Size ) / ( ulong )BytesPerLine );

			FillMemoryArea( graphics, pen, BitmapWidth, StartX, StartY, EndX, EndY, 0 );
		}

		public static void FillMemoryArea( Graphics graphics, Pen pen, int BitmapWidth, int StartX, int StartY, int EndX, int EndY, int MarginWidth )
		{
			Debug.Assert( StartY >= 0 );
			Debug.Assert( EndY >= 0 );

			if( StartY == EndY )
			{
				graphics.DrawLine( pen, StartX, StartY, EndX, EndY );
			}
			else
			{
				graphics.DrawLine( pen, StartX, StartY, MarginWidth + BitmapWidth - 1, StartY );

				if( StartY + 1 < EndY )
				{
					graphics.FillRectangle( pen.Brush, MarginWidth, StartY + 1, BitmapWidth, EndY - ( StartY + 1 ) );
				}

				graphics.DrawLine( pen, MarginWidth, EndY, EndX, EndY );
			}
		}

		public static void UndoLastZoom()
		{
			// button should be unclickable if this isn't the case
			Debug.Assert( ZoomLevels.Count > 0 );

			if( ZoomLevels.Count > 1 )
			{
				ZoomSelectionMemoryBase = ZoomLevels[ ZoomLevels.Count - 2 ].Base;
				ZoomSelectionMemorySize = ZoomLevels[ ZoomLevels.Count - 2 ].Size;
				bZoomSelectionActive = true;

				ZoomLevels.RemoveAt( ZoomLevels.Count - 1 );
			}
			else
			{
				bZoomSelectionActive = false;
				ZoomLevels.Clear();
			}

			if( ZoomLevels.Count == 0 )
			{
				OwnerWindow.MemoryBitmapUndoZoomButton.Enabled = false;
				OwnerWindow.MemoryBitmapResetButton.Enabled = false;
			}

			RefreshMemoryBitmap();
		}

		public static void ResetZoom()
		{
			bZoomSelectionActive = false;
			ZoomLevels.Clear();
			OwnerWindow.MemoryBitmapUndoZoomButton.Enabled = false;
			OwnerWindow.MemoryBitmapResetButton.Enabled = false;

			RefreshMemoryBitmap();
		}

		public static void ResetSelection()
		{
			OwnerWindow.MemoryBitmapCallStackListView.Items.Clear();
			OwnerWindow.MemoryBitmapAllocationHistoryListView.SelectedItems.Clear();
			OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Clear();

			OwnerWindow.MemoryBitmapResetSelectionButton.Enabled = false;

			OwnerWindow.MemoryBitmapMemorySpaceLabel.Text = "";
			OwnerWindow.MemoryBitmapSpaceSizeLabel.Text = "";
			OwnerWindow.MemoryBitmapAllocatedMemoryLabel.Text = "";

			// Refresh panel to clear selection.
			OwnerWindow.MemoryBitmapPanel.Invalidate();
		}

		public static void RefreshMemoryBitmap()
		{
            if ( OwnerWindow == null || OwnerWindow.CurrentSnapshot == null || !FStreamInfo.GlobalInstance.CreationOptions.KeepLifecyclesCheckBox.Checked || OwnerWindow.WindowState == FormWindowState.Minimized )
			{
				return;
			}

			if( MemoryBitmap != null )
			{
				MemoryBitmap.Dispose();
				MemoryBitmap = null;
			}

			if( bZoomSelectionActive )
			{
				MemoryBase = ZoomSelectionMemoryBase;
				MemorySize = ZoomSelectionMemorySize;
			}
			else
			{
				MemoryBase = ulong.MaxValue;
				ulong MemoryTop = ulong.MinValue;

				bool bAtLeastOnePoolSelected = false;
				bool bAtLeastOneAllocation = false;
				foreach( EMemoryPool MemoryPool in FMemoryPoolInfo.GetMemoryPoolEnumerable() )
				{
					if( ( ( OwnerWindow.SelectedMemoryPool & MemoryPool ) != EMemoryPool.MEMPOOL_None )
							== OwnerWindow.IsFilteringIn() )
					{
						bAtLeastOnePoolSelected = true;

						FMemoryPoolInfo MemPoolInfo = FStreamInfo.GlobalInstance.MemoryPoolInfo[ MemoryPool ];

						if( !MemPoolInfo.IsEmpty )
						{
							bAtLeastOneAllocation = true;

							MemoryBase = Math.Min( MemPoolInfo.MemoryBottom, MemoryBase );
							MemoryTop = Math.Max( MemPoolInfo.MemoryTop, MemoryTop );
						}
					}
				}

				if( !bAtLeastOnePoolSelected )
				{
					MessageBox.Show( "All memory pools are being filtered out. No data to display." );
					return;
				}

				if( !bAtLeastOneAllocation )
				{
					MessageBox.Show( "The selected memory pools were never allocated to in this profiling run." );
					return;
				}

				MemorySize = MemoryTop - MemoryBase;
			}

			ulong DiffbaseSnapshotStreamIndex = FStreamInfo.INVALID_STREAM_INDEX;
			ulong CurrentSnapshotStreamIndex = OwnerWindow.CurrentSnapshot.StreamIndex;
			if( OwnerWindow.CurrentSnapshot.bIsDiffResult )
			{
				if( OwnerWindow.DiffStartComboBox.SelectedIndex == 0 )
				{
					DiffbaseSnapshotStreamIndex = 0;
				}
				else
				{
					DiffbaseSnapshotStreamIndex = FStreamInfo.GlobalInstance.SnapshotList[ OwnerWindow.DiffStartComboBox.SelectedIndex - 1 ].StreamIndex;
				}

				if( OwnerWindow.DiffEndComboBox.SelectedIndex == 0 )
				{
					CurrentSnapshotStreamIndex = 0;
				}
				else
				{
					CurrentSnapshotStreamIndex = FStreamInfo.GlobalInstance.SnapshotList[ OwnerWindow.DiffEndComboBox.SelectedIndex - 1 ].StreamIndex;
				}
			}

			OwnerWindow.SetWaitCursor( true );
			MemoryBitmap = ParseSnapshot( OwnerWindow.MemoryBitmapPanel.ClientRectangle.Width - MEMORY_BITMAP_LEFT_MARGIN, OwnerWindow.MemoryBitmapPanel.ClientRectangle.Height, CurrentSnapshotStreamIndex, DiffbaseSnapshotStreamIndex, OwnerWindow.FilterTextBox.Text );
			OwnerWindow.SetWaitCursor( false );

			if( MemoryBitmap != null )
			{
				OwnerWindow.MemoryBitmapMemorySpaceLabel.Text = "0x" + MemoryBase.ToString( "x16" ) + " - 0x" + ( MemoryBase + MemorySize - 1 ).ToString( "x16" );
				OwnerWindow.MemoryBitmapSpaceSizeLabel.Text = MainWindow.FormatSizeString( ( long )MemorySize );
				OwnerWindow.MemoryBitmapAllocatedMemoryLabel.Text = MainWindow.FormatSizeString( AllocatedMemorySize );
			}
			else
			{
				OwnerWindow.MemoryBitmapMemorySpaceLabel.Text = "0x00000000 - 0x00000000";
				OwnerWindow.MemoryBitmapSpaceSizeLabel.Text = "0 bytes";
				OwnerWindow.MemoryBitmapAllocatedMemoryLabel.Text = "0 bytes";
			}

			OwnerWindow.MemoryBitmapBytesPerPixelLabel.Text = BytesPerPixel.ToString();

			OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Clear();
			OwnerWindow.MemoryBitmapCallStackListView.Items.Clear();
			OwnerWindow.MemoryBitmapPanel.Invalidate();
		}

		public static void ClearView()
		{
			if( MemoryBitmap != null )
			{
				MemoryBitmap.Dispose();
				MemoryBitmap = null;
			}

			OwnerWindow.MemoryBitmapCallStackListView.BeginUpdate();
			OwnerWindow.MemoryBitmapCallStackListView.SelectedItems.Clear();
			OwnerWindow.MemoryBitmapCallStackListView.Items.Clear();
			OwnerWindow.MemoryBitmapCallStackListView.EndUpdate();

			OwnerWindow.MemoryBitmapAllocationHistoryListView.BeginUpdate();
			OwnerWindow.MemoryBitmapAllocationHistoryListView.SelectedItems.Clear();
			OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Clear();
			OwnerWindow.MemoryBitmapAllocationHistoryListView.EndUpdate();
		}

		public static void PaintPanel( PaintEventArgs e )
		{
			if( MemoryBitmap != null )
			{
				e.Graphics.DrawImageUnscaled( MemoryBitmap, MEMORY_BITMAP_LEFT_MARGIN, 0 );
				e.Graphics.PixelOffsetMode = PixelOffsetMode.None;

				// Draw overlay for the currently selected memory region.
				if( OwnerWindow.MemoryBitmapAllocationHistoryListView.SelectedItems.Count > 0 )
				{
					FillMemoryArea( e.Graphics, SelectedMemoryPen, MemoryBitmap.Width,
						SelectedMemoryStart.X, SelectedMemoryStart.Y,
						SelectedMemoryEnd.X, SelectedMemoryEnd.Y, MEMORY_BITMAP_LEFT_MARGIN );

					// Draw border around the region to make that region more visible.
					if( SelectedMemoryStart.Y == SelectedMemoryEnd.Y )
					{
						e.Graphics.DrawRectangle( OutlinePen1, SelectedMemoryStart.X - 4, SelectedMemoryStart.Y - 4, SelectedMemoryEnd.X - SelectedMemoryStart.X + 8, 8 );
						e.Graphics.DrawRectangle( OutlinePen2, SelectedMemoryStart.X - 5, SelectedMemoryStart.Y - 5, SelectedMemoryEnd.X - SelectedMemoryStart.X + 10, 10 );
					}
					/*else if( SelectedMemoryEnd.Y - SelectedMemoryStart.Y <= 3 )
					{
						int SelHeight = SelectedMemoryEnd.Y - SelectedMemoryStart.Y;
						e.Graphics.DrawRectangle( OutlinePen1, MEMORY_BITMAP_LEFT_MARGIN, SelectedMemoryStart.Y - 4, MemoryBitmap.Width-2, SelHeight + 8 );
						e.Graphics.DrawRectangle( OutlinePen2, MEMORY_BITMAP_LEFT_MARGIN, SelectedMemoryStart.Y - 5, MemoryBitmap.Width-1, SelHeight + 10 );
					}*/
				}

				if( bZoomSelectionInProgress )
				{
					int Height = Math.Abs( ZoomSelectionEndY - ZoomSelectionStartY ) + 1;
					int Top = Math.Min( ZoomSelectionStartY, ZoomSelectionEndY );

					e.Graphics.FillRectangle( ZoomMemoryPen.Brush, MEMORY_BITMAP_LEFT_MARGIN, Top, MemoryBitmap.Width, Height );
				}

				long BytesPerLine = ( long )BytesPerPixel * ( long )MemoryBitmap.Width;
				float AxisHeight = ( long )BytesPerLine * ( long )MemoryBitmap.Height;

				string YAxisLabel;
				long MajorTick;
				MainWindow.SetUpAxisLabel( 1, ref AxisHeight, out YAxisLabel, out MajorTick );

				// make sure that minorTick divides into majorTick without remainder
				MajorTick = MajorTick / 4 * 4;
				long MinorTick = MajorTick / 4;

				OwnerWindow.DrawYAxis( e.Graphics, Pens.Black, Color.Black, MajorTick, 9, MinorTick, 5, MEMORY_BITMAP_LEFT_MARGIN - 2, 0, MemoryBitmap.Height, 0, MemoryBitmap.Height, YAxisLabel, AxisHeight, true, true );
			}
		}

		

		public static void UnsafeBitmapClick( MouseEventArgs e )
		{
			OwnerWindow.MemoryBitmapCallStackListView.Items.Clear();

			OwnerWindow.MemoryBitmapAllocationHistoryListView.BeginUpdate();
			OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Clear();

			ulong PointerFromPixel = FMemoryBitmapParser.GetPointerFromPixel( MemoryBitmap.Width, BytesPerPixel, e.X - MEMORY_BITMAP_LEFT_MARGIN, e.Y );
			string FilterText = OwnerWindow.FilterTextBox.Text.ToUpperInvariant();

			using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FMemoryBitmapParser.UnsafeBitmapClick" ) )
			{
				foreach( FCallStack CallStack in FStreamInfo.GlobalInstance.CallStackArray )
				{
					if( CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, OwnerWindow.IsFilteringIn(), OwnerWindow.SelectedMemoryPool ) )
					{
						foreach( FAllocationLifecycle AllocLifecycle in CallStack.CompleteLifecycles )
						{
							ProcessLifecycleForPixel( PointerFromPixel, CallStack, AllocLifecycle, true );
						}

						foreach( KeyValuePair<ulong, FAllocationLifecycle> AllocLifecycle in CallStack.IncompleteLifecycles )
						{
							ProcessLifecycleForPixel( PointerFromPixel, CallStack, AllocLifecycle.Value, false );
						}
					}
				}
			}

			// Pointers that were malloced and then realloced by a different callstack will have missing end frames
			// (marked with "-1"), but those end frames are guaranteed to be the same as the start frame of the
			// following allocation at this pointer, so we can just go over the list and fix up the references.
			for( int ItemIndex = 0; ItemIndex < OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Count; ItemIndex++ )
			{
				if( OwnerWindow.MemoryBitmapAllocationHistoryListView.Items[ ItemIndex ].SubItems.Count > 0
					&& OwnerWindow.MemoryBitmapAllocationHistoryListView.Items[ ItemIndex ].SubItems[ 1 ].Text == "-1"
					&& ItemIndex + 1 < OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Count )
				{
					OwnerWindow.MemoryBitmapAllocationHistoryListView.Items[ ItemIndex ].SubItems[ 1 ].Text = OwnerWindow.MemoryBitmapAllocationHistoryListView.Items[ ItemIndex + 1 ].Text;
				}
			}

			OwnerWindow.MemoryBitmapAllocationHistoryListView.EndUpdate();

			if( OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Count > 0 )
			{
				OwnerWindow.MemoryBitmapAllocationHistoryListView.Items[ GetMemoryBitmapActiveAllocationForStreamIndex( OwnerWindow.CurrentSnapshot.StreamIndex ) ].Selected = true;
			}
			else
			{
				// refresh panel to clear selection
				OwnerWindow.MemoryBitmapPanel.Invalidate();
			}
		}

		private static void ProcessLifecycleForPixel( ulong PointerFromPixel, FCallStack CallStack, FAllocationLifecycle AllocLifecycle, bool bUpdateEndFrame )
		{
			int StartFrame = 1;
			int EndFrame = 1;
			bool bAllocated = false;
			ulong AllocatedPointer = 0;
			int AllocatedSize = 0;
			ulong AllocatedStreamIndex = FStreamInfo.INVALID_STREAM_INDEX;

			if( AllocLifecycle.AllocEvent.Pointer <= PointerFromPixel
				&& AllocLifecycle.AllocEvent.Pointer + ( uint )AllocLifecycle.AllocEvent.Size > PointerFromPixel )
			{
				bAllocated = true;
				AllocatedPointer = AllocLifecycle.AllocEvent.Pointer;
				AllocatedSize = AllocLifecycle.AllocEvent.Size;
				AllocatedStreamIndex = AllocLifecycle.AllocEvent.StreamIndex;

				StartFrame = FStreamInfo.GlobalInstance.GetFrameNumberFromStreamIndex( EndFrame, AllocLifecycle.AllocEvent.StreamIndex );
			}

			if( AllocLifecycle.ReallocsEvents != null )
			{
				for( int EventIndex = 0; EventIndex < AllocLifecycle.ReallocsEvents.Count; EventIndex++ )
				{
					if( bAllocated )
					{
						if( AllocLifecycle.ReallocsEvents[ EventIndex ].NewPointer > PointerFromPixel
							|| AllocLifecycle.ReallocsEvents[ EventIndex ].NewPointer + ( uint )AllocLifecycle.ReallocsEvents[ EventIndex ].NewSize < PointerFromPixel )
						{
							bAllocated = false;
							EndFrame = FStreamInfo.GlobalInstance.GetFrameNumberFromStreamIndex( StartFrame, AllocLifecycle.ReallocsEvents[ EventIndex ].StreamIndex );

							InsertItemIntoMemoryBitmapListView( CallStack, AllocatedStreamIndex, StartFrame, EndFrame, AllocatedPointer, AllocatedSize );
						}
					}
					else if( AllocLifecycle.ReallocsEvents[ EventIndex ].NewPointer <= PointerFromPixel
							&& AllocLifecycle.ReallocsEvents[ EventIndex ].NewPointer + ( uint )AllocLifecycle.ReallocsEvents[ EventIndex ].NewSize > PointerFromPixel )
					{
						bAllocated = true;
						AllocatedPointer = AllocLifecycle.ReallocsEvents[ EventIndex ].NewPointer;
						AllocatedSize = AllocLifecycle.ReallocsEvents[ EventIndex ].NewSize;
						AllocatedStreamIndex = AllocLifecycle.ReallocsEvents[ EventIndex ].StreamIndex;

						StartFrame = FStreamInfo.GlobalInstance.GetFrameNumberFromStreamIndex( EndFrame, AllocLifecycle.ReallocsEvents[ EventIndex ].StreamIndex );
					}
				}
			}

			if( bAllocated )
			{
				if( bUpdateEndFrame )
				{
					EndFrame = AllocLifecycle.FreeStreamIndex == FStreamInfo.INVALID_STREAM_INDEX
							? -1
							: FStreamInfo.GlobalInstance.GetFrameNumberFromStreamIndex( StartFrame, AllocLifecycle.FreeStreamIndex );
				}
				else
				{
					EndFrame = 0;
				}

				InsertItemIntoMemoryBitmapListView( CallStack, AllocatedStreamIndex, StartFrame, EndFrame, AllocatedPointer, AllocatedSize );
			}
		}

		public static void BitmapClick( MouseEventArgs e )
		{
			if( MemoryBitmap == null || !FStreamInfo.GlobalInstance.CreationOptions.KeepLifecyclesCheckBox.Checked )
			{
				return;
			}

#if !DEBUG
			try
#endif // !DEBUG
			{
				OwnerWindow.SetWaitCursor( true );
				UnsafeBitmapClick( e );
				OwnerWindow.SetWaitCursor( false );
			}
#if !DEBUG
			catch( Exception ex )
			{
				Console.WriteLine( "Problem picking pixel " + ex.Message );
			}
#endif // !DEBUG
		}

		public static void BitmapMouseDown( MouseEventArgs MouseEvents )
		{
			if( OwnerWindow.MemoryBitmapZoomRowsButton.Checked && MouseEvents.Button == MouseButtons.Left && FStreamInfo.GlobalInstance != null )
			{
				ZoomSelectionStartY = MouseEvents.Y;
				bZoomSelectionInProgress = true;
			}
		}

		public static void BitmapMouseMove( MouseEventArgs MouseEvents )
		{
			if( OwnerWindow.MemoryBitmapZoomRowsButton.Checked && MouseEvents.Button == MouseButtons.Left )
			{
				ZoomSelectionEndY = MouseEvents.Y;
				OwnerWindow.MemoryBitmapPanel.Invalidate();
			}
		}

		public static void BitmapMouseUp( MouseEventArgs MouseEvents )
		{
			if( OwnerWindow.MemoryBitmapZoomRowsButton.Checked && MouseEvents.Button == MouseButtons.Left && FStreamInfo.GlobalInstance != null )
			{
				bZoomSelectionInProgress = false;
				OwnerWindow.MemoryBitmapZoomRowsButton.Checked = false;

				ZoomSelectionStartY = Math.Max( ZoomSelectionStartY, 0 );
				ZoomSelectionEndY = Math.Max( ZoomSelectionEndY, 0 );

				int Top = Math.Min( ZoomSelectionStartY, ZoomSelectionEndY );
				int Height = Math.Abs( ZoomSelectionEndY - ZoomSelectionStartY ) + 1;
				// believe it or not, bytesPerLine easily exceeds 2GB in a 64-bit memory space
				long BytesPerLine = ( long )BytesPerPixel * ( long )MemoryBitmap.Width;
				Debug.Assert( BytesPerLine > 0 );

				ZoomSelectionMemoryBase = FMemoryBitmapParser.GetPointerFromPixel( MemoryBitmap.Width, BytesPerPixel, 0, Top );
				ZoomSelectionMemorySize = ( ulong )( Height * BytesPerLine );
				bZoomSelectionActive = true;

				ZoomLevels.Add( new FMemorySpace( ZoomSelectionMemoryBase, ZoomSelectionMemorySize ) );
				OwnerWindow.MemoryBitmapUndoZoomButton.Enabled = true;
				OwnerWindow.MemoryBitmapResetButton.Enabled = true;

				RefreshMemoryBitmap();
			}
		}

		public static void UpdateAllocationHistory()
		{
			OwnerWindow.MemoryBitmapCallStackListView.BeginUpdate();
			OwnerWindow.MemoryBitmapCallStackListView.Items.Clear();

			if( OwnerWindow.MemoryBitmapAllocationHistoryListView.SelectedItems.Count > 0 )
			{
				FCallStackTag CallStackTag = ( FCallStackTag )OwnerWindow.MemoryBitmapAllocationHistoryListView.SelectedItems[ 0 ].Tag;
				FCallStack CallStack = CallStackTag.CallStack;
				for( int AddressIndex = 0; AddressIndex < CallStack.AddressIndices.Count; AddressIndex++ )
				{
					OwnerWindow.MemoryBitmapCallStackListView.Items.Add( FStreamInfo.GlobalInstance.NameArray[ FStreamInfo.GlobalInstance.CallStackAddressArray[ CallStack.AddressIndices[ AddressIndex ] ].FunctionIndex ] );
				}

				GetPixelRangeFromPointer( MemoryBitmap.Width, MemoryBitmap.Height, BytesPerPixel, CallStackTag.Pointer, CallStackTag.Size );

				OwnerWindow.MemoryBitmapPanel.Invalidate();
			}

			if( OwnerWindow.MemoryBitmapAllocationHistoryListView.SelectedItems.Count == 1 )
			{
				FCallStackTag CallStackTag = ( FCallStackTag )OwnerWindow.MemoryBitmapAllocationHistoryListView.SelectedItems[ 0 ].Tag;
				OwnerWindow.MemoryBitmapCallstackGroupNameLabel.Text = "Group: " + CallStackTag.CallStack.Group.Name;
			}
			else
			{
				OwnerWindow.MemoryBitmapCallstackGroupNameLabel.Text = "Group: ";
			}

			OwnerWindow.MemoryBitmapCallStackListView.EndUpdate();
		}

		private static void InsertItemIntoMemoryBitmapListView( FCallStack CallStack, ulong StreamIndex, int StartFrame, int EndFrame, ulong AllocatedPointer, int AllocatedSize )
		{
			ListViewItem LVItem = new ListViewItem();
			LVItem.Text = StartFrame.ToString();
			LVItem.SubItems.Add( EndFrame == 0 ? "" : EndFrame.ToString() );
			LVItem.SubItems.Add( MainWindow.FormatSizeString( AllocatedSize ) );
			LVItem.Tag = new FCallStackTag( CallStack, StartFrame, StreamIndex, AllocatedPointer, ( ulong )AllocatedSize );

			bool bInserted = false;
			for( int ItemIndex = OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Count - 1; ItemIndex >= 0; ItemIndex-- )
			{
				if( ( ( FCallStackTag )OwnerWindow.MemoryBitmapAllocationHistoryListView.Items[ ItemIndex ].Tag ).StreamIndex <= StreamIndex )
				{
					OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Insert( ItemIndex + 1, LVItem );
					bInserted = true;
					break;
				}
			}

			if( !bInserted )
			{
				OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Insert( 0, LVItem );
			}
		}

		/// <summary>
		/// This function searches the memory bitmap listview for the allocation that was active at the given stream index.
		/// If there was no active allocation at that index, it returns the previous allocation, or failing that, the first allocation.
		/// </summary>
		private static int GetMemoryBitmapActiveAllocationForStreamIndex( ulong StreamIndex )
		{
			for( int ItemIndex = OwnerWindow.MemoryBitmapAllocationHistoryListView.Items.Count - 1; ItemIndex >= 0; ItemIndex-- )
			{
				if( ( ( FCallStackTag )OwnerWindow.MemoryBitmapAllocationHistoryListView.Items[ ItemIndex ].Tag ).StreamIndex <= StreamIndex )
				{
					return ItemIndex;
				}
			}

			return 0;
		}
	}

	/// <summary> Used as the tag for items in the allocation history list view. </summary>
	public class FCallStackTag
	{
		/// <summary> Callstack of this allocation. </summary>
		public FCallStack CallStack;

		/// <summary> Position in the stream. </summary>
		public ulong StreamIndex;

		/// <summary> Pointer of allocation. </summary>
		public ulong Pointer;

		/// <summary> Size of allocation. </summary>
		public ulong Size; 

		/// <summary> Constructor. </summary>
		public FCallStackTag( FCallStack InCallStack, int InStartFrame, ulong InStreamIndex, ulong InPointer, ulong InSize )
		{
			CallStack = InCallStack;
			StreamIndex = InStreamIndex;
			Pointer = InPointer;
			Size = InSize;
		}
	}

	/// <summary> Simple struct used to store current value of zoom level in the memory bitmap view. </summary>
	public struct FMemorySpace
	{
		/// <summary> Base memory pointer. </summary>
		public ulong Base;

		/// <summary> Memory size. </summary>
		public ulong Size;

		/// <summary> Constructor. </summary>
		public FMemorySpace( ulong InMemoryBase, ulong InMemorySize )
		{
			Base = InMemoryBase;
			Size = InMemorySize;
		}
	}
}
