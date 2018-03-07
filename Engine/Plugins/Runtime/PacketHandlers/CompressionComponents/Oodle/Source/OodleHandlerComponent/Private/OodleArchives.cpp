// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OodleArchives.h"
#include "OodleHandlerComponent.h"

#if HAS_OODLE_SDK

// Maximum size of compress/decompress buffers (just under 2GB, due to max int32 value)
#define MAX_COMPRESS_BUFFER (1024 * 1024 * 2047)

// @todo #JohnB: Remove after Oodle update, and after checking with Luigi
#define OODLE_DICTIONARY_SLACK 65536

// @todo #JohnB: Deprecate all pre-CAPTURE_VER_PACKETCOUNT file versions, after a short period


/**
 * FOodleCompressedData
 */

void FOodleArchiveBase::FOodleCompressedData::Serialize(FArchive& Ar)
{
	Ar << Offset;
	Ar << CompressedLength;
	Ar << DecompressedLength;
}


/**
 * FOodleArchiveBase
 */

bool FOodleArchiveBase::SerializeOodleCompressData(FOodleCompressedData& OutDataInfo, uint8* Data, uint32 DataBytes)
{
	bool bSuccess = true;

	bSuccess = ensure(Data != nullptr);
	bSuccess = bSuccess && ensure(DataBytes > 0);
	bSuccess = bSuccess && ensure(DataBytes <= MAX_COMPRESS_BUFFER);

	if (bSuccess)
	{
		OutDataInfo.DecompressedLength.Set(*this, DataBytes);
	
		uint32 CompressBufferLen = OodleLZ_GetCompressedBufferSizeNeeded(DataBytes);
		uint8* CompressBuffer = new uint8[CompressBufferLen];

		SINTa OodleLen = OodleLZ_Compress(OodleLZ_Compressor_LZNIB, Data, (SINTa)DataBytes, CompressBuffer,
											OodleLZ_CompressionLevel_Optimal);

		bSuccess = ensure(OodleLen != OODLELZ_FAILED);

		if (bSuccess)
		{
			OutDataInfo.CompressedLength.Set(*this, OodleLen);

			uint32 OffsetPos = InnerArchive.Tell();
			OutDataInfo.Offset.Set(*this, OffsetPos);

			InnerArchive.Serialize((void*)CompressBuffer, OodleLen);
		}


		delete[] CompressBuffer;
	}

	return bSuccess;
}

bool FOodleArchiveBase::SerializeOodleDecompressData(FOodleCompressedData& DataInfo, uint8*& OutData, uint32& OutDataBytes,
														bool bOutDataSlack/*=false*/)
{
	check(OutData == nullptr);

	bool bSuccess = true;
	uint32 DecompressedLength = DataInfo.DecompressedLength.Get();
	uint32 CompressedLength = DataInfo.CompressedLength.Get();
	uint32 DataOffset = DataInfo.Offset.Get();

	bSuccess = ensure(CompressedLength <= (InnerArchive.TotalSize() - DataOffset));
	bSuccess = bSuccess && ensure(DecompressedLength <= MAX_COMPRESS_BUFFER);
	bSuccess = bSuccess && ensure(CompressedLength <= MAX_COMPRESS_BUFFER);

	if (bSuccess)
	{
		SeekPush(DataOffset);

		// @todo #JohnB: Remove bOutDataSlack after Oodle update, and after checking with Luigi
		uint8* CompressedData = new uint8[CompressedLength + (bOutDataSlack ? OODLE_DICTIONARY_SLACK : 0)];
		uint8* DecompressedData = new uint8[DecompressedLength];

		// @todo #JohnB: Remove after Oodle update, and after checking with Luigi
		if (bOutDataSlack)
		{
			FMemory::Memzero(CompressedData + CompressedLength, OODLE_DICTIONARY_SLACK);
		}

		InnerArchive.Serialize(CompressedData, CompressedLength);

		SINTa OodleLen = OodleLZ_Decompress((void*)CompressedData, CompressedLength, (void*)DecompressedData, DecompressedLength);

		bSuccess = OodleLen != 0;

		if (bSuccess)
		{
			check(OodleLen == DecompressedLength);

			OutData = DecompressedData;
			OutDataBytes = DecompressedLength;
		}
		else
		{
			delete[] DecompressedData;
		}

		delete[] CompressedData;

		SeekPop();
	}

	return bSuccess;
}


#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
/**
 * FCaptureHeader
 */

void FPacketCaptureArchive::FCaptureHeader::SerializeHeader(FPacketCaptureArchive& Ar)
{
	bool bSuccess = true;

	Ar << Magic;
	Ar << CaptureVersion;

	if (Ar.IsLoading())
	{
		bSuccess = ensure(Magic == CAPTURE_HEADER_MAGIC);
		bSuccess = bSuccess && ensure(CaptureVersion <= CAPTURE_FILE_VERSION);

		if (!bSuccess)
		{
			Ar.SetError();
		}
	}

	if (CaptureVersion >= CAPTURE_VER_PACKETCOUNT)
	{
		Ar << PacketCount;
		Ar << PacketDataOffset;
		Ar << PacketDataLength;
	}

	PacketDataOffset.Set(Ar, Ar.Tell());
}


/**
 * FPacketCaptureArchive
 */

void FPacketCaptureArchive::SerializeCaptureHeader()
{
	Header.SerializeHeader(*this);

	if (IsSaving() && bImmediateFlush)
	{
		Flush();
	}
}

void FPacketCaptureArchive::SerializePacket(void* PacketData, uint32& PacketSize)
{
	check(Header.PacketDataOffset.Get() != 0);
	check(Tell() >= Header.PacketDataOffset.Get());

	uint32 PacketBufferSize = (IsLoading() ? PacketSize : 0);
	uint64 StartPos = Tell();
	InnerArchive << PacketSize;

	if (IsLoading())
	{
		if (Header.CaptureVersion >= CAPTURE_VER_PACKETCOUNT)
		{
			// Added PacketSize here, to deliberately overshoot, in case PacketDataLength is not updated in the file (possible)
			check(Tell() < Header.PacketDataOffset.Get() + Header.PacketDataLength.Get() + (uint32)sizeof(PacketSize) + PacketSize);
		}

		// Max 128MB packet - excessive, but this is not meant to be a perfect security check
		if (PacketBufferSize < PacketSize || !ensure(PacketSize <= 134217728))
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("Bad PacketSize value '%i' in loading packet capture file"), PacketSize);
			SetError();

			return;
		}

		if (PacketSize > (InnerArchive.TotalSize() - InnerArchive.Tell()))
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("PacketSize '%i' greater than remaining file data '%i'. Truncated file? ")
					TEXT("(run server with -forcelogflush to reduce chance of truncated capture files)"),
					PacketSize, (InnerArchive.TotalSize() - InnerArchive.Tell()));

			SetError();

			return;
		}
	}

	InnerArchive.Serialize(PacketData, PacketSize);

	if (IsSaving())
	{
		uint32 NewPacketCount = Header.PacketCount.Get() + 1;
		uint32 NewPacketDataLength = Header.PacketDataLength.Get() + (Tell() - StartPos);

		Header.PacketCount.Set(*this, NewPacketCount);
		Header.PacketDataLength.Set(*this, NewPacketDataLength);

		if (bImmediateFlush)
		{
			Flush();
		}
	}
}

void FPacketCaptureArchive::AppendPacketFile(FPacketCaptureArchive& InPacketFile)
{
	check(IsSaving());
	check(Tell() != 0);	// Can't append a packet before writing the header
	check(InPacketFile.IsLoading());
	check(InPacketFile.Tell() == 0);

	// Read past the header
	InPacketFile.SerializeCaptureHeader();

	check(Header.CaptureVersion == InPacketFile.Header.CaptureVersion);

	// For appending, only support 1MB packets
	const uint32 BufferSize = 1024 * 1024;
	uint8* ReadBuffer = new uint8[BufferSize];

	// Iterate through all packets
	while (InPacketFile.Tell() < InPacketFile.TotalSize())
	{
		uint32 PacketSize = BufferSize;

		InPacketFile.SerializePacket((void*)ReadBuffer, PacketSize);

		if (InPacketFile.IsError())
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("Error reading packet capture data. Skipping rest of file."));

			break;
		}


		SerializePacket(ReadBuffer, PacketSize);
	}

	delete[] ReadBuffer;


	if (IsSaving() && bImmediateFlush)
	{
		Flush();
	}
}


uint32 FPacketCaptureArchive::GetPacketCount()
{
	uint32 ReturnVal = 0;

	if (IsSaving())
	{
		ReturnVal = Header.PacketCount.Get();
	}
	else
	{
		if (Header.CaptureVersion >= CAPTURE_VER_PACKETCOUNT)
		{
			ReturnVal = Header.PacketCount.Get();
		}
		// Do it the hard way, by recomputing through stepping through all packets
		// @todo #JohnB: Deprecate this, as part of removing pre-'CAPTURE_VER_PACKETCOUNT' file version support
		else
		{
			int64 ArcTotal = InnerArchive.TotalSize();

			check(Header.PacketDataOffset.Get() != 0);

			SeekPush(Header.PacketDataOffset.Get());

			while ((InnerArchive.Tell() + (int64)sizeof(uint32)) < ArcTotal)
			{
				uint32 PacketSize = 0;

				InnerArchive << PacketSize;

				int64 NewPos = InnerArchive.Tell() + PacketSize;

				if (NewPos <= ArcTotal)
				{
					InnerArchive.Seek(NewPos);
					ReturnVal++;
				}
			}

			SeekPop();
		}
	}

	return ReturnVal;
}
#endif


/**
 * FDictionaryHeader
 */

void FOodleDictionaryArchive::FDictionaryHeader::SerializeHeader(FOodleDictionaryArchive& Ar)
{
	bool bSuccess = true;

	Ar << Magic;
	Ar << DictionaryVersion;
	Ar << OodleMajorHeaderVersion;
	Ar << HashTableSize;
	Ar << DictionaryData;
	Ar << CompressorData;

	if (Ar.IsLoading())
	{
		if (Magic == 0x11235801)
		{
			LowLevelFatalError(TEXT("Tried to load dictionary from old format. Regenerate dictionary using the trainer commandlet."));
			bSuccess = false;
		}

		bSuccess = bSuccess && ensure(Magic == DICTIONARY_HEADER_MAGIC);
		bSuccess = bSuccess && ensure(DictionaryVersion <= DICTIONARY_FILE_VERSION);
		bSuccess = bSuccess && ensure(OodleMajorHeaderVersion == OODLE2_VERSION_MAJOR);
	}

	if (!bSuccess)
	{
		Ar.SetError();
	}
}


/**
 * FOodleDictionaryArchive
 */

FOodleDictionaryArchive::FOodleDictionaryArchive(FArchive& InInnerArchive)
	: FOodleArchiveBase(InInnerArchive)
	, Header()
{
	if (IsSaving())
	{
		Header.DictionaryVersion = DICTIONARY_FILE_VERSION;
		Header.OodleMajorHeaderVersion = OODLE2_VERSION_MAJOR;
	}
}

void FOodleDictionaryArchive::SetDictionaryHeaderValues(int32 InHashTableSize)
{
	check(IsSaving());

	Header.HashTableSize = InHashTableSize;
}

void FOodleDictionaryArchive::SerializeHeader()
{
	Header.SerializeHeader(*this);
}

void FOodleDictionaryArchive::SerializeDictionaryAndState(uint8*& DictionaryData, uint32& DictionaryBytes,
															uint8*& CompactCompresorState, uint32& CompactCompressorStateBytes)
{
	if (IsLoading())
	{
		check(DictionaryData == nullptr);
		check(CompactCompresorState == nullptr);

		// @todo #JohnB: Remove bOutDataSlack after Oodle update, and after checking with Luigi
		SerializeOodleDecompressData(Header.DictionaryData, DictionaryData, DictionaryBytes, true);
		SerializeOodleDecompressData(Header.CompressorData, CompactCompresorState, CompactCompressorStateBytes);
	}
	else
	{
		check(DictionaryBytes > 0);
		check(CompactCompressorStateBytes > 0);

		SerializeOodleCompressData(Header.DictionaryData, DictionaryData, DictionaryBytes);
		SerializeOodleCompressData(Header.CompressorData, CompactCompresorState, CompactCompressorStateBytes);
	}
}

#endif

