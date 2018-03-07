// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace MemoryProfiler2
{
	/**
	 * Header written by capture tool
	 */
	public class FProfileDataHeader
	{
		/** Magic number at beginning of data file.					*/
		public const UInt32 ExpectedMagic = 0xDA15F7D8;

		/** Magic to ensure we're opening the right file.			*/
		public UInt32 Magic;
		/** Version number to detect version mismatches.			*/
		public UInt32 Version;
		/** Platform that was captured.								*/
		public string PlatformName;
		/** Whether symbol information was serialized.				*/
		public bool	bShouldSerializeSymbolInfo;
		/** Name of executable this information was gathered with.	*/
		public string ExecutableName;

		/** Offset in file for meta-data table.						*/
		public UInt64 MetaDataTableOffset;
		/** Number of meta-data table entries.						*/
		public UInt64 MetaDataTableEntries;

		/** Offset in file for name table.							*/
		public UInt64 NameTableOffset;
		/** Number of name table entries.							*/
		public UInt64 NameTableEntries;

		/** Offset in file for callstack address table.				*/
		public UInt64 CallStackAddressTableOffset;
		/** Number of callstack address entries.					*/
		public UInt64 CallStackAddressTableEntries;

		/** Offset in file for callstack table.						*/
		public UInt64 CallStackTableOffset;
		/** Number of callstack entries.							*/
		public UInt64 CallStackTableEntries;

		/** Offset in file for tags table.							*/
		public UInt64 TagsTableOffset;
		/** Number of tags entries.									*/
		public UInt64 TagsTableEntries;

		/** The file offset for module information.					*/
		public UInt64 ModulesOffset;
		/** The number of module entries.							*/
		public UInt64 ModuleEntries;

		/** Number of data files the stream spans.					*/
		public UInt64 NumDataFiles;

		// New in version 3 files

		/** Offset in file for stript callstack table. Requires version 3 or later. */
		public UInt32 ScriptCallstackTableOffset = UInt32.MaxValue;

		/** Offset in file for script name table. Requires version 3 or later. */
		public UInt32 ScriptNameTableOffset = UInt32.MaxValue;

		/** Can/should script callstacks be converted into readable names? */
		public bool bDecodeScriptCallstacks = false;

		/**
		 * Constructor, serializing header from passed in stream.
		 * 
		 * @param	BinaryStream	Stream to serialize header from.
		 */
		public FProfileDataHeader(BinaryReader BinaryStream)
		{
			// Serialize the file format magic first.
			Magic = BinaryStream.ReadUInt32();

			// Stop serializing data if magic number doesn't match. Most likely endian issue.
			if( Magic == ExpectedMagic )
			{
				// Version info for backward compatible serialization.
				Version = BinaryStream.ReadUInt32();
				FStreamToken.Version = Version;

				// We can no longer load anything < version 4
				if (Version < 4)
				{
					throw new InvalidDataException(String.Format("File version is too old! File Version: {0}, but we can only load >= 4.", Version));
				}

				// Read platform name.
				PlatformName = FStreamParser.ReadString( BinaryStream );

				// Whether symbol information was serialized.
				bShouldSerializeSymbolInfo = BinaryStream.ReadUInt32() == 0 ? false : true;

				if (Version >= 6)
				{
					// Meta-data table offset in file and number of entries.
					MetaDataTableOffset = BinaryStream.ReadUInt64();
					MetaDataTableEntries = BinaryStream.ReadUInt64();
				}
				else
				{
					MetaDataTableOffset = 0;
					MetaDataTableEntries = 0;
				}

				if( Version >= 5 )
				{
					// Name table offset in file and number of entries.
					NameTableOffset = BinaryStream.ReadUInt64();
					NameTableEntries = BinaryStream.ReadUInt64();

					// CallStack address table offset and number of entries.
					CallStackAddressTableOffset = BinaryStream.ReadUInt64();
					CallStackAddressTableEntries = BinaryStream.ReadUInt64();

					// CallStack table offset and number of entries.
					CallStackTableOffset = BinaryStream.ReadUInt64();
					CallStackTableEntries = BinaryStream.ReadUInt64();

					if (Version >= 7)
					{
						// Tags table offset and number of entries.
						TagsTableOffset = BinaryStream.ReadUInt64();
						TagsTableEntries = BinaryStream.ReadUInt64();
					}
					else
					{
						TagsTableOffset = 0;
						TagsTableEntries = 0;
					}

					ModulesOffset = BinaryStream.ReadUInt64();
					ModuleEntries = BinaryStream.ReadUInt64();

					NumDataFiles = 1;
				}
				else
				{
					// Name table offset in file and number of entries.
					NameTableOffset = BinaryStream.ReadUInt32();
					NameTableEntries = BinaryStream.ReadUInt32();

					// CallStack address table offset and number of entries.
					CallStackAddressTableOffset = BinaryStream.ReadUInt32();
					CallStackAddressTableEntries = BinaryStream.ReadUInt32();

					// CallStack table offset and number of entries.
					CallStackTableOffset = BinaryStream.ReadUInt32();
					CallStackTableEntries = BinaryStream.ReadUInt32();

					ModulesOffset = BinaryStream.ReadUInt32();
					ModuleEntries = BinaryStream.ReadUInt32();

					// Number of data files the stream spans.
					NumDataFiles = BinaryStream.ReadUInt32();
				}

				// Name of executable.
				ExecutableName = FStreamParser.ReadString( BinaryStream );
			}
		}
	}
}