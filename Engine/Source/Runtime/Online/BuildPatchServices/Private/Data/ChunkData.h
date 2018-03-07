// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/SecureHash.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Guid.h"

namespace BuildPatchServices
{
	class IFileSystem;

	// We are currently using fixed size of 1MB chunks for patching
	static const int32 ChunkDataSize = 1048576;

	/**
	 * Declares flags for chunk headers which specify storage types.
	 */
	enum class EChunkStorageFlags : uint8
	{
		None = 0x00,

		// Flag for compressed data. If also encrypted, decrypt first.
		Compressed = 0x01,

		// Flag for encrypted. If also compressed, decrypt first.
		Encrypted = 0x02,
	};
	ENUM_CLASS_FLAGS(EChunkStorageFlags);

	/**
	 * Declares flags for chunk headers which specify storage types.
	 */
	enum class EChunkHashFlags : uint8
	{
		None = 0x00,

		// Flag for FRollingHash class used, stored in RollingHash on header.
		RollingPoly64 = 0x01,

		// Flag for FSHA1 class used, stored in SHAHash on header.
		Sha1 = 0x02,
	};
	ENUM_CLASS_FLAGS(EChunkHashFlags);

	/**
	 * Enum which describes success, or the reason for failure when loading a chunk.
	 */
	enum class EChunkLoadResult : uint8
	{
		Success = 0,

		// Failed to open the file to load the chunk.
		OpenFileFail,

		// Could not serialize due to wrong archive type.
		BadArchive,

		// The header in the loaded chunk was invalid.
		CorruptHeader,

		// The expected file size in the header did not match the size of the file.
		IncorrectFileSize,

		// The storage type of the chunk is not one which we support.
		UnsupportedStorage,

		// The hash information was missing.
		MissingHashInfo,

		// The serialized data was not successfully understood.
		SerializationError,

		// The data was saved compressed but decompression failed.
		DecompressFailure,

		// The expected data hash in the header did not match the hash of the data.
		HashCheckFailed
	};

	/**
	 * Enum which describes success, or the reason for failure when saving a chunk.
	 */
	enum class EChunkSaveResult : uint8
	{
		Success = 0,

		// Failed to create the file for the chunk.
		FileCreateFail,

		// Could not serialize due to wrong archive type.
		BadArchive,

		// There was a serialization problem when writing to the chunk file.
		SerializationError
	};

	/**
	 * Declares a struct to store the info for a chunk header.
	 */
	struct FChunkHeader
	{
		// The version of this header data.
		uint32 Version;
		// The GUID for this data.
		FGuid Guid;
		// The size of this header.
		uint32 HeaderSize;
		// The size of this data.
		uint32 DataSize;
		// How the chunk data is stored.
		EChunkStorageFlags StoredAs;
		// What type of hash we are using.
		EChunkHashFlags HashType;
		// The FRollingHash hashed value for this chunk data.
		uint64 RollingHash;
		// The FSHA hashed value for this chunk data.
		FSHAHash SHAHash;

		/**
		 * Default constructor sets the version ready for writing out.
		 */
		FChunkHeader();

		/**
		 * Serialization operator.
		 * @param Ar        Archive to serialize to.
		 * @param Header    Header to serialize.
		 * @return Passed in archive.
		 */
		friend FArchive& operator<< (FArchive& Ar, FChunkHeader& Header);
	};

	/**
	 * Declares a struct holding variables to identify chunk and location.
	 */
	struct FChunkLocation
	{
		FGuid ChunkId;
		uint64 ByteStart;
		uint32 ByteSize;
	};

	/**
	 * Declares a struct to store the info for a chunk database header.
	 */
	struct FChunkDatabaseHeader
	{
		// The version of this header data.
		uint32 Version;
		// The size of this header.
		uint32 HeaderSize;
		// The size of the following data.
		uint64 DataSize;
		// The table of contents.
		TArray<FChunkLocation> Contents;

		/**
		 * Default constructor sets the version ready for writing out.
		 */
		FChunkDatabaseHeader();

		/**
		 * Serialization operator.
		 * @param Ar        Archive to serialize to.
		 * @param Header    Header to serialize.
		 * @return Passed in archive.
		 */
		friend FArchive& operator<< (FArchive& Ar, FChunkDatabaseHeader& Header);
	};

	/**
	 * An interface providing locked access to chunk data.
	 */
	class IChunkDataAccess
	{
	public:
		virtual ~IChunkDataAccess() {}

		/**
		 * Gets the thread lock on the data, must call ReleaseDataLock when finished with data.
		 * @param OutChunkData      Receives the pointer to chunk data.
		 * @param OutChunkHeader    Receives the pointer to header.
		 */
		virtual void GetDataLock(const uint8** OutChunkData, const FChunkHeader** OutChunkHeader) const = 0;
		virtual void GetDataLock(      uint8** OutChunkData,       FChunkHeader** OutChunkHeader) = 0;

		/**
		 * Releases access to the data to allow other threads to use.
		 */
		virtual void ReleaseDataLock() const = 0;
	};

	/**
	 * A factory for creating an IChunkDataAccess instance with allocated data.
	 */
	class FChunkDataAccessFactory
	{
	public:
		/**
		 * Creates a chunk data access class.
		 * @param DataSize  The size of the data to be held in bytes.
		 * @return the new IChunkDataAccess instance created.
		 */
		static IChunkDataAccess* Create(uint32 DataSize);
	};

	/**
	 * Provides simple access to the header and data in an IChunkDataAccess, whilst obtaining and releasing the data lock
	 * within the current scope.
	 */
	struct FScopeLockedChunkData
	{
	public:
		FScopeLockedChunkData(IChunkDataAccess* ChunkDataAccess);
		~FScopeLockedChunkData();

		/**
		 * @return the pointer to the chunk header.
		 */
		FChunkHeader* GetHeader() const;
		
		/**
		 * @return the pointer to the chunk data.
		 */
		uint8* GetData() const;

	private:
		// The provided IChunkDataAccess which we lock and release.
		IChunkDataAccess* ChunkDataAccess;
		// Pointer to the header data.
		FChunkHeader* ChunkHeader;
		// Pointer to the chunk data.
		uint8* ChunkData;
	};

	/**
	 * An interface providing serialization for chunk data.
	 */
	class IChunkDataSerialization
	{
	public:
		virtual ~IChunkDataSerialization() {}

		/**
		 * Loads a chunk from a file on disk or network.
		 * @param Filename          The full file path to the file.
		 * @param OutLoadResult     Receives the result, indicating the error reason if return value is nullptr.
		 * @return ptr to an allocated IChunkDataAccess holding the data, nullptr if failed to load.
		 */
		virtual IChunkDataAccess* LoadFromFile(const FString& Filename, EChunkLoadResult& OutLoadResult) const = 0;

		/**
		 * Loads a chunk from memory.
		 * @param Memory            The memory array.
		 * @param OutLoadResult     Receives the result, indicating the error reason if return value is nullptr.
		 * @return ptr to an allocated IChunkDataAccess holding the data, nullptr if failed to load.
		 */
		virtual IChunkDataAccess* LoadFromMemory(const TArray<uint8>& Memory, EChunkLoadResult& OutLoadResult) const = 0;

		/**
		 * Loads a chunk from an archive.
		 * @param Archive           The archive.
		 * @param OutLoadResult     Receives the result, indicating the error reason if return value is nullptr.
		 * @return ptr to an allocated IChunkDataAccess holding the data, nullptr if failed to load.
		 */
		virtual IChunkDataAccess* LoadFromArchive(FArchive& Archive, EChunkLoadResult& OutLoadResult) const = 0;
		
		/**
		 * Saves a chunk to a file on disk or network.
		 * @param Filename          The full file path to the file.
		 * @param ChunkDataAccess   Ptr to the chunk data to save.
		 * @return the result, EChunkSaveResult::Success if successful.
		 */
		virtual EChunkSaveResult SaveToFile(const FString& Filename, const IChunkDataAccess* ChunkDataAccess) const = 0;

		/**
		 * Saves a chunk to memory.
		 * @param Memory            The memory array.
		 * @param ChunkDataAccess   Ptr to the chunk data to save.
		 * @return the result, EChunkSaveResult::Success if successful.
		 */
		virtual EChunkSaveResult SaveToMemory(TArray<uint8>& Memory, const IChunkDataAccess* ChunkDataAccess) const = 0;

		/**
		 * Saves a chunk to an archive.
		 * @param Archive           The archive.
		 * @param ChunkDataAccess   Ptr to the chunk data to save.
		 * @return the result, EChunkSaveResult::Success if successful.
		 */
		virtual EChunkSaveResult SaveToArchive(FArchive& Archive, const IChunkDataAccess* ChunkDataAccess) const = 0;

		/**
		 * Injects an SHA hash for the data into the structure of a serialized chunk.
		 * @param Memory            The memory array containing the serialized chunk.
		 * @param ShaHashData       The SHA hash to inject.
		 */
		virtual void InjectShaToChunkData(TArray<uint8>& Memory, const FSHAHash& ShaHashData) const = 0;
	};

	/**
	 * A factory for creating an IChunkDataSerialization instance.
	 */
	class FChunkDataSerializationFactory
	{
	public:
		static IChunkDataSerialization* Create(IFileSystem* FileSystem);
	};
}
