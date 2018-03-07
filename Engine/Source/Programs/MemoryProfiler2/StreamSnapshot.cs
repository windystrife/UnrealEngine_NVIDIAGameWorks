// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Linq;

namespace MemoryProfiler2
{
	/// <summary> Holds the size and callstack associated with a live allocation. </summary>
	public struct FLiveAllocationInfo
	{
		/// <summary> Size of allocation. </summary>
		public readonly long Size;

		/// <summary> Index of callstack that performed allocation. </summary>
		public readonly int CallStackIndex;

		/// <summary> Tags index associated with this allocation. </summary>
		public readonly int TagsIndex;

		/// <summary> Constructor, initializing all member variables to passed in values. </summary>
		public FLiveAllocationInfo(long InSize, int InCallStackIndex, int InTagsIndex)
		{
			Size = InSize;
			CallStackIndex = InCallStackIndex;
			TagsIndex = InTagsIndex;
		}
	}

	/// <summary> Holds the allocation size and count associated with a given group of tags. </summary>
	public struct FCallStackTagsAllocationInfo
	{
		/// <summary> Size of allocation. </summary>
		public long Size;

		/// <summary> Number of allocations. </summary>
		public int Count;

		/// <summary> Constructor, initializing all member variables to passed in values. </summary>
		public FCallStackTagsAllocationInfo(long InSize, int InCount)
		{
			Size = InSize;
			Count = InCount;
		}

		public static FCallStackTagsAllocationInfo operator+(FCallStackTagsAllocationInfo LHS, FCallStackTagsAllocationInfo RHS)
		{
			return new FCallStackTagsAllocationInfo(LHS.Size + RHS.Size, LHS.Count + RHS.Count);
		}

		public static FCallStackTagsAllocationInfo operator-(FCallStackTagsAllocationInfo LHS, FCallStackTagsAllocationInfo RHS)
		{
			return new FCallStackTagsAllocationInfo(LHS.Size - RHS.Size, LHS.Count - RHS.Count);
		}
	}

	/// <summary> Helper class encapsulating information for a callstack and associated allocation size. Shared with stream parser. </summary>
	public class FCallStackAllocationInfo
	{
		/// <summary> Index of callstack that performed allocation. </summary>
		public readonly int CallStackIndex;

		/// <summary> Allocation info not associated with any tags. </summary>
		FCallStackTagsAllocationInfo UntaggedAllocationInfo;

		/// <summary> Mapping between a tags index and the associated allocation info. </summary>
		Dictionary<int, FCallStackTagsAllocationInfo> TaggedAllocationInfo;

		/// <summary> Total size of allocation. </summary>
		public long TotalSize
		{
			get
			{
				long TotalSize = UntaggedAllocationInfo.Size;
				if (TaggedAllocationInfo != null)
				{
					foreach (var TaggedAllocationInfoPair in TaggedAllocationInfo)
					{
						TotalSize += TaggedAllocationInfoPair.Value.Size;
					}
				}
				return TotalSize;
			}
		}

		/// <summary> Total number of allocations. </summary>
		public int TotalCount
		{
			get
			{
				int TotalCount = UntaggedAllocationInfo.Count;
				if (TaggedAllocationInfo != null)
				{
					foreach (var TaggedAllocationInfoPair in TaggedAllocationInfo)
					{
						TotalCount += TaggedAllocationInfoPair.Value.Count;
					}
				}
				return TotalCount;
			}
		}

		/// <summary> Constructor, initializing all member variables to passed in values. </summary>
		public FCallStackAllocationInfo( long InSize, int InCallStackIndex, int InCount, int InTagsIndex )
		{
			CallStackIndex = InCallStackIndex;
			UntaggedAllocationInfo = new FCallStackTagsAllocationInfo(0, 0);
			TaggedAllocationInfo = null;

			if (InTagsIndex == -1)
			{
				UntaggedAllocationInfo += new FCallStackTagsAllocationInfo(InSize, InCount);
			}
			else
			{
				EnsureTaggedAllocationsAvailable();
				TaggedAllocationInfo.Add(InTagsIndex, new FCallStackTagsAllocationInfo(InSize, InCount));
			}
		}

		/// <summary> Private constructor used by DeepCopy. </summary>
		private FCallStackAllocationInfo( int InCallStackIndex, FCallStackTagsAllocationInfo InUntaggedAllocationInfo, Dictionary<int, FCallStackTagsAllocationInfo> InTaggedAllocationInfo )
		{
			CallStackIndex = InCallStackIndex;
			UntaggedAllocationInfo = InUntaggedAllocationInfo;
			TaggedAllocationInfo = InTaggedAllocationInfo;
		}

		/// <summary> Performs a deep copy of the relevant data structures. </summary>
		public FCallStackAllocationInfo DeepCopy()
		{
			return new FCallStackAllocationInfo( CallStackIndex, UntaggedAllocationInfo, TaggedAllocationInfo != null ? new Dictionary<int, FCallStackTagsAllocationInfo>(TaggedAllocationInfo) : null );
		}

		/// <summary> Call prior to adding data to TaggedAllocationInfo. </summary>
		private void EnsureTaggedAllocationsAvailable()
		{
			if (TaggedAllocationInfo == null)
			{
				TaggedAllocationInfo = new Dictionary<int, FCallStackTagsAllocationInfo>(16);
			}
		}

		/// <summary>
		/// Adds the passed in size and count to this callstack info.
		/// </summary>
		/// <param name="SizeToAdd"> Size to add </param>
		/// <param name="CountToAdd"> Count to add </param>
		/// <param name="InTagsIndex"> Index of the tags associated with the allocation </param>
		public void Add( long SizeToAdd, int CountToAdd, int InTagsIndex )
        {
			if (InTagsIndex == -1)
			{
				UntaggedAllocationInfo += new FCallStackTagsAllocationInfo(SizeToAdd, CountToAdd);
			}
			else
			{
				EnsureTaggedAllocationsAvailable();

				FCallStackTagsAllocationInfo CurrentTagsAllocationInfo;
				if (!TaggedAllocationInfo.TryGetValue(InTagsIndex, out CurrentTagsAllocationInfo))
				{
					CurrentTagsAllocationInfo = new FCallStackTagsAllocationInfo(0, 0);
				}
				TaggedAllocationInfo[InTagsIndex] = CurrentTagsAllocationInfo + new FCallStackTagsAllocationInfo(SizeToAdd, CountToAdd);
			}
		}

		/// <summary>
		/// Adds the passed callstack info to this callstack info.
		/// </summary>
		/// <param name="InOther"> Callstack info to add </param>
		public void Add(FCallStackAllocationInfo InOther)
		{
			if (CallStackIndex != InOther.CallStackIndex)
			{
				throw new InvalidDataException();
			}

			UntaggedAllocationInfo += InOther.UntaggedAllocationInfo;

			if (InOther.TaggedAllocationInfo != null)
			{
				EnsureTaggedAllocationsAvailable();

				foreach (var TaggedAllocationInfoPair in InOther.TaggedAllocationInfo)
				{
					FCallStackTagsAllocationInfo CurrentTagsAllocationInfo;
					if (!TaggedAllocationInfo.TryGetValue(TaggedAllocationInfoPair.Key, out CurrentTagsAllocationInfo))
					{
						CurrentTagsAllocationInfo = new FCallStackTagsAllocationInfo(0, 0);
					}
					TaggedAllocationInfo[TaggedAllocationInfoPair.Key] = CurrentTagsAllocationInfo + TaggedAllocationInfoPair.Value;
				}
			}
		}

		/// <summary>
		/// Diffs the two passed in callstack infos and returns the difference.
		/// </summary>
		/// <param name="New"> Newer callstack info to subtract older from </param>
		/// <param name="Old"> Older callstack info to subtract from older </param>
		public static FCallStackAllocationInfo Diff( FCallStackAllocationInfo New, FCallStackAllocationInfo Old )
        {
            if( New.CallStackIndex != Old.CallStackIndex )
            {
                throw new InvalidDataException();
            }

			FCallStackAllocationInfo DiffData = New.DeepCopy();

			DiffData.UntaggedAllocationInfo -= Old.UntaggedAllocationInfo;

			if (Old.TaggedAllocationInfo != null)
			{
				DiffData.EnsureTaggedAllocationsAvailable();

				foreach (var TaggedAllocationInfoPair in Old.TaggedAllocationInfo)
				{
					FCallStackTagsAllocationInfo CurrentTagsAllocationInfo;
					if (!DiffData.TaggedAllocationInfo.TryGetValue(TaggedAllocationInfoPair.Key, out CurrentTagsAllocationInfo))
					{
						CurrentTagsAllocationInfo = new FCallStackTagsAllocationInfo(0, 0);
					}
					DiffData.TaggedAllocationInfo[TaggedAllocationInfoPair.Key] = CurrentTagsAllocationInfo - TaggedAllocationInfoPair.Value;
				}
			}

			return DiffData;
        }

		/// <summary>
		/// Get a new callstack info that only contains information about allocations that match the given tags (note: this removes any tag information from the resultant object).
		/// </summary>
		/// <param name="InActiveTags"> Tags to filter on </param>
		/// <param name="bInclusiveFilter"> true to include alloctions that match the tags, false to include allocations that don't match the tags </param>
		public FCallStackAllocationInfo GetAllocationInfoForTags(ISet<FAllocationTag> InActiveTags, bool bInclusiveFilter)
		{
			long TagsSize;
			int TagsCount;
			GetSizeAndCountForTags(InActiveTags, bInclusiveFilter, out TagsSize, out TagsCount);
			return new FCallStackAllocationInfo(TagsSize, CallStackIndex, TagsCount, -1);
		}

		public void GetSizeAndCountForTags(ISet<FAllocationTag> InActiveTags, bool bInclusiveFilter, out long OutSize, out int OutCount)
		{
			OutSize = 0;
			OutCount = 0;

			if (AllocationMatchesTagFilter(FStreamInfo.GlobalInstance.TagHierarchy.GetUntaggedNode().GetTag().Value, InActiveTags, bInclusiveFilter))
			{
				OutSize += UntaggedAllocationInfo.Size;
				OutCount += UntaggedAllocationInfo.Count;
			}

			if (TaggedAllocationInfo != null)
			{
				foreach (var TaggedAllocationInfoPair in TaggedAllocationInfo)
				{
					if (AllocationMatchesTagFilter(TaggedAllocationInfoPair.Key, InActiveTags, bInclusiveFilter))
					{
						OutSize += TaggedAllocationInfoPair.Value.Size;
						OutCount += TaggedAllocationInfoPair.Value.Count;
					}
				}
			}
		}

		private bool AllocationMatchesTagFilter(FAllocationTag InTag, ISet<FAllocationTag> InActiveTags, bool bInclusiveFilter)
		{
			if (InActiveTags == null || InActiveTags.Count == 0)
			{
				// No filter, so match anything
				return true;
			}

			return InActiveTags.Contains(InTag) ? bInclusiveFilter : !bInclusiveFilter;
		}

		private bool AllocationMatchesTagFilter(int InTagsIndex, ISet<FAllocationTag> InActiveTags, bool bInclusiveFilter)
		{
			if (InActiveTags == null || InActiveTags.Count == 0)
			{
				// No filter, so match anything
				return true;
			}

			// Inclusive filter will pass if we match ANY tag
			// Exclusive filter will pass if we match NO tags
			foreach (var AllocTag in FStreamInfo.GlobalInstance.TagsArray[InTagsIndex].Tags)
			{
				if (InActiveTags.Contains(AllocTag))
				{
					return bInclusiveFilter;
				}
			}
			return !bInclusiveFilter;
		}
	}

	public enum ESliceTypesV4
	{
		PlatformUsedPhysical,
		BinnedWasteCurrent,
		BinnedUsedCurrent,
		BinnedSlackCurrent,
		MemoryProfilingOverhead,
		OverallAllocatedMemory,
		Count
	};

	public class FMemorySlice
	{
		public long[] MemoryInfo = null;

		public FMemorySlice( FStreamSnapshot Snapshot )
		{
			MemoryInfo = new long[]
			{
				Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.PlatformUsedPhysical],
				Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.BinnedWasteCurrent],
				Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.BinnedUsedCurrent],
				Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.BinnedSlackCurrent],
				Snapshot.MemoryAllocationStats4[FMemoryAllocationStatsV4.MemoryProfilingOverhead],
				Snapshot.AllocationSize,
			};
		}

		public long GetSliceInfoV4( ESliceTypesV4 SliceType )
		{
			return ( MemoryInfo[(int)SliceType] );
		}
	}

	/// <summary> Snapshot of allocation state at a specific time in token stream. </summary>
	public class FStreamSnapshot
	{
		/// <summary> User defined description of time of snapshot. </summary>
		public string Description;

		/// <summary> List of callstack allocations. </summary>
		public List<FCallStackAllocationInfo> ActiveCallStackList;

		/// <summary> List of lifetime callstack allocations for memory churn. </summary>
        public List<FCallStackAllocationInfo> LifetimeCallStackList;

		/// <summary> Position in the stream. </summary>
        public ulong StreamIndex;

		/// <summary> Frame number. </summary>
        public int FrameNumber;

		/// <summary> Current time. </summary>
		public float CurrentTime;

		/// <summary> Current time starting from the previous snapshot marker. </summary>
		public float ElapsedTime;
        
		/// <summary> Snapshot type. </summary>
		public EProfilingPayloadSubType SubType = EProfilingPayloadSubType.SUBTYPE_SnapshotMarker;

		/// <summary> Index of snapshot. </summary>
        public int SnapshotIndex;

		/// <summary> Platform dependent array of memory metrics. </summary>
        public List<long> MetricArray;

		/// <summary> A list of indices into the name table, one for each loaded level including persistent level. </summary>
		public List<int> LoadedLevels = new List<int>();

		/// <summary> Array of names of all currently loaded levels formated for usage in details view tab. </summary>
		public List<string> LoadedLevelNames = new List<string>();

		/// <summary> Generic memory allocation stats. </summary>
		public FMemoryAllocationStatsV4 MemoryAllocationStats4 = new FMemoryAllocationStatsV4();

		/// <summary> Running count of number of allocations. </summary>
		public long AllocationCount = 0;

		/// <summary> Running total of size of allocations. </summary>
		public long AllocationSize = 0;

		/// <summary> Running total of size of allocations. </summary>
		public long AllocationMaxSize = 0;

		/// <summary> True if this snapshot was created as a diff of two snapshots. </summary>
        public bool bIsDiffResult;

		/// <summary> Running total of allocated memory. </summary>
		public List<FMemorySlice> OverallMemorySlice;

		/// <summary> Constructor, naming the snapshot and initializing map. </summary>
		public FStreamSnapshot( string InDescription )
		{
			Description = InDescription;
			ActiveCallStackList = new List<FCallStackAllocationInfo>();
            // Presize lifetime callstack array and populate.
            LifetimeCallStackList = new List<FCallStackAllocationInfo>( FStreamInfo.GlobalInstance.CallStackArray.Count );
            for( int CallStackIndex=0; CallStackIndex<FStreamInfo.GlobalInstance.CallStackArray.Count; CallStackIndex++ )
            {
                LifetimeCallStackList.Add( new FCallStackAllocationInfo( 0, CallStackIndex, 0, -1 ) );
            }
			OverallMemorySlice = new List<FMemorySlice>();
		}

		/// <summary> Performs a deep copy of the relevant data structures. </summary>
		public FStreamSnapshot DeepCopy( Dictionary<ulong, FLiveAllocationInfo> PointerToPointerInfoMap )
		{
			// Create new snapshot object.
			FStreamSnapshot Snapshot = new FStreamSnapshot( "Copy" );

			// Manually perform a deep copy of LifetimeCallstackList
			foreach( FCallStackAllocationInfo AllocationInfo in LifetimeCallStackList )
			{
				Snapshot.LifetimeCallStackList[ AllocationInfo.CallStackIndex ] = AllocationInfo.DeepCopy();
			}

			Snapshot.AllocationCount = AllocationCount;
			Snapshot.AllocationSize = AllocationSize;
			Snapshot.AllocationMaxSize = AllocationMaxSize;

			Snapshot.FinalizeSnapshot( PointerToPointerInfoMap );

			// Return deep copy of this snapshot.
			return Snapshot;
		}

		public void FillActiveCallStackList( Dictionary<ulong, FLiveAllocationInfo> PointerToPointerInfoMap )
		{
			ActiveCallStackList.Clear();
			ActiveCallStackList.Capacity = LifetimeCallStackList.Count;

			foreach( var PointerData in PointerToPointerInfoMap )
			{
				// make sure allocationInfoList is big enough
				while(PointerData.Value.CallStackIndex >= ActiveCallStackList.Count )
				{
					ActiveCallStackList.Add( new FCallStackAllocationInfo( 0, ActiveCallStackList.Count, 0, -1 ) );
				}

				ActiveCallStackList[PointerData.Value.CallStackIndex].Add(PointerData.Value.Size, 1, PointerData.Value.TagsIndex);
			}

			// strip out any callstacks with no allocations
			ActiveCallStackList.RemoveAll(Item => Item.TotalCount == 0);
			ActiveCallStackList.TrimExcess();
		}

		/// <summary> Convert "callstack to allocation" mapping (either passed in or generated from pointer map) to callstack info list. </summary>
		public void FinalizeSnapshot( Dictionary<ulong, FLiveAllocationInfo> PointerToPointerInfoMap )
		{
			FillActiveCallStackList( PointerToPointerInfoMap );
		}

		/// <summary> Diffs two snapshots and creates a result one. </summary>
		public static FStreamSnapshot DiffSnapshots( FStreamSnapshot Old, FStreamSnapshot New )
		{
			// Create result snapshot object.
			FStreamSnapshot ResultSnapshot = new FStreamSnapshot( "Diff " + Old.Description + " <-> " + New.Description );

			using( FScopedLogTimer LoadingTime = new FScopedLogTimer( "FStreamSnapshot.DiffSnapshots" ) )
			{
				// Copy over allocation count so we can track where the graph starts
				ResultSnapshot.AllocationCount = Old.AllocationCount;

				Debug.Assert( Old.MetricArray.Count == New.MetricArray.Count );
				ResultSnapshot.MetricArray = new List<long>( Old.MetricArray.Count );
				for( int CallstackIndex = 0; CallstackIndex < Old.MetricArray.Count; CallstackIndex++ )
				{
					ResultSnapshot.MetricArray.Add( New.MetricArray[ CallstackIndex ] - Old.MetricArray[ CallstackIndex ] );
				}

				ResultSnapshot.MemoryAllocationStats4 = FMemoryAllocationStatsV4.Diff( Old.MemoryAllocationStats4, New.MemoryAllocationStats4 );

				ResultSnapshot.StreamIndex = New.StreamIndex;
				ResultSnapshot.bIsDiffResult = true;

				ResultSnapshot.AllocationMaxSize = New.AllocationMaxSize - Old.AllocationMaxSize;
				ResultSnapshot.AllocationSize = New.AllocationSize - Old.AllocationSize;
				ResultSnapshot.CurrentTime = 0;
				ResultSnapshot.ElapsedTime = New.CurrentTime - Old.CurrentTime;
				ResultSnapshot.FrameNumber = New.FrameNumber - Old.FrameNumber;
				ResultSnapshot.LoadedLevels = New.LoadedLevels;

				// These lists are guaranteed to be sorted by callstack index.
				List<FCallStackAllocationInfo> OldActiveCallStackList = Old.ActiveCallStackList;
				List<FCallStackAllocationInfo> NewActiveCallStackList = New.ActiveCallStackList;
				List<FCallStackAllocationInfo> ResultActiveCallStackList = new List<FCallStackAllocationInfo>( FStreamInfo.GlobalInstance.CallStackArray.Count );

				int OldIndex = 0;
				int NewIndex = 0;
				while( true )
				{
					FCallStackAllocationInfo OldAllocInfo = OldActiveCallStackList[ OldIndex ];
					FCallStackAllocationInfo NewAllocInfo = NewActiveCallStackList[ NewIndex ];

					if( OldAllocInfo.CallStackIndex == NewAllocInfo.CallStackIndex )
					{
						long ResultSize = NewAllocInfo.TotalSize - OldAllocInfo.TotalSize;
						int ResultCount = NewAllocInfo.TotalCount - OldAllocInfo.TotalCount;

						if( ResultSize != 0 || ResultCount != 0 )
						{
							ResultActiveCallStackList.Add( new FCallStackAllocationInfo( ResultSize, NewAllocInfo.CallStackIndex, ResultCount, -1 ) );
						}

						OldIndex++;
						NewIndex++;
					}
					else if( OldAllocInfo.CallStackIndex > NewAllocInfo.CallStackIndex )
					{
						ResultActiveCallStackList.Add( NewAllocInfo );
						NewIndex++;
					}
					else // OldAllocInfo.CallStackIndex < NewAllocInfo.CallStackIndex
					{
						ResultActiveCallStackList.Add( new FCallStackAllocationInfo( -OldAllocInfo.TotalSize, OldAllocInfo.CallStackIndex, -OldAllocInfo.TotalCount, -1 ) );
						OldIndex++;
					}

					if( OldIndex >= OldActiveCallStackList.Count )
					{
						for( ; NewIndex < NewActiveCallStackList.Count; NewIndex++ )
						{
							ResultActiveCallStackList.Add( NewActiveCallStackList[ NewIndex ] );
						}
						break;
					}

					if( NewIndex >= NewActiveCallStackList.Count )
					{
						for( ; OldIndex < OldActiveCallStackList.Count; OldIndex++ )
						{
							ResultActiveCallStackList.Add( OldActiveCallStackList[ OldIndex ] );
						}
						break;
					}
				}

				// Check that list was correctly constructed.
				for( int CallstackIndex = 0; CallstackIndex < ResultActiveCallStackList.Count - 1; CallstackIndex++ )
				{
					Debug.Assert( ResultActiveCallStackList[ CallstackIndex ].CallStackIndex < ResultActiveCallStackList[ CallstackIndex + 1 ].CallStackIndex );
				}

				ResultActiveCallStackList.TrimExcess();
				ResultSnapshot.ActiveCallStackList = ResultActiveCallStackList;

				// Iterate over new lifetime callstack info and subtract previous one.
				for( int CallStackIndex = 0; CallStackIndex < New.LifetimeCallStackList.Count; CallStackIndex++ )
				{
					ResultSnapshot.LifetimeCallStackList[ CallStackIndex ] = FCallStackAllocationInfo.Diff(
																					New.LifetimeCallStackList[ CallStackIndex ],
																					Old.LifetimeCallStackList[ CallStackIndex ] );
				}

				// Handle overall memory timeline
				if( New.OverallMemorySlice.Count > Old.OverallMemorySlice.Count )
				{
					ResultSnapshot.OverallMemorySlice = new List<FMemorySlice>( New.OverallMemorySlice );
					ResultSnapshot.OverallMemorySlice.RemoveRange( 0, Old.OverallMemorySlice.Count );
				}
				else
				{
					ResultSnapshot.OverallMemorySlice = new List<FMemorySlice>( Old.OverallMemorySlice );
					ResultSnapshot.OverallMemorySlice.RemoveRange( 0, New.OverallMemorySlice.Count );
					ResultSnapshot.OverallMemorySlice.Reverse();
				}

			}

			return ResultSnapshot;
		}

		/// <summary> Exports this snapshot to a CSV file of the passed in name. </summary>
		/// <param name="FileName"> File name to export to </param>
		/// <param name="bShouldExportActiveAllocations"> Whether to export active allocations or lifetime allocations </param>
		public void ExportToCSV( string FileName, bool bShouldExportActiveAllocations )
		{
			// Create stream writer used to output call graphs to CSV.
			StreamWriter CSVWriter = new StreamWriter(FileName);

            // Figure out which list to export.
            List<FCallStackAllocationInfo> CallStackList = null;
            if( bShouldExportActiveAllocations )
            {
                CallStackList = ActiveCallStackList;
            }
            else
            {
                CallStackList = LifetimeCallStackList;
            }

			// Iterate over each unique call graph and spit it out. The sorting is per call stack and not
			// allocation size. Excel can be used to sort by allocation if needed. This sorting is more robust
			// and also what the call graph parsing code needs.
			foreach( FCallStackAllocationInfo AllocationInfo in CallStackList )
			{
                // Skip callstacks with no contribution in this snapshot.
                if( AllocationInfo.TotalCount > 0 )
                {
				    // Dump size first, followed by count.
				    CSVWriter.Write(AllocationInfo.TotalSize + "," + AllocationInfo.TotalCount + ",");

				    // Iterate over ach address in callstack and dump to CSV.
				    FCallStack CallStack = FStreamInfo.GlobalInstance.CallStackArray[AllocationInfo.CallStackIndex];
				    foreach( int AddressIndex in CallStack.AddressIndices )
				    {
					    FCallStackAddress Address = FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndex];
					    string SymbolFunctionName = FStreamInfo.GlobalInstance.NameArray[Address.FunctionIndex];
					    string SymbolFileName = FStreamInfo.GlobalInstance.NameArray[Address.FilenameIndex];
					    // Write out function followed by filename and then line number if valid
					    if( SymbolFunctionName != "" || SymbolFileName != "" )
					    {
							CSVWriter.Write( SymbolFunctionName + " @ " + SymbolFileName + ":" + Address.LineNumber + "," );
					    }
					    else
					    {
						    CSVWriter.Write("Unknown,");
					    }
				    }
				    CSVWriter.WriteLine("");
                }
			}

			// Close the file handle now that we're done writing.
			CSVWriter.Close();
		}

		/// <summary> Exports this snapshots internal properties into a human readable format. </summary>
		public override string ToString()
		{
			StringBuilder StrBuilder = new StringBuilder( 1024 );

			///// <summary> User defined description of time of snapshot. </summary>
			//public string Description;
			StrBuilder.AppendLine( "Description: " + Description );

			///// <summary> List of callstack allocations. </summary>
			//public List<FCallStackAllocationInfo> ActiveCallStackList;

			///// <summary> List of lifetime callstack allocations for memory churn. </summary>
			//public List<FCallStackAllocationInfo> LifetimeCallStackList;

			///// <summary> Position in the stream. </summary>
			//public ulong StreamIndex;

			StrBuilder.AppendLine( "Stream Index: " + StreamIndex );

			///// <summary> Frame number. </summary>
			//public int FrameNumber;
			StrBuilder.AppendLine( "Frame Number: " + FrameNumber );

			///// <summary> Current time. </summary>
			//public float CurrentTime;
			StrBuilder.AppendLine( "Current Time: " + CurrentTime + " seconds" );

			///// <summary> Current time starting from the previous snapshot marker. </summary>
			//public float ElapsedTime;
			StrBuilder.AppendLine( "Elapsed Time: " + ElapsedTime + " seconds" );

			///// <summary> Snapshot type. </summary>
			//public EProfilingPayloadSubType SubType = EProfilingPayloadSubType.SUBTYPE_SnapshotMarker;
			StrBuilder.AppendLine( "Snapshot Type: " + SubType.ToString() );

			///// <summary> Index of snapshot. </summary>
			//public int SnapshotIndex;
			StrBuilder.AppendLine( "Snapshot Index: " + SnapshotIndex );

			///// <summary> Array of names of all currently loaded levels formated for usage in details view tab. </summary>
			//public List<string> LoadedLevelNames = new List<string>();
			StrBuilder.AppendLine( "Loaded Levels: " + LoadedLevels.Count );
			foreach( string LevelName  in LoadedLevelNames )
			{
				StrBuilder.AppendLine( "	" + LevelName );
			}

			///// <summary> Generic memory allocation stats. </summary>
			//public FMemoryAllocationStats MemoryAllocationStats = new FMemoryAllocationStats();

			StrBuilder.AppendLine( "Memory Allocation Stats: " );
			StrBuilder.Append( MemoryAllocationStats4.ToString() );
			
			///// <summary> Running count of number of allocations. </summary>
			//public long AllocationCount = 0;
			StrBuilder.AppendLine( "Allocation Count: " + AllocationCount.ToString("N0") );

			///// <summary> Running total of size of allocations. </summary>
			//public long AllocationSize = 0;
			StrBuilder.AppendLine( "Allocation Size: " + MainWindow.FormatSizeString2( AllocationSize ) );

			///// <summary> Running total of size of allocations. </summary>
			//public long AllocationMaxSize = 0;
			StrBuilder.AppendLine( "Allocation Max Size: " + MainWindow.FormatSizeString2( AllocationMaxSize ) );

			///// <summary> True if this snapshot was created as a diff of two snapshots. </summary>
			//public bool bIsDiffResult;
			StrBuilder.AppendLine( "Is Diff Result: " + bIsDiffResult );

			///// <summary> Running total of allocated memory. </summary>
			//public List<FMemorySlice> OverallMemorySlice;
			//StrBuilder.AppendLine( "Overall Memory Slice: @TODO"  );

			return StrBuilder.ToString();
		}

		/// <summary> Encapsulates indices to levels in the diff snapshots in relation to start and end snapshot. </summary>
		struct FLevelIndex
		{
			public FLevelIndex( int InLeftIndex, int InRightIndex )
			{
				LeftIndex = InLeftIndex;
				RightIndex = InRightIndex;
			}

			public override string ToString()
			{
				return LeftIndex + " <-> " + RightIndex;
			}

			public int LeftIndex;
			public int RightIndex;
		}

		/// <summary> 
		/// Prepares three array with level names. Arrays will be placed into LoadedLevelNames properties of each of snapshots.
		/// "-" in level name means that level was unloaded.
		/// "+" in level name means that level was loaded.
		/// </summary>
		public static void PrepareLevelNamesForDetailsTab( FStreamSnapshot LeftSnapshot, FStreamSnapshot DiffSnapshot, FStreamSnapshot RightSnapshot )
		{
			if( DiffSnapshot != null && LeftSnapshot != null && RightSnapshot != null )
			{
				LeftSnapshot.LoadedLevelNames.Clear();
				DiffSnapshot.LoadedLevelNames.Clear();
				RightSnapshot.LoadedLevelNames.Clear();

				List<FLevelIndex> LevelIndexArray = new List<FLevelIndex>( LeftSnapshot.LoadedLevels.Count + RightSnapshot.LoadedLevels.Count );

				List<int> AllLevelIndexArray = new List<int>( LeftSnapshot.LoadedLevels );
				AllLevelIndexArray.AddRange( RightSnapshot.LoadedLevels );

				IEnumerable<int> AllLevelIndicesDistinct = AllLevelIndexArray.Distinct();

				foreach( int LevelIndex in AllLevelIndicesDistinct )
				{
					int StartLevelIndex = LeftSnapshot.LoadedLevels.IndexOf( LevelIndex );
					int EndLevelIndex = RightSnapshot.LoadedLevels.IndexOf( LevelIndex );

					LevelIndexArray.Add( new FLevelIndex( StartLevelIndex, EndLevelIndex ) );
				}

				foreach( FLevelIndex LevelIndex in LevelIndexArray )
				{
					string LeftLevelName = "";
					string DiffLevelName = "";
					string RightLevelName = "";

					if( LevelIndex.LeftIndex != -1 )
					{
						LeftLevelName = FStreamInfo.GlobalInstance.NameArray[ LeftSnapshot.LoadedLevels[ LevelIndex.LeftIndex ] ];
					}

					if( LevelIndex.RightIndex != -1 )
					{
						RightLevelName = FStreamInfo.GlobalInstance.NameArray[ RightSnapshot.LoadedLevels[ LevelIndex.RightIndex ] ];
					}

					if( LevelIndex.LeftIndex != -1 && LevelIndex.RightIndex == -1 )
					{
						DiffLevelName = "-" + LeftLevelName;
					}
					else if( LevelIndex.LeftIndex == -1 && LevelIndex.RightIndex != -1 )
					{
						DiffLevelName = "+" + RightLevelName;
					}
					else if( LevelIndex.LeftIndex != -1 && LevelIndex.RightIndex != -1 )
					{
						DiffLevelName = " " + RightLevelName;
					}

					LeftSnapshot.LoadedLevelNames.Add( LeftLevelName );
					DiffSnapshot.LoadedLevelNames.Add( DiffLevelName );
					RightSnapshot.LoadedLevelNames.Add( RightLevelName );
				}
			}
			else if( LeftSnapshot != null )
			{
				LeftSnapshot.LoadedLevelNames.Clear();
				foreach( int LevelIndex in LeftSnapshot.LoadedLevels )
				{
					LeftSnapshot.LoadedLevelNames.Add( FStreamInfo.GlobalInstance.NameArray[ LevelIndex ] );
				}

			}
			else if( RightSnapshot != null )
			{
				RightSnapshot.LoadedLevelNames.Clear();
				foreach( int LevelIndex in RightSnapshot.LoadedLevels )
				{
					RightSnapshot.LoadedLevelNames.Add( FStreamInfo.GlobalInstance.NameArray[ LevelIndex ] );
				}
			}
		}
	};
}