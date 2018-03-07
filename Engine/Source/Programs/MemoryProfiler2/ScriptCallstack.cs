// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;

namespace MemoryProfiler2
{
	/// <summary> Encapsulates script callstack information. </summary>
	public class FScriptCallStack
	{
		/// <summary> All frames which belongs to this script callstack. </summary>
		public FScriptCallStackFrame[] Frames;

		/// <summary> Serializing constructor. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
		public FScriptCallStack( BinaryReader BinaryStream )
		{
			Frames = new FScriptCallStackFrame[ BinaryStream.ReadInt32() ];
			for( int FrameIndex = 0; FrameIndex < Frames.Length; FrameIndex++ )
			{
				Frames[ FrameIndex ] = new FScriptCallStackFrame( BinaryStream );
			}
		}

		/// <summary> Converts this script callstack to its equivalent string representation. </summary>
		public override string ToString()
		{
			StringBuilder Result = new StringBuilder();
			foreach( FScriptCallStackFrame Frame in Frames )
			{
				Result.AppendLine( Frame.ToString() );
			}

			return Result.ToString();
		}
	}

	/// <summary> Encapsulates one frame in a script callstack. </summary>
	public struct FScriptCallStackFrame
	{
		/// <summary> Package name index. </summary>
		public int PackageNameIndex;

		/// <summary> Class name index. </summary>
		public int ClassNameIndex;

		/// <summary> Function name represented as an index in the array of unique names. </summary>
		public int FunctionNameIndex;

		/// <summary> Callstack adres index to the name of script function. </summary>
		public int CallStackAddressIndex;

		/// <summary> Serializing constructor. </summary>
		/// <param name="BinaryStream"> Stream to serialize data from </param>
		public FScriptCallStackFrame( BinaryReader BinaryStream )
		{
			int ClassCompactedName = BinaryStream.ReadInt32();
			int FunctionCompactedName = BinaryStream.ReadInt32();
			int PackageCompactedName = BinaryStream.ReadInt32();

			ClassNameIndex = ClassCompactedName & 0x00FFFFFF;
			FunctionNameIndex = FunctionCompactedName & 0x00FFFFFF;
			PackageNameIndex = PackageCompactedName & 0x00FFFFFF;
			CallStackAddressIndex = -1;

			int NameIndex = FStreamInfo.GlobalInstance.GetNameIndex( ToString(), true );
			if( !FStreamInfo.GlobalInstance.ScriptCallstackMapping.TryGetValue( NameIndex, out CallStackAddressIndex ) )
			{
				CallStackAddressIndex = FStreamInfo.GlobalInstance.CallStackAddressArray.Count;
				FStreamInfo.GlobalInstance.CallStackAddressArray.Add( new FCallStackAddress( NameIndex ) );
				FStreamInfo.GlobalInstance.ScriptCallstackMapping.Add( NameIndex, CallStackAddressIndex );
			}
		}

		/// <summary> Converts this script callstack frame to its equivalent string representation. </summary>
		public override string ToString()
		{
			string PackageName = FStreamInfo.GlobalInstance.ScriptNameArray[ PackageNameIndex ];
			string ClassName = FStreamInfo.GlobalInstance.ScriptNameArray[ ClassNameIndex ];
			string FunctionName = FStreamInfo.GlobalInstance.ScriptNameArray[ FunctionNameIndex ];

			return "[Script] " + PackageName + "." + ClassName + ":" + FunctionName;
		}
	}

	/// <summary> Encapsulates a script-object type. An object that has been allocated inside script code. </summary>
	public class FScriptObjectType
	{
		/// <summary> Callstack adres index to the name of allocating function. </summary>
		public int CallStackAddressIndex;

		/// <summary> Constructor. </summary>
		public FScriptObjectType( string InTypeName )
		{
			CallStackAddressIndex = FStreamInfo.GlobalInstance.CallStackAddressArray.Count;
			FStreamInfo.GlobalInstance.CallStackAddressArray.Add( new FCallStackAddress( FStreamInfo.GlobalInstance.GetNameIndex( "[" + InTypeName + "] UObject::StaticAllocateObject", true ) ) );
		}
	}
}
