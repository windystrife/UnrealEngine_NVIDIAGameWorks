// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;
using System.Diagnostics;
using System.Windows.Forms;
using System.ComponentModel;
using System.Drawing.Drawing2D;

namespace MemoryProfiler2
{
	public static class FCallStackHistoryView
	{
		private const int CALLSTACK_HISTORY_GRAPH_X_BORDER = 30;
		private const int CALLSTACK_HISTORY_GRAPH_Y_BORDER = 40;
		private const int MAX_CALLSTACK_HISTORY_ZOOM_HORIZONTAL = 16;
		private const int MAX_CALLSTACK_HISTORY_ZOOM_VERTICAL = 16;

		public static List<FCallStack> HistoryCallStacks = new List<FCallStack>();
		private static List<FSizeGraphPoint> GraphPoints = new List<FSizeGraphPoint>();
		private static Bitmap GraphBitmap;

		/// <summary> True if the graph generator was aborted. </summary>
		private static bool bIsIncomplete;

		private static Rectangle GraphRect = new Rectangle( 90, 30, 0, 0 );
		private static double YAxisHeight = 1024;
		public static int VZoom = 1;
		public static int HZoom = 1;
		private static int StartSnapshotIndex;
		private static int EndSnapshotIndex;

		private static Font SnapshotFont = new Font( FontFamily.GenericSansSerif, 13, GraphicsUnit.Pixel );

		private static Pen GrayPen = new Pen( Color.FromArgb( 0xd0, 0xd0, 0xd0 ) );
		private static Pen DarkGrayPen = new Pen( Color.FromArgb( 0x80, 0x80, 0x80 ) );
		private static Pen GrayDashedPen = new Pen( Color.FromArgb( 0xd0, 0xd0, 0xd0 ) );

		/// <summary> Reference to the main memory profiler window. </summary>
		private static MainWindow OwnerWindow;

		public static void SetProfilerWindow( MainWindow InMainWindow )
		{
			GrayDashedPen.DashStyle = DashStyle.Dash;
			OwnerWindow = InMainWindow;
		}

		public static void SetHistoryCallstacks( List<FCallStack> CallStacks )
		{
			OwnerWindow.UpdateStatus( "Updating history graph for " + OwnerWindow.CurrentFilename );

			HistoryCallStacks = CallStacks;

			if( HistoryCallStacks.Count > 0 )
			{
				long MaxSize;
				CombineSizeGraphsAsync( HistoryCallStacks, GraphPoints, out MaxSize );
				YAxisHeight = Math.Max( MaxSize, 1024 );
			}

			HZoom = 1;
			VZoom = 1;
			RefreshCallStackHistoryGraph();
			UpdateCallstackHistoryZoomLabel();
		}

		private static void CombineSizeGraphsAsync( List<FCallStack> InCallStacks, List<FSizeGraphPoint> CombinedGraphPoints, out long MaximumSize )
		{
			long TempMaximumSize = 0;
			OwnerWindow.ProgressDialog.OnBeginBackgroundWork = delegate( BackgroundWorker BGWorker, DoWorkEventArgs EventArgs )
			{
				CombineSizeGraphs( InCallStacks, CombinedGraphPoints, BGWorker, out TempMaximumSize, EventArgs );
				bIsIncomplete = false;
			};

			if( OwnerWindow.ProgressDialog.ShowDialog() != DialogResult.OK )
			{
				bIsIncomplete = true;
				OwnerWindow.UpdateStatus( "Failed to create history graph, due to '" + OwnerWindow.ProgressDialog.ExceptionResult + "'" );
			}
			else
			{
				OwnerWindow.UpdateStatus( "Displaying " + OwnerWindow.CurrentFilename );
			}

			MaximumSize = TempMaximumSize;

			OwnerWindow.CallStackHistoryHScroll.Visible = true;
			OwnerWindow.CallStackHistoryVScroll.Visible = true;
		}

		private static void CombineSizeGraphs( List<FCallStack> InCallStacks, List<FSizeGraphPoint> CombinedGraphPoints, BackgroundWorker BGWorker, out long MaximumSize, DoWorkEventArgs EventArgs )
		{
			BGWorker.ReportProgress( 0, "Combining size graphs" );

			MaximumSize = 0;
			CombinedGraphPoints.Clear();

			int TotalGraphPoints = 0;
			List<FCallStack> CallStacks = new List<FCallStack>( InCallStacks.Count );
			// Cache this to avoid the hit of calling ToUpperInvariant for every callstack
			string FilterText = OwnerWindow.FilterTextBox.Text.ToUpperInvariant();

			foreach( FCallStack CallStack in InCallStacks )
			{
				// leave out callstacks with empty sizegraphs or not in the selected memory pool
				if( CallStack.SizeGraphPoints.Count > 0 && CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, OwnerWindow.IsFilteringIn(), OwnerWindow.SelectedMemoryPool ) )
				{
					TotalGraphPoints += CallStack.SizeGraphPoints.Count;
					CallStacks.Add( CallStack );
				}
			}
		
			List<int> Indices = new List<int>( CallStacks.Count );
			for( int i = 0; i < CallStacks.Count; i++ )
			{
				Indices.Add( 0 );
			}

			int ProgressUpdateCounter = 0;

			long Size = 0;
			while( CallStacks.Count > 0 )
			{
				// Check for pending cancellation of a background operation.
				if( BGWorker.CancellationPending )
				{
					EventArgs.Cancel = true;
					return;
				}

				ulong MinStreamIndex = ulong.MaxValue;
				int BestGraphIndex = -1;
				bool bBestPointIsFreshRealloc = false;
				for( int Index = 0; Index < Indices.Count; Index++ )
				{
					FSizeGraphPoint SizeGraphPoint = CallStacks[ Index ].SizeGraphPoints[ Indices[ Index ] ];
					ulong StreamIndex = SizeGraphPoint.StreamIndex;

					if( StreamIndex < MinStreamIndex )
					{
						MinStreamIndex = StreamIndex;
						BestGraphIndex = Index;
						bBestPointIsFreshRealloc = SizeGraphPoint.bFreshRealloc;
					}
					else if( StreamIndex == MinStreamIndex )
					{
						if( bBestPointIsFreshRealloc && !SizeGraphPoint.bFreshRealloc )
						{
							// this point beats the current best point, because the best point is a fresh realloc
							// (which causes less accurate memory accounting) and this point isn't
							bBestPointIsFreshRealloc = SizeGraphPoint.bFreshRealloc;

							// skip previous best point
							Indices[ BestGraphIndex ]++;
							if( Indices[ BestGraphIndex ] >= CallStacks[ BestGraphIndex ].SizeGraphPoints.Count )
							{
								// point was last in graph, so remove callstack
								CallStacks.RemoveAt( BestGraphIndex );
								Indices.RemoveAt( BestGraphIndex );

								// bestGraph must be less than i, so we need to adjust i after removing bestGraph
								Index--;
							}

							BestGraphIndex = Index;
						}
						else
						{
							// This point is worse than or the same as the best point, so skip it to stop it
							// from coming up in the next iteration (we don't want two graph points for the same
							// streamindex).
							Indices[ Index ]++;
							if( Indices[ Index ] >= CallStacks[ Index ].SizeGraphPoints.Count )
							{
								// point was last in graph, so remove callstack and continue processing from next callstack
								CallStacks.RemoveAt( Index );
								Indices.RemoveAt( Index );
								Index--;
							}
						}
					}
				}

				// make sure no two graph points have the same streamindex
				Debug.Assert( CombinedGraphPoints.Count == 0 || CombinedGraphPoints[ CombinedGraphPoints.Count - 1 ].StreamIndex != MinStreamIndex );

				FSizeGraphPoint BestPoint = CallStacks[ BestGraphIndex ].SizeGraphPoints[ Indices[ BestGraphIndex ] ];
				Size += BestPoint.SizeChange;

				CombinedGraphPoints.Add( new FSizeGraphPoint( MinStreamIndex, BestPoint.SizeChange, false ) );

				Indices[ BestGraphIndex ]++;
				if( Indices[ BestGraphIndex ] >= CallStacks[ BestGraphIndex ].SizeGraphPoints.Count )
				{
					CallStacks.RemoveAt( BestGraphIndex );
					Indices.RemoveAt( BestGraphIndex );
				}

				if( Size > MaximumSize )
				{
					MaximumSize = Size;
				}

				ProgressUpdateCounter++;
				if( BGWorker != null && ProgressUpdateCounter > 10000 )
				{
					BGWorker.ReportProgress( ( int )( ( float )CombinedGraphPoints.Count / ( float )TotalGraphPoints * 100 ), "Combining size graphs" );
					ProgressUpdateCounter = 0;
				}
			}
		}

		public static void ClearView()
		{
			GraphPoints.Clear();

			OwnerWindow.CallStackHistoryHScroll.Visible = false;
			OwnerWindow.CallStackHistoryVScroll.Visible = false;
		}

		public static void ResetZoom()
		{
			HZoom = 1;
			VZoom = 1;
			OwnerWindow.CallstackHistoryResetZoomButton.Enabled = false;

			RefreshCallStackHistoryGraph();
			UpdateCallstackHistoryZoomLabel();
		}

		public static void UpdateZoom( int Delta, ref int ZoomValue )
		{
			int OldZoomValue = ZoomValue;

			ZoomValue += Delta;
			
			if( ZoomValue < 1 )
			{
				ZoomValue = 1;
			}
			else if( ZoomValue > 10 )
			{
				ZoomValue = 10;
			}

			if( ZoomValue > 1 )
			{
				OwnerWindow.CallstackHistoryResetZoomButton.Enabled = true;
			}

			if( HZoom == 1 && VZoom == 1 )
			{
				OwnerWindow.CallstackHistoryResetZoomButton.Enabled = false;
			}

			if( ZoomValue != OldZoomValue )
			{
				RefreshCallStackHistoryGraph();
				UpdateCallstackHistoryZoomLabel();
			}
		}

		private static void UpdateCallstackHistoryZoomLabel()
		{
			OwnerWindow.CallstackHistoryZoomLabelH.Text = HZoom.ToString();
			OwnerWindow.CallstackHistoryZoomLabelV.Text = VZoom.ToString();
		}

		public static void RefreshCallStackHistoryGraph()
		{
			if( OwnerWindow == null )
			{
				return;
			}

			GraphRect.Width = OwnerWindow.CallStackHistoryPanel.Width - GraphRect.X - OwnerWindow.CallStackHistoryVScroll.Width - CALLSTACK_HISTORY_GRAPH_X_BORDER;
			GraphRect.Height = OwnerWindow.CallStackHistoryPanel.Height - GraphRect.Y - OwnerWindow.CallStackHistoryHScroll.Height - CALLSTACK_HISTORY_GRAPH_Y_BORDER;

			if( FStreamInfo.GlobalInstance == null || GraphPoints.Count == 0 )
			{
				// force panel to be redrawn blank so that any old data is cleared
				OwnerWindow.CallStackHistoryPanel.Invalidate();
				return;
			}

			if( GraphRect.Width < 1 || GraphRect.Height < 1 )
			{
				// window is probably minimised, or at least too small to display a graph
				return;
			}

			OwnerWindow.SetWaitCursor( true );

			int GraphWidth = ( int )( HZoom * GraphRect.Width );
			int GraphHeight = ( int )( VZoom * GraphRect.Height );

			Size OldGraphSize = new Size();
			if( GraphBitmap != null )
			{
				OldGraphSize = GraphBitmap.Size;
				GraphBitmap.Dispose();
				GraphBitmap = null;
			}

			while( true )
			{
				try
				{
					GraphBitmap = new Bitmap( GraphWidth, GraphHeight );
					break;
				}
				catch( Exception )
				{
					// sometimes this fails with an ArgumentException when the arguments are fine
					// i suspect it means to throw OutOfMemoryException, as it only occurs with large images

					if( GraphBitmap != null && ( GraphWidth > OldGraphSize.Width || GraphHeight > OldGraphSize.Height ) )
					{
						// try to use the old size instead
						GraphWidth = OldGraphSize.Width;
						GraphHeight = OldGraphSize.Height;
					}
					else
					{
						// abort the whole function
						return;
					}
				}
			}

			SetUpScrollBar( OwnerWindow.CallStackHistoryHScroll, GraphWidth, GraphRect.Width );
			SetUpScrollBar( OwnerWindow.CallStackHistoryVScroll, GraphHeight, GraphRect.Height );

			if( OwnerWindow.CallstackHistoryShowCompleteHistoryButton.Checked )
			{
				StartSnapshotIndex = -1;
				EndSnapshotIndex = FStreamInfo.GlobalInstance.SnapshotList.Count - 1;
			}
			else
			{
				StartSnapshotIndex = OwnerWindow.GetStartSnapshotIndex();
				EndSnapshotIndex = OwnerWindow.GetEndSnapshotIndex();

				// swap snapshots if end is before start
				if( EndSnapshotIndex < StartSnapshotIndex )
				{
					int Temp = StartSnapshotIndex;
					StartSnapshotIndex = EndSnapshotIndex;
					EndSnapshotIndex = Temp;
				}
			}

			float XBase;
			float XScale;
			if( OwnerWindow.CallstackHistoryAxisUnitFramesButton.Checked )
			{
				int StartFrame = StartSnapshotIndex == -1 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ StartSnapshotIndex ].FrameNumber;
				int EndFrame = EndSnapshotIndex == -1 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ EndSnapshotIndex ].FrameNumber;
				int FrameRange = EndFrame - StartFrame;
				XScale = ( float )GraphWidth / ( float )FrameRange;
				XBase = StartFrame * XScale;
			}
			else
			{
				ulong StartStreamIndex = StartSnapshotIndex == -1 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ StartSnapshotIndex ].StreamIndex;
				ulong EndStreamIndex = EndSnapshotIndex == -1 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ EndSnapshotIndex ].StreamIndex;
				ulong StreamIndexRange = EndStreamIndex - StartStreamIndex;
				XScale = ( float )( ( double )GraphWidth / ( double )StreamIndexRange );
				XBase = StartStreamIndex * XScale;
			}
			float YScale = ( float )( GraphHeight / YAxisHeight );

			RenderSizeGraph( Graphics.FromImage( GraphBitmap ), GraphPoints, GraphWidth, GraphHeight, XBase, XScale, YScale, OwnerWindow.CallstackHistoryAxisUnitFramesButton.Checked, !bIsIncomplete );

			OwnerWindow.CallStackHistoryPanel.Invalidate();

			OwnerWindow.SetWaitCursor( false );
		}

		private static void RenderSizeGraph( Graphics MyGraphics, List<FSizeGraphPoint> GraphPoints, int GraphWidth, int GraphHeight, float XBase, float XScale, float YScale, bool bXAxisIsFrames, bool bShouldCompleteGraph )
		{
			// Progress bar.
			OwnerWindow.ToolStripProgressBar.Value = 0;
			OwnerWindow.ToolStripProgressBar.Visible = true;
			long ProgressInterval = GraphPoints.Count / 20;
			long NextProgressUpdate = ProgressInterval;

			Debug.Assert( GraphPoints != null && GraphPoints.Count > 0 );

			//MyGraphics.SmoothingMode = SmoothingMode.AntiAlias;

			FSizeGraphPoint OldPoint = new FSizeGraphPoint( GraphPoints[ 0 ].StreamIndex, 0, false );
			float OldX = 0;
			float OldY = 0;
			int NextFrame = 1;
			int CurrentSize = 0;
			for( int PointIndex = 0; PointIndex < GraphPoints.Count; PointIndex++ )
			{
				// Update progress bar.
				if( PointIndex >= NextProgressUpdate )
				{
					OwnerWindow.ToolStripProgressBar.PerformStep();
					NextProgressUpdate += ProgressInterval;
					Debug.WriteLine( "FCallStackHistoryView.RenderSizeGraph " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
				}

				FSizeGraphPoint SizeGraphPoint = GraphPoints[ PointIndex ];

				float X;
				if( bXAxisIsFrames )
				{
					X = CalculateXForTime( ref NextFrame, SizeGraphPoint.StreamIndex ) * XScale - XBase;
				}
				else
				{
					X = SizeGraphPoint.StreamIndex * XScale - XBase;
				}

				CurrentSize += SizeGraphPoint.SizeChange;

				float Y = CurrentSize * YScale;
				if( X >= 0 )
				{
					if( X < GraphWidth )
					{
						MyGraphics.DrawLine( Pens.Black, OldX, GraphHeight - OldY, X, GraphHeight - OldY );
						MyGraphics.DrawLine( Pens.Black, X, GraphHeight - OldY, X, GraphHeight - Y );
					}
					else
					{
						break;
					}
				}

				OldPoint = SizeGraphPoint;
				OldX = X;
				OldY = Y;
			}

			// Connect final point to right edge of graph.
			if( bShouldCompleteGraph )
			{
				float FinalLineY = GraphHeight - OldY;
				MyGraphics.DrawLine( Pens.Black, OldX, FinalLineY, GraphWidth, FinalLineY );
			}

			OwnerWindow.ToolStripProgressBar.Visible = false;
		}

		private static void SetUpScrollBar( ScrollBar MyScrollBar, int Maximum, int WindowWidth )
		{
			MyScrollBar.Maximum = Maximum;
			MyScrollBar.LargeChange = WindowWidth + 1;

			// The maximum value reachable by the user with the scrollbar (different to Maximum).
			int MaximumScrollable = Maximum - WindowWidth;

			int Value = MyScrollBar.Value;
			if( Value > MaximumScrollable )
			{
				Value = MaximumScrollable;
			}

			if( Value < 0 )
			{
				Value = 0;
			}

			MyScrollBar.Value = Value;
		}

		private static float CalculateXForTime( ref int MinFrame, ulong StreamIndex )
		{
			MinFrame = FStreamInfo.GlobalInstance.GetFrameNumberFromStreamIndex( MinFrame, StreamIndex );

			ulong PrevStreamIndex = FStreamInfo.GlobalInstance.FrameStreamIndices[ MinFrame - 1 ];
			ulong StreamIndicesInFrame = FStreamInfo.GlobalInstance.FrameStreamIndices[ MinFrame ] - PrevStreamIndex;

			return MinFrame - 1 + ( float )( StreamIndex - PrevStreamIndex ) / ( float )StreamIndicesInFrame;
		}

		public static void PaintHistoryPanel( PaintEventArgs e )
		{
			if( FStreamInfo.GlobalInstance == null || GraphPoints.Count == 0 )
			{
				TextRenderer.DrawText(e.Graphics, "No data to display", SnapshotFont, new Point(GraphRect.Left, GraphRect.Top), Color.Black);
			}
			else
			{
				PaintCallStackHistoryGraph( e.Graphics, SnapshotFont, GraphRect, ( float )YAxisHeight, OwnerWindow.CallStackHistoryHScroll.Value, OwnerWindow.CallStackHistoryVScroll.Value, HZoom, VZoom, OwnerWindow.CallstackHistoryAxisUnitFramesButton.Checked, OwnerWindow.CallstackHistoryShowSnapshotsButton.Checked, GraphBitmap, bIsIncomplete );
			}
		}

		private static void PaintCallStackHistoryGraph( Graphics MyGraphics, Font SnapshotFont, Rectangle GraphRect, float YAxisHeight, int HScrollValue, int VScrollValue, int HZoom, int VZoom, bool bXAxisIsTime, bool bShowSnapshots, Bitmap GraphImage, bool bGraphIncomplete )
		{
			int GraphTop = GraphRect.Top;
			int GraphHeight = GraphRect.Height;
			int GraphLeft = GraphRect.Left;
			int GraphWidth = GraphRect.Width;

			string YAxisLabel;
			long MajorTick;
			MainWindow.SetUpAxisLabel( VZoom, ref YAxisHeight, out YAxisLabel, out MajorTick );

			// Make sure that majorTick is divisible by four, so that minorTick divides into it without remainder.
			MajorTick = MajorTick / 4 * 4;
			long MinorTick = MajorTick / 4;

			// Y axis
			MyGraphics.DrawLine( Pens.Black, GraphLeft, GraphTop, GraphLeft, GraphTop + GraphHeight );
			int TrueGraphHeight = ( int )( VZoom * GraphHeight );
			OwnerWindow.DrawYAxis( MyGraphics, Pens.Black, Color.Black, MajorTick, MainWindow.DEFAULT_MAJOR_TICK_WIDTH, MinorTick, MainWindow.DEFAULT_MINOR_TICK_WIDTH, GraphLeft, GraphTop, GraphHeight, VScrollValue, TrueGraphHeight, YAxisLabel, YAxisHeight, false, false );

			// X axis
			MyGraphics.DrawLine( Pens.Black, GraphLeft, GraphTop + GraphHeight, GraphLeft + GraphWidth, GraphTop + GraphHeight );

			// Shade loadmaps and GCs.
			Brush LoadMapBrush = new SolidBrush( Color.FromArgb( 209, 223, 255 ) );
			Brush GCBrush = new SolidBrush( Color.FromArgb( 205, 255, 204 ) );

			int StartSnapshot = Math.Max( StartSnapshotIndex, 0 );
			int EndSnapshot = EndSnapshotIndex;
			ulong StartStreamIndex = StartSnapshotIndex == -1 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ StartSnapshotIndex ].StreamIndex;
			ulong EndStreamIndex = EndSnapshotIndex == -1 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ EndSnapshotIndex ].StreamIndex;
			int StartFrame = StartSnapshotIndex == -1 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ StartSnapshotIndex ].FrameNumber;
			int EndFrame = EndSnapshotIndex == -1 ? 0 : FStreamInfo.GlobalInstance.SnapshotList[ EndSnapshotIndex ].FrameNumber;

			float XScale;
			if( bXAxisIsTime )
			{
				int FrameRange = EndFrame - StartFrame;
				XScale = ( float )GraphWidth / ( float )FrameRange * HZoom;
			}
			else
			{
				ulong StreamIndexRange = ( ulong )( EndStreamIndex - StartStreamIndex );
				XScale = ( float )GraphWidth / ( float )StreamIndexRange * HZoom;
			}

			// Draw frame ticks.
			if( bXAxisIsTime )
			{
				int FrameTick = Math.Max( ( int )( 4 / XScale ), 1 );
				for( int FrameIndex = 0; FrameIndex < FStreamInfo.GlobalInstance.FrameStreamIndices.Count; FrameIndex += FrameTick )
				{
					float FrameX = FrameIndex * XScale;

					if( FrameX >= HScrollValue && FrameX <= HScrollValue + GraphWidth )
					{
						float panelX = GraphLeft + FrameX - HScrollValue;

						if( bXAxisIsTime )
						{
							// Draw vertical dashed line.
							MyGraphics.DrawLine( GrayDashedPen, panelX, GraphTop, panelX, GraphTop + GraphHeight );
						}
					}
				}

				TextRenderer.DrawText(MyGraphics, "Frames per gridline: " + FrameTick, SnapshotFont, new Point(GraphLeft, GraphTop + GraphHeight + 5), Color.Black);
			}

			int MinFrame = 1;
			if( bShowSnapshots )
			{
				if( FStreamInfo.GlobalInstance.CreationOptions.LoadMapStartSnapshotsCheckBox.Checked && FStreamInfo.GlobalInstance.CreationOptions.LoadMapEndSnapshotsCheckBox.Checked )
				{
					ulong LoadMapStartStreamIndex = 0;
					bool bInLoadmap = false;
					for( int SnapshotIndex = StartSnapshot; SnapshotIndex <= EndSnapshot; SnapshotIndex++ )
					{
						if( FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Start )
						{
							LoadMapStartStreamIndex = FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex;
							bInLoadmap = true;
						}
						else if( FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_End || ( SnapshotIndex == FStreamInfo.GlobalInstance.SnapshotList.Count - 1 && bInLoadmap ) )
						{
							float VirtualStartX, VirtualEndX;
							if( bXAxisIsTime )
							{
								VirtualStartX = CalculateXForTime( ref MinFrame, LoadMapStartStreamIndex ) - StartFrame;
								VirtualEndX = CalculateXForTime( ref MinFrame, FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex ) - StartFrame;
							}
							else
							{
								VirtualStartX = LoadMapStartStreamIndex - StartStreamIndex;
								VirtualEndX = FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex - StartStreamIndex;
							}

							float StartX = Math.Min( Math.Max( GraphLeft + VirtualStartX * XScale - HScrollValue, GraphLeft ), GraphLeft + GraphWidth );
							float EndX = Math.Min( Math.Max( GraphLeft + VirtualEndX * XScale - HScrollValue, GraphLeft ), GraphLeft + GraphWidth );

							if( StartX != EndX )
							{
								MyGraphics.FillRectangle( LoadMapBrush, StartX, GraphTop, EndX - StartX, GraphHeight );
							}

							bInLoadmap = false;
						}
					}
				}

				// Shade GC regions on top of LoadMap regions.
				if( FStreamInfo.GlobalInstance.CreationOptions.GCStartSnapshotsCheckBox.Checked && FStreamInfo.GlobalInstance.CreationOptions.GCEndSnapshotsCheckBox.Checked )
				{
					ulong GCStartStreamIndex = 0;
					bool bInGC = false;
					MinFrame = 1;
					for( int SnapshotIndex = 0; SnapshotIndex < FStreamInfo.GlobalInstance.SnapshotList.Count; SnapshotIndex++ )
					{
						if( FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_Start )
						{
							GCStartStreamIndex = FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex;
							bInGC = true;
						}
						else if( FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].SubType == EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_End || ( SnapshotIndex == FStreamInfo.GlobalInstance.SnapshotList.Count - 1 && bInGC ) )
						{
							float VirtualStartX, VirtualEndX;
							if( bXAxisIsTime )
							{
								VirtualStartX = CalculateXForTime( ref MinFrame, GCStartStreamIndex ) - StartFrame;
								VirtualEndX = CalculateXForTime( ref MinFrame, FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex ) - StartFrame;
							}
							else
							{
								VirtualStartX = GCStartStreamIndex - StartStreamIndex;
								VirtualEndX = FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex - StartStreamIndex;
							}

							float StartX = Math.Min( Math.Max( GraphLeft + VirtualStartX * XScale - HScrollValue, GraphLeft ), GraphLeft + GraphWidth );
							float EndX = Math.Min( Math.Max( GraphLeft + VirtualEndX * XScale - HScrollValue, GraphLeft ), GraphLeft + GraphWidth );

							if( StartX != EndX )
							{
								MyGraphics.FillRectangle( GCBrush, StartX, GraphTop, EndX - StartX, GraphHeight );
							}

							bInGC = false;
						}
					}
				}

				LoadMapBrush.Dispose();
				GCBrush.Dispose();

				// Draw snapshot lines.
				MinFrame = 1;
				for( int SnapshotIndex = 0; SnapshotIndex < FStreamInfo.GlobalInstance.SnapshotList.Count; SnapshotIndex++ )
				{
					float SnapshotX;
					if( bXAxisIsTime )
					{
						SnapshotX = ( CalculateXForTime( ref MinFrame, FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex ) - StartFrame ) * XScale;
					}
					else
					{
						SnapshotX = ( FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex - StartStreamIndex ) * XScale;
					}

					if( SnapshotX >= HScrollValue && SnapshotX <= HScrollValue + GraphWidth )
					{
						float panelX = GraphLeft + SnapshotX - HScrollValue;
						MyGraphics.DrawLine( GrayPen, panelX, GraphTop, panelX, GraphTop + GraphHeight - 1 );
					}
				}
			}

				// Copy region from graph image.
				try
				{
					MyGraphics.DrawImage( GraphImage, GraphRect, HScrollValue, VScrollValue, GraphRect.Width, GraphRect.Height, GraphicsUnit.Pixel );
				}
				catch( Exception e )
				{
					if( e is OutOfMemoryException || e is ArgumentException )
					{
						TextRenderer.DrawText(MyGraphics, "NOTE: Graph failed to render: Try zooming out", SnapshotFont, new Point(GraphLeft, 0), Color.Red);
					}
					else
					{
						throw e;
					}
				}

			if( bShowSnapshots )
			{
				// Label snapshots.
				float SnapshotRight = -1;
				float LastTextY = GraphTop;
				MinFrame = 1;
				for( int SnapshotIndex = 0; SnapshotIndex < FStreamInfo.GlobalInstance.SnapshotList.Count; SnapshotIndex++ )
				{
					float SnapshotX;
					if( bXAxisIsTime )
					{
						SnapshotX = ( CalculateXForTime( ref MinFrame, FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex ) - StartFrame ) * XScale;
					}
					else
					{
						SnapshotX = ( FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].StreamIndex - StartStreamIndex ) * XScale;
					}

					if( SnapshotX >= HScrollValue && SnapshotX <= HScrollValue + GraphWidth )
					{
						float TextY = GraphTop;
						if( SnapshotX < SnapshotRight )
						{
							TextY = LastTextY + SnapshotFont.Height;
						}
						LastTextY = TextY;

						float SnapshotTextWidth = MyGraphics.MeasureString( FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].Description, SnapshotFont ).Width;
						if( SnapshotX + SnapshotTextWidth > SnapshotRight )
						{
							SnapshotRight = SnapshotX + SnapshotTextWidth;
						}

						float PanelX = GraphLeft + SnapshotX - HScrollValue;
						TextRenderer.DrawText(MyGraphics, FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].Description, SnapshotFont, new Point((int)PanelX - 1, (int)TextY - 1), Color.White);
						TextRenderer.DrawText(MyGraphics, FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].Description, SnapshotFont, new Point((int)PanelX - 1, (int)TextY + 1), Color.White);
						TextRenderer.DrawText(MyGraphics, FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].Description, SnapshotFont, new Point((int)PanelX + 1, (int)TextY - 1), Color.White);
						TextRenderer.DrawText(MyGraphics, FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].Description, SnapshotFont, new Point((int)PanelX + 1, (int)TextY + 1), Color.White);
						TextRenderer.DrawText(MyGraphics, FStreamInfo.GlobalInstance.SnapshotList[ SnapshotIndex ].Description, SnapshotFont, new Point((int)PanelX, (int)TextY), Color.DarkGray);
					}
				}
			}

			if( bGraphIncomplete )
			{
				TextRenderer.DrawText(MyGraphics, "NOTE: This graph is incomplete", SnapshotFont, new Point(GraphLeft, 0), Color.Red);
			}
		}
	}
}