// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveProxy.h"
#include "Misc/CommandLine.h"

/**
 * Whether or not to enable Oodle dev code (packet capturing, dictionary training, and automatic dictionary finding) in shipping mode.
 *
 * This may be useful for multiplayer game mod authors, to optimize netcode compression for their mod (not officially supported).
 * However, Oodle compression makes the games network protocol harder to reverse-engineer - enabling this removes that slight benefit.
 */
#ifndef OODLE_DEV_SHIPPING
#define OODLE_DEV_SHIPPING	0
#endif


#if HAS_OODLE_SDK

#define CAPTURE_HEADER_MAGIC		0x41091CC4
#define CAPTURE_FILE_VERSION		0x00000002

#define DICTIONARY_HEADER_MAGIC		0x1B1BACD4
#define DICTIONARY_FILE_VERSION		0x00000001


// Markers for capture file version updates
#define CAPTURE_VER_PACKETCOUNT		0x00000002	// Added packet count to file header


/**
 * Base file archive for the Oodle plugin.
 *
 * Contains some common code, such as Oodle file compression, and helper structs/types, for archive writing/navigation
 */
class FOodleArchiveBase : public FArchiveProxy
{
protected:
	/**
	 * Encapsulates a value written to an archive, which can be seamlessly rewritten at any time, without disturbing seek position
	 * NOTE: Don't use this with types of variable size, e.g. FString, only with types of well defined size
	 */
	template<class Type>
	struct TRewritable
	{
		/** Written variables */
	private:
		/** The property value */
		Type Value;


		/** Runtime variables */
	private:
		/** The offset of the value, into the archive */
		uint32 Offset;


		/**
		 * Disable default constructor
		 */
		TRewritable() {}

	public:
		/**
		 * Initializing constructor
		 */
		TRewritable(Type DefaultVal)
			: Value(DefaultVal)
			, Offset(0)
		{
		}

		/**
		 * Retrieves the property value
		 *
		 * @return	Returns the property value
		 */
		FORCEINLINE Type Get()
		{
			return Value;
		}

		/**
		 * Sets the property value
		 *
		 * @param Ar		The archive being serialized to
		 * @param InValue	The new value
		 */
		FORCEINLINE void Set(FOodleArchiveBase& Ar, Type InValue)
		{
			Value = InValue;

			if (Offset != 0)
			{
				Ar.SeekPush(Offset);
				Ar << InValue;
				Ar.SeekPop();
			}
		}

		/**
		 * Serializes the value, to/from an archive
		 *
		 * @param Ar	The archive being serialized to/from
		 */
		FORCEINLINE void Serialize(FArchive& Ar)
		{
			if (Offset == 0)
			{
				Offset = Ar.Tell();
			}
			else
			{
				check(Offset == Ar.Tell());
			}

			Ar << Value;
		}
	};

	/**
	 * Struct for handling compressed data within the archive
	 */
	struct FOodleCompressedData
	{
		/** Written values */
	public:
		/** The offset of the compressed data, within the archive */
		TRewritable<uint32> Offset;

		/** The compressed length of the data */
		TRewritable<uint32> CompressedLength;

		/** The decompressed length of the data */
		TRewritable<uint32> DecompressedLength;

	public:
		/**
		 * Base constructor
		 */
		FOodleCompressedData()
			: Offset(0)
			, CompressedLength(0)
			, DecompressedLength(0)
		{
		}

		/**
		 * Serialize this struct to/from an archive
		 *
		 * @param Ar	The archive to serialize to/from
		 */
		void Serialize(FArchive& Ar);
	};

protected:
	/** Whether or not to flush immediately */
	bool bImmediateFlush;

private:
	/** Stack of previous seek positions */
	TArray<int64> SeekStack;

public:
	/**
	 * Base constructor
	 */
	FOodleArchiveBase(FArchive& InInnerArchive)
		: FArchiveProxy(InInnerArchive)
	{
		bImmediateFlush = FParse::Param(FCommandLine::Get(), TEXT("FORCELOGFLUSH"));
	}

	/**
	 * Deletes the inner archive
	 */
	void DeleteInnerArchive()
	{
		delete (FArchive*)&InnerArchive;
	}


	/**
	 * Pushes the current archive position onto a stack
	 */
	void SeekPush()
	{
		SeekStack.Push(InnerArchive.Tell());
	}

	/**
	 * Pushes the current archive position onto a stack, and seeks to a new position
	 *
	 * @param SeekPos	The position to seek to
	 */
	void SeekPush(int64 SeekPos)
	{
		SeekStack.Push(InnerArchive.Tell());
		InnerArchive.Seek(SeekPos);
	}

	/**
	 * Pops the most recent archive position from the stack, and seeks to that position
	 */
	void SeekPop()
	{
		check(SeekStack.Num() > 0);
		InnerArchive.Seek(SeekStack.Pop());
	}


	/**
	 * Compresses data and serializes it into the archive, and writes/serializes info to the specified FOodleCompressedData struct
	 *
	 * @param OutDataInfo	The FOodleCompressedData struct in the file header, which will store info needed for decompression
	 * @param Data			The data to be compressed
	 * @param DataBytes		The length of Data
	 * @return				Whether or not compression was successful
	 */
	bool SerializeOodleCompressData(FOodleCompressedData& OutDataInfo, uint8* Data, uint32 DataBytes);

	/**
	 * Decompresses the data referenced by the input FOodleCompressedData struct
	 *
	 * @param DataInfo		The FOodleCompressedData struct, describing the location of the data, and info necessary for decompressing
	 * @param OutData		Outputs the newly decompressed/allocated data, to a pointer (NOTE: Initialize the pointer to nullptr)
	 * @param OutDataBytes	Outputs the size of OutData
	 * @param bOutDataSlack	Adds some memory slack to OutData (@todo #JohnB: Remove after Oodle update, and after checking with Luigi)
	 * @return				Whether or not decompression was successful
	 */
	bool SerializeOodleDecompressData(FOodleCompressedData& DataInfo, uint8*& OutData, uint32& OutDataBytes, bool bOutDataSlack=false);


	/**
	 * Serializes an FOodleCompressedData struct to/from an archive
	 *
	 * @param Ar		The archive to serialize to/from
	 * @param Value		The struct value to serialize
	 * @return			Returns a chained-reference to the archive
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FOodleCompressedData& Value)
	{
		Value.Serialize(Ar);
		return Ar;
	}

	/**
	 * Serializes a rewritable (e.g. seeks to value, writes, then returns to previous position) value to/from an archive
	 *
	 * @param Ar		The archive to serialize to/from
	 * @param Value		The rewritable value to serialize
	 * @return			Returns a chained-reference to the archive
	 */
	template<class Type> FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TRewritable<Type>& Value)
	{
		Value.Serialize(Ar);
		return Ar;
	}
};


#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
/**
 * Archive for handling packet capture (.ucap) files
 */
class FPacketCaptureArchive : public FOodleArchiveBase
{
protected:
	/**
	 * Capture file header
	 */
	struct FCaptureHeader
	{
		/** Written header values */
	public:
		/** Unique value indicating this file type */
		uint32 Magic;

		/** Capture file format version */
		uint32 CaptureVersion;

		/** Total number of captured packets */
		TRewritable<uint32> PacketCount;

		/** Position in the archive, where the packet data starts */
		TRewritable<uint32> PacketDataOffset;

		/** Total length of all packet data */
		TRewritable<uint32> PacketDataLength;


	public:
		/**
		 * Base constructor
		 */
		FCaptureHeader()
			: Magic(CAPTURE_HEADER_MAGIC)
			, CaptureVersion(0)
			, PacketCount(0)
			, PacketDataOffset(0)
			, PacketDataLength(0)
		{
		}

		/**
		 * Serialize the capture file header
		 *
		 * @param Ar	The capture archive being serialized to/from
		 */
		void SerializeHeader(FPacketCaptureArchive& Ar);
	};


	/** The capture file header */
	FCaptureHeader Header;

public:
	/**
	 * Base constructor
	 */
	FPacketCaptureArchive(FArchive& InInnerArchive)
		: FOodleArchiveBase(InInnerArchive)
		, Header()
	{
		if (IsSaving())
		{
			Header.CaptureVersion = CAPTURE_FILE_VERSION;
		}
		else
		{
			Header.CaptureVersion = 0;
		}
	}


	/**
	 * Serializes the file header, containing the file format UID (magic) and file version
	 */
	void SerializeCaptureHeader();

	/**
	 * Serialize an individual packet to/from the archive.
	 * It is possible for there to be an incomplete packet stored - in which case, attempting to read will set the archives error mode
	 *
	 * @param PacketData	The location of/for the packet data
	 * @param PacketSize	For saving, the size of the packet, for loading, inputs the size of the buffer, and outputs the packet size
	 */
	void SerializePacket(void* PacketData, uint32& PacketSize);


	/**
	 * Used for merging multiple packet files. Appends the specified packet file to this one.
	 *
	 * @param InPacketFile	The packet file archive, to append to this one.
	 */
	void AppendPacketFile(FPacketCaptureArchive& InPacketFile);

	/**
	 * Returns PacketCount, the total number of packets in the file
	 */
	uint32 GetPacketCount();
};
#endif


/**
 * Archive for handling UE4 Oodle dictionary (.udic) files
 */
class FOodleDictionaryArchive : public FOodleArchiveBase
{
public:
	/**
	 * Dictionary file header
	 */
	class FDictionaryHeader
	{
		/** Written header values */
	public:
		/** Unique value indicating this file type */
		uint32 Magic;

		/** Dictionary file format version */
		uint32 DictionaryVersion;

		/** Oodle header version - noting changes in Oodle data format (only the major-version reflects file format changes) */
		uint32 OodleMajorHeaderVersion;

		/** Size of the hash table used for the dictionary */
		TRewritable<int32> HashTableSize;

		/** Compressed dictionary data, within the archive */
		FOodleCompressedData DictionaryData;

		/** Compressed Oodle compressor state data, within the archive */
		FOodleCompressedData CompressorData;


		/**
		 * Base constructor
		 */
		FDictionaryHeader()
			: Magic(DICTIONARY_HEADER_MAGIC)
			, DictionaryVersion(0)
			, OodleMajorHeaderVersion(0)
			, HashTableSize(INDEX_NONE)
			, DictionaryData()
			, CompressorData()
		{
		}


		/**
		 * Serializes the dictionary file header
		 *
		 * @param Ar	The dictionary archive to serialize to/from
		 */
		void SerializeHeader(FOodleDictionaryArchive& Ar);
	};


	/** The dictionary file header */
	FDictionaryHeader Header;

public:
	/**
	 * Base constructor
	 */
	FOodleDictionaryArchive(FArchive& InInnerArchive);

	/**
	 * Sets dictionary header values, that should be set prior to serializing the header to file
	 *
	 * @param InHashTableSize	The HashTableSize to set in the header
	 */
	void SetDictionaryHeaderValues(int32 InHashTableSize);

	/**
	 * Serializes initial basic header values
	 */
	void SerializeHeader();

	/**
	 * Serializes raw dictionary data and compressor state to/from file, compressing/decompressing the data as needed
	 *
	 * IMPORTANT: When serializing from disk, the caller is responsible for freeing DictionaryData and CompactCompressorState pointers
	 *
	 * @param DictionaryData				Pointer to dictionary data, when saving, and reference to nullptr void*, when loading
	 * @param DictionaryBytes				Inputs/Outputs the size of the dictionary
	 * @param CompactCompressorState		Pointer to compressor data, when saving, and reference to nullptr void*, when loading
	 * @param CompactCompressorStateBytes	Inputs/Outputs the size of the compressor data
	 */
	void SerializeDictionaryAndState(uint8*& DictionaryData, uint32& DictionaryBytes,
										uint8*& CompactCompresorState, uint32& CompactCompressorStateBytes);
};

#endif



