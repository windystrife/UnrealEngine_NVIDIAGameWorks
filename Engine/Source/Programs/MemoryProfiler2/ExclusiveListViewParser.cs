// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Linq;
using System.Diagnostics;

namespace MemoryProfiler2
{
	/// <summary> Class parsing snapshot into a tree from a given root node. </summary>
	public static class FExclusiveListViewParser
	{
        private static string[] UnhelpfulCallSites = new string[]
        {
            "appMalloc",
			"appMalloc(unsigned long, unsigned long)",
            "appRealloc",
			"appRealloc(void*, unsigned long, unsigned long)",
            "appPhysicalAlloc",
			"operator<<"
        };

		/// <summary> Reference to the main memory profiler window. </summary>
		private static MainWindow OwnerWindow;

		public static void SetProfilerWindow( MainWindow InMainWindow )
		{
			OwnerWindow = InMainWindow;
		}

		public static void ClearView()
		{
			OwnerWindow.ExclusiveListView.BeginUpdate();
			OwnerWindow.ExclusiveListView.Items.Clear();
			OwnerWindow.ExclusiveListView.SelectedItems.Clear();
			OwnerWindow.ExclusiveListView.EndUpdate();

			OwnerWindow.ExclusiveSingleCallStackView.BeginUpdate();
			OwnerWindow.ExclusiveSingleCallStackView.Items.Clear();
			OwnerWindow.ExclusiveSingleCallStackView.SelectedItems.Clear();
			OwnerWindow.ExclusiveSingleCallStackView.EndUpdate();
		}

		public static void ParseSnapshot( ListViewEx ExclusiveListView, List<FCallStackAllocationInfo> CallStackList, bool bShouldSortBySize, string FilterText )
		{
			const int MaximumEntries = 400;

			// Progress bar.
			long ProgressInterval = MaximumEntries / 20;
			long NextProgressUpdate = ProgressInterval;
			int CallStackCurrent = 0;
			OwnerWindow.ToolStripProgressBar.Value = 0;
			OwnerWindow.ToolStripProgressBar.Visible = true;

			OwnerWindow.UpdateStatus("Updating exclusive list view for " + OwnerWindow.CurrentFilename);

			ExclusiveListView.BeginUpdate();

			ExclusiveListView.ListViewItemSorter = null; // clear this to avoid a Sort for each call to Add

			bool bFilterIn = OwnerWindow.IsFilteringIn();

			using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FExclusiveListViewParser.ParseSnapshot" ) )
			{
				var FilteredCallstackList = new List<FCallStackAllocationInfo>(CallStackList.Count);
				foreach (var AllocationInfo in CallStackList)
				{
					var FilteredAllocationInfo = AllocationInfo.GetAllocationInfoForTags(OwnerWindow.GetTagsFilter(), bFilterIn);
					if (FilteredAllocationInfo.TotalCount != 0)
					{
						FilteredCallstackList.Add(FilteredAllocationInfo);
					}
				}

				// Sort based on passed in metric.
				if (bShouldSortBySize)
				{
					FilteredCallstackList.Sort(CompareAbsSize);
				}
				else
				{
					FilteredCallstackList.Sort(CompareCount);
				}

				// Figure out total size and count for percentages.
				long TotalSize = 0;
				long TotalCount = 0;
				foreach( FCallStackAllocationInfo AllocationInfo in FilteredCallstackList )
				{
					// Apply optional filter.
					if( FStreamInfo.GlobalInstance.CallStackArray[ AllocationInfo.CallStackIndex ].RunFilters( FilterText, OwnerWindow.Options.ClassGroups, bFilterIn, OwnerWindow.SelectedMemoryPool ) )
					{
						TotalSize += AllocationInfo.TotalSize;
						TotalCount += AllocationInfo.TotalCount;
					}
				}

				// Clear out existing entries and add top 400.
				ExclusiveListView.Items.Clear();
				for( int CallStackIndex = 0; CallStackIndex < FilteredCallstackList.Count && ExclusiveListView.Items.Count <= MaximumEntries; CallStackIndex++ )
				{
					// Update progress bar.
					if( CallStackCurrent >= NextProgressUpdate )
					{
						OwnerWindow.ToolStripProgressBar.PerformStep();
						NextProgressUpdate += ProgressInterval;
						Debug.WriteLine( "FExclusiveListViewParser.ParseSnapshot " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
					}
					CallStackCurrent++;

					FCallStackAllocationInfo AllocationInfo = FilteredCallstackList[ CallStackIndex ];

					// Apply optional filter.
					FCallStack CallStack = FStreamInfo.GlobalInstance.CallStackArray[ AllocationInfo.CallStackIndex ];
					if( CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, bFilterIn, OwnerWindow.SelectedMemoryPool ) )
					{
						string FunctionName = "";
						int FirstStackFrameIndex;
						if( OwnerWindow.ContainersSplitButton.Text == " Show Containers" )
						{
							FirstStackFrameIndex = CallStack.AddressIndices.Count - 1;
						}
						else
						{
							FirstStackFrameIndex = CallStack.FirstNonContainer;
						}

						do
						{
							FCallStackAddress Address = FStreamInfo.GlobalInstance.CallStackAddressArray[ CallStack.AddressIndices[ FirstStackFrameIndex ] ];
							FunctionName = FStreamInfo.GlobalInstance.NameArray[ Address.FunctionIndex ];

							FirstStackFrameIndex--;
						}
						while( UnhelpfulCallSites.Contains( FunctionName ) && FirstStackFrameIndex > 0 );

						var AllocationSize = AllocationInfo.TotalSize;
						var AllocationCount = AllocationInfo.TotalCount;

						string SizeInKByte = String.Format( "{0:0}", ( float )AllocationSize / 1024 ).PadLeft( 10, ' ' );
						string SizePercent = String.Format( "{0:0.00}", ( float )AllocationSize / TotalSize * 100 ).PadLeft( 10, ' ' );
						string Count = String.Format( "{0:0}", AllocationCount ).PadLeft( 10, ' ' );
						string CountPercent = String.Format( "{0:0.00}", ( float )AllocationCount / TotalCount * 100 ).PadLeft( 10, ' ' );
						string GroupName = ( CallStack.Group != null ) ? CallStack.Group.Name : "Ungrouped";

						string[] Row = new string[]
								{
									SizeInKByte,
									SizePercent,
									Count,
									CountPercent,
									GroupName,
			                        FunctionName
								};

						ListViewItem Item = new ListViewItem( Row );
						Item.Tag = AllocationInfo;
						ExclusiveListView.Items.Add( Item );
					}
				}
			}

			var ColumnSorter = new MainWindow.FColumnSorter();
			ColumnSorter.ColumnSortModeAscending = false;
			ColumnSorter.ColumnToSortBy = 0;
			ExclusiveListView.ListViewItemSorter = ColumnSorter; // Assignment automatically calls Sort

			ExclusiveListView.SetSortArrow( ColumnSorter.ColumnToSortBy, ColumnSorter.ColumnSortModeAscending );
			ExclusiveListView.EndUpdate();

			OwnerWindow.ToolStripProgressBar.Visible = false;
		}

		/// <summary> Compare helper function, sorting FCallStackAllocation by size. </summary>
		private static int CompareSize( FCallStackAllocationInfo A, FCallStackAllocationInfo B )
		{
			return Math.Sign( B.TotalSize - A.TotalSize);
		}

		/// <summary> Compare helper function, sorting FCallStackAllocation by abs(size). </summary>
        private static int CompareAbsSize(FCallStackAllocationInfo A, FCallStackAllocationInfo B)
        {
            return Math.Sign(Math.Abs(B.TotalSize) - Math.Abs(A.TotalSize));
        }

		/// <summary> Compare helper function, sorting FCallStackAllocation by count. </summary>
		private static int CompareCount( FCallStackAllocationInfo A, FCallStackAllocationInfo B )
		{
			return Math.Sign( B.TotalCount - A.TotalCount);
		}	
	};
}