// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace NetworkProfiler
{
	/**
	 * Encapsulates entire network stream, split into frames. Also contains name table
	 * used to convert indices back into strings.
	 */
	public class NetworkStream
	{
		/** Per packet overhead to take into account for total outgoing bandwidth. */
		//public static int PacketOverhead = 48;
		public static int PacketOverhead = 28;

		/** Array of unique names. Code has fixed indexes into it.					*/
		public List<string> NameArray = new List<string>();

		/** Array of unique addresses. Code has fixed indexes into it.				*/
		public List<UInt64> AddressArray = new List<UInt64>();

		/** Last address index parsed from token stream								*/
		public int CurrentConnectionIndex = 0;

		/** Internal dictionary from class name to index in name array, used by GetClassNameIndex. */
		private Dictionary<string,int> ClassNameToNameIndex = new Dictionary<string,int>();
		
		/** Index of "Unreal" name in name array.									*/
		public int NameIndexUnreal = -1;

		/** At the highest level, the entire stream is a series of frames.			*/
		public List<PartialNetworkStream> Frames = new List<PartialNetworkStream>();

		/** Mapping from property name to summary */
		public Dictionary<int,TypeSummary> PropertyNameToSummary = new Dictionary<int,TypeSummary>();
		/** Mapping from actor name to summary */
		public Dictionary<int,TypeSummary> ActorNameToSummary = new Dictionary<int,TypeSummary>();
		/** Mapping from RPC name to summary */
		public Dictionary<int,TypeSummary> RPCNameToSummary = new Dictionary<int,TypeSummary>();

		/**
		 * Returns the name associated with the passed in index.
		 * 
		 * @param	Index	Index in name table
		 * @return	Name associated with name table index
		 */
		public string GetName(int Index)
		{
			return NameArray[Index];
		}

		/**
		 * Returns the ip address string associated with the passed in connection index.
		 * 
		 * @param	ConnectionIndex	Index in address table
		 * @return	Ip string associated with adress table index
		 */
		public string GetIpString( int ConnectionIndex )
		{
			UInt64 Addr = AddressArray[ConnectionIndex];
			UInt32 IP	= ( UInt32 )( Addr >> 32 );
			UInt32 Port = ( UInt32 )( Addr & ( ( ( UInt64 )1 << 32 ) - 1 ) );

			byte ip0 = ( byte )( ( IP >> 24 ) & 255 );
			byte ip1 = ( byte )( ( IP >> 16 ) & 255 );
			byte ip2 = ( byte )( ( IP >> 8 ) & 255 );
			byte ip3 = ( byte )( ( IP >> 0 ) & 255 );

			//return string.Format( "{0,3:000}.{1,3:000}.{2,3:000}.{3,3:000}: {4,-5}", ip0, ip1, ip2, ip3, Port );
			//return string.Format( "{0,3}.{1,3}.{2,3}.{3,3}: {4,-5}", ip0, ip1, ip2, ip3, Port );
			return string.Format( "{0}.{1}.{2}.{3}: {4}", ip0, ip1, ip2, ip3, Port );
		}

		/**
		 * Returns the class name index for the passed in actor name index
		 * 
		 * @param	ActorNameIndex	Name table entry of actor
		 * @return	Class name table index of actor's class
		 */
		public int GetClassNameIndex(int ActorNameIndex)
		{
			
            int ClassNameIndex = 0;
            try
            {
                // Class name is actor name with the trailing _XXX cut off.
                string ActorName = GetName(ActorNameIndex);
                string ClassName = ActorName;
                

                int CharIndex = ActorName.LastIndexOf('_');
                if (CharIndex >= 0)
                {
                    ClassName = ActorName.Substring(0, CharIndex);
                }

                

                // Find class name index in name array.
                
                if (ClassNameToNameIndex.ContainsKey(ClassName))
                {
                    // Found.
                    ClassNameIndex = ClassNameToNameIndex[ClassName];
                }
                // Not found, add to name array and then also dictionary.
                else
                {
                    ClassNameIndex = NameArray.Count;
                    NameArray.Add(ClassName);
                    ClassNameToNameIndex.Add(ClassName, ClassNameIndex);
                }
            }
            catch (System.Exception e)
            {
                System.Console.WriteLine("Error Parsing ClassName for Actor: " + ActorNameIndex + e.ToString());
            }

			return ClassNameIndex;
		}

		/**
		 * Returns the class name index for the passed in actor name
		 * 
		 * @param	ClassName	Name table entry of actor
		 * @return	Class name table index of actor's class
		 */
		public int GetIndexFromClassName( string ClassName )
		{
			if ( ClassNameToNameIndex.ContainsKey( ClassName ) )
			{
				return ClassNameToNameIndex[ClassName];
			}

			return -1;
		}

		/**
		 * Updates the passed in summary dictionary with information of new event.
		 * 
		 * @param	Summaries	Summaries dictionary to update (usually ref to ones contained in this class)
		 * @param	NameIndex	Index of object in name table (e.g. property, actor, RPC)
		 * @param	SizeBits	Size in bits associated with object occurence
		 */
		public void UpdateSummary( ref Dictionary<int,TypeSummary> Summaries, int NameIndex, int SizeBits, float TimeInMS )
		{
			if( Summaries.ContainsKey( NameIndex ) )
			{
				var Summary = Summaries[NameIndex];
				Summary.Count++;
				Summary.SizeBits += SizeBits;
                Summary.TimeInMS += TimeInMS;
			}
			else
			{
				Summaries.Add( NameIndex, new TypeSummary( SizeBits, TimeInMS ) );
			}
		}
	}

	/** Type agnostic summary for property & actor replication and RPCs. */
	public class TypeSummary
	{
		/** Number of times property was replicated or RPC was called, ... */ 
		public long Count = 1;
		/** Total size in bits. */
		public long SizeBits;
		/** Total ms */
		public float TimeInMS;

		/** Constructor */
		public TypeSummary( long InSizeBits, float InTimeInMS )
		{
			SizeBits = InSizeBits;
            TimeInMS = InTimeInMS;
		}
	}
}
