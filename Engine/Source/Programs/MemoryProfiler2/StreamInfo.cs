// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace MemoryProfiler2
{
	/// <summary> Helper class containing information shared for all snapshots of a given stream. </summary>
	public class FStreamInfo
	{
		public const ulong INVALID_STREAM_INDEX = ulong.MaxValue;

		/// <summary> Global public stream info so we don't need to pass it around. </summary>
		public static FStreamInfo GlobalInstance = null;

		/// <summary> File name associated with this stream. </summary>
		public string FileName;

		/// <summary> Meta-data associated with this stream. </summary>
		public Dictionary<string, string> MetaData;

		/// <summary> Array of unique tags. Code has fixed indexes into it. </summary>
		public List<FAllocationTags> TagsArray;

		/// <summary> Hierarchy of tags built as they are parsed. </summary>
		public FAllocationTagHierarchy TagHierarchy = new FAllocationTagHierarchy();

		/// <summary> Array of unique names. Code has fixed indexes into it. </summary>
		private List<string> InternalNameArray;
		private Dictionary<string, int> InternalNameIndexLookupMap;
		public IList<string> NameArray
		{
			get
			{
				return InternalNameArray.AsReadOnly();
			}
		}

		/// <summary> Array of unique names used for script, object types, etc... (FName table from runtime). </summary>
		public List<string> ScriptNameArray;

		/// <summary> Array of unique call stacks. Code has fixed indexes into it. </summary>
		public List<FCallStack> CallStackArray;

		/// <summary> Array of unique call stack addresses. Code has fixed indexes into it. </summary>
		public List<FCallStackAddress> CallStackAddressArray;

		/// <summary> Proper symbol parser will be created based on platform. </summary>
		public ISymbolParser SymbolParser = null;

		/// <summary> Memory pool ranges for all pools. </summary>
		public FMemoryPoolInfoCollection MemoryPoolInfo = new FMemoryPoolInfoCollection();

		/// <summary> Array of frame indices located in the stream. </summary>
		public List<ulong> FrameStreamIndices = new List<ulong>();

		// 2011-Sep-08, 16:20 JarekS@TODO This should be replaced with GCurrentTime for more accurate results??
		/// <summary> Array of delta time for all frames. </summary> 
		public List<float> DeltaTimeArray = new List<float>(); 

		/// <summary> List of snapshots created by parser and used by various visualizers and dumping tools. </summary>
		public List<FStreamSnapshot> SnapshotList = new List<FStreamSnapshot>();

		/// <summary> Platform that was captured V4. </summary>
		public string PlatformName;

		/// <summary> Profiling options, default all options are enabled. UI is not provided yet. </summary>
		public ProfilingOptions CreationOptions;

		/// <summary> Array of script callstacks. </summary>
		public List<FScriptCallStack> ScriptCallstackArray;

		/// <summary> Index of UObject::ProcessInternal function in the main names array. </summary>
		public int ProcessInternalNameIndex;

		/// <summary> Index of UObject::StaticAllocateObject function in the main names array. </summary>
		public int StaticAllocateObjectNameIndex;

		/// <summary>
		/// Array of indices to all functions related to UObject Virtual Machine in the main names array.
		/// This array includes following functions:
		/// UObject::exec*
		/// UObject::CallFunction
		/// UObject::ProcessEvent
		/// </summary>
		public List<int> ObjectVMFunctionIndexArray = new List<int>( 64 );

		/// <summary> Mapping from the script-function name ([Script] package.class:function) in the main names array to the index in the main callstacks array. </summary>
		public Dictionary<int, int> ScriptCallstackMapping = new Dictionary<int, int>();

		/// <summary> Mapping from the script-object name ([Type] UObject::StaticAllocateObject) in the main names array to the index in the main callstacks array. </summary>
		public Dictionary<int, FScriptObjectType> ScriptObjectTypeMapping = new Dictionary<int, FScriptObjectType>();

		/// <summary> True if some callstacks has been allocated to multiple pools. </summary>
		public bool bHasMultiPoolCallStacks;

		/// <summary> Constructor, associating this stream info with a filename. </summary>
		/// <param name="InFileName"> FileName to use for this stream </param>
		public FStreamInfo( string InFileName, ProfilingOptions InCreationOptions )
		{
			FileName = InFileName;
			CreationOptions = new ProfilingOptions( InCreationOptions, true );
		}

		/// <summary> Initializes and sizes the arrays. Size is known as soon as header is serialized. </summary>
		public void Initialize( FProfileDataHeader Header )
		{
			PlatformName = Header.PlatformName;
			MetaData = new Dictionary<string, string>( (int)Header.MetaDataTableEntries );
			TagsArray = new List<FAllocationTags>( (int)Header.TagsTableEntries );
			InternalNameArray = new List<string>( (int)Header.NameTableEntries );
			InternalNameIndexLookupMap = new Dictionary<string, int>( (int)Header.NameTableEntries );
			CallStackArray = new List<FCallStack>( (int)Header.CallStackTableEntries );
			CallStackAddressArray = new List<FCallStackAddress>( (int)Header.CallStackAddressTableEntries );
		}

		/// <summary> 
		/// Returns index of the name, if the name doesn't exit creates a new one if bCreateIfNonExistent is true
		/// </summary>
		public int GetNameIndex( string Name, bool bCreateIfNonExistent )
		{
			// This is the only method where there should be concurrent access of NameArray, so
			// don't worry about locking it anywhere else.
			lock(InternalNameArray)
			{
				int NameIndex;
				if (!InternalNameIndexLookupMap.TryGetValue(Name, out NameIndex))
				{
					NameIndex = -1;
				}

				if (NameIndex == -1 && bCreateIfNonExistent)
				{
					InternalNameArray.Add(Name);
					NameIndex = InternalNameArray.Count - 1;
					InternalNameIndexLookupMap.Add(Name, NameIndex);
				}

				return NameIndex;
			}
		}

		/// <summary> 
		/// If found returns index of the name, returns –1 otherwise.
		/// This method uses method Contains during looking up for the name.
		/// NOTE: this is slow because it has to do a reverse lookup into NameArray
		/// </summary>
		/// <param name="PartialName"> Partial name that we want find the index. </param>
		public int GetNameIndex( string PartialName )
		{
			// This is the only method where there should be concurrent access of NameArray, so
			// don't worry about locking it anywhere else.
			lock(InternalNameArray)
			{
				for( int NameIndex = 0; NameIndex < InternalNameArray.Count; NameIndex ++ )
				{
					if(InternalNameArray[NameIndex].Contains( PartialName ) )
					{
						return NameIndex;
					}
				}

				return -1;
			}
		}

		/// <summary> Returns bank size in megabytes. </summary>
		public static int GetMemoryBankSize( int BankIndex )
		{
			Debug.Assert( BankIndex >= 0 );
			return BankIndex == 0 ? 512 : 0;
		}

		/// <summary> Returns a frame number based on the stream index. </summary>
		public int GetFrameNumberFromStreamIndex( int StartFrame, ulong StreamIndex )
		{
			// Check StartFrame is in the correct range.
			Debug.Assert( StartFrame >= 1 && StartFrame < FrameStreamIndices.Count );

			// Check StartFrame is valid.
			Debug.Assert( StreamIndex != INVALID_STREAM_INDEX );

			// The only case where these can be equal is if they are both 0.
			Debug.Assert( StreamIndex >= FrameStreamIndices[ StartFrame - 1 ] );

			int FoundFrame = FrameStreamIndices.BinarySearch(StartFrame, FrameStreamIndices.Count - StartFrame, StreamIndex, null);
			if (FoundFrame < 0)
			{
				FoundFrame = ~FoundFrame;
			}

			return FoundFrame;
		}

		/// <summary> Returns the time of the stream index. </summary>
		/// <param name="StartFrame"> Stream index that we want to know the time. </param>
		public float GetTimeFromStreamIndex( ulong StreamIndex )
		{
			// Check StartFrame is valid.
			Debug.Assert( StreamIndex != INVALID_STREAM_INDEX );

			// Check StreamIndex is in the correct range.
			Debug.Assert( StreamIndex >= 0 && StreamIndex < FrameStreamIndices[ FrameStreamIndices.Count - 1 ] );

			int FrameNumber = GetFrameNumberFromStreamIndex( 1, StreamIndex );
			float Time = GetTimeFromFrameNumber( FrameNumber );
			return Time;
		}

		/// <summary> Returns the time of the frame. </summary>
		/// <param name="FrameNumber"> Frame number that we want to know the time. </param>
		public float GetTimeFromFrameNumber( int FrameNumber )
		{
			// Check StartFrame is in the correct range.
			Debug.Assert( FrameNumber >= 0 && FrameNumber < FrameStreamIndices.Count );

			return DeltaTimeArray[FrameNumber];
		}

		/// <summary> Returns the total time of the frame. </summary>
		/// <param name="FrameNumber"> Frame number that we want to know the total time. </param>
		public float GetTotalTimeFromFrameNumber( int FrameNumber )
		{
			// Check StartFrame is in the correct range.
			Debug.Assert( FrameNumber >= 0 && FrameNumber < FrameStreamIndices.Count );

			float TotalTime = 0.0f;

			for( int FrameIndex = 0; FrameIndex < FrameNumber; FrameIndex ++ )
			{
				TotalTime += DeltaTimeArray[ FrameIndex ];
			}

			return TotalTime;
		}

		public void Shutdown()
		{
			if (SymbolParser != null)
			{
				SymbolParser.ShutdownSymbolService();
			}
		}
	}

	/// <summary> Helper class for logging timing in cases scopes.
	///	
	///	Example of usage
	/// 
	/// using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "HistogramParser.ParseSnapshot" ) )
	/// {
	///		code...
	/// }
	///
	/// </summary>
	public class FScopedLogTimer : IDisposable
	{
		/// <summary> Constructor. </summary>
		/// <param name="InDescription"> Global tag to use for logging </param>
		public FScopedLogTimer( string InDescription )
		{
			Description = InDescription;
			Timer = new Stopwatch();
			Start();
		}

		public void Start()
		{
			Timer.Start();
		}

		public void Stop()
		{
			Timer.Stop();
		}

		/// <summary> Destructor, logging delta time from constructor. </summary>
		public void Dispose()
		{
			Debug.WriteLine( Description + " took " + ( float )Timer.ElapsedMilliseconds / 1000.0f + " seconds to finish" );
		}

		/// <summary> Global tag to use for logging. </summary>
		protected string Description;

		/// <summary> Timer used to measure the timing. </summary>
		protected Stopwatch Timer;
	}

	public class FGlobalTimer
	{
		public FGlobalTimer( string InName )
		{
			Name = InName;
		}

		public void Start()
		{
			CurrentTicks = DateTime.Now.Ticks;
		}

		public void Stop()
		{
			long ElapsedTicks = DateTime.Now.Ticks - CurrentTicks;
			TotalTicks += ElapsedTicks;

			CallsNum++;
		}

		public override string ToString()
		{
			return string.Format( "Global timer for {0} took {1} ms with {2} calls", Name, ( float )TotalTicks / 10000.0f, CallsNum );
		}

		long CallsNum = 0;
		string Name;
		Stopwatch Timer = new Stopwatch();

		long TotalTicks = 0;
		long CurrentTicks = 0;
	};
}
