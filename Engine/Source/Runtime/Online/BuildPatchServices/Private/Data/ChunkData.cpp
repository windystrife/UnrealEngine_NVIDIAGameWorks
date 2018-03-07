// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Data/ChunkData.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Common/FileSystem.h"
#include "BuildPatchHash.h"
#include "BuildPatchManifest.h"

// The chunk header magic codeword, for quick checking that the opened file is a chunk file.
#define CHUNK_HEADER_MAGIC      0xB1FE3AA2

// The chunkdb header magic codeword, for quick checking that the opened file is a chunkdb file.
#define CHUNKDB_HEADER_MAGIC    0xB1FE3AA3

namespace BuildPatchServices
{
	namespace HeaderHelpers
	{
		void ZeroHeader(FChunkHeader& Header)
		{
			FMemory::Memzero(Header);
		}

		void ZeroHeader(FChunkDatabaseHeader& Header)
		{
			Header.Version = 0;
			Header.HeaderSize = 0;
			Header.DataSize = 0;
			Header.Contents.Empty();
		}
	}

	/**
	 * Enum which describes the chunk header version.
	 */
	enum class EChunkVersion : uint32
	{
		Invalid = 0,
		Original,
		StoresShaAndHashType,

		// Always after the latest version, signifies the latest version plus 1 to allow initialization simplicity
		LatestPlusOne,
		Latest = (LatestPlusOne - 1)
	};

	// The constant minimum sizes for each version of a header struct. Must be updated.
	// If new member variables are added the version MUST be bumped and handled properly here,
	// and these values must never change.
	static const uint32 ChunkHeaderVersionSizes[(uint32)EChunkVersion::LatestPlusOne] =
	{
		// Dummy for indexing.
		0,
		// Version 1 is 41 bytes (32b Magic, 32b Version, 32b HeaderSize, 32b DataSize, 4x32b GUID, 64b Hash, 8b StoredAs).
		41,
		// Version 2 is 62 bytes (328b Version1, 160b SHA1, 8b HashType).
		62
	};

	FChunkHeader::FChunkHeader()
		: Version((uint32)EChunkVersion::Latest)
		, Guid()
		, HeaderSize(ChunkHeaderVersionSizes[(uint32)EChunkVersion::Latest])
		, DataSize(0)
		, StoredAs(EChunkStorageFlags::None)
		, HashType(EChunkHashFlags::None)
		, RollingHash(0)
		, SHAHash()
	{
	}

	FArchive& operator<<(FArchive& Ar, FChunkHeader& Header)
	{
		// Calculate how much space left in the archive for reading data ( will be 0 when writing ).
		const int64 StartPos = Ar.Tell();
		const int64 ArchiveSizeLeft = Ar.TotalSize() - StartPos;
		uint32 ExpectedSerializedBytes = 0;
		// Make sure the archive has enough data to read from, or we are saving instead.
		bool bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= ChunkHeaderVersionSizes[(uint32)EChunkVersion::Original]);
		if (bSuccess)
		{
			uint32 Magic = CHUNK_HEADER_MAGIC;
			uint8 StoredAs = (uint8)Header.StoredAs;
			Ar << Magic
			   << Header.Version
			   << Header.HeaderSize
			   << Header.DataSize
			   << Header.Guid
			   << Header.RollingHash
			   << StoredAs;
			Header.StoredAs = (EChunkStorageFlags)StoredAs;
			bSuccess = Magic == CHUNK_HEADER_MAGIC && !Ar.IsError();
			ExpectedSerializedBytes = ChunkHeaderVersionSizes[(uint32)EChunkVersion::Original];

			// From version 2, we have a hash type choice. Previous versions default as only rolling.
			if (bSuccess && Header.Version >= (uint32)EChunkVersion::StoresShaAndHashType)
			{
				bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= ChunkHeaderVersionSizes[(uint32)EChunkVersion::StoresShaAndHashType]);
				if (bSuccess)
				{
					uint8 HashType = (uint8)Header.HashType;
					Ar.Serialize(Header.SHAHash.Hash, FSHA1::DigestSize);
					Ar << HashType;
					Header.HashType = (EChunkHashFlags)HashType;
					bSuccess = !Ar.IsError();
				}
				ExpectedSerializedBytes = ChunkHeaderVersionSizes[(uint32)EChunkVersion::StoresShaAndHashType];
			}
		}

		// Make sure the expected number of bytes were serialized. In practice this will catch errors where type
		// serialization operators changed their format and that will need investigating.
		bSuccess = bSuccess && (Ar.Tell() - StartPos) == ExpectedSerializedBytes;

		if (bSuccess)
		{
			// Make sure the archive now points to data location.
			Ar.Seek(StartPos + Header.HeaderSize);
		}
		else
		{
			// If we had a serialization error, zero out the header values.
			if (Ar.IsLoading())
			{
				HeaderHelpers::ZeroHeader(Header);
			}
		}

		return Ar;
	}

	/**
	 * Enum which describes the chunk database header version.
	 */
	enum class EChunkDatabaseVersion : uint32
	{
		Invalid = 0,
		Original,

		// Always after the latest version, signifies the latest version plus 1 to allow initialization simplicity
		LatestPlusOne,
		Latest = (LatestPlusOne - 1)
	};

	// The constant minimum sizes for each version of a header struct. Must be updated.
	// If new member variables are added the version MUST be bumped and handled properly here,
	// and these values must never change.
	static const uint32 ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::LatestPlusOne] =
	{
		// Dummy for indexing.
		0,
		// Version 1 is 24 bytes (32b Magic, 32b Version, 32b HeaderSize, 64b DataSize, 32b ChunkCount).
		24
	};

	FChunkDatabaseHeader::FChunkDatabaseHeader()
		: Version((uint32)EChunkDatabaseVersion::Latest)
		, HeaderSize(ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::Latest])
		, Contents()
	{
	}

	FArchive& operator<<(FArchive& Ar, FChunkDatabaseHeader& Header)
	{
		// Calculate how much space left in the archive for reading data (will be 0 when writing).
		const int64 StartPos = Ar.Tell();
		const int64 ArchiveSizeLeft = Ar.TotalSize() - StartPos;
		uint32 ExpectedSerializedBytes = 0;
		// Make sure the archive has enough data to read from, or we are saving instead.
		bool bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::Original]);
		if (bSuccess)
		{
			uint32 Magic = CHUNKDB_HEADER_MAGIC;
			// Chunk entry is 28 bytes (4x32b GUID, 64b FileStart, 32b FileSize).
			static const uint32 ChunkEntrySize = 28;
			int32 ChunkCount = Header.Contents.Num();
			Header.HeaderSize = ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::Original] + (ChunkCount * ChunkEntrySize);
			Ar << Magic
			   << Header.Version
			   << Header.HeaderSize
			   << Header.DataSize
			   << ChunkCount;
			bSuccess = Magic == CHUNKDB_HEADER_MAGIC && !Ar.IsError();
			ExpectedSerializedBytes = ChunkDatabaseHeaderVersionSizes[(uint32)EChunkDatabaseVersion::Original];

			// Serialize all chunk info.
			if (bSuccess)
			{
				Header.Contents.SetNumZeroed(ChunkCount);
				for (int32 ChunkIdx = 0; ChunkIdx < ChunkCount; ++ChunkIdx)
				{
					FChunkLocation& Location = Header.Contents[ChunkIdx];
					Ar << Location.ChunkId
					   << Location.ByteStart
					   << Location.ByteSize;
				}
				ExpectedSerializedBytes += ChunkCount * ChunkEntrySize;
			}
		}

		// Make sure the expected number of bytes were serialized. In practice this will catch errors where type
		// serialization operators changed their format and that will need investigating.
		bSuccess = bSuccess && (Ar.Tell() - StartPos) == ExpectedSerializedBytes;

		if (bSuccess)
		{
			// Make sure the archive now points to data location.
			Ar.Seek(StartPos + Header.HeaderSize);
		}
		else
		{
			// If we had a serialization error, zero out the header values.
			if (Ar.IsLoading())
			{
				HeaderHelpers::ZeroHeader(Header);
			}
		}

		return Ar;
	}


	FScopeLockedChunkData::FScopeLockedChunkData(IChunkDataAccess* InChunkDataAccess)
		: ChunkDataAccess(InChunkDataAccess)
	{
		ChunkDataAccess->GetDataLock(&ChunkData, &ChunkHeader);
	}

	FScopeLockedChunkData::~FScopeLockedChunkData()
	{
		ChunkDataAccess->ReleaseDataLock();
	}

	BuildPatchServices::FChunkHeader* FScopeLockedChunkData::GetHeader() const
	{
		return ChunkHeader;
	}

	uint8* FScopeLockedChunkData::GetData() const
	{
		return ChunkData;
	}


	class FChunkDataAccess : public IChunkDataAccess
	{
	public:
		FChunkDataAccess(uint32 DataSize)
		{
			ChunkData.Reserve(DataSize);
			ChunkData.SetNumUninitialized(DataSize);
		}

		~FChunkDataAccess()
		{
		}

		// IChunkDataAccess interface begin.
		virtual void GetDataLock(uint8** OutChunkData, FChunkHeader** OutChunkHeader) override
		{
			ThreadLock.Lock();
			if (OutChunkData)
			{
				(*OutChunkData) = ChunkData.GetData();
			}
			if (OutChunkHeader)
			{
				(*OutChunkHeader) = &ChunkHeader;
			}
		}
		virtual void GetDataLock(const uint8** OutChunkData, const FChunkHeader** OutChunkHeader) const override
		{
			ThreadLock.Lock();
			if (OutChunkData)
			{
				(*OutChunkData) = ChunkData.GetData();
			}
			if (OutChunkHeader)
			{
				(*OutChunkHeader) = &ChunkHeader;
			}
		}
		virtual void ReleaseDataLock() const override
		{
			ThreadLock.Unlock();
		}
		// IChunkDataAccess interface end.

	private:
		FChunkHeader ChunkHeader;
		TArray<uint8> ChunkData;
		mutable FCriticalSection ThreadLock;
	};

	IChunkDataAccess* FChunkDataAccessFactory::Create(uint32 DataSize)
	{
		return new FChunkDataAccess(DataSize);
	}


	class FChunkDataSerialization : public IChunkDataSerialization
	{
	public:
		FChunkDataSerialization(IFileSystem* InFileSystem)
			: FileSystem(InFileSystem)
		{
		}

		~FChunkDataSerialization()
		{
		}

		// IChunkDataAccess interface begin.
		virtual IChunkDataAccess* LoadFromFile(const FString& Filename, EChunkLoadResult& OutLoadResult) const override
		{
			IChunkDataAccess* ChunkData = nullptr;
			// Read the chunk file.
			TUniquePtr<FArchive> FileReader(FileSystem->CreateFileReader(*Filename));
			bool bSuccess = FileReader.IsValid();
			if (bSuccess)
			{
				ChunkData = Load(*FileReader, OutLoadResult);
				// Close the file.
				FileReader->Close();
			}
			else
			{
				OutLoadResult = EChunkLoadResult::OpenFileFail;
			}

			return ChunkData;
		}

		virtual IChunkDataAccess* LoadFromMemory(const TArray<uint8>& Memory, EChunkLoadResult& OutLoadResult) const override
		{
			FMemoryReader MemoryReader(Memory);
			return Load(MemoryReader, OutLoadResult);
		}

		virtual IChunkDataAccess* LoadFromArchive(FArchive& Archive, EChunkLoadResult& OutLoadResult) const override
		{
			if (Archive.IsLoading())
			{
				return Load(Archive, OutLoadResult);
			}
			else
			{
				OutLoadResult = EChunkLoadResult::BadArchive;
				return nullptr;
			}
		}

		virtual EChunkSaveResult SaveToFile(const FString& Filename, const IChunkDataAccess* ChunkDataAccess) const override
		{
			EChunkSaveResult SaveResult;
			TUniquePtr<FArchive> FileOut(FileSystem->CreateFileWriter(*Filename));
			if (FileOut.IsValid())
			{
				SaveResult = SaveToArchive(*FileOut, ChunkDataAccess);
			}
			else
			{
				SaveResult = EChunkSaveResult::FileCreateFail;
			}
			return SaveResult;
		}

		virtual EChunkSaveResult SaveToMemory(TArray<uint8>& Memory, const IChunkDataAccess* ChunkDataAccess) const override
		{
			FMemoryWriter MemoryWriter(Memory);
			return Save(MemoryWriter, ChunkDataAccess);
		}

		virtual EChunkSaveResult SaveToArchive(FArchive& Archive, const IChunkDataAccess* ChunkDataAccess) const override
		{
			if (Archive.IsSaving())
			{
				return Save(Archive, ChunkDataAccess);
			}
			else
			{
				return EChunkSaveResult::BadArchive;
			}
		}

		virtual void InjectShaToChunkData(TArray<uint8>& Memory, const FSHAHash& ShaHashData) const override
		{
			FMemoryReader MemoryReader(Memory);
			FMemoryWriter MemoryWriter(Memory);

			FChunkHeader Header;
			MemoryReader << Header;
			Header.HashType |= EChunkHashFlags::Sha1;
			Header.SHAHash = ShaHashData;
			MemoryWriter << Header;
		}

		// IChunkDataAccess interface end.

	private:
		IChunkDataAccess* Load(FArchive& Reader, EChunkLoadResult& OutLoadResult) const
		{
			IChunkDataAccess* ChunkData = nullptr;

			// Begin of read pos.
			const int64 StartPos = Reader.Tell();

			// Available read size.
			const int64 AvailableSize = Reader.TotalSize() - StartPos;

			// Read and check the header.
			FChunkHeader HeaderCheck;
			Reader << HeaderCheck;

			// Get file size.
			const int64 FileSize = HeaderCheck.HeaderSize + HeaderCheck.DataSize;

			if (HeaderCheck.Guid.IsValid())
			{
				if (HeaderCheck.HashType != EChunkHashFlags::None)
				{
					if ((HeaderCheck.HeaderSize + HeaderCheck.DataSize) <= AvailableSize)
					{
						if ((HeaderCheck.StoredAs & EChunkStorageFlags::Encrypted) == EChunkStorageFlags::None)
						{
							// Create the data.
							ChunkData = FChunkDataAccessFactory::Create(FileSize);

							// Lock data.
							FChunkHeader* Header;
							uint8* Data;
							ChunkData->GetDataLock(&Data, &Header);
							*Header = HeaderCheck;

							// Read the data.
							Reader.Serialize(Data, Header->DataSize);
							if (Reader.IsError() == false)
							{
								OutLoadResult = EChunkLoadResult::Success;
								// Decompress.
								if ((Header->StoredAs & EChunkStorageFlags::Compressed) != EChunkStorageFlags::None)
								{
									// Create a new data instance.
									IChunkDataAccess* NewChunkData = FChunkDataAccessFactory::Create(BuildPatchServices::ChunkDataSize);
									// Lock data.
									FChunkHeader* NewHeader;
									uint8* NewData;
									NewChunkData->GetDataLock(&NewData, &NewHeader);
									// Uncompress the memory.
									bool bSuccess = FCompression::UncompressMemory(
										static_cast<ECompressionFlags>(COMPRESS_ZLIB | COMPRESS_BiasMemory),
										NewData,
										BuildPatchServices::ChunkDataSize,
										Data,
										Header->DataSize);
									// If successful, switch over to new data.
									if (bSuccess)
									{
										FMemory::Memcpy(*NewHeader, *Header);
										NewHeader->StoredAs = EChunkStorageFlags::None;
										NewHeader->DataSize = BuildPatchServices::ChunkDataSize;
										ChunkData->ReleaseDataLock();
										delete ChunkData;
										ChunkData = NewChunkData;
										Header = NewHeader;
										Data = NewData;
									}
									// Otherwise delete new data.
									else
									{
										OutLoadResult = EChunkLoadResult::DecompressFailure;
										NewChunkData->ReleaseDataLock();
										delete NewChunkData;
										NewChunkData = nullptr;
									}
								}
								// Verify.
								if (OutLoadResult == EChunkLoadResult::Success && (Header->HashType & EChunkHashFlags::RollingPoly64) != EChunkHashFlags::None)
								{
									if (Header->DataSize != BuildPatchServices::ChunkDataSize || Header->RollingHash != FRollingHash<BuildPatchServices::ChunkDataSize>::GetHashForDataSet(Data))
									{
										OutLoadResult = EChunkLoadResult::HashCheckFailed;
									}
								}
								FSHAHashData ShaHashCheck;
								if (OutLoadResult == EChunkLoadResult::Success && (Header->HashType & EChunkHashFlags::Sha1) != EChunkHashFlags::None)
								{
									FSHA1::HashBuffer(Data, Header->DataSize, ShaHashCheck.Hash);
									if (!(ShaHashCheck == Header->SHAHash))
									{
										OutLoadResult = EChunkLoadResult::HashCheckFailed;
									}
								}
							}
							else
							{
								OutLoadResult = EChunkLoadResult::SerializationError;
							}
						}
						else
						{
							OutLoadResult = EChunkLoadResult::UnsupportedStorage;
						}
					}
					else
					{
						OutLoadResult = EChunkLoadResult::IncorrectFileSize;
					}
				}
				else
				{
					OutLoadResult = EChunkLoadResult::MissingHashInfo;
				}
			}
			else
			{
				OutLoadResult = EChunkLoadResult::CorruptHeader;
			}

			if (ChunkData != nullptr)
			{
				// Release data.
				ChunkData->ReleaseDataLock();

				// Delete data if failed
				if (OutLoadResult != EChunkLoadResult::Success)
				{
					delete ChunkData;
					ChunkData = nullptr;
				}
			}

			return ChunkData;
		}

		EChunkSaveResult Save(FArchive& Writer, const IChunkDataAccess* ChunkDataAccess) const
		{
			EChunkSaveResult SaveResult;
			const uint8* ChunkDataSource;
			const FChunkHeader* ChunkAccessHeader;
			ChunkDataAccess->GetDataLock(&ChunkDataSource, &ChunkAccessHeader);
			// Setup to handle compression.
			bool bDataIsCompressed = true;
			int32 ChunkDataSourceSize = BuildPatchServices::ChunkDataSize;
			TArray<uint8> TempCompressedData;
			TempCompressedData.Empty(ChunkDataSourceSize);
			TempCompressedData.AddUninitialized(ChunkDataSourceSize);
			int32 CompressedSize = ChunkDataSourceSize;

			// Compression can increase data size, too. This call will return false in that case.
			bDataIsCompressed = FCompression::CompressMemory(
				static_cast<ECompressionFlags>(COMPRESS_ZLIB | COMPRESS_BiasMemory),
				TempCompressedData.GetData(),
				CompressedSize,
				ChunkDataSource,
				BuildPatchServices::ChunkDataSize);

			// If compression succeeded, set data vars.
			if (bDataIsCompressed)
			{
				ChunkDataSource = TempCompressedData.GetData();
				ChunkDataSourceSize = CompressedSize;
			}

			// Setup Header.
			FChunkHeader Header;
			FMemory::Memcpy(Header, *ChunkAccessHeader);
			const int32 StartPos = Writer.Tell();
			Writer << Header;
			Header.HeaderSize = Writer.Tell() - StartPos;
			Header.StoredAs = bDataIsCompressed ? EChunkStorageFlags::Compressed : EChunkStorageFlags::None;
			Header.DataSize = ChunkDataSourceSize;
			// Make sure we at least have rolling hash.
			if ((Header.HashType & EChunkHashFlags::RollingPoly64) == EChunkHashFlags::None)
			{
				Header.HashType = EChunkHashFlags::RollingPoly64;
			}

			// Write out files.
			Writer.Seek(StartPos);
			Writer << Header;
			// We must use const_cast here due to FArchive::Serialize supporting both read and write. We created a writer.
			Writer.Serialize(const_cast<uint8*>(ChunkDataSource), ChunkDataSourceSize);
			if (!Writer.IsError())
			{
				SaveResult = EChunkSaveResult::Success;
			}
			else
			{
				SaveResult = EChunkSaveResult::SerializationError;
			}
			ChunkDataAccess->ReleaseDataLock();
			return SaveResult;
		}

	private:
		IFileSystem* FileSystem;
	};

	IChunkDataSerialization* FChunkDataSerializationFactory::Create(IFileSystem* FileSystem)
	{
		check(FileSystem != nullptr);
		return new FChunkDataSerialization(FileSystem);
	}
}
