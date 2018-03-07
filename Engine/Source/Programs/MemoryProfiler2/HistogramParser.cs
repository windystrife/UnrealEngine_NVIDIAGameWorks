// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using System.Drawing.Drawing2D;

namespace MemoryProfiler2
{
	/// <summary> Histogram parser. </summary>
	public static class FHistogramParser
	{
		/// <summary>
		/// The assumption that this constant is 2 is still hardcoded
		/// in some places, so increasing the number of banks is not
		/// as simple as changing this constant.
		///
		/// Second memory bank is used only on PS3 to show local memory.
		/// </summary>
		public const int NUM_MEMORY_BANKS = 2;

		/// <summary> Array of histogram bars. </summary>
		static public List<FHistogramBar>[] HistogramBars;

		/// <summary> Array of histogram bars for selected main histogram bar. </summary>
		static private List<FHistogramBar> HistogramSelectionBars = new List<FHistogramBar>();

		/// <summary> Currently selected histogram bar from the memory bank. </summary>
		static private FHistogramBar SelectedHistogramBar;

		/// <summary> Currently selected histogram bar from the detailed bank. </summary>
		static private FHistogramBar SubselectedHistogramBar;

		/// <summary> Index of the bank for the currently selected histogram bar. </summary>
		static private int SelectedMemoryBankIndex = 0;

		/// <summary> Indices of the currently selected histogram bars in the memory banks. </summary>
		static private int[] SelectedHistogramBarIndex = new int[ 2 ] { 0, 0 };

		static private Pen BlackDashedPen = new Pen( Color.Black );
		static private Pen RedDashedPen = new Pen( Color.Red );

		private static Brush CheckerBWBrush1x1 = new HatchBrush( HatchStyle.Percent50, Color.FromArgb( 224, Color.Plum ), Color.FromArgb( 224, Color.SkyBlue ) );
		private static Brush CheckerBWBrush2x2 = new HatchBrush( HatchStyle.SmallCheckerBoard, Color.FromArgb( 192, Color.Plum ), Color.FromArgb( 192, Color.SkyBlue ) );
		private static Brush CheckerBWBrush4x4 = new HatchBrush( HatchStyle.LargeCheckerBoard, Color.FromArgb( 160, Color.Plum ), Color.FromArgb( 160, Color.SkyBlue ) );
		
		private static Pen OutlinePen1 = new Pen( Color.FromArgb( 128, Color.White ) );
		private static Pen OutlinePen2 = new Pen( Color.FromArgb( 192, Color.Black ) );

		/// <summary> Pen with 1x1 checker used to draw selected histogram bar. </summary>
		private static Pen CheckerPen1x1 = new Pen( CheckerBWBrush1x1 );

		/// <summary> Pen with 2x2 checker used to draw selected histogram bar. </summary>
		private static Pen CheckerPen2x2 = new Pen( CheckerBWBrush2x2 );

		/// <summary> Pen with 4x4 checker used to draw selected histogram bar. </summary>
		private static Pen CheckerPen4x4 = new Pen( CheckerBWBrush4x4 );

		private static Pen SelectedSolidPen = new Pen( new SolidBrush( Color.LightSteelBlue ) );

		/// <summary> Pen used to draw the selected histogram bar. Size of the checker is based on the selection height. </summary>
		private static Pen SelectedBarPen( FHistogramBar HistogramBar )
		{
			int SelectionHeight = ( int )HistogramBar.Rect.Height;

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

		/// <summary> Reference to the main memory profiler window. </summary>
		private static MainWindow OwnerWindow;

		public static void SetProfilerWindow( MainWindow InMainWindow )
		{
			BlackDashedPen.DashStyle = DashStyle.Dash;
			RedDashedPen.DashStyle = DashStyle.Dash;

			OwnerWindow = InMainWindow;
		}

		public static void ParseSnapshot( List<FCallStackAllocationInfo> CallStackList, string FilterText )
		{
			// Progress bar
			long ProgressInterval = CallStackList.Count / 20;
			long NextProgressUpdate = ProgressInterval;
			int CallStackCurrent = 0;
			OwnerWindow.ToolStripProgressBar.Value = 0;
			OwnerWindow.ToolStripProgressBar.Visible = true;

			OwnerWindow.UpdateStatus( "Updating histogram view for " + OwnerWindow.CurrentFilename );

			List<ClassGroup> CallStackGroups = OwnerWindow.Options.ClassGroups;
			List<FHistogramBar>[] Bars = new List<FHistogramBar>[ NUM_MEMORY_BANKS ];

			for( int BankIndex = 0; BankIndex < Bars.Length; BankIndex++ )
			{
				Bars[ BankIndex ] = new List<FHistogramBar>();

				// The first bar in each column is for callstacks unmatched by any pattern.
				Bars[ BankIndex ].Add( new FHistogramBar( "Other", Color.White ) );

				// Add all groups to all memory bank columns.
				foreach( ClassGroup CallStackGroup in CallStackGroups )
				{
					var Bar = new FHistogramBar( CallStackGroup );
					Bar.BeginBatchAddition();
					Bars[ BankIndex ].Add( Bar );
				}
			}

			using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "HistogramParser.ParseSnapshot" ) )
			{
				long Size = 0;
				int Count = 0;

				bool bFilterIn = OwnerWindow.IsFilteringIn();

				var FilteredCallstackList = new List<FCallStackAllocationInfo>(CallStackList.Count);
				foreach (var AllocationInfo in CallStackList)
				{
					var FilteredAllocationInfo = AllocationInfo.GetAllocationInfoForTags(OwnerWindow.GetTagsFilter(), bFilterIn);
					if (FilteredAllocationInfo.TotalCount != 0)
					{
						FilteredCallstackList.Add(FilteredAllocationInfo);
					}
				}

				foreach ( FCallStackAllocationInfo AllocationInfo in FilteredCallstackList )
				{
					// Update progress bar.
					if ( CallStackCurrent >= NextProgressUpdate )
					{
						OwnerWindow.ToolStripProgressBar.PerformStep();
						NextProgressUpdate += ProgressInterval;
						Debug.WriteLine( "FHistogramParser.ParseSnapshot " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
					}
					CallStackCurrent++;

					FCallStack OriginalCallStack = FStreamInfo.GlobalInstance.CallStackArray[AllocationInfo.CallStackIndex ];
					if( OriginalCallStack.RunFilters( FilterText, CallStackGroups, bFilterIn, OwnerWindow.SelectedMemoryPool ) )
					{
						bool bFound = false;
						int Column = FMemoryPoolInfo.GetMemoryPoolHistogramColumn( OriginalCallStack.MemoryPool );
						if( Column == -1 )
						{
							// If the callstack is in multiple pools, just put it in the first bank.
							// The user has already been warned about multi-pool callstacks.
							Column = 0;
						}

						for( int GroupIndex = 0; GroupIndex < CallStackGroups.Count; GroupIndex++ )
						{
							foreach( CallStackPattern CallStackPatternIt in CallStackGroups[ GroupIndex ].CallStackPatterns )
							{
								if (CallStackPatternIt.ContainsCallStack(FStreamInfo.GlobalInstance.CallStackArray[AllocationInfo.CallStackIndex]))
								{
									Bars[Column][GroupIndex + 1].AddAllocation(AllocationInfo);
									bFound = true;
									goto HackyBreakAll;
								}
							}
						}
HackyBreakAll:

						if( !bFound )
						{
							// No pattern matched this callstack, so add it to the Other bar
							Bars[ Column ][ 0 ].AddAllocation( AllocationInfo );
						}
					}

					Size += AllocationInfo.TotalSize;
					Count += AllocationInfo.TotalCount;
				}
			}

			// End the update batch and allow things to sort
			for( int BankIndex = 0; BankIndex < Bars.Length; BankIndex++ )
			{
				foreach( ClassGroup CallStackGroup in CallStackGroups )
				{
					foreach (var Bar in Bars[ BankIndex ])
					{
						Bar.EndBatchAddition();
					}
				}
			}

			OwnerWindow.ToolStripProgressBar.Visible = false;
			HistogramBars = Bars;
				
			// Select first valid histogram bar.
			SelectFirstValidHistogramBar();
		}

		/// <summary> Selects first valid histogram bar, searches through all memory banks. </summary>
		private static void SelectFirstValidHistogramBar()
		{
			SelectedMemoryBankIndex = 0;
			SelectedHistogramBarIndex[ 0 ] = 0;
			SelectedHistogramBarIndex[ 1 ] = 0;
			SubselectedHistogramBar = null;

			for( int MemoryBankIndex = 0; MemoryBankIndex < HistogramBars.Length; MemoryBankIndex++ )
			{
				for( int BarIndex = 0; BarIndex < HistogramBars[ MemoryBankIndex ].Count; BarIndex++ )
				{
					FHistogramBar Bar = FHistogramParser.HistogramBars[ MemoryBankIndex ][ BarIndex ];
					if( Bar.AllocationCount > 0 )
					{
						SelectedMemoryBankIndex = MemoryBankIndex;
						SelectedHistogramBarIndex[ SelectedMemoryBankIndex ] = BarIndex;
						SelectHistogramBar( Bar );
						return;
					}
				}
			}

			SelectedHistogramBar = null;
		}

		public static void ClearView()
		{
			if( HistogramBars != null )
			{
				HistogramBars[ 0 ].Clear();
				HistogramBars[ 1 ].Clear();
				
			}
			HistogramSelectionBars.Clear();

			SelectedMemoryBankIndex = 0;
			SelectedHistogramBarIndex[ 0 ] = 0;
			SelectedHistogramBarIndex[ 1 ] = 0;

			SelectedHistogramBar = null;
			SubselectedHistogramBar = null;

			OwnerWindow.HistogramViewCallStackListView.BeginUpdate();
			OwnerWindow.HistogramViewCallStackListView.Items.Clear();
			OwnerWindow.HistogramViewCallStackListView.SelectedItems.Clear();
			OwnerWindow.HistogramViewCallStackListView.EndUpdate();

			OwnerWindow.HistogramViewNameLabel.Text = "";
			OwnerWindow.HistogramViewSizeLabel.Text = "";
			OwnerWindow.HistogramViewAllocationsLabel.Text = "";
			OwnerWindow.MemoryBitmapAllocatedMemoryLabel.Text = "";
		}

		public static void PaintPanel( PaintEventArgs e )
		{
			int[] MemorySizes = new int[]
			{ 
				FStreamInfo.GetMemoryBankSize( 0 ), 
				FStreamInfo.GetMemoryBankSize( 1 )
			};

			string[] MemoryCaptions = new string[]
			{
				"MB (Local)",
				"MB (Video)"
			};

			const int MinorTick = 2;
			const int MajorTick = 10;
			const int TickRight = 70;
			const int GraphYBorder = 30;
			const int BarLeft = TickRight + 10;

			int NumColumns = 3;
			int TotalBordersSize = NumColumns * BarLeft;

			int GraphWidth = Math.Min( ( OwnerWindow.HistogramPanel.Width - TotalBordersSize ) / 3, 150 );

			float BarWidth = GraphWidth * 0.6f;
			float GraphXGap = GraphWidth * 0.4f;

			int MaxMemorySize = 0;
			for( int BankIndex = 0; BankIndex < NUM_MEMORY_BANKS; BankIndex++ )
			{
				MaxMemorySize = Math.Max( MemorySizes[ BankIndex ], MaxMemorySize );
			}

			float TotalGraphHeight = OwnerWindow.HistogramPanel.Height - GraphYBorder * 2;

			if( HistogramBars != null )
			{
				for( int MemoryBankIndex = 0; MemoryBankIndex < MemorySizes.Length; MemoryBankIndex++ )
				{
					float GraphTop = GraphYBorder;
					float GraphLeft = MemoryBankIndex * ( BarLeft + BarWidth + GraphXGap );
					float YScale = TotalGraphHeight / MemorySizes[ MemoryBankIndex ];

					// Draw vertical axes.
					OwnerWindow.DrawYAxis( e.Graphics, Pens.Black, Color.Black, MajorTick, MinorTick, GraphLeft + TickRight, GraphTop, TotalGraphHeight, MemoryCaptions[ MemoryBankIndex ], MemorySizes[ MemoryBankIndex ] );

					float BarY = GraphTop + YScale * MemorySizes[ MemoryBankIndex ];
					for( int BarIndex = HistogramBars[ MemoryBankIndex ].Count - 1; BarIndex >= 0; BarIndex-- )
					{
						FHistogramBar Bar = HistogramBars[ MemoryBankIndex ][ BarIndex ];

						if( Bar.AllocationCount == 0 )
						{
							continue;
						}

						float BarHeight = ( float )Bar.MemorySize / ( 1024 * 1024 ) * YScale;

						Bar.Rect.X = GraphLeft + BarLeft;
						Bar.Rect.Y = BarY - BarHeight;
						Bar.Rect.Width = BarWidth;
						Bar.Rect.Height = BarHeight;

						e.Graphics.FillRectangle( new SolidBrush( Bar.Colour ), Bar.Rect.X, Bar.Rect.Y, Bar.Rect.Width, Bar.Rect.Height );
						BarY -= BarHeight;
					}
				}

				if( SelectedHistogramBar != null )
				{
					e.Graphics.FillRectangle
					( 
						SelectedBarPen( SelectedHistogramBar ).Brush, 
						SelectedHistogramBar.Rect.X, 
						SelectedHistogramBar.Rect.Y, 
						SelectedHistogramBar.Rect.Width,
						Math.Max( SelectedHistogramBar.Rect.Height, 1.0f )
					);

					float MarkerHeight = Math.Max( SelectedHistogramBar.Rect.Height, 1.0f );
					float MarkerPosX = SelectedHistogramBar.Rect.Left + SelectedHistogramBar.Rect.Width + 1.0f;
					float MarkerPosY = ( float )Math.Ceiling( ( double )SelectedHistogramBar.Rect.Top ) - 1;

					e.Graphics.DrawLine( Pens.Black, MarkerPosX + 0, MarkerPosY, MarkerPosX + 5, MarkerPosY );
					e.Graphics.DrawLine( Pens.Black, MarkerPosX + 5, MarkerPosY, MarkerPosX + 5, MarkerPosY + MarkerHeight );
					e.Graphics.DrawLine( Pens.Black, MarkerPosX + 0, MarkerPosY + MarkerHeight + 1, MarkerPosX + 5, MarkerPosY + MarkerHeight + 1 );
				}

				TextRenderer.DrawText(
					e.Graphics, 
					"Use key up or key down to change selected allocation", 
					OwnerWindow.AxisFont, 
					new Point((int)(BarLeft * 0.5f), (int)(OwnerWindow.HistogramPanel.Height - (GraphYBorder + OwnerWindow.AxisFont.Height) * 0.5f)), 
					Color.Black
					);
			}

			/*
			// Draw "Total Memory Used" line.
			if( OwnerWindow.CurrentSnapshot != null && OwnerWindow.CurrentSnapshot.MetricArray.Count > 0 )
			{
				float YScale = TotalGraphHeight / MemorySizes[ 0 ];
				float GraphBottom = GraphYBorder + YScale * MemorySizes[ 0 ];
				float TotalUsedLineY = GraphBottom - YScale * ( OwnerWindow.CurrentSnapshot.MemoryAllocationStats.TotalAllocated / 1024f / 1024f );
				float OverheadLineY = TotalUsedLineY + YScale * ( OwnerWindow.CurrentSnapshot.MetricArray[ ( int )ESnapshotMetricV3.MemoryProfilingOverhead ] / 1024f / 1024f );

				e.Graphics.DrawLine( BlackDashedPen, TickRight, TotalUsedLineY, BarLeft + BarWidth, TotalUsedLineY );
				e.Graphics.DrawLine( RedDashedPen, TickRight, OverheadLineY, BarLeft + BarWidth, OverheadLineY );
			}
			*/

			if( SelectedHistogramBar != null && SelectedHistogramBar.MemorySize > 0 )
			{
				float GraphLeft = MemorySizes.Length * ( BarLeft + BarWidth + GraphXGap );
				float MemorySizeMB = ( float )( ( double )SelectedHistogramBar.MemorySize / ( 1024 * 1024 ) );
				float SelectedYScale = TotalGraphHeight / MemorySizeMB;

				string AxisLabel = "MB";
				int AxisMemorySize = ( int )MemorySizeMB;
				if( AxisMemorySize < 32 )
				{
					// Drop down into kilobytes.
					AxisMemorySize = ( int )( SelectedHistogramBar.MemorySize / 1024 );

					AxisLabel = "KB";
				}

				if( AxisMemorySize < 32 )
				{
					// Drop down into bytes.
					AxisMemorySize = ( int )Math.Max( SelectedHistogramBar.MemorySize, 32 );
					AxisLabel = "bytes";
				}

				// Select a major tick that's divisible by 4 so that the minor tick divides into it without remainder.
				int SelectedMajorTick = ( AxisMemorySize / 8 ) / 4 * 4;
				int SelectedMinorTick = SelectedMajorTick / 4;
				OwnerWindow.DrawYAxis( e.Graphics, Pens.Black, Color.Black, SelectedMajorTick, SelectedMinorTick, GraphLeft + TickRight, GraphYBorder, TotalGraphHeight, AxisLabel, AxisMemorySize );

				{
					// Used to batch up drawing as individual calls are slow
					var FillRects = new Dictionary<Color, List<RectangleF>>();
					var DrawRects = new List<RectangleF>();

					float BarY = GraphYBorder + SelectedYScale * MemorySizeMB;
					for (int SelBarIndex = HistogramSelectionBars.Count - 1; SelBarIndex >= 0; SelBarIndex--)
					{
						FHistogramBar Bar = HistogramSelectionBars[SelBarIndex];

						float BarHeight = (float)((double)Bar.MemorySize / (1024 * 1024) * SelectedYScale);

						Bar.Rect.X = GraphLeft + BarLeft;
						Bar.Rect.Y = BarY - BarHeight;
						Bar.Rect.Width = BarWidth;
						Bar.Rect.Height = BarHeight;

						List<RectangleF> FillRectsList;
						if (FillRects.TryGetValue(Bar.Colour, out FillRectsList))
						{
							FillRectsList.Add(new RectangleF(Bar.Rect.X, Bar.Rect.Y, Bar.Rect.Width, Bar.Rect.Height));
						}
						else
						{
							FillRectsList = new List<RectangleF>() { new RectangleF(Bar.Rect.X, Bar.Rect.Y, Bar.Rect.Width, Bar.Rect.Height) };
							FillRects.Add(Bar.Colour, FillRectsList);
						}

						DrawRects.Add(new RectangleF(Bar.Rect.X, Bar.Rect.Y, Bar.Rect.Width - 1, Bar.Rect.Height));

						BarY -= BarHeight;
					}

					// Draw batched
					foreach (var FillRectPair in FillRects)
					{
						e.Graphics.FillRectangles(new SolidBrush(FillRectPair.Key), FillRectPair.Value.ToArray());
					}
					e.Graphics.DrawRectangles(Pens.Black, DrawRects.ToArray());
				}

				if( SubselectedHistogramBar != null )
				{
					e.Graphics.FillRectangle
					( 
						SelectedBarPen( SubselectedHistogramBar ).Brush, 
						SubselectedHistogramBar.Rect.X, 
						SubselectedHistogramBar.Rect.Y, 
						SubselectedHistogramBar.Rect.Width,
						Math.Max( SubselectedHistogramBar.Rect.Height, 1.0f )
					);

					float MarkerHeight = Math.Max( SubselectedHistogramBar.Rect.Height, 1.0f );
					float MarkerPosX = SubselectedHistogramBar.Rect.Left + SubselectedHistogramBar.Rect.Width + 1.0f;
					float MarkerPosY = ( float )Math.Ceiling( ( double )SubselectedHistogramBar.Rect.Top ) - 1;

					e.Graphics.DrawLine( Pens.Black, MarkerPosX + 0, MarkerPosY, MarkerPosX + 5, MarkerPosY );
					e.Graphics.DrawLine( Pens.Black, MarkerPosX + 5, MarkerPosY, MarkerPosX + 5, MarkerPosY + MarkerHeight );
					e.Graphics.DrawLine( Pens.Black, MarkerPosX + 0, MarkerPosY + MarkerHeight + 1, MarkerPosX + 5, MarkerPosY + MarkerHeight + 1 );;
				}
			}
		}

		static public void UnsafeMouseClick( object sender, MouseEventArgs e )
		{
			// Work out which bar, if any, the user clicked on.
			for( int MemoryBankIndex = 0; MemoryBankIndex < HistogramBars.Length; MemoryBankIndex++ )
			{
				for( int BarIndex = 0; BarIndex < HistogramBars[ MemoryBankIndex ].Count; BarIndex++ )
				{
					FHistogramBar Bar = FHistogramParser.HistogramBars[ MemoryBankIndex ][ BarIndex ];
					if( Bar.Rect.Contains( e.X, e.Y ) )
					{
						SelectedMemoryBankIndex = MemoryBankIndex;
						SelectedHistogramBarIndex[ SelectedMemoryBankIndex ] = BarIndex;
						SelectHistogramBar( Bar );

						if( e.Button == MouseButtons.Right )
						{
							OwnerWindow.ViewHistoryContextMenu.Tag = Bar;
							OwnerWindow.ViewHistoryContextMenu.Show( ( Control )sender, e.Location );
						}

						return;
					}
				}
			}

			// Check selection bars (the graph that appears when you select a bar in another graph).
			if( SelectedHistogramBar != null )
			{
				for( int DetailedBarIndex = 0; DetailedBarIndex < HistogramSelectionBars.Count; DetailedBarIndex++ )
				{
					FHistogramBar HistogramBar = HistogramSelectionBars[ DetailedBarIndex ];
					if( HistogramBar.Rect.Contains( e.X, e.Y ) )
					{
						SubselectHistogramBar( HistogramBar );

						if( e.Button == MouseButtons.Right )
						{
							OwnerWindow.ViewHistoryContextMenu.Tag = HistogramBar;
							OwnerWindow.ViewHistoryContextMenu.Show( ( Control )sender, e.Location );
						}

						return;
					}
				}
			}

            SubselectHistogramBar(null);
        }

		static public bool ProcessKeys( Keys KeyData )
		{
			switch( KeyData )
			{
				case Keys.Up:
				{
					if( SubselectedHistogramBar != null )
					{
						int Index = HistogramSelectionBars.IndexOf( SubselectedHistogramBar );
						if( Index > 0 )
						{
							SubselectHistogramBar( HistogramSelectionBars[ Index - 1 ] );
						}
					}
					else if( SelectedHistogramBar != null )
					{
						for( int BarIndex = 0; BarIndex < HistogramBars.Length; BarIndex++ )
						{
							List<FHistogramBar> HistogramBarArray = HistogramBars[ BarIndex ];
							int Index = HistogramBarArray.IndexOf( SelectedHistogramBar );
							if( Index > 0 ) 
							{
								for( ; Index > 0; Index -- )
								{
									if( HistogramBarArray[ Index - 1 ].AllocationCount > 0 )
									{
										SelectedHistogramBarIndex[ SelectedMemoryBankIndex ] = Index - 1;
										SelectHistogramBar( HistogramBarArray[ Index - 1 ] );
										break;
									}
								}

								break;
							}
						}
					}

					return true;
				}

				case Keys.Down:
				{
					if( SubselectedHistogramBar != null )
					{
						int Index = HistogramSelectionBars.IndexOf( SubselectedHistogramBar );
						if( Index != -1 && Index < HistogramSelectionBars.Count - 1 )
						{
							SubselectHistogramBar( HistogramSelectionBars[ Index + 1 ] );
						}
					}
					else if( SelectedHistogramBar != null )
					{	
						for( int BarIndex = 0; BarIndex < HistogramBars.Length; BarIndex++ )
						{
							List<FHistogramBar> HistogramBarArray = HistogramBars[ BarIndex ];
							int Index = HistogramBarArray.IndexOf( SelectedHistogramBar );
							if( Index != -1 && Index < HistogramBarArray.Count - 1 )
							{
								for( ; Index < HistogramBarArray.Count - 2; Index++ )
								{
									if( HistogramBarArray[ Index + 1 ].AllocationCount > 0 )
									{
										SelectedHistogramBarIndex[ SelectedMemoryBankIndex ] = Index + 1;
										SelectHistogramBar( HistogramBarArray[ Index + 1 ] );
										break;
									}
								}

								break;
							}
						}
					}

					return true;
				}
			}

			// Not processed
			return false;
		}

		public static void SelectHistogramBar( FHistogramBar Bar )
		{
			if( Bar != SelectedHistogramBar )
			{
				// Cancel any subselection if the main selection is changing.
				SubselectedHistogramBar = null;
			}

			SelectedHistogramBar = Bar;

			if( SelectedHistogramBar != null )
			{
				HistogramSelectionBars.Clear();
				foreach( FCallStackAllocationInfo AllocationInfo in SelectedHistogramBar.CallStackList )
				{
					int Address = FStreamInfo.GlobalInstance.CallStackArray[ AllocationInfo.CallStackIndex ].AddressIndices[ 0 ];
					string FunctionName = FStreamInfo.GlobalInstance.NameArray[ FStreamInfo.GlobalInstance.CallStackAddressArray[ Address ].FunctionIndex ];

					FHistogramBar AllocBar = new FHistogramBar( FunctionName, SelectedHistogramBar.Colour );
					AllocBar.AddAllocation( AllocationInfo );

					HistogramSelectionBars.Add( AllocBar );
				}
			}

			UpdateHistogramDetails();
			OwnerWindow.HistogramPanel.Invalidate();
		}

		public static void SubselectHistogramBar( FHistogramBar HistogramBar )
		{
			SubselectedHistogramBar = HistogramBar;
			UpdateHistogramDetails();
			OwnerWindow.HistogramPanel.Invalidate();
		}

		private static void UpdateHistogramDetails()
		{
			OwnerWindow.HistogramViewCallStackListView.BeginUpdate();
			OwnerWindow.HistogramViewCallStackListView.Items.Clear();

			FHistogramBar Bar = null;
			if( SubselectedHistogramBar != null )
			{
				Bar = SubselectedHistogramBar;
			}
			else if( SelectedHistogramBar != null )
			{
				Bar = SelectedHistogramBar;
			}

			if( Bar != null )
			{
				if( Bar != SubselectedHistogramBar )
				{
					OwnerWindow.HistogramViewNameLabel.Text = Bar.Description;
				}
				OwnerWindow.HistogramViewSizeLabel.Text = MainWindow.FormatSizeString2( Bar.MemorySize );
				OwnerWindow.HistogramViewAllocationsLabel.Text = Bar.AllocationCount.ToString("N0");

				if( Bar.CallStackList.Count == 1 )
				{
					foreach( int AddressIndex in FStreamInfo.GlobalInstance.CallStackArray[ Bar.CallStackList[ 0 ].CallStackIndex ].AddressIndices )
					{
						string FunctionName = FStreamInfo.GlobalInstance.NameArray[ FStreamInfo.GlobalInstance.CallStackAddressArray[ AddressIndex ].FunctionIndex ];
						OwnerWindow.HistogramViewCallStackListView.Items.Add( FunctionName );
					}
				}
			}

			OwnerWindow.HistogramViewCallStackListView.EndUpdate();
		}
	}

	/// <summary> Encapsulates histogram bar information. </summary>
	public class FHistogramBar
	{
		/// <summary> List of callstack allocations, sorted by size. </summary>
		public List<FCallStackAllocationInfo> CallStackList = new List<FCallStackAllocationInfo>();

		/// <summary> Memory allocated in this bar. </summary>
		public long MemorySize;

		/// <summary> Number of allocation in this bar. </summary>
		public int AllocationCount;

		/// <summary> The class group that this bar is associated with. </summary>
		public ClassGroup CallStackGroup;

		/// <summary> Description of this bar, usually taken from the callstack group. </summary>
		public string Description;

		/// <summary> The colour used to draw this bar, usually taken from the callstack group. </summary>
		public Color Colour;
		
		/// <summary> Rectangle used to draw this bar. </summary>
		public RectangleF Rect;

		/// <summary> > 0 if we are batch adding. See BeginBatchAddition and EndBatchAddition. </summary>
		int BatchAddingCount = 0;

		/// <summary> Default constructor. </summary>
		public FHistogramBar( ClassGroup InCallStackGroup )
		{
			CallStackGroup = InCallStackGroup;
			Description = InCallStackGroup.Name;
			Colour = InCallStackGroup.Color;
		}

		/// <summary> Custom constructor. </summary>
		public FHistogramBar( string InDescription, Color InColour )
		{
			Description = InDescription;
			Colour = InColour;
		}

		/// <summary> Begin the process of adding a batch of new entries to this bar. Calls to AddAllocation will defer the Sort until EndBatchAddition is called. </summary>
		public void BeginBatchAddition()
		{
			++BatchAddingCount;
		}

		/// <summary> End the process of adding a batch of new entries to this bar. Calls Sort to ensure new entries are in the correct order. </summary>
		public void EndBatchAddition()
		{
			if (--BatchAddingCount < 0)
			{
				BatchAddingCount = 0;
			}

			if (BatchAddingCount == 0)
			{
				// Sorting largest -> smallest
				CallStackList.Sort((First, Second) => Math.Sign(First.TotalSize - Second.TotalSize));
			}
		}

		/// <summary> Inserts the new allocation so that the list stays in size order. </summary>
		public void AddAllocation( FCallStackAllocationInfo AllocationInfo )
		{
			bool bInserted = false;

			if (BatchAddingCount == 0)
			{
				for (int Index = 0; Index < CallStackList.Count; Index++)
				{
					if (CallStackList[Index].TotalSize > AllocationInfo.TotalSize)
					{
						CallStackList.Insert(Index, AllocationInfo);
						bInserted = true;
						break;
					}
				}
			}

			if (!bInserted)
			{
				CallStackList.Add(AllocationInfo);
			}

			MemorySize += AllocationInfo.TotalSize;
			AllocationCount += AllocationInfo.TotalCount;
		}
	}
}
