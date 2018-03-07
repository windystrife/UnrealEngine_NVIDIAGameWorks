// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;

namespace MemoryProfiler2
{
	[Flags]
	/// <summary> Memory pool types. </summary>
	public enum EMemoryPool
	{
		MEMPOOL_None = 0x00,

		/// <summary> IMPORTANT: This mask must *only* include the *valid* values of this enumeration.. </summary>
		MEMPOOL_All = 0x0f,

		/// <summary> Main memory pool. </summary>
		MEMPOOL_Main = 0x01,

		/// <summary> Local memory pool, video memory (RSX), only used on PS3. </summary>
		MEMPOOL_Local = 0x02,

		/// <summary> Default host memory pool, main memory, only used on PS3. </summary>
		MEMPOOL_HostDefault = 0x04,

		/// <summary> Movies host memory pool, main memory, only used on PS3. </summary>
		MEMPOOL_HostMovies = 0x08,
	}

	/// <summary> Memory pool types same as EMemoryPool, but serialized values. NOTE: This enum must match EMemoryPool from FMallocGcmProfiler.h. </summary>
	public enum EMemoryPoolSerialized
	{
		/// <summary> Invalid entry goes first so that it's the default value. </summary>
		MEMPOOL_Invalid = 0xff,

		/// <summary> Main memory pool. </summary>
		MEMPOOL_Main = 0x00,

		/// <summary> Local memory pool, video memory (RSX), only used on PS3. </summary>
		MEMPOOL_Local = 0x20,

		/// <summary> Default host memory pool, main memory, only used on PS3. </summary>
		MEMPOOL_HostDefault = 0x40,

		/// <summary> Movies host memory pool, main memory, only used on PS3. </summary>
		MEMPOOL_HostMovies = 0x80,
	}

	/// <summary> A collection of memory pool informations. Used to determine how much memory is stored in each of memory pool. </summary>
	public class FMemoryPoolInfoCollection
	{
		private FMemoryPoolInfo[] MemoryPoolInfos = new FMemoryPoolInfo[ 4 ];

		public FMemoryPoolInfo this[ EMemoryPool MemoryPool ]
		{
			get
			{
				if( MemoryPool == EMemoryPool.MEMPOOL_None )
				{
					return null;
				}

				// Work out index from flag (effectively log base 2).
				int Index = 0;
				int MemoryPoolIndex = ( int )MemoryPool;
				while( MemoryPoolIndex != 1 )
				{
					MemoryPoolIndex = MemoryPoolIndex >> 1;
					Index++;
				}

				return MemoryPoolInfos[ Index ];
			}
		}

		public FMemoryPoolInfoCollection()
		{
			for( int i = 0; i < MemoryPoolInfos.Length; i++ )
			{
				MemoryPoolInfos[ i ] = new FMemoryPoolInfo();
			}
		}
	}

	/// <summary> A memory pool information. </summary>
	public class FMemoryPoolInfo
	{
		/// <summary> Inclusive lower bound. </summary>
		public ulong MemoryBottom = ulong.MaxValue;

		/// <summary> Exclusive upper bound. </summary>
		public ulong MemoryTop = ulong.MinValue;

		public bool IsEmpty
		{
			get
			{
				return MemoryTop == ulong.MinValue;
			}
		}

		public void AddPointer( ulong Pointer, long Size )
		{
			Debug.Assert( Size >= 0 );
			ulong UnsignedSize = ( ulong )Size;

			if( Pointer < MemoryBottom )
			{
				MemoryBottom = Pointer;
			}

			if( Pointer + UnsignedSize > MemoryTop )
			{
				MemoryTop = Pointer + UnsignedSize;
			}
		}

		/// <summary> Converts serialized value of memory pool type into usable value. </summary>
		public static EMemoryPool ConvertToMemoryPoolFlag( EMemoryPoolSerialized MemoryPoolSerialized )
		{
			switch( MemoryPoolSerialized )
			{
				case EMemoryPoolSerialized.MEMPOOL_Invalid:
				return EMemoryPool.MEMPOOL_None;

				case EMemoryPoolSerialized.MEMPOOL_Main:
				return EMemoryPool.MEMPOOL_Main;

				case EMemoryPoolSerialized.MEMPOOL_Local:
				return EMemoryPool.MEMPOOL_Local;

				case EMemoryPoolSerialized.MEMPOOL_HostDefault:
				return EMemoryPool.MEMPOOL_HostDefault;

				case EMemoryPoolSerialized.MEMPOOL_HostMovies:
				return EMemoryPool.MEMPOOL_HostMovies;
			}

			return EMemoryPool.MEMPOOL_None;
		}

		/// <summary> 
		/// Each memory pool should only have memory allocated in one of the two
		/// histogram columns (Main memory or VRAM). This function returns which
		/// histogram column is valid for the given pool.
		/// </summary> 
		public static int GetMemoryPoolHistogramColumn( EMemoryPool MemoryPool )
		{
			switch( MemoryPool )
			{
				case EMemoryPool.MEMPOOL_Main:
				return 0;

				case EMemoryPool.MEMPOOL_Local:
				return 1;

				case EMemoryPool.MEMPOOL_HostDefault:
				return 0;

				case EMemoryPool.MEMPOOL_HostMovies:
				return 0;

				default:
				// Some combination of the above flags.
				return -1;
			}
		}

		public static IEnumerable<EMemoryPool> GetMemoryPoolEnumerable()
		{
			return new MemoryPoolEnumerable();
		}

		class MemoryPoolEnumerable : IEnumerable<EMemoryPool>
		{
			public IEnumerator<EMemoryPool> GetEnumerator()
			{
				return new MemoryPoolEnumerator();
			}

			// noone uses the old, un-generic interface
			System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
			{
				throw new NotImplementedException();
			}
		}

		class MemoryPoolEnumerator : IEnumerator<EMemoryPool>
		{
			private EMemoryPool _Current = EMemoryPool.MEMPOOL_None;

			public EMemoryPool Current
			{
				get
				{
					return _Current;
				}
			}

			public bool MoveNext()
			{
				if( _Current == EMemoryPool.MEMPOOL_None )
				{
					_Current = EMemoryPool.MEMPOOL_Main;
				}
				else
				{
					_Current = ( EMemoryPool )( ( int )_Current << 1 );
				}

				// only return true if current is still a valid value
				return ( _Current & EMemoryPool.MEMPOOL_All ) != EMemoryPool.MEMPOOL_None;
			}

			public void Reset()
			{
				_Current = EMemoryPool.MEMPOOL_None;
			}

			public void Dispose()
			{
				// nothing to clean up
			}

			object System.Collections.IEnumerator.Current
			{
				get
				{
					// noone uses the old, un-generic interface
					throw new NotImplementedException();
				}
			}
		}
	}
}
