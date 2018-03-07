// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchManifest.cpp: Implements the manifest classes.
=============================================================================*/

#include "BuildPatchManifest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Data/ChunkData.h"
#include "Core/BlockStructure.h"

using namespace BuildPatchServices;

#define LOCTEXT_NAMESPACE "BuildPatchManifest"

// The manifest header magic codeword, for quick checking that the opened file is probably a manifest file.
#define MANIFEST_HEADER_MAGIC		0x44BEC00C

// The maximum number of FNames that we expect a manifest to generate. This is not a technical limitation, just a sanity check
// and can be increased if more properties are added to our manifest UObject. FNames are only used by the UOject serialization system.
#define MANIFEST_MAX_NAMES			50

/**
 * Helper functions that convert generic types to and from string blobs for use with JSON parsing.
 * It's kind of horrible but guarantees no loss of data as the JSON reader/writer only supports float functionality 
 * which would result in data loss with high int32 values, and we'll be using uint64.
 */
template< typename DataType >
bool FromStringBlob( const FString& StringBlob, DataType& ValueOut )
{
	void* AsBuffer = &ValueOut;
	return FString::ToBlob( StringBlob, static_cast< uint8* >( AsBuffer ), sizeof( DataType ) );
}
template< typename DataType >
FString ToStringBlob( const DataType& DataVal )
{
	const void* AsBuffer = &DataVal;
	return FString::FromBlob( static_cast<const uint8*>( AsBuffer ), sizeof( DataType ) );
}
template< typename DataType >
bool FromHexString(const FString& HexString, DataType& ValueOut)
{
	void* AsBuffer = &ValueOut;
	if (HexString.Len() == (sizeof(DataType)* 2))
	{
		HexToBytes(HexString, static_cast<uint8*>(AsBuffer));
		return true;
	}
	return false;
}
template< typename DataType >
FString ToHexString(const DataType& DataVal)
{
	const void* AsBuffer = &DataVal;
	return BytesToHex(static_cast<const uint8*>(AsBuffer), sizeof(DataType));
}

/**
 * Helper functions to decide whether the passed in data is a JSON string we expect to deserialize a manifest from
 */
bool BufferIsJsonManifest(const TArray<uint8>& DataInput)
{
	// The best we can do is look for the mandatory first character open curly brace,
	// it will be within the first 4 characters (may have BOM)
	for (int32 idx = 0; idx < 4 && idx < DataInput.Num(); ++idx)
	{
		if (DataInput[idx] == TEXT('{'))
		{
			return true;
		}
	}
	return false;
}

/* EBuildPatchAppManifestVersion implementation
*****************************************************************************/
EBuildPatchAppManifestVersion::Type EBuildPatchAppManifestVersion::GetLatestVersion()
{
	return static_cast<EBuildPatchAppManifestVersion::Type>(LatestPlusOne - 1);
}

EBuildPatchAppManifestVersion::Type EBuildPatchAppManifestVersion::GetLatestJsonVersion()
{
	return GetLatestVersion();
}

EBuildPatchAppManifestVersion::Type EBuildPatchAppManifestVersion::GetLatestFileDataVersion()
{
	return static_cast<EBuildPatchAppManifestVersion::Type>(StoresChunkFileSizes);
}

EBuildPatchAppManifestVersion::Type EBuildPatchAppManifestVersion::GetLatestChunkDataVersion()
{
	return GetLatestVersion();
}

const FString& EBuildPatchAppManifestVersion::GetChunkSubdir(const EBuildPatchAppManifestVersion::Type ManifestVersion)
{
	static const FString ChunksV1 = TEXT("Chunks");
	static const FString ChunksV2 = TEXT("ChunksV2");
	static const FString ChunksV3 = TEXT("ChunksV3");
	return ManifestVersion < DataFileRenames ? ChunksV1
		: ManifestVersion < ChunkCompressionSupport ? ChunksV2
		: ChunksV3;
}

const FString& EBuildPatchAppManifestVersion::GetFileSubdir(const EBuildPatchAppManifestVersion::Type ManifestVersion)
{
	static const FString FilesV1 = TEXT("Files");
	static const FString FilesV2 = TEXT("FilesV2");
	static const FString FilesV3 = TEXT("FilesV3");
	return ManifestVersion < DataFileRenames ? FilesV1
		: ManifestVersion <= StoredAsCompressedUClass ? FilesV2
		: FilesV3;
}

/* FManifestFileHeader - The header for a compressed/encoded manifest file
*****************************************************************************/
FManifestFileHeader::FManifestFileHeader()
	: Magic(MANIFEST_HEADER_MAGIC)
	, HeaderSize(0)
	, DataSize(0)
	, CompressedSize(0)
	, SHAHash()
	, StoredAs(0)
{
}

bool FManifestFileHeader::CheckMagic() const
{
	return Magic == MANIFEST_HEADER_MAGIC;
}

FArchive& operator<<(FArchive& Ar, FManifestFileHeader& Header)
{
	Ar << Header.Magic;
	Ar << Header.HeaderSize;
	Ar << Header.DataSize;
	Ar << Header.CompressedSize;
	Ar.Serialize(Header.SHAHash.Hash, FSHA1::DigestSize);
	Ar << Header.StoredAs;
	return Ar;
}

/* FCustomFieldData implementation
*****************************************************************************/
FCustomFieldData::FCustomFieldData()
	: Key(TEXT(""))
	, Value(TEXT(""))
{
}

FCustomFieldData::FCustomFieldData(const FString& InKey, const FString& InValue)
	: Key(InKey)
	, Value(InValue)
{
}

/* FSHAHashData implementation
*****************************************************************************/
FSHAHashData::FSHAHashData()
{
	FMemory::Memset(Hash, 0, FSHA1::DigestSize);
}

FString FSHAHashData::ToString() const
{
	return BytesToHex(Hash, FSHA1::DigestSize);
}

bool FSHAHashData::IsZero() const
{
	static const uint8 Zero[FSHA1::DigestSize] = {0};
	return FMemory::Memcmp(Hash, Zero, FSHA1::DigestSize) == 0;
}

/* FChunkInfoData implementation
*****************************************************************************/
FChunkInfoData::FChunkInfoData()
	: Guid()
	, Hash(0)
	, ShaHash()
	, FileSize(0)
	, GroupNumber(0)
{
}

/* FChunkPartData implementation
*****************************************************************************/
FChunkPartData::FChunkPartData()
	: Guid()
	, Offset(0)
	, Size(0)
{
}

/* FFileManifestData implementation
*****************************************************************************/
FFileManifestData::FFileManifestData()
	: Filename(TEXT(""))
	, FileHash()
	, FileChunkParts()
	, bIsUnixExecutable(false)
	, SymlinkTarget(TEXT(""))
	, bIsReadOnly(false)
	, bIsCompressed(false)
	, FileSize(INDEX_NONE)
{
}

int64 FFileManifestData::GetFileSize() const
{
	return FileSize;
}

bool FFileManifestData::operator<(const FFileManifestData& Other) const
{
	return Filename < Other.Filename;
}

void FFileManifestData::Init()
{
	FileSize = 0;
	for (auto FileChunkPartIt = FileChunkParts.CreateConstIterator(); FileChunkPartIt; ++FileChunkPartIt)
	{
		FileSize += (*FileChunkPartIt).Size;
	}
}

/**
 * Archive for writing a manifest into memory
 */
class FManifestWriter : public FArchive
{
public:
	FManifestWriter()
		: FArchive()
		, Offset(0)
	{
		ArIsPersistent = false;
		ArIsSaving = true;
	}

	virtual void Seek(int64 InPos) override
	{
		Offset = InPos;
	}

	virtual int64 Tell() override
	{
		return Offset;
	}

	virtual FString GetArchiveName() const override
	{
		return TEXT("FManifestWriter");
	}

	virtual FArchive& operator<<(FName& N) override
	{
		if (!FNameIndexLookup.Contains(N))
		{
			const int32 ArNameIndex = FNameIndexLookup.Num();
			FNameIndexLookup.Add(N, ArNameIndex);
		}
		*this << FNameIndexLookup[N];
		return *this;
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		if (Num && !ArIsError)
		{
			const int64 NumBytesToAdd = Offset + Num - Bytes.Num();
			if (NumBytesToAdd > 0)
			{
				const int64 NewArrayCount = Bytes.Num() + NumBytesToAdd;
				if (NewArrayCount >= MAX_int32)
				{
					ArIsError = true;
					return;
				}

				Bytes.AddUninitialized((int32)NumBytesToAdd);
			}

			check((Offset + Num) <= Bytes.Num());

			if (Num)
			{
				FMemory::Memcpy(&Bytes[Offset], Data, Num);
				Offset += Num;
			}
		}
	}

	virtual int64 TotalSize() override
	{
		return Bytes.Num();
	}

	void Finalize()
	{
		TArray<uint8> FinalData;
		FMemoryWriter NameTableWriter(FinalData);

		int32 NumNames = FNameIndexLookup.Num();
		check(NumNames <= MANIFEST_MAX_NAMES);
		NameTableWriter << NumNames;
		for (auto& MapEntry : FNameIndexLookup)
		{
			FName& Name = MapEntry.Key;
			int32& Index = MapEntry.Value;
			NameTableWriter << Name;
			NameTableWriter << Index;
		}

		FinalData.Append(Bytes);

		Bytes = MoveTemp(FinalData);
	}

	TArray<uint8>& GetBytes()
	{
		return Bytes;
	}

private:
	int64 Offset;
	TArray<uint8> Bytes;
	TMap<FName, int32> FNameIndexLookup;
};

/**
 * Archive for reading a manifest from data in memory
 */
class FManifestReader : public FArchive
{
public:
	FManifestReader(const TArray<uint8>& InBytes)
		: FArchive()
		, Offset(0)
		, Bytes(InBytes)
	{
		ArIsPersistent = false;
		ArIsLoading = true;

		// Must load table immediately
		FMemoryReader NameTableReader(InBytes);
		int32 NumNames;
		NameTableReader << NumNames;

		// Check not insane, we know to expect a small number for a manifest
		if (NumNames < MANIFEST_MAX_NAMES)
		{
			FNameLookup.Empty(NumNames);
			for (; NumNames > 0; --NumNames)
			{
				FName Name;
				int32 Index;
				NameTableReader << Name;
				NameTableReader << Index;
				FNameLookup.Add(Index, Name);
			}
		}
		else
		{
			ArIsError = true;
		}

		Offset = NameTableReader.Tell();
	}

	virtual FString GetArchiveName() const override
	{
		return TEXT("FManifestReader");
	}

	virtual void Seek(int64 InPos) override
	{
		check(InPos <= Bytes.Num());
		Offset = InPos;
	}

	virtual int64 Tell() override
	{
		return Offset;
	}

	virtual FArchive& operator<<(FName& N) override
	{
		if (ArIsError)
		{
			N = NAME_None;
		}
		else
		{
			// Read index and lookup
			int32 ArNameIndex;
			*this << ArNameIndex;
			if (FNameLookup.Contains(ArNameIndex))
			{
				N = FNameLookup[ArNameIndex];
			}
			else
			{
				N = NAME_None;
				ArIsError = true;
			}
		}
		return *this;
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		if (Num && !ArIsError)
		{
			// Only serialize if we have the requested amount of data
			if (Offset + Num <= Bytes.Num())
			{
				FMemory::Memcpy(Data, &Bytes[Offset], Num);
				Offset += Num;
			}
			else
			{
				ArIsError = true;
			}
		}
	}

	virtual int64 TotalSize() override
	{
		return Bytes.Num();
	}

private:
	int64 Offset;
	const TArray<uint8>& Bytes;
	TMap<int32, FName> FNameLookup;
};

/* UBuildPatchManifest implementation
*****************************************************************************/
UBuildPatchManifest::UBuildPatchManifest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ManifestFileVersion(EBuildPatchAppManifestVersion::Invalid)
	, bIsFileData(false)
	, AppID(INDEX_NONE)
	, AppName(TEXT(""))
	, BuildVersion(TEXT(""))
	, LaunchExe(TEXT(""))
	, LaunchCommand(TEXT(""))
	, PrereqIds()
	, PrereqName(TEXT(""))
	, PrereqPath(TEXT(""))
	, PrereqArgs(TEXT(""))
	, FileManifestList()
	, ChunkList()
	, CustomFields()
{
}

void UBuildPatchManifest::Clear()
{
	ManifestFileVersion = EBuildPatchAppManifestVersion::Invalid;
	bIsFileData = false;
	AppID = INDEX_NONE;
	AppName.Empty();
	BuildVersion.Empty();
	LaunchExe.Empty();
	LaunchCommand.Empty();
	PrereqIds.Empty();
	PrereqName.Empty();
	PrereqPath.Empty();
	PrereqArgs.Empty();
	FileManifestList.Empty();
	ChunkList.Empty();
	CustomFields.Empty();
}

UBuildPatchManifest::~UBuildPatchManifest()
{
}

/* FBuildPatchCustomField implementation
*****************************************************************************/
FBuildPatchCustomField::FBuildPatchCustomField(const FString& Value)
	: CustomValue(Value)
{
}

FString FBuildPatchCustomField::AsString() const
{
	return CustomValue;
}

double FBuildPatchCustomField::AsDouble() const
{
	// The Json parser currently only supports float so we have to decode string blob instead
	double Rtn;
	if( FromStringBlob( CustomValue, Rtn ) )
	{
		return Rtn;
	}
	return 0;
}

int64 FBuildPatchCustomField::AsInteger() const
{
	// The Json parser currently only supports float so we have to decode string blob instead
	int64 Rtn;
	if( FromStringBlob( CustomValue, Rtn ) )
	{
		return Rtn;
	}
	return 0;
}

/* BuildPatchAppManifest namespace contains helper functions for handling multiple type options
*****************************************************************************/

namespace BuildPatchAppManifest
{
	template<typename ContainerType>
	int64 GetFileSizeHelper(const FBuildPatchAppManifest& Manifest, const ContainerType& Filenames)
	{
		int64 TotalSize = 0;
		for (const FString& Filename : Filenames)
		{
			TotalSize += Manifest.GetFileSize(Filename);
		}
		return TotalSize;
	}

	template<typename ContainerType>
	int64 GetDataSizeHelper(const FBuildPatchAppManifest& Manifest, const ContainerType& DataList)
	{
		int64 TotalSize = 0;
		for (const FGuid& DataId : DataList)
		{
			TotalSize += Manifest.GetDataSize(DataId);
		}
		return TotalSize;
	}
}

/* FBuildPatchAppManifest implementation
*****************************************************************************/

FBuildPatchAppManifest::FBuildPatchAppManifest()
	: ManifestFileVersion(EBuildPatchAppManifestVersion::Invalid)
	, bIsFileData(false)
	, AppID(INDEX_NONE)
	, AppName(TEXT(""))
	, BuildVersion(TEXT(""))
	, LaunchExe(TEXT(""))
	, LaunchCommand(TEXT(""))
	, PrereqIds()
	, PrereqName(TEXT(""))
	, PrereqPath(TEXT(""))
	, PrereqArgs(TEXT(""))
	, FileManifestList()
	, ChunkList()
	, CustomFields()
	, TotalBuildSize(INDEX_NONE)
	, TotalDownloadSize(INDEX_NONE)
	, bNeedsResaving(false)
{
}

FBuildPatchAppManifest::FBuildPatchAppManifest(const uint32& InAppID, const FString& InAppName)
	: ManifestFileVersion(EBuildPatchAppManifestVersion::Invalid)
	, bIsFileData(false)
	, AppID(InAppID)
	, AppName(InAppName)
	, BuildVersion(TEXT(""))
	, LaunchExe(TEXT(""))
	, LaunchCommand(TEXT(""))
	, PrereqIds()
	, PrereqName(TEXT(""))
	, PrereqPath(TEXT(""))
	, PrereqArgs(TEXT(""))
	, FileManifestList()
	, ChunkList()
	, CustomFields()
	, TotalBuildSize(INDEX_NONE)
	, TotalDownloadSize(INDEX_NONE)
	, bNeedsResaving(false)
{
}

FBuildPatchAppManifest::FBuildPatchAppManifest(const FBuildPatchAppManifest& Other)
	: ManifestFileVersion(Other.ManifestFileVersion)
	, bIsFileData(Other.bIsFileData)
	, AppID(Other.AppID)
	, AppName(Other.AppName)
	, BuildVersion(Other.BuildVersion)
	, LaunchExe(Other.LaunchExe)
	, LaunchCommand(Other.LaunchCommand)
	, PrereqIds(Other.PrereqIds)
	, PrereqName(Other.PrereqName)
	, PrereqPath(Other.PrereqPath)
	, PrereqArgs(Other.PrereqArgs)
	, FileManifestList(Other.FileManifestList)
	, ChunkList(Other.ChunkList)
	, CustomFields(Other.CustomFields)
	, TotalBuildSize(INDEX_NONE)
	, TotalDownloadSize(INDEX_NONE)
	, bNeedsResaving(Other.bNeedsResaving)
{
	InitLookups();
}

FBuildPatchAppManifest::~FBuildPatchAppManifest()
{
	DestroyData();
}

bool FBuildPatchAppManifest::SaveToFile(const FString& Filename, bool bUseBinary)
{
	bool bSuccess = false;
	FArchive* FileOut = IFileManager::Get().CreateFileWriter(*Filename);
	if (FileOut)
	{
		if (bUseBinary)
		{
			FManifestWriter ManifestData;
			Serialize(ManifestData);
			ManifestData.Finalize();
			if (!ManifestData.IsError())
			{
				int32 DataSize = ManifestData.TotalSize();
				TArray<uint8> TempCompressed;
				TempCompressed.AddUninitialized(DataSize);
				int32 CompressedSize = DataSize;
				bool bDataIsCompressed = FCompression::CompressMemory(
					static_cast<ECompressionFlags>(COMPRESS_ZLIB | COMPRESS_BiasMemory),
					TempCompressed.GetData(),
					CompressedSize,
					ManifestData.GetBytes().GetData(),
					DataSize);
				TempCompressed.SetNum(CompressedSize);

				TArray<uint8>& FileData = bDataIsCompressed ? TempCompressed : ManifestData.GetBytes();

				FManifestFileHeader Header;
				Header.StoredAs = bDataIsCompressed ? EManifestFileHeader::STORED_COMPRESSED : EManifestFileHeader::STORED_RAW;
				Header.DataSize = DataSize;
				Header.CompressedSize = bDataIsCompressed ? CompressedSize : 0;
				FSHA1::HashBuffer(FileData.GetData(), FileData.Num(), Header.SHAHash.Hash);

				// Write to disk
				*FileOut << Header;
				Header.HeaderSize = FileOut->Tell();
				FileOut->Seek(0);
				*FileOut << Header;
				FileOut->Serialize(FileData.GetData(), FileData.Num());
			}
		}
		else
		{
			FString JSONOutput;
			SerializeToJSON(JSONOutput);
			FTCHARToUTF8 JsonUTF8(*JSONOutput);
			FileOut->Serialize((UTF8CHAR*)JsonUTF8.Get(), JsonUTF8.Length() * sizeof(UTF8CHAR));
		}
		bSuccess = FileOut->Close();
		delete FileOut;
		FileOut = nullptr;
	}

	return bSuccess;
}

bool FBuildPatchAppManifest::LoadFromFile(const FString& Filename)
{
	TArray<uint8> FileData;
	if (FFileHelper::LoadFileToArray(FileData, *Filename))
	{
		return DeserializeFromData(FileData);
	}
	return false;
}

bool FBuildPatchAppManifest::DeserializeFromData(const TArray<uint8>& DataInput)
{
	if (DataInput.Num())
	{
		if (BufferIsJsonManifest(DataInput))
		{
			FString JsonManifest;
			FFileHelper::BufferToString(JsonManifest, DataInput.GetData(), DataInput.Num());
			return DeserializeFromJSON(JsonManifest);
		}
		else
		{
			FMemoryReader ManifestFile(DataInput);
			FManifestFileHeader Header;
			ManifestFile << Header;
			const int32 SignedHeaderSize = Header.HeaderSize;
			if (Header.CheckMagic() && DataInput.Num() > SignedHeaderSize)
			{
				FSHAHashData DataHash;
				FSHA1::HashBuffer(&DataInput[Header.HeaderSize], DataInput.Num() - Header.HeaderSize, DataHash.Hash);
				if (DataHash == Header.SHAHash)
				{
					TArray<uint8> UncompressedData;
					if (Header.StoredAs == EManifestFileHeader::STORED_COMPRESSED && (Header.CompressedSize + Header.HeaderSize) == DataInput.Num())
					{
						UncompressedData.AddUninitialized(Header.DataSize);
						if (!FCompression::UncompressMemory(
							static_cast<ECompressionFlags>(COMPRESS_ZLIB | COMPRESS_BiasMemory),
							UncompressedData.GetData(),
							Header.DataSize,
							&DataInput[Header.HeaderSize],
							DataInput.Num() - Header.HeaderSize))
						{
							return false;
						}
					}
					else if ((Header.DataSize + Header.HeaderSize) == DataInput.Num())
					{
						UncompressedData.Append(&DataInput[Header.HeaderSize], Header.DataSize);
					}
					else
					{
						return false;
					}
					FManifestReader ManifestData(UncompressedData);
					return Serialize(ManifestData);
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

bool FBuildPatchAppManifest::Serialize(FArchive& Ar)
{
	UBuildPatchManifest* Data = NewObject<UBuildPatchManifest>();
	Data->AddToRoot();

	// Make sure we use the correct serialization version, this is now fixed and must never use a newer version,
	// because the property tag has changed in structure meaning older clients would not read correctly.
	Ar.SetUE4Ver(VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG - 1);

	if (Ar.IsLoading())
	{
		DestroyData();
	}
	else
	{
		Data->ManifestFileVersion = ManifestFileVersion;
		Data->bIsFileData = bIsFileData;
		Data->AppID = AppID;
		Data->AppName = AppName;
		Data->BuildVersion = BuildVersion;
		Data->LaunchExe = LaunchExe;
		Data->LaunchCommand = LaunchCommand;
		Data->PrereqIds = PrereqIds;
		Data->PrereqName = PrereqName;
		Data->PrereqPath = PrereqPath;
		Data->PrereqArgs = PrereqArgs;
		Data->FileManifestList = FileManifestList;
		Data->ChunkList = ChunkList;
		Data->CustomFields = CustomFields;
	}

	Data->Serialize(Ar);

	if (Ar.IsLoading())
	{
		ManifestFileVersion = MoveTemp(Data->ManifestFileVersion);
		bIsFileData = MoveTemp(Data->bIsFileData);
		AppID = MoveTemp(Data->AppID);
		AppName = MoveTemp(Data->AppName);
		BuildVersion = MoveTemp(Data->BuildVersion);
		LaunchExe = MoveTemp(Data->LaunchExe);
		LaunchCommand = MoveTemp(Data->LaunchCommand);
		PrereqIds = MoveTemp(Data->PrereqIds);
		PrereqName = MoveTemp(Data->PrereqName);
		PrereqPath = MoveTemp(Data->PrereqPath);
		PrereqArgs = MoveTemp(Data->PrereqArgs);
		FileManifestList = MoveTemp(Data->FileManifestList);
		ChunkList = MoveTemp(Data->ChunkList);
		CustomFields = MoveTemp(Data->CustomFields);

		// If we didn't load the version number, we know it was skipped when saving therefore must be
		// the first UObject version
		if (ManifestFileVersion == static_cast<uint8>(EBuildPatchAppManifestVersion::Invalid))
		{
			ManifestFileVersion = EBuildPatchAppManifestVersion::StoredAsCompressedUClass;
		}

		// Setup internal lookups
		InitLookups();
	}

	Data->Clear();
	Data->RemoveFromRoot();
	return !Ar.IsError();
}

void FBuildPatchAppManifest::DestroyData()
{
	// Clear Manifest data
	ManifestFileVersion = EBuildPatchAppManifestVersion::Invalid;
	bIsFileData = false;
	AppID = INDEX_NONE;
	AppName.Empty();
	BuildVersion.Empty();
	LaunchExe.Empty();
	LaunchCommand.Empty();
	PrereqIds.Empty();
	PrereqName.Empty();
	PrereqPath.Empty();
	PrereqArgs.Empty();
	FileManifestList.Empty();
	ChunkList.Empty();
	CustomFields.Empty();
	FileNameLookup.Empty();
	FileManifestLookup.Empty();
	ChunkInfoLookup.Empty();
	CustomFieldLookup.Empty();
	TotalBuildSize = INDEX_NONE;
	TotalDownloadSize = INDEX_NONE;
	bNeedsResaving = false;
}

void FBuildPatchAppManifest::InitLookups()
{
	// Make sure file list is sorted
	FileManifestList.Sort();

	// Setup internals
	TotalBuildSize = 0;
	FileManifestLookup.Empty(FileManifestList.Num());
	TaggedFilesLookup.Empty();
	FileNameLookup.Empty(bIsFileData ? FileManifestList.Num() : 0);
	for (FFileManifestData& File : FileManifestList)
	{
		File.Init();
		TotalBuildSize += File.GetFileSize();
		FileManifestLookup.Add(File.Filename, &File);
		if (bIsFileData)
		{
			// File data chunk parts should have been checked already
			FileNameLookup.Add(File.FileChunkParts[0].Guid, &File.Filename);
		}
		if (File.InstallTags.Num() == 0)
		{
			// Untagged files are required
			TaggedFilesLookup.FindOrAdd(TEXT("")).Add(&File);
		}
		else
		{
			// Fill out lookup for optional files
			for (const FString& FileTag : File.InstallTags)
			{
				TaggedFilesLookup.FindOrAdd(FileTag).Add(&File);
			}
		}
	}
	TotalDownloadSize = 0;
	ChunkInfoLookup.Empty(ChunkList.Num());
	for (FChunkInfoData& Chunk : ChunkList)
	{
		ChunkInfoLookup.Add(Chunk.Guid, &Chunk);
		TotalDownloadSize += Chunk.FileSize;
	}
	CustomFieldLookup.Empty(CustomFields.Num());
	for (FCustomFieldData& CustomField : CustomFields)
	{
		CustomFieldLookup.Add(CustomField.Key, &CustomField);
	}
}

void FBuildPatchAppManifest::SerializeToJSON(FString& JSONOutput)
{
#if UE_BUILD_DEBUG // We'll use this to switch between human readable JSON
	TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy< TCHAR > > > Writer = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy< TCHAR > >::Create(&JSONOutput);
#else
	TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy< TCHAR > > > Writer = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy< TCHAR > >::Create(&JSONOutput);
#endif //ALLOW_DEBUG_FILES

	Writer->WriteObjectStart();
	{
		// Write general data
		Writer->WriteValue(TEXT("ManifestFileVersion"), ToStringBlob(static_cast<int32>(ManifestFileVersion)));
		Writer->WriteValue(TEXT("bIsFileData"), bIsFileData);
		Writer->WriteValue(TEXT("AppID"), ToStringBlob(AppID));
		Writer->WriteValue(TEXT("AppNameString"), AppName);
		Writer->WriteValue(TEXT("BuildVersionString"), BuildVersion);
		Writer->WriteValue(TEXT("LaunchExeString"), LaunchExe);
		Writer->WriteValue(TEXT("LaunchCommand"), LaunchCommand);
		Writer->WriteArrayStart(TEXT("PrereqIds"));
		for (const FString& PrereqId : PrereqIds)
		{
			Writer->WriteValue(PrereqId);
		}
		Writer->WriteArrayEnd();
		Writer->WriteValue(TEXT("PrereqName"), PrereqName);
		Writer->WriteValue(TEXT("PrereqPath"), PrereqPath);
		Writer->WriteValue(TEXT("PrereqArgs"), PrereqArgs);
		// Write file manifest data
		Writer->WriteArrayStart(TEXT("FileManifestList"));
		for (const FFileManifestData& FileManifest : FileManifestList)
		{
			Writer->WriteObjectStart();
			{
				Writer->WriteValue(TEXT("Filename"), FileManifest.Filename);
				Writer->WriteValue(TEXT("FileHash"), FString::FromBlob(FileManifest.FileHash.Hash, FSHA1::DigestSize));
				if (FileManifest.bIsUnixExecutable)
				{
					Writer->WriteValue(TEXT("bIsUnixExecutable"), FileManifest.bIsUnixExecutable);
				}
				if (FileManifest.bIsReadOnly)
				{
					Writer->WriteValue(TEXT("bIsReadOnly"), FileManifest.bIsReadOnly);
				}
				if (FileManifest.bIsCompressed)
				{
					Writer->WriteValue(TEXT("bIsCompressed"), FileManifest.bIsCompressed);
				}
				const bool bIsSymlink = !FileManifest.SymlinkTarget.IsEmpty();
				if (bIsSymlink)
				{
					Writer->WriteValue(TEXT("SymlinkTarget"), FileManifest.SymlinkTarget);
				}
				else
				{
					Writer->WriteArrayStart(TEXT("FileChunkParts"));
					{
						for (const FChunkPartData& FileChunkPart : FileManifest.FileChunkParts)
						{
							Writer->WriteObjectStart();
							{
								Writer->WriteValue(TEXT("Guid"), FileChunkPart.Guid.ToString());
								Writer->WriteValue(TEXT("Offset"), ToStringBlob(FileChunkPart.Offset));
								Writer->WriteValue(TEXT("Size"), ToStringBlob(FileChunkPart.Size));
							}
							Writer->WriteObjectEnd();
						}
					}
					Writer->WriteArrayEnd();
				}
				if (FileManifest.InstallTags.Num() > 0)
				{
					Writer->WriteArrayStart(TEXT("InstallTags"));
					{
						for (const FString& InstallTag : FileManifest.InstallTags)
						{
							Writer->WriteValue(InstallTag);
						}
					}
					Writer->WriteArrayEnd();
				}
			}
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		// Write chunk hash list
		Writer->WriteObjectStart(TEXT("ChunkHashList"));
		for (const FChunkInfoData& ChunkInfo : ChunkList)
		{
			const FGuid& ChunkGuid = ChunkInfo.Guid;
			const uint64& ChunkHash = ChunkInfo.Hash;
			Writer->WriteValue(ChunkGuid.ToString(), ToStringBlob(ChunkHash));
		}
		Writer->WriteObjectEnd();
		// Write chunk sha list
		Writer->WriteObjectStart(TEXT("ChunkShaList"));
		for (const FChunkInfoData& ChunkInfo : ChunkList)
		{
			const FGuid& ChunkGuid = ChunkInfo.Guid;
			const FSHAHashData& ChunkSha = ChunkInfo.ShaHash;
			Writer->WriteValue(ChunkGuid.ToString(), ToHexString(ChunkSha));
		}
		Writer->WriteObjectEnd();
		// Write data group list
		Writer->WriteObjectStart(TEXT("DataGroupList"));
		for (const FChunkInfoData& ChunkInfo : ChunkList)
		{
			const FGuid& DataGuid = ChunkInfo.Guid;
			const uint8& DataGroup = ChunkInfo.GroupNumber;
			Writer->WriteValue(DataGuid.ToString(), ToStringBlob(DataGroup));
		}
		Writer->WriteObjectEnd();
		// Write chunk size list
		Writer->WriteObjectStart(TEXT("ChunkFilesizeList"));
		for (const FChunkInfoData& ChunkInfo : ChunkList)
		{
			const FGuid& ChunkGuid = ChunkInfo.Guid;
			const int64& ChunkSize = ChunkInfo.FileSize;
			Writer->WriteValue(ChunkGuid.ToString(), ToStringBlob(ChunkSize));
		}
		Writer->WriteObjectEnd();
		// Write custom fields
		Writer->WriteObjectStart(TEXT("CustomFields"));
		for (const FCustomFieldData& CustomField : CustomFields)
		{
			Writer->WriteValue(CustomField.Key, CustomField.Value);
		}
		Writer->WriteObjectEnd();
	}
	Writer->WriteObjectEnd();

	Writer->Close();
}

// @TODO LSwift: Perhaps replace FromBlob and ToBlob usage with hexadecimal notation instead
bool FBuildPatchAppManifest::DeserializeFromJSON( const FString& JSONInput )
{
	bool bSuccess = true;
	TSharedPtr<FJsonObject> JSONManifestObject;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JSONInput);

	// Clear current data
	DestroyData();

	// Attempt to deserialize JSON
	if (!FJsonSerializer::Deserialize(Reader, JSONManifestObject) || !JSONManifestObject.IsValid())
	{
		return false;
	}

	// Store a list of all data GUID for later use
	TSet<FGuid> AllDataGuids;

	// Get the values map
	TMap<FString, TSharedPtr<FJsonValue>>& JsonValueMap = JSONManifestObject->Values;

	// Manifest version did not always exist
	int32 ManifestFileVersionInt = 0;
	TSharedPtr<FJsonValue> JsonManifestFileVersion = JsonValueMap.FindRef(TEXT("ManifestFileVersion"));
	if (JsonManifestFileVersion.IsValid() && FromStringBlob(JsonManifestFileVersion->AsString(), ManifestFileVersionInt))
	{
		ManifestFileVersion = static_cast<EBuildPatchAppManifestVersion::Type>(ManifestFileVersionInt);
	}
	else
	{
		// Then we presume version just before we started outputting the version
		ManifestFileVersion = static_cast<EBuildPatchAppManifestVersion::Type>(EBuildPatchAppManifestVersion::StartStoringVersion - 1);
	}

	// Get the app and version strings
	TSharedPtr<FJsonValue> JsonAppID = JsonValueMap.FindRef(TEXT("AppID"));
	TSharedPtr<FJsonValue> JsonAppNameString = JsonValueMap.FindRef(TEXT("AppNameString"));
	TSharedPtr<FJsonValue> JsonBuildVersionString = JsonValueMap.FindRef(TEXT("BuildVersionString"));
	TSharedPtr<FJsonValue> JsonLaunchExe = JsonValueMap.FindRef(TEXT("LaunchExeString"));
	TSharedPtr<FJsonValue> JsonLaunchCommand = JsonValueMap.FindRef(TEXT("LaunchCommand"));
	TSharedPtr<FJsonValue> JsonPrereqName = JsonValueMap.FindRef(TEXT("PrereqName"));
	TSharedPtr<FJsonValue> JsonPrereqPath = JsonValueMap.FindRef(TEXT("PrereqPath"));
	TSharedPtr<FJsonValue> JsonPrereqArgs = JsonValueMap.FindRef(TEXT("PrereqArgs"));
	bSuccess = bSuccess && JsonAppID.IsValid();
	if( bSuccess )
	{
		bSuccess = bSuccess && FromStringBlob( JsonAppID->AsString(), AppID );
	}
	bSuccess = bSuccess && JsonAppNameString.IsValid();
	if( bSuccess )
	{
		AppName = JsonAppNameString->AsString();
	}
	bSuccess = bSuccess && JsonBuildVersionString.IsValid();
	if( bSuccess )
	{
		BuildVersion = JsonBuildVersionString->AsString();
	}
	bSuccess = bSuccess && JsonLaunchExe.IsValid();
	if( bSuccess )
	{
		LaunchExe = JsonLaunchExe->AsString();
	}
	bSuccess = bSuccess && JsonLaunchCommand.IsValid();
	if( bSuccess )
	{
		LaunchCommand = JsonLaunchCommand->AsString();
	}

	// Get the prerequisites installer info.  These are optional entries.
	PrereqName = JsonPrereqName.IsValid() ? JsonPrereqName->AsString() : FString();
	PrereqPath = JsonPrereqPath.IsValid() ? JsonPrereqPath->AsString() : FString();
	PrereqArgs = JsonPrereqArgs.IsValid() ? JsonPrereqArgs->AsString() : FString();

	// Get the FileManifestList
	TSharedPtr<FJsonValue> JsonFileManifestList = JsonValueMap.FindRef(TEXT("FileManifestList"));
	bSuccess = bSuccess && JsonFileManifestList.IsValid();
	if( bSuccess )
	{
		TArray<TSharedPtr<FJsonValue>> JsonFileManifestArray = JsonFileManifestList->AsArray();
		for (auto JsonFileManifestIt = JsonFileManifestArray.CreateConstIterator(); JsonFileManifestIt && bSuccess; ++JsonFileManifestIt)
		{
			TSharedPtr<FJsonObject> JsonFileManifest = (*JsonFileManifestIt)->AsObject();

			const int32 FileIndex = FileManifestList.Add(FFileManifestData());
			FFileManifestData& FileManifest = FileManifestList[FileIndex];
			FileManifest.Filename = JsonFileManifest->GetStringField(TEXT("Filename"));
			bSuccess = bSuccess && FString::ToBlob(JsonFileManifest->GetStringField(TEXT("FileHash")), FileManifest.FileHash.Hash, FSHA1::DigestSize);
			TArray<TSharedPtr<FJsonValue>> JsonChunkPartArray = JsonFileManifest->GetArrayField(TEXT("FileChunkParts"));
			for (auto JsonChunkPartIt = JsonChunkPartArray.CreateConstIterator(); JsonChunkPartIt && bSuccess; ++JsonChunkPartIt)
			{
				const int32 ChunkIndex = FileManifest.FileChunkParts.Add(FChunkPartData());
				FChunkPartData& FileChunkPart = FileManifest.FileChunkParts[ChunkIndex];
				TSharedPtr<FJsonObject> JsonChunkPart = (*JsonChunkPartIt)->AsObject();
				bSuccess = bSuccess && FGuid::Parse(JsonChunkPart->GetStringField(TEXT("Guid")), FileChunkPart.Guid);
				bSuccess = bSuccess && FromStringBlob(JsonChunkPart->GetStringField(TEXT("Offset")), FileChunkPart.Offset);
				bSuccess = bSuccess && FromStringBlob(JsonChunkPart->GetStringField(TEXT("Size")), FileChunkPart.Size);
				AllDataGuids.Add(FileChunkPart.Guid);
			}
			if (JsonFileManifest->HasTypedField<EJson::Array>(TEXT("InstallTags")))
			{
				TArray<TSharedPtr<FJsonValue>> JsonInstallTagsArray = JsonFileManifest->GetArrayField(TEXT("InstallTags"));
				for (auto JsonInstallTagIt = JsonInstallTagsArray.CreateConstIterator(); JsonInstallTagIt && bSuccess; ++JsonInstallTagIt)
				{
					FileManifest.InstallTags.Add((*JsonInstallTagIt)->AsString());
				}
			}
			FileManifest.bIsUnixExecutable = JsonFileManifest->HasField(TEXT("bIsUnixExecutable")) && JsonFileManifest->GetBoolField(TEXT("bIsUnixExecutable"));
			FileManifest.bIsReadOnly = JsonFileManifest->HasField(TEXT("bIsReadOnly")) && JsonFileManifest->GetBoolField(TEXT("bIsReadOnly"));
			FileManifest.bIsCompressed = JsonFileManifest->HasField(TEXT("bIsCompressed")) && JsonFileManifest->GetBoolField(TEXT("bIsCompressed"));
			FileManifest.SymlinkTarget = JsonFileManifest->HasField(TEXT("SymlinkTarget")) ? JsonFileManifest->GetStringField(TEXT("SymlinkTarget")) : TEXT("");
			FileManifest.Init();
		}
	}
	FileManifestList.Sort();
	for (FFileManifestData& FileManifest : FileManifestList)
	{
		FileManifestLookup.Add(FileManifest.Filename, &FileManifest);
	}

	// For each chunk setup its info
	for (const FGuid& DataGuid : AllDataGuids)
	{
		int32 ChunkIndex = ChunkList.Add(FChunkInfoData());
		ChunkList[ChunkIndex].Guid = DataGuid;
	}

	// Setup chunk info lookup
	for (FChunkInfoData& ChunkInfo : ChunkList)
	{
		ChunkInfoLookup.Add(ChunkInfo.Guid, &ChunkInfo);
	}

	// Get the ChunkHashList
	bool bHasChunkHashList = false;
	TSharedPtr<FJsonValue> JsonChunkHashList = JsonValueMap.FindRef(TEXT("ChunkHashList"));
	bSuccess = bSuccess && JsonChunkHashList.IsValid();
	if (bSuccess)
	{
		TSharedPtr<FJsonObject> JsonChunkHashListObj = JsonChunkHashList->AsObject();
		for (auto ChunkHashIt = JsonChunkHashListObj->Values.CreateConstIterator(); ChunkHashIt && bSuccess; ++ChunkHashIt)
		{
			FGuid ChunkGuid;
			uint64 ChunkHash = 0;
			bSuccess = bSuccess && FGuid::Parse(ChunkHashIt.Key(), ChunkGuid);
			bSuccess = bSuccess && FromStringBlob(ChunkHashIt.Value()->AsString(), ChunkHash);
			if (bSuccess && ChunkInfoLookup.Contains(ChunkGuid))
			{
				FChunkInfoData* ChunkInfoData = ChunkInfoLookup[ChunkGuid];
				ChunkInfoData->Hash = ChunkHash;
				bHasChunkHashList = true;
			}
		}
	}

	// Get the ChunkShaList (optional)
	TSharedPtr<FJsonValue> JsonChunkShaList = JsonValueMap.FindRef(TEXT("ChunkShaList"));
	if (JsonChunkShaList.IsValid())
	{
		TSharedPtr<FJsonObject> JsonChunkHashListObj = JsonChunkShaList->AsObject();
		for (auto ChunkHashIt = JsonChunkHashListObj->Values.CreateConstIterator(); ChunkHashIt && bSuccess; ++ChunkHashIt)
		{
			FGuid ChunkGuid;
			FSHAHashData ChunkSha;
			bSuccess = bSuccess && FGuid::Parse(ChunkHashIt.Key(), ChunkGuid);
			bSuccess = bSuccess && FromHexString(ChunkHashIt.Value()->AsString(), ChunkSha);
			if (bSuccess && ChunkInfoLookup.Contains(ChunkGuid))
			{
				FChunkInfoData* ChunkInfoData = ChunkInfoLookup[ChunkGuid];
				ChunkInfoData->ShaHash = ChunkSha;
			}
		}
	}

	// Get the PrereqIds (optional)
	TSharedPtr<FJsonValue> JsonPrereqIds = JsonValueMap.FindRef(TEXT("PrereqIds"));
	if (bSuccess && JsonPrereqIds.IsValid())
	{
		TArray<TSharedPtr<FJsonValue>> JsonPrereqIdsArray = JsonPrereqIds->AsArray();
		for (TSharedPtr<FJsonValue> JsonPrereqId : JsonPrereqIdsArray)
		{
			PrereqIds.Add(JsonPrereqId->AsString());
		}
	}
	else
	{
		// We fall back to using the hash of the prereq exe if we have no prereq ids specified
		FString PrereqFilename = PrereqPath;
		PrereqFilename.ReplaceInline(TEXT("\\"), TEXT("/"));
		const FFileManifestData* const * FoundFileManifest = FileManifestLookup.Find(PrereqFilename);
		if (FoundFileManifest)
		{
			FSHAHashData PrereqHash;
			FMemory::Memcpy(PrereqHash.Hash, (*FoundFileManifest)->FileHash.Hash, FSHA1::DigestSize);
			PrereqIds.Add(PrereqHash.ToString());
		}
	}

	// Get the DataGroupList
	TSharedPtr<FJsonValue> JsonDataGroupList = JsonValueMap.FindRef(TEXT("DataGroupList"));
	if (JsonDataGroupList.IsValid())
	{
		TSharedPtr<FJsonObject> JsonDataGroupListObj = JsonDataGroupList->AsObject();
		for (auto DataGroupIt = JsonDataGroupListObj->Values.CreateConstIterator(); DataGroupIt && bSuccess; ++DataGroupIt)
		{
			FGuid DataGuid;
			uint8 DataGroup = INDEX_NONE;
			// If the list exists, we must be able to parse it ok otherwise error
			bSuccess = bSuccess && FGuid::Parse(DataGroupIt.Key(), DataGuid);
			bSuccess = bSuccess && FromStringBlob(DataGroupIt.Value()->AsString(), DataGroup);
			if (bSuccess && ChunkInfoLookup.Contains(DataGuid))
			{
				FChunkInfoData* ChunkInfoData = ChunkInfoLookup[DataGuid];
				ChunkInfoData->GroupNumber = DataGroup;
			}
		}
	}
	else if (bSuccess)
	{
		// If the list did not exist in the manifest then the grouping is the deprecated crc functionality, as long
		// as there are no previous parsing errors we can build the group list from the Guids.
		for (FChunkInfoData& ChunkInfo : ChunkList)
		{
			ChunkInfo.GroupNumber = FCrc::MemCrc_DEPRECATED(&ChunkInfo.Guid, sizeof(FGuid)) % 100;
		}
	}

	// Get the ChunkFilesizeList
	bool bHasChunkFilesizeList = false;
	TSharedPtr< FJsonValue > JsonChunkFilesizeList = JsonValueMap.FindRef(TEXT("ChunkFilesizeList"));
	if (JsonChunkFilesizeList.IsValid())
	{
		TSharedPtr< FJsonObject > JsonChunkFilesizeListObj = JsonChunkFilesizeList->AsObject();
		for (auto ChunkFilesizeIt = JsonChunkFilesizeListObj->Values.CreateConstIterator(); ChunkFilesizeIt; ++ChunkFilesizeIt)
		{
			FGuid ChunkGuid;
			int64 ChunkSize = 0;
			if (FGuid::Parse(ChunkFilesizeIt.Key(), ChunkGuid))
			{
				FromStringBlob(ChunkFilesizeIt.Value()->AsString(), ChunkSize);
				if (ChunkInfoLookup.Contains(ChunkGuid))
				{
					FChunkInfoData* ChunkInfoData = ChunkInfoLookup[ChunkGuid];
					ChunkInfoData->FileSize = ChunkSize;
					bHasChunkFilesizeList = true;
				}
			}
		}
	}
	if (bHasChunkFilesizeList == false)
	{
		// Missing chunk list, version before we saved them compressed.. Assume chunk size
		for (FChunkInfoData& ChunkInfo : ChunkList)
		{
			ChunkInfo.FileSize = BuildPatchServices::ChunkDataSize;
		}
	}

	// Get the bIsFileData value. The variable will exist in versions of StoresIfChunkOrFileData or later, otherwise the previous method is to check
	// if ChunkHashList is empty.
	TSharedPtr<FJsonValue> JsonIsFileData = JsonValueMap.FindRef(TEXT("bIsFileData"));
	if (JsonIsFileData.IsValid() && JsonIsFileData->Type == EJson::Boolean)
	{
		bIsFileData = JsonIsFileData->AsBool();
	}
	else
	{
		bIsFileData = !bHasChunkHashList;
	}

	// Get the custom fields. This is optional, and should not fail if it does not exist
	TSharedPtr< FJsonValue > JsonCustomFields = JsonValueMap.FindRef( TEXT( "CustomFields" ) );
	if( JsonCustomFields.IsValid() )
	{
		TSharedPtr< FJsonObject > JsonCustomFieldsObj = JsonCustomFields->AsObject();
		for( auto CustomFieldIt = JsonCustomFieldsObj->Values.CreateConstIterator(); CustomFieldIt && bSuccess; ++CustomFieldIt )
		{
			CustomFields.Add(FCustomFieldData(CustomFieldIt.Key(), CustomFieldIt.Value()->AsString()));
		}
	}
	CustomFieldLookup.Empty(CustomFields.Num());
	for (FCustomFieldData& CustomField : CustomFields)
	{
		CustomFieldLookup.Add(CustomField.Key, &CustomField);
	}

	// If this is file data, fill out the guid to filename lookup, and chunk file size and SHA.
	if (bIsFileData)
	{
		for (FFileManifestData& FileManifest : FileManifestList)
		{
			if (FileManifest.FileChunkParts.Num() == 1)
			{
				const FGuid& Guid = FileManifest.FileChunkParts[0].Guid;
				FileNameLookup.Add(Guid, &FileManifest.Filename);
				if (ChunkInfoLookup.Contains(Guid))
				{
					FChunkInfoData* ChunkInfoData = ChunkInfoLookup[Guid];
					ChunkInfoData->FileSize = FileManifest.GetFileSize();
					ChunkInfoData->ShaHash = FileManifest.FileHash;
				}
			}
			else
			{
				bSuccess = false;
			}
		}
	}

	// Calculate build size
	TotalBuildSize = 0;
	TotalDownloadSize = 0;
	if (bSuccess)
	{
		for (const FFileManifestData& FileManifest : FileManifestList)
		{
			TotalBuildSize += FileManifest.GetFileSize();
		}
		for (const FChunkInfoData& Chunk : ChunkList)
		{
			TotalDownloadSize += Chunk.FileSize;
		}
	}

	// Mark as should be re-saved, client that stores manifests should start using binary
	bNeedsResaving = true;

	// Setup internal lookups
	InitLookups();

	// Make sure we don't have any half loaded data
	if( !bSuccess )
	{
		DestroyData();
	}

	return bSuccess;
}

EBuildPatchAppManifestVersion::Type FBuildPatchAppManifest::GetManifestVersion() const
{
	return static_cast<EBuildPatchAppManifestVersion::Type>(ManifestFileVersion);
}

void FBuildPatchAppManifest::GetChunksRequiredForFiles(const TSet<FString>& Filenames, TSet<FGuid>& RequiredChunks) const
{
	for (const FString& Filename : Filenames)
	{
		const FFileManifestData* FileManifest = GetFileManifest(Filename);
		if (FileManifest != nullptr)
		{
			for (const FChunkPartData& ChunkPart : FileManifest->FileChunkParts)
			{
				RequiredChunks.Add(ChunkPart.Guid);
			}
		}
	}
}

int64 FBuildPatchAppManifest::GetDownloadSize() const
{
	return TotalDownloadSize;
}

int64 FBuildPatchAppManifest::GetDownloadSize(const TSet<FString>& Tags) const
{
	// For each tag we iterate the files and for each new chunk we find we add the download size for it.
	TSet<FGuid> RequiredChunks;
	int64 TotalSize = 0;
	for (const FString& Tag : Tags)
	{
		const TArray<FFileManifestData*>* Files = TaggedFilesLookup.Find(Tag);
		if (Files != nullptr)
		{
			for (const FFileManifestData* File : *Files)
			{
				for (const FChunkPartData& ChunkPart : File->FileChunkParts)
				{
					bool bAlreadyInSet;
					RequiredChunks.Add(ChunkPart.Guid, &bAlreadyInSet);
					if (!bAlreadyInSet)
					{
						const FChunkInfoData * const * ChunkInfo = ChunkInfoLookup.Find(ChunkPart.Guid);
						if (ChunkInfo != nullptr)
						{
							TotalSize += (*ChunkInfo)->FileSize;
						}
					}
				}
			}
		}
	}
	return TotalSize;
}

int64 FBuildPatchAppManifest::GetDeltaDownloadSize(const TSet<FString>& Tags, const IBuildManifestRef& PreviousVersion) const
{
	return GetDeltaDownloadSize(Tags, PreviousVersion, Tags);
}

int64 FBuildPatchAppManifest::GetDeltaDownloadSize(const TSet<FString>& InTags, const IBuildManifestRef& InPreviousVersion, const TSet<FString>& InPreviousTags) const
{
	TSet<FString> Tags = InTags;
	FBuildPatchAppManifestRef PreviousVersion = StaticCastSharedRef< FBuildPatchAppManifest >(InPreviousVersion);
	TSet<FString> PreviousTags = InPreviousTags;
	if (Tags.Num() == 0)
	{
		GetFileTagList(Tags);
	}
	if (PreviousTags.Num() == 0)
	{
		PreviousVersion->GetFileTagList(PreviousTags);
	}

	// Enumerate what is available.
	TSet<FString> FilesInstalled;
	TSet<FGuid> ChunksInstalled;
	PreviousVersion->GetTaggedFileList(PreviousTags, FilesInstalled);
	PreviousVersion->GetChunksRequiredForFiles(FilesInstalled, ChunksInstalled);

	// Enumerate what has changed.
	FString DummyString;
	TSet<FString> OutdatedFiles;
	GetOutdatedFiles(PreviousVersion, DummyString, OutdatedFiles);

	// Enumerate what is needed for the update.
	TSet<FString> FilesNeeded;
	TSet<FGuid> ChunksNeeded;
	GetTaggedFileList(Tags, FilesNeeded);
	FilesNeeded = OutdatedFiles.Intersect(FilesNeeded);
	GetChunksRequiredForFiles(FilesNeeded, ChunksNeeded);
	ChunksNeeded = ChunksNeeded.Difference(ChunksInstalled);

	// Return download size of required chunks.
	return GetDataSize(ChunksNeeded);
}

int64 FBuildPatchAppManifest::GetBuildSize() const
{
	return TotalBuildSize;
}

int64 FBuildPatchAppManifest::GetBuildSize(const TSet<FString>& Tags) const
{
	// For each tag we iterate the files and for each new file we find we add the size for it.
	TSet<const FFileManifestData*> RequiredFiles;
	int64 TotalSize = 0;
	for (const FString& Tag : Tags)
	{
		const TArray<FFileManifestData*>* Files = TaggedFilesLookup.Find(Tag);
		if (Files != nullptr)
		{
			for (const FFileManifestData* File : *Files)
			{
				bool bAlreadyInSet;
				RequiredFiles.Add(File, &bAlreadyInSet);
				if (!bAlreadyInSet)
				{
					TotalSize += File->GetFileSize();
				}
			}
		}
	}
	return TotalSize;
}

TArray<FString> FBuildPatchAppManifest::GetBuildFileList() const
{
	TArray<FString> Filenames;
	GetFileList(Filenames);
	return MoveTemp(Filenames);
}

int64 FBuildPatchAppManifest::GetFileSize(const TArray<FString>& Filenames) const
{
	return BuildPatchAppManifest::GetFileSizeHelper(*this, Filenames);
}

int64 FBuildPatchAppManifest::GetFileSize(const TSet<FString>& Filenames) const
{
	return BuildPatchAppManifest::GetFileSizeHelper(*this, Filenames);
}

int64 FBuildPatchAppManifest::GetFileSize(const FString& Filename) const
{
	FFileManifestData* const * FileManifest = FileManifestLookup.Find(Filename);
	if (FileManifest)
	{
		return (*FileManifest)->GetFileSize();
	}
	return 0;
}

int64 FBuildPatchAppManifest::GetDataSize(const FGuid& DataGuid) const
{
	if (ChunkInfoLookup.Contains(DataGuid))
	{
		// Chunk file sizes are stored in the info
		return ChunkInfoLookup[DataGuid]->FileSize;
	}
	else if (bIsFileData)
	{
		// For file data, the file must exist in the list
		check(FileNameLookup.Contains(DataGuid));
		return GetFileSize(*FileNameLookup[DataGuid]);
	}
	else
	{
		// Default chunk size to be the data size. Inaccurate, but represents original behavior.
		return BuildPatchServices::ChunkDataSize;
	}
}

int64 FBuildPatchAppManifest::GetDataSize(const TArray<FGuid>& DataGuids) const
{
	return BuildPatchAppManifest::GetDataSizeHelper(*this, DataGuids);
}

int64 FBuildPatchAppManifest::GetDataSize(const TSet<FGuid>& DataGuids) const
{
	return BuildPatchAppManifest::GetDataSizeHelper(*this, DataGuids);
}

uint32 FBuildPatchAppManifest::GetNumFiles() const
{
	return FileManifestList.Num();
}

void FBuildPatchAppManifest::GetFileList(TArray<FString>& Filenames) const
{
	FileManifestLookup.GetKeys(Filenames);
}

void FBuildPatchAppManifest::GetFileList(TSet<FString>& Filenames) const
{
	TArray<FString> FilenameArray;
	FileManifestLookup.GetKeys(FilenameArray);
	Filenames.Append(MoveTemp(FilenameArray));
}

void FBuildPatchAppManifest::GetFileTagList(TSet<FString>& Tags) const
{
	TArray<FString> TagsArray;
	TaggedFilesLookup.GetKeys(TagsArray);
	Tags.Append(MoveTemp(TagsArray));
}

void FBuildPatchAppManifest::GetTaggedFileList(const TSet<FString>& Tags, TSet<FString>& TaggedFiles) const
{
	for (const FString& Tag : Tags)
	{
		const TArray<FFileManifestData*>* Files = TaggedFilesLookup.Find(Tag);
		if (Files != nullptr)
		{
			for (const FFileManifestData* File : *Files)
			{
				TaggedFiles.Add(File->Filename);
			}
		}
	}
}

void FBuildPatchAppManifest::GetDataList(TArray<FGuid>& DataGuids) const
{
	ChunkInfoLookup.GetKeys(DataGuids);
}

void FBuildPatchAppManifest::GetDataList(TSet<FGuid>& DataGuids) const
{
	DataGuids.Empty(ChunkInfoLookup.Num());
	for (const TPair<FGuid, FChunkInfoData*> Pair : ChunkInfoLookup)
	{
		DataGuids.Add(Pair.Key);
	}
}

const FFileManifestData* FBuildPatchAppManifest::GetFileManifest(const FString& Filename) const
{
	const FFileManifestData* const * FileManifest = FileManifestLookup.Find(Filename);
	return (FileManifest) ? (*FileManifest) : nullptr;
}

bool FBuildPatchAppManifest::IsFileDataManifest() const
{
	return bIsFileData;
}

bool FBuildPatchAppManifest::GetChunkHash(const FGuid& ChunkGuid, uint64& OutHash) const
{
	const FChunkInfoData* const * ChunkInfo = ChunkInfoLookup.Find(ChunkGuid);
	if (ChunkInfo)
	{
		OutHash = (*ChunkInfo)->Hash;
		return true;
	}
	return false;
}

bool FBuildPatchAppManifest::GetChunkShaHash(const FGuid& ChunkGuid, FSHAHashData& OutHash) const
{
	const FChunkInfoData* const * ChunkInfo = ChunkInfoLookup.Find(ChunkGuid);
	if (ChunkInfo != nullptr)
	{
		OutHash = (*ChunkInfo)->ShaHash;
		return OutHash.IsZero() == false;
	}
	return false;
}

bool FBuildPatchAppManifest::GetFileHash(const FGuid& FileGuid, FSHAHashData& OutHash) const
{
	const FString* const * FoundFilename = FileNameLookup.Find(FileGuid);
	if (FoundFilename)
	{
		return GetFileHash(**FoundFilename, OutHash);
	}
	return false;
}

bool FBuildPatchAppManifest::GetFileHash(const FString& Filename, FSHAHashData& OutHash) const
{
	const FFileManifestData* const * FoundFileManifest = FileManifestLookup.Find(Filename);
	if (FoundFileManifest)
	{
		FMemory::Memcpy(OutHash.Hash, (*FoundFileManifest)->FileHash.Hash, FSHA1::DigestSize);
		return true;
	}
	return false;
}

bool FBuildPatchAppManifest::GetFilePartHash(const FGuid& FilePartGuid, uint64& OutHash) const
{
	const FChunkInfoData* const * FilePartInfo = ChunkInfoLookup.Find(FilePartGuid);
	if (FilePartInfo)
	{
		OutHash = (*FilePartInfo)->Hash;
		return true;
	}
	return false;
}

uint32 FBuildPatchAppManifest::GetAppID() const
{
	return AppID;
}

const FString& FBuildPatchAppManifest::GetAppName() const
{
	return AppName;
}

const FString& FBuildPatchAppManifest::GetVersionString() const
{
	return BuildVersion;
}

const FString& FBuildPatchAppManifest::GetLaunchExe() const
{
	return LaunchExe;
}

const FString& FBuildPatchAppManifest::GetLaunchCommand() const
{
	return LaunchCommand;
}

const TSet<FString>& FBuildPatchAppManifest::GetPrereqIds() const
{
	return PrereqIds;
}

const FString& FBuildPatchAppManifest::GetPrereqName() const
{
	return PrereqName;
}

const FString& FBuildPatchAppManifest::GetPrereqPath() const
{
	return PrereqPath;
}

const FString& FBuildPatchAppManifest::GetPrereqArgs() const
{
	return PrereqArgs;
}

IBuildManifestRef FBuildPatchAppManifest::Duplicate() const
{
	return MakeShareable(new FBuildPatchAppManifest(*this));
}

void FBuildPatchAppManifest::CopyCustomFields(const IBuildManifestRef& InOther, bool bClobber)
{
	// Cast manifest parameters
	FBuildPatchAppManifestRef Other = StaticCastSharedRef< FBuildPatchAppManifest >(InOther);

	// Use lookup to overwrite existing and list additional fields
	TArray<FCustomFieldData> Extras;
	for (const FCustomFieldData& CustomField : Other->CustomFields)
	{
		if (CustomFieldLookup.Contains(CustomField.Key))
		{
			if (bClobber)
			{
				CustomFieldLookup[CustomField.Key]->Value = CustomField.Value;
			}
		}
		else
		{
			Extras.Add(CustomField);
		}
	}

	// Add the extra fields
	CustomFields.Append(Extras);

	// Reset the loookup
	CustomFieldLookup.Empty(CustomFields.Num());
	for (FCustomFieldData& CustomField : CustomFields)
	{
		CustomFieldLookup.Add(CustomField.Key, &CustomField);
	}
}

const IManifestFieldPtr FBuildPatchAppManifest::GetCustomField(const FString& FieldName) const
{
	IManifestFieldPtr Return;
	if (CustomFieldLookup.Contains(FieldName))
	{
		Return = MakeShareable(new FBuildPatchCustomField(CustomFieldLookup[FieldName]->Value));
	}
	return Return;
}

const IManifestFieldPtr FBuildPatchAppManifest::SetCustomField(const FString& FieldName, const FString& Value)
{
	if (CustomFieldLookup.Contains(FieldName))
	{
		CustomFieldLookup[FieldName]->Value = Value;
	}
	else
	{
		CustomFields.Add(FCustomFieldData(FieldName, Value));
		CustomFieldLookup.Empty(CustomFields.Num());
		for (FCustomFieldData& CustomField : CustomFields)
		{
			CustomFieldLookup.Add(CustomField.Key, &CustomField);
		}
	}
	return GetCustomField(FieldName);
}

const IManifestFieldPtr FBuildPatchAppManifest::SetCustomField(const FString& FieldName, const double& Value)
{
	return SetCustomField(FieldName, ToStringBlob(Value));
}

const IManifestFieldPtr FBuildPatchAppManifest::SetCustomField(const FString& FieldName, const int64& Value)
{
	return SetCustomField(FieldName, ToStringBlob(Value));
}

void FBuildPatchAppManifest::RemoveCustomField(const FString& FieldName)
{
	CustomFields.RemoveAll([&](const FCustomFieldData& Entry){ return Entry.Key == FieldName; });
	CustomFieldLookup.Empty(CustomFields.Num());
	for (FCustomFieldData& CustomField : CustomFields)
	{
		CustomFieldLookup.Add(CustomField.Key, &CustomField);
	}
}

int32 FBuildPatchAppManifest::EnumerateProducibleChunks(const FString& InstallDirectory, const TSet<FGuid>& ChunksRequired, TSet<FGuid>& ChunksAvailable) const
{
	int32 Count = 0;
	// For each required chunk, check we have the data available.
	TMap<FString, int64> InstallationFileSizes;
	for (const FGuid& ChunkRequired : ChunksRequired)
	{
		if (ChunksAvailable.Contains(ChunkRequired) == false && ChunkInfoLookup.Contains(ChunkRequired))
		{
			// Check each file.
			TArray<FFileChunkPart> FileChunkParts = GetFilePartsForChunk(ChunkRequired);
			bool bCanMakeChunk = FileChunkParts.Num() > 0;
			for (int32 FileChunkPartIdx = 0; FileChunkPartIdx < FileChunkParts.Num() && bCanMakeChunk; ++FileChunkPartIdx)
			{
				const FFileChunkPart& FileChunkPart = FileChunkParts[FileChunkPartIdx];
				if (InstallationFileSizes.Contains(FileChunkPart.Filename) == false)
				{
					InstallationFileSizes.Add(FileChunkPart.Filename, IFileManager::Get().FileSize(*(InstallDirectory / FileChunkPart.Filename)));
				}
				bCanMakeChunk = bCanMakeChunk && GetFileSize(FileChunkPart.Filename) == InstallationFileSizes[FileChunkPart.Filename];
			}
			if (bCanMakeChunk)
			{
				ChunksAvailable.Add(ChunkRequired);
				++Count;
			}
		}
	}
	return Count;
}

TArray<FFileChunkPart> FBuildPatchAppManifest::GetFilePartsForChunk(const FGuid& ChunkId) const
{
	TArray<FFileChunkPart> FileParts;
	FBlockStructure FoundParts;
	for (const FFileManifestData& FileManifest: FileManifestList)
	{
		uint64 FileOffset = 0;
		for (const FChunkPartData& ChunkPart : FileManifest.FileChunkParts)
		{
			if (ChunkId == ChunkPart.Guid)
			{
				FFileChunkPart FileChunkPart;
				FileChunkPart.Filename = FileManifest.Filename;
				FileChunkPart.ChunkPart = ChunkPart;
				FileChunkPart.FileOffset = FileOffset;
				FileParts.Add(FileChunkPart);
				FoundParts.Add(ChunkPart.Offset, ChunkPart.Size, ESearchDir::FromEnd);
			}
			FileOffset += ChunkPart.Size;
		}
	}

	// If the structure is not a single complete block of correct size, then the chunk is not recoverable.
	if (FoundParts.GetHead() == nullptr || FoundParts.GetHead() != FoundParts.GetFoot() || FoundParts.GetHead()->GetSize() != BuildPatchServices::ChunkDataSize)
	{
		FileParts.Empty();
	}
	return FileParts;
}

bool FBuildPatchAppManifest::HasFileAttributes() const
{
	for (const TPair<FString, FFileManifestData*>& FileManifestPair : FileManifestLookup)
	{
		const FFileManifestData* FileManifest = FileManifestPair.Value;
		if (FileManifest->bIsReadOnly || FileManifest->bIsUnixExecutable || FileManifest->bIsCompressed)
		{
			return true;
		}
	}
	return false;
}

void FBuildPatchAppManifest::GetRemovableFiles(const IBuildManifestRef& InOldManifest, TArray< FString >& RemovableFiles) const
{
	// Cast manifest parameters
	const FBuildPatchAppManifestRef OldManifest = StaticCastSharedRef< FBuildPatchAppManifest >(InOldManifest);
	// Simply put, any files that exist in the OldManifest file list, but do not in this manifest's file list, are assumed
	// to be files no longer required by the build
	for (const FFileManifestData& OldFile : OldManifest->FileManifestList)
	{
		if (!FileManifestLookup.Contains(OldFile.Filename))
		{
			RemovableFiles.Add(OldFile.Filename);
		}
	}
}

void FBuildPatchAppManifest::GetRemovableFiles(const TCHAR* InstallPath, TArray< FString >& RemovableFiles) const
{
	TArray<FString> AllFiles;
	IFileManager::Get().FindFilesRecursive(AllFiles, InstallPath, TEXT("*"), true, false);
	
	FString BasePath = InstallPath;

#if PLATFORM_MAC
	// On Mac paths in manifest start with app bundle name
	if (BasePath.EndsWith(".app"))
	{
		BasePath = FPaths::GetPath(BasePath) + "/";
	}
#endif
	
	for (int32 FileIndex = 0; FileIndex < AllFiles.Num(); ++FileIndex)
	{
		const FString& Filename = AllFiles[FileIndex].RightChop(BasePath.Len());
		if (!FileManifestLookup.Contains(Filename))
		{
			RemovableFiles.Add(AllFiles[FileIndex]);
		}
	}
}

bool FBuildPatchAppManifest::NeedsResaving() const
{
	// The bool is marked during file load if we load an old version that should be upgraded
	return bNeedsResaving;
}

void FBuildPatchAppManifest::GetOutdatedFiles(const FBuildPatchAppManifestPtr& OldManifest, const FString& InstallDirectory, TSet< FString >& OutDatedFiles) const
{
	const bool bCheckExistingFile = InstallDirectory.IsEmpty() == false;
	if (!OldManifest.IsValid())
	{
		// All files are outdated if no OldManifest
		TArray<FString> Filenames;
		FileManifestLookup.GetKeys(Filenames);
		OutDatedFiles.Append(MoveTemp(Filenames));
	}
	else
	{
		// Enumerate files in the this file list, that do not exist, or have different hashes in the OldManifest
		// to be files no longer required by the build
		for (const FFileManifestData& NewFile : FileManifestList)
		{
			const int64 ExistingFileSize = IFileManager::Get().FileSize(*(InstallDirectory / NewFile.Filename));
			// Check changed
			if (IsFileOutdated(OldManifest.ToSharedRef(), NewFile.Filename))
			{
				OutDatedFiles.Add(NewFile.Filename);
			}
			// Double check an unchanged file is not missing (size will be -1) or is incorrect size
			else if (bCheckExistingFile && ExistingFileSize != NewFile.GetFileSize())
			{
				OutDatedFiles.Add(NewFile.Filename);
			}
		}
	}
}

bool FBuildPatchAppManifest::IsFileOutdated(const FBuildPatchAppManifestRef& OldManifest, const FString& Filename) const
{
	// If both app manifests are the same, return false as only repair would touch the file.
	if (&OldManifest.Get() == this)
	{
		return false;
	}
	// Get file manifests
	const FFileManifestData* OldFile = OldManifest->GetFileManifest(Filename);
	const FFileManifestData* NewFile = GetFileManifest(Filename);
	// Out of date if not in either manifest
	if (!OldFile || !NewFile)
	{
		return true;
	}
	// Different hash means different file
	if (OldFile->FileHash != NewFile->FileHash)
	{
		return true;
	}
	return false;
}

uint32 FBuildPatchAppManifest::GetNumberOfChunkReferences(const FGuid& ChunkGuid) const
{
	uint32 RefCount = 0;
	// For each file in the manifest, check if it has references to this chunk
	for (const FFileManifestData& FileManifest : FileManifestList)
	{
		for (const FChunkPartData& ChunkPart : FileManifest.FileChunkParts)
		{
			if (ChunkPart.Guid == ChunkGuid)
			{
				++RefCount;
			}
		}
	}
	return RefCount;
}

#undef LOCTEXT_NAMESPACE
