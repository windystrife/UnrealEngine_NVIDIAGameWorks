// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.ComponentModel;
using System.Xml.Serialization;

namespace MemoryProfiler2
{
	public class FCallStackFunctionFilter
	{
		public enum EFilterMode
		{
			SubString,
			StartsWith,
			EndsWith,
			RegEx,
		}

		public FCallStackFunctionFilter()
        {
			FilterMode = EFilterMode.SubString;
		}

		public FCallStackFunctionFilter(string InFilterPattern, EFilterMode InFilterMode)
		{
			FilterPattern = InFilterPattern;
			FilterMode = InFilterMode;
			CompileExpression();
		}

		void CompileExpression()
		{
			if (FilterMode == EFilterMode.RegEx)
			{
				CompiledRegex = new Regex(FilterPattern, RegexOptions.Compiled | RegexOptions.CultureInvariant);
			}
			else
			{
				CompiledRegex = null;
			}
		}

		public bool EvaluateExpression(string ToEvaluate)
		{
			switch (FilterMode)
			{
				case EFilterMode.SubString:
					return ToEvaluate.Contains(FilterPattern);
				case EFilterMode.StartsWith:
					return ToEvaluate.StartsWith(FilterPattern);
				case EFilterMode.EndsWith:
					return ToEvaluate.EndsWith(FilterPattern);
				case EFilterMode.RegEx:
					return CompiledRegex.Match(ToEvaluate).Success;
			}
			return false;
		}

		[XmlIgnore]
		[Browsable(false)]
		public string Name
		{
			get { return FilterMode.ToString() + " " + FilterPattern; }
		}

		string _FilterPattern;
		[Description("The filter pattern to match.")]
		[XmlAttribute]
		public string FilterPattern
		{
			get
			{
				return _FilterPattern;
			}

			set
			{
				_FilterPattern = value;
				CompileExpression();
			}
		}

		EFilterMode _FilterMode;
		[Description("How to process the filter pattern.")]
		[XmlAttribute]
		public EFilterMode FilterMode
		{
			get
			{
				return _FilterMode;
			}

			set
			{
				_FilterMode = value;
				CompileExpression();
			}
		}

		[XmlIgnore]
		[Browsable(false)]
		Regex CompiledRegex;
	}

	/// <summary> Encapsulates callstack information. </summary>
	public class FCallStack
	{
		/// <summary> CRC of callstack pointers. </summary>
		private Int32 CRC;

		/// <summary> Callstack as indices into address list, from top to bottom. </summary>
		public List<int> AddressIndices;

		/// <summary> First entry in the callstack that is *not* a container. </summary>
		public int FirstNonContainer;

		/// <summary> The class group that this callstack is associated with. </summary>
		public ClassGroup Group;

		/// <summary> Whether this callstack is truncated. </summary>
		public bool bIsTruncated;

		/// <summary> Memory pool that this callstack belongs to. </summary>
		public EMemoryPool MemoryPool = EMemoryPool.MEMPOOL_None;

		/// <summary> Maximum amount of memory that has been allocated in this callstack. </summary>
		public long MaxSize;

		/// <summary> Current amount of memory that is allocated in this callstack. </summary>
		public long LatestSize;

		/// <summary> The last processed stream index </summary>
		private ulong LastStreamIndex = ulong.MaxValue;

		/// <summary> Array of graph points used to draw a timeline graph in the callstack history view. </summary>
		public List<FSizeGraphPoint> SizeGraphPoints;

		/// <summary> Array of allocations that have been allocated and then freed. </summary>
		public List<FAllocationLifecycle> CompleteLifecycles;

		/// <summary> Array of allocations that are still in memory. </summary>
		public Dictionary<ulong, FAllocationLifecycle> IncompleteLifecycles;

		/// <summary> 
		/// Reference to the original callstack. 
		/// Any callstacks that differ only by script callstack should set this field to the original FCallStack they were copied from.
		/// </summary>
		public FCallStack Original;

		/// <summary> Array of virtual callstacks. Callstacks with decoded script callstack or script object type. </summary>
		public List<FCallStack> Children = new List<FCallStack>();

		/// <summary> Indices of virtual callstacks into the array of unique callstacks. </summary>
		public List<int> ChildIndices = new List<int>();

		/// <summary> Script callstack. May be null if there is no associated script callstack. </summary>
		public FScriptCallStack ScriptCallStack;

		/// <summary> Script object type. Occurs when a script object is allocated using StaticAllocateObject method. </summary>
		public FScriptObjectType ScriptObjectType;

		/// <summary> Default constructor. </summary>
		private FCallStack()
		{
			AddressIndices = new List<int>();
			SizeGraphPoints = FStreamInfo.GlobalInstance.CreationOptions.GenerateSizeGraphsCheckBox.Checked ? new List<FSizeGraphPoint>() : null;
			CompleteLifecycles = FStreamInfo.GlobalInstance.CreationOptions.KeepLifecyclesCheckBox.Checked ? new List<FAllocationLifecycle>() : null;
			IncompleteLifecycles = new Dictionary<ulong, FAllocationLifecycle>();
		}

		/// <summary> Serializing constructor. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
		public FCallStack(BinaryReader BinaryStream)
			: this()
		{
			// Read CRC of original callstack.
			CRC = BinaryStream.ReadInt32();

			// Read call stack address indices and parse into arrays.
			int AddressIndex = BinaryStream.ReadInt32();
			while (AddressIndex >= 0)
			{
				AddressIndices.Add(AddressIndex);
				AddressIndex = BinaryStream.ReadInt32();
			}

			// Normal callstacks are -1 terminated, whereof truncated ones are -2 terminated.
			if (AddressIndex == -2)
			{
				bIsTruncated = true;
			}
			else
			{
				bIsTruncated = false;
			}

			FirstNonContainer = AddressIndices.Count - 1;

			// We added bottom to top but prefer top bottom for hierarchical view.
			AddressIndices.Reverse();
		}

		/// <summary> Based on original callstack initializes a new callstack with decoded script callstack and script object type. </summary>
		public FCallStack(FCallStack InOriginal, FScriptCallStack InScriptCallStack, FScriptObjectType InScriptObjectType, int InCallStackIndex)
			: this()
		{
			Debug.Assert(InOriginal != null);
			Debug.Assert(ScriptCallStack == null || ScriptCallStack.Frames.Length > 0);

			Original = InOriginal;
			ScriptCallStack = InScriptCallStack;
			ScriptObjectType = InScriptObjectType;

			Original.Children.Add(this);
			Original.ChildIndices.Add(InCallStackIndex);

			CRC = Original.CRC;
			AddressIndices = new List<int>(Original.AddressIndices);
			FirstNonContainer = AddressIndices.Count - 1;
			bIsTruncated = Original.bIsTruncated;

			// If there is a script call stack, rename functions
			if (ScriptCallStack != null)
			{
				int ScriptFrameIndex = 0;
				for (int AddressIndex = AddressIndices.Count - 1; AddressIndex >= 0; AddressIndex--)
				{
					int FunctionNameIndex = FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndices[AddressIndex]].FunctionIndex;
					if (FunctionNameIndex == FStreamInfo.GlobalInstance.ProcessInternalNameIndex)
					{
						AddressIndices[AddressIndex] = ScriptCallStack.Frames[ScriptFrameIndex].CallStackAddressIndex;
						ScriptFrameIndex++;
						if (ScriptFrameIndex >= ScriptCallStack.Frames.Length)
						{
							break;
						}
					}
				}
			}

			// If the call stack has a script type allocation, replace the StaticAllocateObject call with the appropriate type-tagged one
			if (ScriptObjectType != null)
			{
				for (int AddressIndex = AddressIndices.Count - 1; AddressIndex >= 0; AddressIndex--)
				{
					int FunctionNameIndex = FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndices[AddressIndex]].FunctionIndex;
					if (FunctionNameIndex == FStreamInfo.GlobalInstance.StaticAllocateObjectNameIndex)
					{
						AddressIndices[AddressIndex] = ScriptObjectType.CallStackAddressIndex;
						break;
					}
				}
			}
		}

		/// <summary> Array of common names used to find the first non templated argument in the callstack. </summary>
		static private List<string> CommonNames = new List<string>()
		{
			"operator new<",
			"operator<<",
			">::",
			"FString::operator=",
			"FStringNoInit::operator=",
			"FString::FString",
			"FBestFitAllocator::",
			"FHeapAllocator::",
			"appMalloc",
			"appRealloc"
		};

		/// <summary> Find the first non templated argument in the callstack. </summary>
		public void EvaluateFirstNonContainer()
		{
			for (int AddressIndex = AddressIndices.Count - 1; AddressIndex > 0; AddressIndex--)
			{
				bool bIsContainer = false;
				string FunctionName = FStreamInfo.GlobalInstance.NameArray[FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndices[AddressIndex]].FunctionIndex];

				// See if the function name is one of the common set to ignore
				foreach (string CommonName in CommonNames)
				{
					if (FunctionName.Contains(CommonName))
					{
						bIsContainer = true;
						break;
					}
				}

				// if none are templates - we're good!
				if (!bIsContainer)
				{
					FirstNonContainer = AddressIndex;
					break;
				}
			}
		}

		/// <summary> Compares two callstacks for sorting. </summary>
		/// <param name="A"> First callstack to compare </param>
		/// <param name="B"> Second callstack to compare </param>
		public static int Compare(FCallStack A, FCallStack B)
		{
			// Not all callstacks have the same depth. Figure out min for comparision.
			int MinSize = Math.Min(A.AddressIndices.Count, B.AddressIndices.Count);

			// Iterate over matching size and compare.
			for (int i = 0; i < MinSize; i++)
			{
				// Sort by address
				if (A.AddressIndices[i] > B.AddressIndices[i])
				{
					return 1;
				}
				else if (A.AddressIndices[i] < B.AddressIndices[i])
				{
					return -1;
				}
			}

			// If we got here it means that the subset of addresses matches. In theory this means
			// that the callstacks should have the same size as you can't have the same address
			// doing the same thing, but let's simply be thorough and handle this case if the
			// stackwalker isn't 100% accurate.

			// Matching length means matching callstacks.
			if (A.AddressIndices.Count == B.AddressIndices.Count)
			{
				return 0;
			}
			// Sort by additional length.
			else
			{
				return A.AddressIndices.Count > B.AddressIndices.Count ? 1 : -1;
			}
		}

		/// <summary> Adds callstack information into the listview. </summary>
		public void AddToListView(ListView CallStackListView, bool bShowFromBottomUp)
		{
			for (int AdressIndex = 0; AdressIndex < AddressIndices.Count; AdressIndex++)
			{
				// Handle iterating over addresses in reverse order.
				int AddressIndexIndex = bShowFromBottomUp ? AddressIndices.Count - 1 - AdressIndex : AdressIndex;
				FCallStackAddress Address = FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndices[AddressIndexIndex]];

				string[] Row = new string[]
				{
					FStreamInfo.GlobalInstance.NameArray[Address.FunctionIndex],
					FStreamInfo.GlobalInstance.NameArray[Address.FilenameIndex],
					Address.LineNumber.ToString()
				};
				CallStackListView.Items.Add(new ListViewItem(Row));
			}
		}

		/// <summary> Removes entries related to allocation or the malloc profilers. </summary>
		public void TrimAllocatorEntries(List<FCallStackFunctionFilter> AllocatorFunctionFilters)
		{
			if (AllocatorFunctionFilters.Count == 0)
			{
				return;
			}

			bool bFoundAllocator = false;
			for (int AddressIndex = AddressIndices.Count - 1; AddressIndex >= 0; AddressIndex--)
			{
				string FunctionName = FStreamInfo.GlobalInstance.NameArray[FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndices[AddressIndex]].FunctionIndex];

				bool bFunctionIsAllocator = false;
				foreach (var AllocatorFunctionFilter in AllocatorFunctionFilters)
				{
					if (AllocatorFunctionFilter.EvaluateExpression(FunctionName))
					{
						bFunctionIsAllocator = true;
						break;
					}
				}

				if (bFunctionIsAllocator)
				{
					bFoundAllocator = true;
				}
				else if (bFoundAllocator)
				{
					AddressIndices.RemoveRange(AddressIndex + 1, AddressIndices.Count - AddressIndex - 1);
					break;
				}
			}
		}

		/// <summary> Removes all functions related to UObject Virtual Machine. </summary>
		public void FilterOutObjectVMFunctions()
		{
			for (int AddressIndex = AddressIndices.Count - 1; AddressIndex >= 0; AddressIndex--)
			{
				int FunctionIndex = FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndices[AddressIndex]].FunctionIndex;
				string FunctionName = FStreamInfo.GlobalInstance.NameArray[FunctionIndex];

				// Works only on PS3.
				bool bIsActorExecFunction = FunctionName.Contains("::exec") && FunctionName.Contains("FFrame&, void*");

				bool bIsVMFunction = FStreamInfo.GlobalInstance.ObjectVMFunctionIndexArray.Contains(FunctionIndex) || bIsActorExecFunction;
				if (bIsVMFunction)
				{
					AddressIndices.RemoveAt(AddressIndex);
				}
			}
		}

		/// <summary> 
		/// Filters this callstack based on passed in filter. This can either be an inclusion or exclusion.
		/// An inclusion means the test will pass if any of the addresses in the callstack match the inclusion
		/// filter passed in. 
		/// </summary>
		/// <param name="FilterText"> Filter to use </param>
		/// <returns> TRUE if callstack passes filter, FALSE otherwise </returns>
		public bool PassesTextFilterTest(string FilterText)
		{
			// Check whether any of the addresses in the call graph match the filter.
			bool bIsMatch = false;
			foreach (int AddressIndex in AddressIndices)
			{
				string FunctionName = FStreamInfo.GlobalInstance.NameArray[FStreamInfo.GlobalInstance.CallStackAddressArray[AddressIndex].FunctionIndex];
				bIsMatch = FunctionName.ToUpperInvariant().Contains(FilterText);
				if (bIsMatch)
				{
					break;
				}
			}

			return bIsMatch;
		}

		/// <summary> Filter callstacks in based on the text filter AND the class filter. </summary>
		private bool FilterIn(string FilterInText, List<ClassGroup> ClassGroups, EMemoryPool MemoryPoolFilter)
		{
			// Check against memory pool filter
			if ((MemoryPoolFilter & MemoryPool) != EMemoryPool.MEMPOOL_None)
			{
				// Check against the simple text filter
				if ((FilterInText.Length == 0) || PassesTextFilterTest(FilterInText))
				{
					// Check against any active classes
					if (ClassGroups.Contains(Group))
					{
						return (true);
					}
				}
			}

			return (false);
		}

		/// <summary> Filter callstacks out based on the text filter AND the class filter. </summary>
		private bool FilterOut(string FilterInText, List<ClassGroup> ClassGroups, EMemoryPool MemoryPoolFilter)
		{
			if ((MemoryPoolFilter & MemoryPool) != EMemoryPool.MEMPOOL_None)
			{
				// This callstack is in the selected pool, so filter it out
				return false;
			}

			// Check against the simple text filter
			if ((FilterInText.Length > 0) && PassesTextFilterTest(FilterInText))
			{
				// Found match, we do not want this callstack
				return (false);
			}

			// Check against any active classes
			if (ClassGroups.Contains(Group))
			{
				return (false);
			}

			return (true);
		}

		/// <summary> Runs all the current filters on this callstack. </summary>
		public bool RunFilters(string FilterInText, List<ClassGroup> ClassGroups, bool bFilterInClasses, EMemoryPool MemoryPoolFilter)
		{
			// Create a list of active groups
			List<ClassGroup> ActiveGroups = new List<ClassGroup>();
			foreach (ClassGroup Group in ClassGroups)
			{
				if (Group.bFilter)
				{
					ActiveGroups.Add(Group);
				}
			}

			// Filter groups in or out
			if (bFilterInClasses)
			{
				return FilterIn(FilterInText, ActiveGroups, MemoryPoolFilter);
			}
			else
			{
				return FilterOut(FilterInText, ActiveGroups, MemoryPoolFilter);
			}
		}

		/// <summary> Processes malloc operation for this callstack and updates lifecycles if needed. </summary>
		public void ProcessMalloc(FStreamToken StreamToken, ref FAllocationLifecycle NewLifecycle)
		{
			// Initialize a new lifecycle and add it to the array of incomplete lifecycles.
			NewLifecycle.Malloc(StreamToken, null, null);
			IncompleteLifecycles.Add(NewLifecycle.LatestPointer, NewLifecycle);
			NewLifecycle = null;

			LatestSize += StreamToken.Size;
			if (LatestSize > MaxSize)
			{
				MaxSize = LatestSize;
			}

			if (SizeGraphPoints != null)
			{
				Debug.Assert(SizeGraphPoints.Count == 0 || SizeGraphPoints[SizeGraphPoints.Count - 1].StreamIndex != StreamToken.StreamIndex);
				SizeGraphPoints.Add(new FSizeGraphPoint(StreamToken.StreamIndex, StreamToken.Size, false));
			}
		}

		/// <summary> Processes free operation for this callstack and updates lifecycles if needed. </summary>
		public FAllocationLifecycle ProcessFree(FStreamToken StreamToken)
		{
			int SizeChange = 0;
			FAllocationLifecycle Result = null;

			FAllocationLifecycle Lifecycle;
			if (IncompleteLifecycles.TryGetValue(StreamToken.Pointer, out Lifecycle))
			{
				SizeChange = -Lifecycle.CurrentSize;

				Lifecycle.Free(StreamToken);

				if (FStreamInfo.GlobalInstance.CreationOptions.KeepLifecyclesCheckBox.Checked)
				{
					CompleteLifecycles.Add(Lifecycle);
				}
				IncompleteLifecycles.Remove(StreamToken.Pointer);

				Result = Lifecycle;
			}
			else
			{
				// this should be caught by the stream parser, but an extra check doesn't hurt
				Debug.WriteLine("Free without malloc! StreamIndex = " + StreamToken.StreamIndex);
			}

			LatestSize += SizeChange;

			// it's possible that this point was already added to the graph via realloc chain propagation
			if (SizeGraphPoints != null && (SizeGraphPoints.Count == 0 || SizeGraphPoints[SizeGraphPoints.Count - 1].StreamIndex != StreamToken.StreamIndex))
			{
				SizeGraphPoints.Add(new FSizeGraphPoint(StreamToken.StreamIndex, SizeChange, false));
			}

			return Result;
		}

		/// <summary> Processes realloc operation for this callstack and updates lifecycles if needed. </summary>
		public FAllocationLifecycle ProcessRealloc(FStreamToken StreamToken, ref FAllocationLifecycle NewLifecycle, FCallStack PreviousCallStack, FAllocationLifecycle PreviousLifecycle)
		{
			FAllocationLifecycle Result = null;
			int SizeChange = 0;
			bool bFreshRealloc = true;

			FAllocationLifecycle Lifecycle;
			if (IncompleteLifecycles.TryGetValue(StreamToken.OldPointer, out Lifecycle))
			{
				IncompleteLifecycles.Remove(StreamToken.OldPointer);
				Lifecycle.Realloc(StreamToken, this, out SizeChange);
				if (Lifecycle.bIsComplete)
				{
					if (FStreamInfo.GlobalInstance.CreationOptions.KeepLifecyclesCheckBox.Checked)
					{
						CompleteLifecycles.Add(Lifecycle);
					}
				}
				else
				{
					IncompleteLifecycles.Add(Lifecycle.LatestPointer, Lifecycle);
				}

				bFreshRealloc = false;
				Result = Lifecycle;
			}
			else
			{
				Debug.Assert(NewLifecycle != null);
				NewLifecycle.Malloc(StreamToken, PreviousCallStack, PreviousLifecycle);
				Result = NewLifecycle;
				NewLifecycle = null;

				IncompleteLifecycles.Add(Result.LatestPointer, Result);

				bFreshRealloc = true;
				SizeChange = StreamToken.Size;
			}

			LatestSize += SizeChange;
			if (LatestSize > MaxSize)
			{
				MaxSize = LatestSize;
			}

			// it's possible that this point was already added to the graph via realloc chain propagation
			if (SizeGraphPoints != null && (SizeGraphPoints.Count == 0 || SizeGraphPoints[SizeGraphPoints.Count - 1].StreamIndex != StreamToken.StreamIndex))
			{
				SizeGraphPoints.Add(new FSizeGraphPoint(StreamToken.StreamIndex, SizeChange, bFreshRealloc));
			}

			return Result;
		}

		public void PropagateSizeGraphPoint(FAllocationLifecycle Lifecycle, ulong StreamIndex, int SizeChange)
		{
#if NOT_ENABLED
			if (SizeGraphPoints == null)
			{
				return;
			}

			PropagateSizeGraphPointInner(StreamIndex, SizeChange);

			FCallStack PreviousCallStack = Lifecycle.AllocEvent.PreviousCallStack;
			FAllocationLifecycle PreviousLifecycle = Lifecycle.AllocEvent.PreviousLifecycle;

			while (PreviousLifecycle != null)
			{
				if (PreviousCallStack.SizeGraphPoints == null)
				{
					break;
				}

				PreviousCallStack.PropagateSizeGraphPointInner(StreamIndex, SizeChange);

				PreviousCallStack = PreviousLifecycle.AllocEvent.PreviousCallStack;
				PreviousLifecycle = PreviousLifecycle.AllocEvent.PreviousLifecycle;
			}
#endif
		}

		private void PropagateSizeGraphPointInner(ulong StreamIndex, int SizeChange)
		{
			if (LastStreamIndex == StreamIndex)
			{
				FSizeGraphPoint LastPoint = SizeGraphPoints[SizeGraphPoints.Count - 1];

				LatestSize += SizeChange;
				if (LatestSize > MaxSize)
				{
					MaxSize = LatestSize;
				}

				SizeGraphPoints[SizeGraphPoints.Count - 1] = new FSizeGraphPoint(StreamIndex, LastPoint.SizeChange + SizeChange, false);
			}
			else
			{
				LatestSize += SizeChange;
				if (LatestSize > MaxSize)
				{
					MaxSize = LatestSize;
				}

				SizeGraphPoints.Add(new FSizeGraphPoint(StreamIndex, SizeChange, false));
				LastStreamIndex = StreamIndex;
			}
		}
	};

	/// <summary>
	/// Represents a single allocation, so size can't be greater than 2GB.
	/// There are a lot of these objects, so keeping the size in 32-bits
	/// saves a significant amount of memory.
	/// </summary>
	public class FAllocationLifecycle
	{
		/// <summary> Currently allocated pointer. </summary>
		public ulong LatestPointer;

		/// <summary> Allocation event. </summary>
		public FAllocationEvent AllocEvent;

		/// <summary> Reallocation events. </summary>
		public List<FReallocationEvent> ReallocsEvents;

		/// <summary> Position in the stream when the allocation has been freed. </summary>
		public ulong FreeStreamIndex = FStreamInfo.INVALID_STREAM_INDEX;

		/// <summary> True if allocation has been freed. </summary>
		public bool bIsComplete;

		/// <summary> Returns the current size of this allocation. </summary>
		public int CurrentSize
		{
			get
			{
				if (bIsComplete)
				{
					return 0;
				}
				else if (ReallocsEvents == null)
				{
					return AllocEvent.Size;
				}
				else
				{
					return ReallocsEvents[ReallocsEvents.Count - 1].NewSize;
				}
			}
		}

		/// <summary> Returns the peak size of this allocation. </summary>
		public int PeakSize
		{
			get
			{
				int Result = AllocEvent.Size;
				if (ReallocsEvents != null)
				{
					for (int i = 0; i < ReallocsEvents.Count; i++)
					{
						if (ReallocsEvents[i].NewSize > Result)
						{
							Result = ReallocsEvents[i].NewSize;
						}
					}
				}

				return Result;
			}
		}

		/// <summary> Empty constructor. </summary>
		public FAllocationLifecycle()
		{
		}

		/// <summary> Processes malloc operation for this allocation lifecycle. </summary>
		public void Malloc(FStreamToken StreamToken, FCallStack PreviousCallStack, FAllocationLifecycle PreviousLifecycle)
		{
			AllocEvent = new FAllocationEvent(StreamToken, PreviousCallStack, PreviousLifecycle);
			LatestPointer = AllocEvent.Pointer;

			// if PreviousCallStack != null, initial allocation was made by another callstack
			if (PreviousCallStack != null)
			{
				PreviousCallStack.PropagateSizeGraphPoint(PreviousLifecycle, StreamToken.StreamIndex, StreamToken.Size);
			}
		}

		/// <summary> Processes free operation for this allocation lifecycle. </summary>
		public void Free(FStreamToken StreamToken)
		{
			if (AllocEvent.PreviousCallStack != null)
			{
				AllocEvent.PreviousCallStack.PropagateSizeGraphPoint(AllocEvent.PreviousLifecycle, StreamToken.StreamIndex, -CurrentSize);
			}

			FreeStreamIndex = StreamToken.StreamIndex;
			bIsComplete = true;
		}

		/// <summary> Processes realloc operation for this allocation lifecycle. </summary>
		public void Realloc(FStreamToken StreamToken, FCallStack InitialCallStack, out int SizeChange)
		{
			// reallocs that are really frees should be handled by free()
			Debug.Assert(StreamToken.Size > 0);

			int InitialSize = CurrentSize;

			LatestPointer = StreamToken.NewPointer;
			FCallStack ReallocCallStack = FStreamInfo.GlobalInstance.CallStackArray[StreamToken.CallStackIndex];

			if (ReallocsEvents == null)
			{
				ReallocsEvents = new List<FReallocationEvent>();
				ReallocsEvents.Capacity = 1;
			}
			ReallocsEvents.Add(new FReallocationEvent(StreamToken, ReallocCallStack));

			if (ReallocCallStack != InitialCallStack)
			{
				// pointer has been realloced by a different callstack
				// it hasn't been freed, but it won't be tracked by this lifecycle object anymore,
				// so mark this object complete
				SizeChange = -InitialSize;
				bIsComplete = true;
			}
			else
			{
				SizeChange = StreamToken.Size - InitialSize;
			}

			if (AllocEvent.PreviousCallStack != null)
			{
				AllocEvent.PreviousCallStack.PropagateSizeGraphPoint(AllocEvent.PreviousLifecycle, StreamToken.StreamIndex, SizeChange);
			}
		}

		/// <summary>
		/// Size is returned as a uint instead of an int, because it is
		/// frequently added to the returned pointer and for some reason
		/// you can't add a ulong and an int.
		/// </summary>
		public ulong GetPointerAtStreamIndex(ulong StreamIndex, out uint Size)
		{
			if (StreamIndex < AllocEvent.StreamIndex || (FreeStreamIndex != FStreamInfo.INVALID_STREAM_INDEX && StreamIndex > FreeStreamIndex))
			{
				// StreamIndex was before initial allocation or after final free
				Size = 0;
				return 0;
			}
			else if (ReallocsEvents != null)
			{
				if (StreamIndex > ReallocsEvents[ReallocsEvents.Count - 1].StreamIndex)
				{
					// StreamIndex is after last realloc
					if (FreeStreamIndex == FStreamInfo.INVALID_STREAM_INDEX && bIsComplete)
					{
						// lifecycle was ended by a realloc in another callstack, and StreamIndex is after that
						Size = 0;
						return 0;
					}
					else
					{
						// lifecycle is still active, so return latest size
						Size = (uint)ReallocsEvents[ReallocsEvents.Count - 1].NewSize;
						return LatestPointer;
					}
				}
				else if (StreamIndex < ReallocsEvents[0].StreamIndex)
				{
					// StreamIndex is before first realloc
					Size = (uint)AllocEvent.Size;
					return AllocEvent.Pointer;
				}
				else
				{
					for (int EventIndex = 1; EventIndex < ReallocsEvents.Count; EventIndex++)
					{
						if (StreamIndex < ReallocsEvents[EventIndex].StreamIndex)
						{
							Size = (uint)ReallocsEvents[EventIndex - 1].NewSize;
							return ReallocsEvents[EventIndex - 1].NewPointer;
						}
					}

					// should never happen
					Debug.Assert(false, "Unhandled case");
					Size = 0;
					return 0;
				}
			}
			else
			{
				// StreamIndex is between malloc and free, and there were no reallocs
				Size = (uint)AllocEvent.Size;
				return AllocEvent.Pointer;
			}
		}
	}

	/// <summary> Encapsulates allocation event information. </summary>
	public struct FAllocationEvent
	{
		/// <summary> Position in the stream. </summary>
		public ulong StreamIndex;

		/// <summary> Pointer of allocation. </summary>
		public ulong Pointer;

		/// <summary> Size of allocation. </summary>
		public int Size;

		/// <summary> Previous callstack used for this allocation, happens after reallocation. </summary>
		public FCallStack PreviousCallStack;

		/// <summary> Previous allocation livecycle used for this allocation, happens after reallocation. </summary>
		public FAllocationLifecycle PreviousLifecycle;

		/// <summary> Constructor. </summary>
		public FAllocationEvent(FStreamToken StreamToken, FCallStack InPreviousCallStack, FAllocationLifecycle InPreviousLifecycle)
		{
			StreamIndex = StreamToken.StreamIndex;
			Pointer = StreamToken.Type == EProfilingPayloadType.TYPE_Realloc ? StreamToken.NewPointer : StreamToken.Pointer;
			Size = StreamToken.Size;
			PreviousCallStack = InPreviousCallStack;
			PreviousLifecycle = InPreviousLifecycle;
		}
	}

	/// <summary> Encapsulates reallocation event information. </summary>
	public struct FReallocationEvent
	{
		/// <summary> Position in the stream. </summary>
		public ulong StreamIndex;

		/// <summary> New pointer of reallocation. </summary>
		public ulong NewPointer;

		/// <summary> New size of reallocation. </summary>
		public int NewSize;

		/// <summary> Constructor. </summary>
		public FReallocationEvent(FStreamToken StreamToken, FCallStack InCallstack)
		{
			StreamIndex = StreamToken.StreamIndex;
			NewPointer = StreamToken.NewPointer;
			NewSize = StreamToken.Size;
		}
	}

	/// <summary>
	/// Encapsulates history of allocation for callstack.
	/// IMPORTANT: don't change this to a class, it's far more efficient as a struct.
	/// </summary>
	public struct FSizeGraphPoint
	{
		/// <summary> Position in the stream. </summary>
		public ulong StreamIndex;

		/// <summary> Change of allocation size. </summary>
		public int SizeChange;

		/// <summary> True if allocation comes from realloc operation. </summary>
		public bool bFreshRealloc;

		public FSizeGraphPoint(ulong InStreamIndex, int InSizeChange, bool bInFreshRealloc)
		{
			StreamIndex = InStreamIndex;
			SizeChange = InSizeChange;
			bFreshRealloc = bInFreshRealloc;
		}
	}
}