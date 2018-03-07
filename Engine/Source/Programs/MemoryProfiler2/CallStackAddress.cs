// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace MemoryProfiler2
{
    /// <summary> Information about an address in a callstack. </summary>
    public class FCallStackAddress
    {
        /// <summary> Program counter. </summary>
        public ulong ProgramCounter;
        /// <summary> Index of filename in name array. </summary>
        public Int32 FilenameIndex;
        /// <summary> Index of function in name array. </summary>
        public Int32 FunctionIndex;
        /// <summary> Line number. </summary>
        public Int32 LineNumber;

        /// <summary> True if all data is loaded for this address, e.g. filename and line number. </summary>
        public bool bIsComplete;

        /// <summary> Constructor, initializing indices to a passed in name index and other values to 0. </summary>
        /// <param name="NameIndex"> Name index to propagate to all indices. </param>
        public FCallStackAddress(int NameIndex)
        {
            ProgramCounter = 0;
            FilenameIndex = NameIndex;
            FunctionIndex = NameIndex;
            LineNumber = 0;
        }

        /// <summary> Serializing constructor. </summary>
        /// <param name="BinaryStream"> Stream to serialize data from </param>
        /// <param name="bShouldSerializeSymbolInfo"> Whether symbol info is being serialized </param>
        public FCallStackAddress(BinaryReader BinaryStream,bool bShouldSerializeSymbolInfo)
        {
            ProgramCounter = BinaryStream.ReadUInt64();
            // Platforms not supporting run-time symbol lookup won't serialize the below
            if( bShouldSerializeSymbolInfo )
            {
                FilenameIndex = BinaryStream.ReadInt32();
                FunctionIndex = BinaryStream.ReadInt32();
                LineNumber = BinaryStream.ReadInt32();
            }
        }
    }
}