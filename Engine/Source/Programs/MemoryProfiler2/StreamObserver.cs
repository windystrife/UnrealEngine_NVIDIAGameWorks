// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MemoryProfiler2
{
	/// <summary> For future usage. </summary>
	public abstract class StreamObserver
	{
		/// <summary>
		/// Return true if you need callstacks to be assigned to memory poolsbefore allocation processing begins. 
		/// </summary>
		public virtual bool NeedsPrePass
		{
			get
			{
				return true;
			}
		}

		/// <summary>
		/// Called after the callstack, name, etc. tables have been loaded and
		/// allocation stream parsing is about to begin. It's safe to call
		/// HistogramParser.LinkCallstackGroups at this point.
		/// </summary>
		public virtual void DataTablesLoaded()
		{
		}

		public virtual void SetStreamSize( int TotalAllocationEvents, List<int> AllocationEventsPerFrame )
		{
		}

		public virtual void AllocationEvent( FCallStack Callstack, EProfilingPayloadType EventType, long Size )
		{
		}

		public virtual void Snapshot( EProfilingPayloadSubType SnapshotType )
		{
		}

		public virtual void NewCallstack( FCallStack Callstack )
		{
		}
	}
}
