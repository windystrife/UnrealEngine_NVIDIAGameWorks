// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CookedIterativeNetworkFile.h"
#include "Templates/ScopedPointer.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "HAL/IPlatformFileModule.h"
#include "UniquePtr.h"
#include "Misc/EngineVersion.h"

DEFINE_LOG_CATEGORY(LogCookedIterativeNetworkFile);

bool FCookedIterativeNetworkFile::InitializeInternal(IPlatformFile* Inner, const TCHAR* HostIP)
{
	if ( Inner->GetLowerLevel() == nullptr )
	{
		UE_LOG(LogCookedIterativeNetworkFile, Fatal, TEXT("Platform file is missing it's inner.  Is pak file deployed?") );
	}
	

	PakPlatformFile = Inner;

	return FNetworkPlatformFile::InitializeInternal(Inner->GetLowerLevel(), HostIP );
}


void FCookedIterativeNetworkFile::ProcessServerCachedFilesResponse(FArrayReader& Response, const int32 ServerPackageVersion, const int32 ServerPackageLicenseeVersion)
{
	FNetworkPlatformFile::ProcessServerCachedFilesResponse(Response, ServerPackageVersion, ServerPackageLicenseeVersion);


	// some of our stuff is on here too!
	
	// receive a list of the cache files and their timestamps
	TMap<FString, FDateTime> ServerValidPakFileFiles;
	Response << ServerValidPakFileFiles;


	for ( const auto& ServerPakFileFile : ServerValidPakFileFiles )
	{
		// the server should contain all these files
		FString Path = FPaths::GetPath( ServerPakFileFile.Key);
		FString Filename = FPaths::GetCleanFilename(ServerPakFileFile.Key);

		FServerTOC::FDirectory* ServerDirectory = ServerFiles.FindDirectory(Path);
		if ( !ServerDirectory)
		{
			UE_LOG(LogCookedIterativeNetworkFile, Warning, TEXT("Unable to find directory %s while trying to resolve pak file %s"), *Path, *ServerPakFileFile.Key );
		}
		else
		{
			FDateTime* ServerDate = ServerDirectory->Find(Filename);
			if ( !ServerDate )
			{
				UE_LOG(LogCookedIterativeNetworkFile, Warning, TEXT("Unable to find filename %s while trying to resolve pak file %s"), *Filename, *ServerPakFileFile.Key);
			}
		}

		// check the file is accessable 
		if (!PakPlatformFile->FileExists(*ServerPakFileFile.Key))
		{
			UE_LOG(LogCookedIterativeNetworkFile, Warning, TEXT("Unable to find file %s in pak file.  Server says it should be!"), *ServerPakFileFile.Key)
		}

		ValidPakFileFiles.AddFileOrDirectory(ServerPakFileFile.Key, ServerPakFileFile.Value);

		UE_LOG(LogCookedIterativeNetworkFile, Display, TEXT("Using pak file %s"), *ServerPakFileFile.Key);
	}
}

FString FCookedIterativeNetworkFile::GetVersionInfo() const
{
	FString VersionInfo = FString::Printf(TEXT("%s %d"), *FEngineVersion::CompatibleWith().GetBranch(), FEngineVersion::CompatibleWith().GetChangelist() );

	return VersionInfo;
}

bool FCookedIterativeNetworkFile::ShouldPassToPak(const TCHAR* Filename) const
{
	if ( FCString::Stricmp( *FPaths::GetExtension(Filename), TEXT("ufont") ) == 0 )
	{
		FString Path = FPaths::GetPath( Filename );

		const FServerTOC::FDirectory* Directory = ValidPakFileFiles.FindDirectory(Path);
		if ( Directory )
		{
			FString StringFilename = Filename;
			for ( const auto& File : *Directory )
			{
				if ( File.Key.StartsWith( StringFilename ) )
				{
					return true;
				}
			}
			return false;
		}
		else
		{
			return false;
		}
	}

	if ( ValidPakFileFiles.FindFile(Filename) != nullptr )
	{
		return true;
	}
	// if we are searching for the .uexp or .ubulk or any of those content extra files.... 
	// then change it to the original,  if we are using the pak version of the original file then we want the bulk / uexp file to be the same
	// potential async issue here if the file is invalidated after we start loading the original but before we load the uexp 
	// not much we can do about that though... 
	FString OriginalName = FPaths::ChangeExtension(Filename, TEXT("uasset"));
	if ( ValidPakFileFiles.FindFile(OriginalName) )
	{
		return true;
	}
	OriginalName = FPaths::ChangeExtension(Filename, TEXT("umap"));
	if ( ValidPakFileFiles.FindFile(OriginalName))
	{
		return true;
	}

	return false;
}


bool		FCookedIterativeNetworkFile::FileExists(const TCHAR* Filename)
{
	if ( ShouldPassToPak(Filename) )
	{
		PakPlatformFile->FileExists(Filename);
		return true;
	}
	return FNetworkPlatformFile::FileExists(Filename);
}

int64 FCookedIterativeNetworkFile::FileSize(const TCHAR* Filename)
{
	if ( ShouldPassToPak(Filename) )
	{
		return PakPlatformFile->FileSize(Filename);
	}
	else
	{
		return FNetworkPlatformFile::FileSize(Filename);
	}
}
bool FCookedIterativeNetworkFile::DeleteFile(const TCHAR* Filename)
{
	// delete both of these entries
	ValidPakFileFiles.RemoveFileOrDirectory(Filename);
	return FNetworkPlatformFile::DeleteFile(Filename);
}

bool FCookedIterativeNetworkFile::IsReadOnly(const TCHAR* Filename)
{
	if ( ShouldPassToPak(Filename) )
	{
		return PakPlatformFile->IsReadOnly(Filename);
	}

	return FNetworkPlatformFile::IsReadOnly(Filename);
}

bool FCookedIterativeNetworkFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	if (ShouldPassToPak(Filename))
	{
		return PakPlatformFile->SetReadOnly(Filename, bNewReadOnlyValue);
	}
	return FNetworkPlatformFile::SetReadOnly(Filename, bNewReadOnlyValue);
}
FDateTime FCookedIterativeNetworkFile::GetTimeStamp(const TCHAR* Filename)
{
	if ( ShouldPassToPak(Filename) )
	{
		return PakPlatformFile->GetTimeStamp(Filename);
	}
	return FNetworkPlatformFile::GetTimeStamp(Filename);
}


void FCookedIterativeNetworkFile::OnFileUpdated(const FString& LocalFilename)
{
	FNetworkPlatformFile::OnFileUpdated(LocalFilename);
	ValidPakFileFiles.RemoveFileOrDirectory(LocalFilename);
}


IFileHandle* FCookedIterativeNetworkFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	if ( ShouldPassToPak(Filename) )
	{
		return PakPlatformFile->OpenRead(Filename, bAllowWrite);
	}
	return FNetworkPlatformFile::OpenRead(Filename, bAllowWrite);
}
IFileHandle* FCookedIterativeNetworkFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	if (ShouldPassToPak(Filename))
	{
		return PakPlatformFile->OpenWrite(Filename, bAppend, bAllowRead);
	}
	// FNetworkPlatformFile::CreateDirectoryTree(Directory);
	return FNetworkPlatformFile::OpenWrite(Filename, bAppend, bAllowRead);
}

bool FCookedIterativeNetworkFile::CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags, EPlatformFileWrite WriteFlags)
{
	return PakPlatformFile->CopyFile(To, From, ReadFlags, WriteFlags);
}

bool FCookedIterativeNetworkFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
	if (ShouldPassToPak(From))
	{
		return PakPlatformFile->MoveFile(To, From);
	}
	return FNetworkPlatformFile::MoveFile(To, From);
}

bool FCookedIterativeNetworkFile::DirectoryExists(const TCHAR* Directory)
{
	if ( ValidPakFileFiles.FindDirectory(Directory) )
	{
		return true;
	}
	return FNetworkPlatformFile::DirectoryExists(Directory);
}

bool FCookedIterativeNetworkFile::CreateDirectoryTree(const TCHAR* Directory)
{
	return FNetworkPlatformFile::CreateDirectoryTree(Directory);
}

bool FCookedIterativeNetworkFile::CreateDirectory(const TCHAR* Directory)
{
	return FNetworkPlatformFile::CreateDirectory(Directory);
}

bool FCookedIterativeNetworkFile::DeleteDirectory(const TCHAR* Directory)
{
	bool bSucceeded = false;
	if( ValidPakFileFiles.FindDirectory(Directory) )
	{
		bSucceeded |= ValidPakFileFiles.RemoveFileOrDirectory(Directory) > 0 ? true : false;
	}
	bSucceeded |= FNetworkPlatformFile::DeleteDirectory(Directory);
	return bSucceeded;
}

bool FCookedIterativeNetworkFile::DeleteDirectoryRecursively(const TCHAR* Directory)
{
	bool bSucceeded = false;
	if (ValidPakFileFiles.FindDirectory(Directory))
	{
		bSucceeded |= ValidPakFileFiles.RemoveFileOrDirectory(Directory) > 0 ? true : false;
	}
	bSucceeded |= FNetworkPlatformFile::DeleteDirectoryRecursively(Directory);
	return bSucceeded;
}

bool		FCookedIterativeNetworkFile::IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor)
{
	//return PakPlatformFile->IterateDirectory(Directory, Visitor);
	return FNetworkPlatformFile::IterateDirectory(Directory, Visitor);
}

bool FCookedIterativeNetworkFile::IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor)
{
	return FNetworkPlatformFile::IterateDirectoryRecursively(Directory, Visitor);
}

FFileStatData FCookedIterativeNetworkFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	if (ShouldPassToPak(FilenameOrDirectory))
	{
		return PakPlatformFile->GetStatData(FilenameOrDirectory);
	}
	return FNetworkPlatformFile::GetStatData(FilenameOrDirectory);
}

bool FCookedIterativeNetworkFile::IterateDirectoryStat(const TCHAR* Directory, IPlatformFile::FDirectoryStatVisitor& Visitor)
{
	return FNetworkPlatformFile::IterateDirectoryStat(Directory, Visitor);
}

bool FCookedIterativeNetworkFile::IterateDirectoryStatRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryStatVisitor& Visitor)
{
	return FNetworkPlatformFile::IterateDirectoryStatRecursively(Directory, Visitor);
}

bool FCookedIterativeNetworkFile::ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
{
	bool bResult = FNetworkPlatformFile::ShouldBeUsed(Inner, CmdLine);

	if (bResult)
	{
		bResult = FParse::Param(CmdLine, TEXT("precookednetwork"));
	}

	return bResult;
}

FCookedIterativeNetworkFile::~FCookedIterativeNetworkFile()
{
}

/**
 * Module for the streaming file
 */
class FCookedIterativeFileModule
	: public IPlatformFileModule
{
public:

	virtual IPlatformFile* GetPlatformFile() override
	{
		static TUniquePtr<IPlatformFile> AutoDestroySingleton = MakeUnique<FCookedIterativeNetworkFile>();
		return AutoDestroySingleton.Get();
	}
};


IMPLEMENT_MODULE(FCookedIterativeFileModule, CookedIterativeFile);
