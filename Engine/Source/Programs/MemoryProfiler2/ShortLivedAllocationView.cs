// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;

namespace MemoryProfiler2
{
	public static class FShortLivedAllocationView
	{
		private static MainWindow OwnerWindow;

		private static int ColumnToSortBy = 3;

		private static bool ColumnSortModeAscending = false;

		private static List<int> ColumnSortOrder = new List<int> { 3, 4, 2, 0, 1 };

		private class FShortLivedListViewComparer : IComparer
		{
			public int Compare(object A, object B)
			{
				FShortLivedCallStackTag AValues = (FShortLivedCallStackTag)((ListViewItem)A).Tag;
				FShortLivedCallStackTag BValues = (FShortLivedCallStackTag)((ListViewItem)B).Tag;

				foreach (int ColumnIndex in ColumnSortOrder)
				{
					if (AValues.ColumnValues[ColumnIndex] < BValues.ColumnValues[ColumnIndex])
					{
						return ColumnSortModeAscending ? -1 : 1;
					}
					else if (AValues.ColumnValues[ColumnIndex] > BValues.ColumnValues[ColumnIndex])
					{
						return ColumnSortModeAscending ? 1 : -1;
					}
				}

				return 0;
			}
		}

		public static void SetProfilerWindow(MainWindow InMainWindow)
		{
			OwnerWindow = InMainWindow;
		}

		public static void ClearView()
		{
			OwnerWindow.ShortLivedListView.BeginUpdate();
			OwnerWindow.ShortLivedListView.Items.Clear();
			OwnerWindow.ShortLivedListView.SelectedItems.Clear();
			OwnerWindow.ShortLivedListView.EndUpdate();

			OwnerWindow.ShortLivedCallStackListView.BeginUpdate();
			OwnerWindow.ShortLivedCallStackListView.Items.Clear();
			OwnerWindow.ShortLivedCallStackListView.SelectedItems.Clear();
			OwnerWindow.ShortLivedCallStackListView.EndUpdate();

			ColumnToSortBy = 3;
			ColumnSortModeAscending = false;
			ColumnSortOrder = new List<int> { 3, 4, 2, 0, 1 };
		}

		public static void ParseSnapshot( string FilterText )
		{
			if( !FStreamInfo.GlobalInstance.CreationOptions.KeepLifecyclesCheckBox.Checked )
			{
				return;
			}

			// Progress bar
			long ProgressInterval = FStreamInfo.GlobalInstance.CallStackArray.Count / 20;
			long NextProgressUpdate = ProgressInterval;
			int CallStackCurrent = 0;
			OwnerWindow.ToolStripProgressBar.Value = 0;
			OwnerWindow.ToolStripProgressBar.Visible = true;
			OwnerWindow.UpdateStatus( "Updating short lived allocation view for " + OwnerWindow.CurrentFilename );

			OwnerWindow.ShortLivedListView.BeginUpdate();
			OwnerWindow.ShortLivedListView.Items.Clear();

			OwnerWindow.ShortLivedListView.ListViewItemSorter = null; // clear this to avoid a Sort for each call to Add

			const int MaxLifetime = 1;
			const int MinAllocations = 100;

			ulong StartStreamIndex = OwnerWindow.GetStartSnapshotStreamIndex();
			ulong EndStreamIndex = OwnerWindow.GetEndSnapshotStreamIndex();

			uint[] AllocationLifetimes = new uint[ MaxLifetime + 1 ];

			using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "FShortLivedAllocationView.ParseSnapshot" ) )
			{
				foreach( FCallStack CallStack in FStreamInfo.GlobalInstance.CallStackArray )
				{
					// Update progress bar.
					if( CallStackCurrent >= NextProgressUpdate )
					{
						OwnerWindow.ToolStripProgressBar.PerformStep();
						NextProgressUpdate += ProgressInterval;
						Debug.WriteLine( "FShortLivedAllocationView.ParseSnapshot " + OwnerWindow.ToolStripProgressBar.Value + "/20" );
					}
					CallStackCurrent++;

					if( CallStack.RunFilters( FilterText, OwnerWindow.Options.ClassGroups, OwnerWindow.IsFilteringIn(), OwnerWindow.SelectedMemoryPool ) )
					{
						Array.Clear( AllocationLifetimes, 0, AllocationLifetimes.Length );

						int NumAllocations = 0;
						uint UniqueFramesWithAge0Allocs = 0;
						int LastFrameWithAge0Allocs = 0;

						int CurrentRun = 0;
						float CurrentTotalAllocSize = 0;
						int CurrentRunFrame = -1;
						int LongestRun = 0;
						float LongestRunTotalAllocSize = 0;

						int LastEndFrame = 1;
						int LastSnapshot = 0;
						foreach( FAllocationLifecycle Lifecycle in CallStack.CompleteLifecycles )
						{
							// only process allocations that were really freed, not just realloced
							if( Lifecycle.FreeStreamIndex != FStreamInfo.INVALID_STREAM_INDEX
								&& Lifecycle.AllocEvent.StreamIndex > StartStreamIndex && Lifecycle.FreeStreamIndex < EndStreamIndex )
							{
								// CompleteLifecycles are sorted by FreeStreamIndex, so this search pattern ensures
								// that the frames will be found as quickly as possible.
								int EndFrame;
								if( Lifecycle.FreeStreamIndex < FStreamInfo.GlobalInstance.FrameStreamIndices[ LastEndFrame ] )
								{
									EndFrame = LastEndFrame;
								}
								else
								{
									if( Lifecycle.FreeStreamIndex > FStreamInfo.GlobalInstance.SnapshotList[ LastSnapshot ].StreamIndex )
									{
										// lifecycle isn't even in same snapshot, so search by snapshot first (much faster than
										// searching through frames one by one)
										LastSnapshot = OwnerWindow.GetSnapshotIndexFromStreamIndex( LastSnapshot, Lifecycle.FreeStreamIndex );

										if( LastSnapshot == 0 )
										{
											LastEndFrame = 1;
										}
										else
										{
											LastEndFrame = FStreamInfo.GlobalInstance.SnapshotList[ LastSnapshot - 1 ].FrameNumber;
										}
									}

									EndFrame = FStreamInfo.GlobalInstance.GetFrameNumberFromStreamIndex( LastEndFrame, Lifecycle.FreeStreamIndex );
								}

								int StartFrame = EndFrame;
								while( FStreamInfo.GlobalInstance.FrameStreamIndices[ StartFrame ] > Lifecycle.AllocEvent.StreamIndex
										   && StartFrame > 0 && EndFrame - StartFrame <= MaxLifetime + 1 )
								{
									StartFrame--;
								}
								StartFrame++;

								int Age = EndFrame - StartFrame;

								if( Age <= MaxLifetime )
								{
									AllocationLifetimes[ Age ]++;
									NumAllocations++;

									if( Age == 0 )
									{
										if( StartFrame != LastFrameWithAge0Allocs )
										{
											UniqueFramesWithAge0Allocs++;
											LastFrameWithAge0Allocs = StartFrame;
										}
									}
									else if( Age == 1 )
									{
										if( StartFrame == CurrentRunFrame )
										{
											CurrentRun++;

											CurrentTotalAllocSize += Lifecycle.PeakSize;

											if( CurrentRun > LongestRun )
											{
												LongestRun = CurrentRun;
												LongestRunTotalAllocSize = CurrentTotalAllocSize;
											}
										}
										else if( StartFrame > CurrentRunFrame )
										{
											CurrentRun = 1;
											CurrentTotalAllocSize = Lifecycle.PeakSize;
										}
										else if( EndFrame == CurrentRunFrame )
										{
											CurrentTotalAllocSize += Lifecycle.PeakSize;

											if( CurrentRun == LongestRun && LongestRunTotalAllocSize < CurrentTotalAllocSize )
											{
												LongestRunTotalAllocSize = CurrentTotalAllocSize;
											}
										}

										CurrentRunFrame = EndFrame;
									}
								}
							}
						}

						if( NumAllocations > MinAllocations )
						{
							float LongestRunAvgAllocSize = LongestRun == 0 ? 0 : LongestRunTotalAllocSize / LongestRun;

							uint[] ColumnValues = new uint[5];
							ColumnValues[0] = AllocationLifetimes[0];
							ColumnValues[1] = UniqueFramesWithAge0Allocs;
							ColumnValues[2] = AllocationLifetimes[1];
							ColumnValues[3] = (uint)LongestRun;
							ColumnValues[4] = (uint)LongestRunAvgAllocSize;

							var LVItem = new ListViewItem();

							LVItem.Tag = new FShortLivedCallStackTag(CallStack, ColumnValues);

							LVItem.Text = ColumnValues[0].ToString();
							for (int ValueIndex = 1; ValueIndex < ColumnValues.Length; ValueIndex++)
							{
								LVItem.SubItems.Add(ColumnValues[ValueIndex].ToString());
							}

							OwnerWindow.ShortLivedListView.Items.Add(LVItem);
						}
					}
				}
			}

			OwnerWindow.ShortLivedListView.ListViewItemSorter = new FShortLivedListViewComparer(); // Assignment automatically calls Sort
			OwnerWindow.ShortLivedListView.SetSortArrow(ColumnToSortBy, ColumnSortModeAscending);

			OwnerWindow.ShortLivedListView.EndUpdate();

			OwnerWindow.ToolStripProgressBar.Visible = false;
		}

		public static void ListColumnClick( ColumnClickEventArgs e )
		{
			if (OwnerWindow.ShortLivedListView.Items.Count == 0)
			{
				return;
			}

			if (e.Column == ColumnToSortBy)
			{
				ColumnSortModeAscending = !ColumnSortModeAscending;
			}
			else
			{
				ColumnToSortBy = e.Column;
				ColumnSortModeAscending = false;
			}

			// Move the current sort column to the start of the sort list
			ColumnSortOrder.RemoveAll(x => x == ColumnToSortBy);
			ColumnSortOrder.Insert(0, ColumnToSortBy);

			OwnerWindow.ShortLivedListView.BeginUpdate();
			OwnerWindow.ShortLivedListView.SetSortArrow(ColumnToSortBy, ColumnSortModeAscending);
			OwnerWindow.ShortLivedListView.Sort();
			OwnerWindow.ShortLivedListView.EndUpdate();
		}
	}

	/// <summary> Used as the tag for items in the MainMProfWindow.ShortLivedListView and others. </summary>
	public class FShortLivedCallStackTag
	{
		public FCallStack CallStack;
		public uint[] ColumnValues;

		public FShortLivedCallStackTag( FCallStack InCallStack, uint[] InColumnValues )
		{
			CallStack = InCallStack;
			ColumnValues = InColumnValues;
		}
	}
}
