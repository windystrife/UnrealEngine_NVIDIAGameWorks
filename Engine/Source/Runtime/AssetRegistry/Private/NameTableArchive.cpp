// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NameTableArchive.h"
#include "HAL/FileManager.h"
#include "AssetRegistryPrivate.h"

FNameTableArchiveReader::FNameTableArchiveReader(int32 SerializationVersion, const FString& Filename)
	: FArchive()
	, ProxyAr(nullptr)
	, FileAr(nullptr)
{
	ArIsLoading = true;

	FileAr = IFileManager::Get().CreateFileReader(*Filename, FILEREAD_Silent);
	if (FileAr && !FileAr->IsError() && FileAr->TotalSize() > 0)
	{
		ProxyAr = FileAr;

		int32 MagicNumber = 0;
		*this << MagicNumber;

		if (!IsError() && MagicNumber == PACKAGE_FILE_TAG)
		{
			int32 VersionNumber = 0;
			*this << VersionNumber;

			if (!IsError() && VersionNumber == SerializationVersion)
			{
				if (SerializeNameMap())
				{
					// Succesfully loaded
					return;
				}
			}
		}
	}
	
	// If we got here it failed to load properly
	SetError();
}

FNameTableArchiveReader::FNameTableArchiveReader(FArchive& WrappedArchive)
	: FArchive()
	, ProxyAr(&WrappedArchive)
	, FileAr(nullptr)
{
	ArIsLoading = true;

	if (!SerializeNameMap())
	{
		SetError();
	}
}

FNameTableArchiveReader::~FNameTableArchiveReader()
{
	if (FileAr)
	{
		delete FileAr;
		FileAr = nullptr;
	}
}

bool FNameTableArchiveReader::SerializeNameMap()
{
	int64 NameOffset = 0;
	*this << NameOffset;

	if (IsError() || NameOffset > TotalSize())
	{
		// The file was corrupted. Return false to fail to load the cache an thus regenerate it.
		return false;
	}

	if( NameOffset > 0 )
	{
		int64 OriginalOffset = Tell();
		Seek( NameOffset );

		int32 NameCount = 0;
		*this << NameCount;

		if (IsError())
		{
			return false;
		}

		for ( int32 NameMapIdx = 0; NameMapIdx < NameCount; ++NameMapIdx )
		{
			// Read the name entry from the file.
			FNameEntrySerialized NameEntry(ENAME_LinkerConstructor);
			*this << NameEntry;

			if (IsError())
			{
				return false;
			}

			NameMap.Add(FName(NameEntry));
		}

		Seek( OriginalOffset );
	}

	return true;
}

void FNameTableArchiveReader::Serialize(void* V, int64 Length)
{
	if (ProxyAr && !IsError())
	{
		ProxyAr->Serialize(V, Length);

		if (ProxyAr->IsError())
		{
			SetError();
		}
	}
}

bool FNameTableArchiveReader::Precache(int64 PrecacheOffset, int64 PrecacheSize)
{
	if (ProxyAr && !IsError())
	{
		return ProxyAr->Precache(PrecacheOffset, PrecacheSize);
	}

	return false;
}

void FNameTableArchiveReader::Seek(int64 InPos)
{
	if (ProxyAr && !IsError())
	{
		ProxyAr->Seek(InPos);
	}
}

int64 FNameTableArchiveReader::Tell()
{
	if (ProxyAr)
	{
		return ProxyAr->Tell();
	}

	return 0;
}

int64 FNameTableArchiveReader::TotalSize()
{
	if (ProxyAr)
	{
		return ProxyAr->TotalSize();
	}

	return 0;
}

const FCustomVersionContainer& FNameTableArchiveReader::GetCustomVersions() const
{
	if (ProxyAr)
	{
		return ProxyAr->GetCustomVersions();
	}
	return FArchive::GetCustomVersions();
}

void FNameTableArchiveReader::SetCustomVersions(const FCustomVersionContainer& NewVersions)
{
	if (ProxyAr)
	{
		ProxyAr->SetCustomVersions(NewVersions);
	}
}

void FNameTableArchiveReader::ResetCustomVersions()
{
	if (ProxyAr)
	{
		ProxyAr->ResetCustomVersions();
	}
}

FArchive& FNameTableArchiveReader::operator<<( FName& Name )
{
	int32 NameIndex;
	FArchive& Ar = *this;
	Ar << NameIndex;

	if( !NameMap.IsValidIndex(NameIndex) )
	{
		UE_LOG(LogAssetRegistry, Error, TEXT("Bad name index reading cache %i/%i"), NameIndex, NameMap.Num() );
		SetError();
	}

	const FName& MappedName = NameMap.IsValidIndex(NameIndex) ? NameMap[NameIndex] : NAME_None;
	if (MappedName.IsNone())
	{
		int32 TempNumber;
		Ar << TempNumber;
		Name = NAME_None;
	}
	else
	{
		int32 Number;
		Ar << Number;
		// simply create the name from the NameMap's name and the serialized instance number
		Name = FName(MappedName, Number);
	}

	return *this;
}

FNameTableArchiveWriter::FNameTableArchiveWriter(int32 SerializationVersion, const FString& Filename)
	: FArchive()
	, ProxyAr(nullptr)
	, FileAr(nullptr)
	, FinalFilename(Filename)
	, TempFilename(Filename + TEXT(".tmp"))
{
	ArIsSaving = true;

	// Save to a temp file first, then move to the destination to avoid corruption
	FileAr = IFileManager::Get().CreateFileWriter(*TempFilename, 0);
	if (FileAr)
	{
		ProxyAr = FileAr;

		int32 MagicNumber = PACKAGE_FILE_TAG;
		*this << MagicNumber;

		int32 VersionToWrite = SerializationVersion;
		*this << VersionToWrite;

		// Just write a 0 for the name table offset for now. This will be overwritten later when we are done serializing
		NameOffsetLoc = Tell();
		int64 NameOffset = 0;
		*this << NameOffset;
	}
	else
	{
		UE_LOG(LogAssetRegistry, Error, TEXT("Failed to open file for write %s"), *Filename);
		SetError();
	}
}

FNameTableArchiveWriter::FNameTableArchiveWriter(FArchive& WrappedArchive)
	: FArchive()
	, ProxyAr(&WrappedArchive)
	, FileAr(nullptr)
{
	ArIsSaving = true;

	// Just write a 0 for the name table offset for now. This will be overwritten later when we are done serializing
	NameOffsetLoc = Tell();
	int64 NameOffset = 0;
	*this << NameOffset;
}

FNameTableArchiveWriter::~FNameTableArchiveWriter()
{
	if (ProxyAr)
	{
		int64 ActualNameOffset = Tell();
		SerializeNameMap();

		int64 EndOffset = Tell();
		Seek(NameOffsetLoc);
		*this << ActualNameOffset;
		Seek(EndOffset);
	}

	if (FileAr)
	{
		delete FileAr;
		FileAr = nullptr;

		IFileManager::Get().Move(*FinalFilename, *TempFilename);
	}
}

void FNameTableArchiveWriter::SerializeNameMap()
{
	int32 NameCount = NameMap.Num();
	*this << NameCount;
	if( NameCount > 0 )
	{
		// Must still be sorted in add order
		int32 NameMapIdx = 0;
		for (auto& Pair : NameMap)
		{
			check(NameMapIdx == Pair.Value);
			*this << *const_cast<FNameEntry*>(Pair.Key.GetDisplayNameEntry());
			NameMapIdx++;
		}
	}
}

void FNameTableArchiveWriter::Serialize( void* V, int64 Length )
{
	if (ProxyAr)
	{
		ProxyAr->Serialize( V, Length );

		if (ProxyAr->IsError())
		{
			SetError();
		}
	}

}

bool FNameTableArchiveWriter::Precache( int64 PrecacheOffset, int64 PrecacheSize )
{
	if (ProxyAr)
	{
		return ProxyAr->Precache( PrecacheOffset, PrecacheSize );
	}
	
	return false;
}

void FNameTableArchiveWriter::Seek( int64 InPos )
{
	if (ProxyAr)
	{
		ProxyAr->Seek( InPos );
	}
}

int64 FNameTableArchiveWriter::Tell()
{
	if (ProxyAr)
	{
		return ProxyAr->Tell();
	}

	return 0.f;
}

int64 FNameTableArchiveWriter::TotalSize()
{
	if (ProxyAr)
	{
		return ProxyAr->TotalSize();
	}

	return 0.f;
}

const FCustomVersionContainer& FNameTableArchiveWriter::GetCustomVersions() const
{
	if (ProxyAr)
	{
		return ProxyAr->GetCustomVersions();
	}
	return FArchive::GetCustomVersions();
}

void FNameTableArchiveWriter::SetCustomVersions(const FCustomVersionContainer& NewVersions)
{
	if (ProxyAr)
	{
		ProxyAr->SetCustomVersions(NewVersions);
	}
}

void FNameTableArchiveWriter::ResetCustomVersions()
{
	if (ProxyAr)
	{
		ProxyAr->ResetCustomVersions();
	}
}

FArchive& FNameTableArchiveWriter::operator<<( FName& Name )
{
	int32* NameIndexPtr = NameMap.Find(Name);
	int32 NameIndex = NameIndexPtr ? *NameIndexPtr : INDEX_NONE;
	if ( NameIndex == INDEX_NONE )
	{
		// We need to store the FName without the number, as the number is stored separately and we don't 
		// want duplicate entries in the name table just because of the number
		const FName NameNoNumber(Name, 0);
		NameIndex = NameMap.Num();
		NameMap.Add(NameNoNumber, NameIndex);
	}

	FArchive& Ar = *this;
	Ar << NameIndex;

	if (Name == NAME_None)
	{
		int32 TempNumber = 0;
		Ar << TempNumber;
	}
	else
	{
		int32 Number = Name.GetNumber();
		Ar << Number;
	}

	return *this;
}
