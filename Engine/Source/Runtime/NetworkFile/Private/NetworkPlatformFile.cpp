// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NetworkPlatformFile.h"
#include "Templates/ScopedPointer.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ScopedEvent.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "DerivedDataCacheInterface.h"
#include "Misc/PackageName.h"

#include "HTTPTransport.h"
#include "TCPTransport.h"

#include "HAL/IPlatformFileModule.h"
#include "UniquePtr.h"

#include "Object.h"

DEFINE_LOG_CATEGORY(LogNetworkPlatformFile);

FString FNetworkPlatformFile::MP4Extension = TEXT(".mp4");
FString FNetworkPlatformFile::BulkFileExtension = TEXT(".ubulk");
FString FNetworkPlatformFile::ExpFileExtension = TEXT(".uexp");
FString FNetworkPlatformFile::FontFileExtension = TEXT(".ufont");

FNetworkPlatformFile::FNetworkPlatformFile()
	: bHasLoadedDDCDirectories(false)
	, InnerPlatformFile(NULL)
	, bIsUsable(false)
	, FileServerPort(0)
	, ConnectionFlags(EConnectionFlags::None)
	, HeartbeatFrequency(5.0f)
	, FinishedAsyncNetworkReadUnsolicitedFiles(NULL)
	, FinishedAsyncWriteUnsolicitedFiles(NULL)
	, Transport(NULL)
{


	
	TotalWriteTime = 0.0; // total non async time spent writing to disk
	TotalNetworkSyncTime = 0.0; // total non async time spent syncing to network
	TotalTimeSpentInUnsolicitedPackages = 0.0; // total time async processing unsolicited packages
	TotalWaitForAsyncUnsolicitedPackages = 0.0; // total time spent waiting for unsolicited packages
	TotalFilesSynced = 0; // total number files synced from network
	TotalFilesFoundLocally = 0;
	TotalUnsolicitedPackages = 0; // total number unsolicited files synced  
	UnsolicitedPackagesHits = 0; // total number of hits from waiting on unsolicited packages
	UnsolicitedPackageWaits = 0; // total number of waits on unsolicited packages
	

}

bool FNetworkPlatformFile::ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
{
	FString HostIp;
	return FParse::Value(CmdLine, TEXT("-FileHostIP="), HostIp);
}

ITransport *CreateTransportForHostAddress(const FString &HostIp )
{
	if ( HostIp.StartsWith(TEXT("tcp://")))
	{
		return new FTCPTransport();
	}

	if ( HostIp.StartsWith(TEXT("http://")))
	{
#if ENABLE_HTTP_FOR_NF
		return new FHTTPTransport();
#endif
	}

	// no transport specified assuming tcp
	return new FTCPTransport();
}

bool FNetworkPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	bool bResult = false;
	FString HostIpString;	
	if (FParse::Value(CmdLine, TEXT("-FileHostIP="), HostIpString))
	{
		TArray<FString> HostIpList;
		if (HostIpString.ParseIntoArray(HostIpList, TEXT("+"), true) > 0)
		{
			for (int32 HostIpIndex = 0; !bResult && HostIpIndex < HostIpList.Num(); ++HostIpIndex)
			{
				// Try to initialize with each of the IP addresses found in the command line until we 
				// get a working one.

				// find the correct transport for this ip address 
				Transport = CreateTransportForHostAddress( HostIpList[HostIpIndex] );

				UE_LOG(LogNetworkPlatformFile, Warning, TEXT("Created transport for %s."), *HostIpList[HostIpIndex]);

				if ( Transport )
				{
					bResult = Transport->Initialize( *HostIpList[HostIpIndex] ) && InitializeInternal(Inner, *HostIpList[HostIpIndex]);		
					if (bResult)
						break;

					UE_LOG(LogNetworkPlatformFile, Warning, TEXT("Failed to initialize %s."), *HostIpList[HostIpIndex]);

					// try a different host might be a different protocol
					delete Transport;
				}
				Transport = NULL;
			}
		}
	}

	return bResult;
}

bool FNetworkPlatformFile::InitializeInternal(IPlatformFile* Inner, const TCHAR* HostIP)
{
	// This platform file requires an inner.
	check(Inner != NULL);
	InnerPlatformFile = Inner;
	if (HostIP == NULL)
	{
		UE_LOG(LogNetworkPlatformFile, Error, TEXT("No Host IP specified in the commandline."));
		bIsUsable = false;
		return false;
	}

	// Save and Intermediate directories are always local
	LocalDirectories.Add(FPaths::EngineDir() / TEXT("Binaries"));
	LocalDirectories.Add(FPaths::EngineIntermediateDir());
	LocalDirectories.Add(FPaths::ProjectDir() / TEXT("Binaries"));
	LocalDirectories.Add(FPaths::ProjectIntermediateDir());
	LocalDirectories.Add(FPaths::ProjectSavedDir() / TEXT("Backup"));
	LocalDirectories.Add(FPaths::ProjectSavedDir() / TEXT("Config"));
	LocalDirectories.Add(FPaths::ProjectSavedDir() / TEXT("Logs"));
	LocalDirectories.Add(FPaths::ProjectSavedDir() / TEXT("Sandboxes"));

	if (InnerPlatformFile->GetLowerLevel())
	{
		InnerPlatformFile->GetLowerLevel()->AddLocalDirectories(LocalDirectories);
	}
	else
	{
		InnerPlatformFile->AddLocalDirectories(LocalDirectories);
	}
	

	FNetworkFileArchive Payload(NFS_Messages::Heartbeat); 
	FArrayReader Out;
	if (!SendPayloadAndReceiveResponse(Payload,Out))
		bIsUsable = true; 


	// lets see we can test whether the server is up. 
	if (Out.Num())
	{
		FCommandLine::AddToSubprocessCommandline( *FString::Printf( TEXT("-FileHostIP=%s"), HostIP ) );
		bIsUsable = true; 
	}
	return bIsUsable;
}

bool FNetworkPlatformFile::SendPayloadAndReceiveResponse(TArray<uint8>& In, TArray<uint8>& Out)
{
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		if ( FinishedAsyncNetworkReadUnsolicitedFiles )
		{
			delete FinishedAsyncNetworkReadUnsolicitedFiles;
			FinishedAsyncNetworkReadUnsolicitedFiles = NULL;
		}
	}
	
	return Transport->SendPayloadAndReceiveResponse( In, Out );
}

bool FNetworkPlatformFile::ReceiveResponse(TArray<uint8> &Out )
{
	return Transport->ReceiveResponse( Out );
}


void FNetworkPlatformFile::InitializeAfterSetActive()
{
	double NetworkFileStartupTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(NetworkFileStartupTime);

		// send the filenames and timestamps to the server
		FNetworkFileArchive Payload(NFS_Messages::GetFileList);
		FillGetFileList(Payload);

		// send the directories over, and wait for a response
		FArrayReader Response;
		if (!SendPayloadAndReceiveResponse(Payload, Response))
		{
			delete Transport; 
			return; 
		}
		else
		{
			// receive the cooked version information
			int32 ServerPackageVersion = 0;
			int32 ServerPackageLicenseeVersion = 0;
			ProcessServerInitialResponse(Response, ServerPackageVersion, ServerPackageLicenseeVersion);
			ProcessServerCachedFilesResponse(Response, ServerPackageVersion, ServerPackageLicenseeVersion);

			// make sure we can sync a file
			FString TestSyncFile = FPaths::Combine(*(FPaths::EngineDir()), TEXT("Config/BaseEngine.ini"));

			InnerPlatformFile->SetReadOnly(*TestSyncFile, false);
			InnerPlatformFile->DeleteFile(*TestSyncFile);
			if (InnerPlatformFile->FileExists(*TestSyncFile))
			{
				UE_LOG(LogNetworkPlatformFile, Fatal, TEXT("Could not delete file sync test file %s."), *TestSyncFile);
			}

			EnsureFileIsLocal(TestSyncFile);

			if (!InnerPlatformFile->FileExists(*TestSyncFile) || InnerPlatformFile->FileSize(*TestSyncFile) < 1)
			{
				UE_LOG(LogNetworkPlatformFile, Fatal, TEXT("Could not sync test file %s."), *TestSyncFile);
			}
		}
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Network file startup time: %5.3f seconds\n"), NetworkFileStartupTime);

}

void FNetworkPlatformFile::ProcessServerCachedFilesResponse(FArrayReader& Response, const int32 ServerPackageVersion, const int32 ServerPackageLicenseeVersion)
{
	/* The server root content directories */
	TArray<FString> ServerRootContentDirectories;
	Response << ServerRootContentDirectories;

	// receive a list of the cache files and their timestamps
	TMap<FString, FDateTime> ServerCachedFiles;
	Response << ServerCachedFiles;

	bool bDeleteAllFiles = true;
	// Check the stored cooked version
	FString CookedVersionFile = FPaths::GeneratedConfigDir() / TEXT("CookedVersion.txt");

	if (InnerPlatformFile->FileExists(*CookedVersionFile) == true)
	{
		IFileHandle* FileHandle = InnerPlatformFile->OpenRead(*CookedVersionFile);
		if (FileHandle != NULL)
		{
			int32 StoredPackageCookedVersion;
			int32 StoredPackageCookedLicenseeVersion;
			if (FileHandle->Read((uint8*)&StoredPackageCookedVersion, sizeof(int32)) == true)
			{
				if (FileHandle->Read((uint8*)&StoredPackageCookedLicenseeVersion, sizeof(int32)) == true)
				{
					if ((ServerPackageVersion == StoredPackageCookedVersion) &&
						(ServerPackageLicenseeVersion == StoredPackageCookedLicenseeVersion))
					{
						bDeleteAllFiles = false;
					}
					else
					{
						UE_LOG(LogNetworkPlatformFile, Display,
							TEXT("Engine version mismatch: Server %d.%d, Stored %d.%d\n"),
							ServerPackageVersion, ServerPackageLicenseeVersion,
							StoredPackageCookedVersion, StoredPackageCookedLicenseeVersion);
					}
				}
			}

			delete FileHandle;
		}
	}
	else
	{
		UE_LOG(LogNetworkPlatformFile, Display, TEXT("Cooked version file missing: %s\n"), *CookedVersionFile);
	}

	if (bDeleteAllFiles == true)
	{
		// Make sure the config file exists...
		InnerPlatformFile->CreateDirectoryTree(*(FPaths::GeneratedConfigDir()));
		// Update the cooked version file
		IFileHandle* FileHandle = InnerPlatformFile->OpenWrite(*CookedVersionFile);
		if (FileHandle != NULL)
		{
			FileHandle->Write((const uint8*)&ServerPackageVersion, sizeof(int32));
			FileHandle->Write((const uint8*)&ServerPackageLicenseeVersion, sizeof(int32));
			delete FileHandle;
		}
	}

	// list of directories to skip
	TArray<FString> DirectoriesToSkip;
	TArray<FString> DirectoriesToNotRecurse;
	// use the timestamp grabbing visitor to get all the content times
	FLocalTimestampDirectoryVisitor Visitor(*InnerPlatformFile, DirectoriesToSkip, DirectoriesToNotRecurse, false);

	/*TArray<FString> RootContentPaths;
	FPackageName::QueryRootContentPaths(RootContentPaths); */
	for (TArray<FString>::TConstIterator RootPathIt(ServerRootContentDirectories); RootPathIt; ++RootPathIt)
	{
		/*const FString& RootPath = *RootPathIt;
		const FString& ContentFolder = FPackageName::LongPackageNameToFilename(RootPath);*/
		const FString& ContentFolder = *RootPathIt;
		InnerPlatformFile->IterateDirectory(*ContentFolder, Visitor);
	}

	// delete out of date files using the server cached files
	for (TMap<FString, FDateTime>::TIterator It(ServerCachedFiles); It; ++It)
	{
		bool bDeleteFile = bDeleteAllFiles;
		FString ServerFile = It.Key();

		// Convert the filename to the client version
		ConvertServerFilenameToClientFilename(ServerFile);

		// Set it in the visitor file times list
		// If there is any pathing difference (relative path, or whatever) between the server's filelist and the results
		// of platform directory iteration then this will Add a new entry rather than override the existing one.  This causes local file deletes
		// and longer loads as we will never see the benefits of local device caching.
		Visitor.FileTimes.Add(ServerFile, FDateTime::MinValue());

		if (bDeleteFile == false)
		{
			// Check the time stamps...
			// get local time
			FDateTime LocalTime = InnerPlatformFile->GetTimeStamp(*ServerFile);
			// If local time == MinValue than the file does not exist in the cache.
			if (LocalTime != FDateTime::MinValue())
			{
				FDateTime ServerTime = It.Value();
				// delete if out of date
				// We will use 1.0 second as the tolerance to cover any platform differences in resolution
				FTimespan TimeDiff = LocalTime - ServerTime;
				double TimeDiffInSeconds = TimeDiff.GetTotalSeconds();
				bDeleteFile = (TimeDiffInSeconds > 1.0) || (TimeDiffInSeconds < -1.0);
				if (bDeleteFile == true)
				{
					if (InnerPlatformFile->FileExists(*ServerFile) == true)
					{
						UE_LOG(LogNetworkPlatformFile, Display, TEXT("Deleting cached file: TimeDiff %5.3f, %s"), TimeDiffInSeconds, *It.Key());
					}
					else
					{
						// It's a directory
						bDeleteFile = false;
					}
				}
				else
				{
					UE_LOG(LogNetworkPlatformFile, Display, TEXT("Keeping cached file: %s, TimeDiff worked out ok"), *ServerFile);
				}
			}
		}
		if (bDeleteFile == true)
		{
			UE_LOG(LogNetworkPlatformFile, Display, TEXT("Deleting cached file: %s"), *ServerFile);
			InnerPlatformFile->DeleteFile(*ServerFile);
		}
	}

	// Any content files we have locally that were not cached, delete them
	for (TMap<FString, FDateTime>::TIterator It(Visitor.FileTimes); It; ++It)
	{
		if ( FCString::Stricmp( *FPaths::GetExtension( It.Key() ), TEXT("pak")) == 0 )
		{
			// ignore pak files they won't be mounted anyway 
			continue;
		}
		if (It.Value() != FDateTime::MinValue())
		{
			// This was *not* found in the server file list... delete it
			UE_LOG(LogNetworkPlatformFile, Display, TEXT("Deleting cached file: %s"), *It.Key());
			InnerPlatformFile->DeleteFile(*It.Key());
		}
	}
}

FNetworkPlatformFile::~FNetworkPlatformFile()
{
	if (!GIsRequestingExit) // the socket subsystem is probably already gone, so it will crash if we clean up
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		if ( FinishedAsyncNetworkReadUnsolicitedFiles )
		{
			delete FinishedAsyncNetworkReadUnsolicitedFiles; // wait here for any async unsolicited files to finish reading being read from the network 
			FinishedAsyncNetworkReadUnsolicitedFiles = NULL;
		}
		if ( FinishedAsyncWriteUnsolicitedFiles )
		{
			delete FinishedAsyncWriteUnsolicitedFiles; // wait here for any async unsolicited files to finish writing 
			FinishedAsyncWriteUnsolicitedFiles = NULL;
		}
		
		delete Transport; // close our sockets.
	}
}

bool FNetworkPlatformFile::DeleteFile(const TCHAR* Filename)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// make and send payload (this is how we would do for sending all commands over the network)
//	FNetworkFileArchive Payload(NFS_Messages::DeleteFile);
//	Payload << Filename;
//	return FNFSMessageHeader::WrapAndSendPayload(Payload, FileSocket);

	// perform a local operation
	return InnerPlatformFile->DeleteFile(Filename);
}

bool FNetworkPlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// make and send payload (this is how we would do for sending all commands over the network)
//	FNetworkFileArchive Payload(NFS_Messages::MoveFile);
//	Payload << To << From;
//	return FNFSMessageHeader::WrapAndSendPayload(Payload, FileSocket);

	FString RelativeFrom = From;
	MakeStandardNetworkFilename(RelativeFrom);

	// don't copy files in local directories
	if (!IsInLocalDirectory(RelativeFrom))
	{
		// make sure the source file exists here
		EnsureFileIsLocal(RelativeFrom);
	}

	// perform a local operation
	return InnerPlatformFile->MoveFile(To, From);
}

bool FNetworkPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// perform a local operation
	return InnerPlatformFile->SetReadOnly(Filename, bNewReadOnlyValue);
}

void FNetworkPlatformFile::SetTimeStamp(const TCHAR* Filename, FDateTime DateTime)
{
	// perform a local operation
	InnerPlatformFile->SetTimeStamp(Filename, DateTime);
}

IFileHandle* FNetworkPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	FString RelativeFilename = Filename;
	MakeStandardNetworkFilename(RelativeFilename);
	// don't copy files in local directories
	if (!IsInLocalDirectory(RelativeFilename))
	{
		EnsureFileIsLocal(RelativeFilename);
	}

	double StartTime;
	float ThisTime;

	StartTime = FPlatformTime::Seconds();
	IFileHandle* Result = InnerPlatformFile->OpenRead(Filename, bAllowWrite);

	ThisTime = 1000.0f * float(FPlatformTime::Seconds() - StartTime);
	//UE_LOG(LogNetworkPlatformFile, Display, TEXT("Open local file %6.2fms"), ThisTime);
	return Result;
}

IFileHandle* FNetworkPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// just let the physical file interface write the file (we don't write over the network)
	return InnerPlatformFile->OpenWrite(Filename, bAppend, bAllowRead);
}

bool FNetworkPlatformFile::CreateDirectoryTree(const TCHAR* Directory)
{
	//	FScopeLock ScopeLock(&SynchronizationObject);

	// perform a local operation
	return InnerPlatformFile->CreateDirectoryTree(Directory);
}

bool FNetworkPlatformFile::CreateDirectory(const TCHAR* Directory)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// perform a local operation
	return InnerPlatformFile->CreateDirectory(Directory);
}

bool FNetworkPlatformFile::DeleteDirectory(const TCHAR* Directory)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// perform a local operation
	return InnerPlatformFile->DeleteDirectory(Directory);
}

FFileStatData FNetworkPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// perform a local operation
	return InnerPlatformFile->GetStatData(FilenameOrDirectory);
}

bool FNetworkPlatformFile::IterateDirectory(const TCHAR* InDirectory, IPlatformFile::FDirectoryVisitor& Visitor)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// for .dll, etc searches that don't specify a path, we need to strip off the path
	// before we send it to the visitor
	bool bHadNoPath = InDirectory[0] == 0;

	// local files go right to the source
	FString RelativeDirectory = InDirectory;
	MakeStandardNetworkFilename(RelativeDirectory);
	if (IsInLocalDirectory(RelativeDirectory))
	{
		return InnerPlatformFile->IterateDirectory(InDirectory, Visitor);
	}

	// we loop until this is false
	bool RetVal = true;

	FServerTOC::FDirectory* ServerDirectory = ServerFiles.FindDirectory(RelativeDirectory);
	if (ServerDirectory != NULL)
	{
		// loop over the server files and look if they are in this exact directory
		for (FServerTOC::FDirectory::TIterator It(*ServerDirectory); It && RetVal == true; ++It)
		{
			if (FPaths::GetPath(It.Key()) == RelativeDirectory)
			{
				// timestamps of 0 mean directories
				bool bIsDirectory = It.Value() == 0;
			
				// visit (stripping off the path if needed)
				RetVal = Visitor.Visit(bHadNoPath ? *FPaths::GetCleanFilename(It.Key()) : *It.Key(), bIsDirectory);
			}
		}
	}

	return RetVal;
}

bool FNetworkPlatformFile::IterateDirectoryRecursively(const TCHAR* InDirectory, IPlatformFile::FDirectoryVisitor& Visitor)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// local files go right to the source
	FString RelativeDirectory = InDirectory;
	MakeStandardNetworkFilename(RelativeDirectory);

	if (IsInLocalDirectory(RelativeDirectory))
	{
		return InnerPlatformFile->IterateDirectoryRecursively(InDirectory, Visitor);
	}

	// we loop until this is false
	bool RetVal = true;

	for (TMap<FString, FServerTOC::FDirectory*>::TIterator DirIt(ServerFiles.Directories); DirIt && RetVal == true; ++DirIt)
	{
		if (DirIt.Key().StartsWith(RelativeDirectory))
		{
			FServerTOC::FDirectory& ServerDirectory = *DirIt.Value();
		
			// loop over the server files and look if they are in this exact directory
			for (FServerTOC::FDirectory::TIterator It(ServerDirectory); It && RetVal == true; ++It)
			{
				// timestamps of 0 mean directories
				bool bIsDirectory = It.Value() == 0;

				// visit!
				RetVal = Visitor.Visit(*It.Key(), bIsDirectory);
			}
		}
	}

	return RetVal;
}

bool FNetworkPlatformFile::IterateDirectoryStat(const TCHAR* InDirectory, IPlatformFile::FDirectoryStatVisitor& Visitor)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// for .dll, etc searches that don't specify a path, we need to strip off the path
	// before we send it to the visitor
	bool bHadNoPath = InDirectory[0] == 0;

	// local files go right to the source
	FString RelativeDirectory = InDirectory;
	MakeStandardNetworkFilename(RelativeDirectory);
	if (IsInLocalDirectory(RelativeDirectory))
	{
		return InnerPlatformFile->IterateDirectoryStat(InDirectory, Visitor);
	}

	// we loop until this is false
	bool RetVal = true;

	FServerTOC::FDirectory* ServerDirectory = ServerFiles.FindDirectory(RelativeDirectory);
	if (ServerDirectory != NULL)
	{
		// loop over the server files and look if they are in this exact directory
		for (FServerTOC::FDirectory::TIterator It(*ServerDirectory); It && RetVal == true; ++It)
		{
			if (FPaths::GetPath(It.Key()) == RelativeDirectory)
			{
				// timestamps of 0 mean directories
				bool bIsDirectory = It.Value() == 0;
			
				// todo: this data is just wrong for most things, but can we afford to get the files from the server to get the correct info? Could the server provide this instead?
				const FFileStatData StatData(
					FDateTime::MinValue(), 
					FDateTime::MinValue(), 
					(bIsDirectory) ? FDateTime::MinValue() : It.Value(),
					-1,		// FileSize
					bIsDirectory, 
					true	// IsReadOnly
					);

				// visit (stripping off the path if needed)
				RetVal = Visitor.Visit(bHadNoPath ? *FPaths::GetCleanFilename(It.Key()) : *It.Key(), StatData);
			}
		}
	}

	return RetVal;
}

bool FNetworkPlatformFile::IterateDirectoryStatRecursively(const TCHAR* InDirectory, IPlatformFile::FDirectoryStatVisitor& Visitor)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// local files go right to the source
	FString RelativeDirectory = InDirectory;
	MakeStandardNetworkFilename(RelativeDirectory);

	if (IsInLocalDirectory(RelativeDirectory))
	{
		return InnerPlatformFile->IterateDirectoryStatRecursively(InDirectory, Visitor);
	}

	// we loop until this is false
	bool RetVal = true;

	for (TMap<FString, FServerTOC::FDirectory*>::TIterator DirIt(ServerFiles.Directories); DirIt && RetVal == true; ++DirIt)
	{
		if (DirIt.Key().StartsWith(RelativeDirectory))
		{
			FServerTOC::FDirectory& ServerDirectory = *DirIt.Value();
		
			// loop over the server files and look if they are in this exact directory
			for (FServerTOC::FDirectory::TIterator It(ServerDirectory); It && RetVal == true; ++It)
			{
				// timestamps of 0 mean directories
				bool bIsDirectory = It.Value() == 0;

				// todo: this data is just wrong for most things, but can we afford to get the files from the server to get the correct info? Could the server provide this instead?
				const FFileStatData StatData(
					FDateTime::MinValue(), 
					FDateTime::MinValue(), 
					(bIsDirectory) ? FDateTime::MinValue() : It.Value(),
					0,		// FileSize
					bIsDirectory, 
					true	// IsReadOnly
					);

				// visit!
				RetVal = Visitor.Visit(*It.Key(), StatData);
			}
		}
	}

	return RetVal;
}

bool FNetworkPlatformFile::DeleteDirectoryRecursively(const TCHAR* Directory)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	// perform a local operation
	return InnerPlatformFile->DeleteDirectory(Directory);
}

bool FNetworkPlatformFile::CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags, EPlatformFileWrite WriteFlags)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	FString RelativeFrom = From;
	MakeStandardNetworkFilename(RelativeFrom);

	// don't copy files in local directories
	if (!IsInLocalDirectory(RelativeFrom))
	{
		// make sure the source file exists here
		EnsureFileIsLocal(RelativeFrom);
	}

	// perform a local operation
	return InnerPlatformFile->CopyFile(To, From, ReadFlags, WriteFlags);
}

FString FNetworkPlatformFile::ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename )
{
	FString RelativeFrom = Filename;
	MakeStandardNetworkFilename(RelativeFrom);
	
	if (!IsInLocalDirectory(RelativeFrom))
	{
		EnsureFileIsLocal(RelativeFrom);
	}
	return InnerPlatformFile->ConvertToAbsolutePathForExternalAppForRead(Filename);
}

FString FNetworkPlatformFile::ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename )
{
	FString RelativeFrom = Filename;
	MakeStandardNetworkFilename(RelativeFrom);

	if (!IsInLocalDirectory(RelativeFrom))
	{
		EnsureFileIsLocal(RelativeFrom);
	}
	return InnerPlatformFile->ConvertToAbsolutePathForExternalAppForWrite(Filename);
}

bool FNetworkPlatformFile::DirectoryExists(const TCHAR* Directory)
{
	if (InnerPlatformFile->DirectoryExists(Directory))
	{
		return true;
	}
	// If there are any syncable files in this directory, consider it existing
	FString RelativeDirectory = Directory;
	MakeStandardNetworkFilename(RelativeDirectory);		

	FServerTOC::FDirectory* ServerDirectory = ServerFiles.FindDirectory(RelativeDirectory);
	return ServerDirectory != NULL;
}

void FNetworkPlatformFile::GetFileInfo(const TCHAR* Filename, FFileInfo& Info)
{
	FString RelativeFilename = Filename;
	MakeStandardNetworkFilename(RelativeFilename);

	// don't copy files in local directories
	if (!IsInLocalDirectory(RelativeFilename))
	{
		EnsureFileIsLocal(RelativeFilename);
	}

	const FFileStatData StatData = InnerPlatformFile->GetStatData(Filename);
	Info.FileExists = StatData.bIsValid && !StatData.bIsDirectory;
	Info.ReadOnly = StatData.bIsReadOnly;
	Info.Size = StatData.FileSize;
	Info.TimeStamp = StatData.ModificationTime;
	Info.AccessTimeStamp = StatData.AccessTime;
}

void FNetworkPlatformFile::ConvertServerFilenameToClientFilename(FString& FilenameToConvert)
{
	FNetworkPlatformFile::ConvertServerFilenameToClientFilename(FilenameToConvert, ServerEngineDir, ServerProjectDir);
}

void FNetworkPlatformFile::FillGetFileList(FNetworkFileArchive& Payload)
{
	TArray<FString> TargetPlatformNames;
	FPlatformMisc::GetValidTargetPlatforms(TargetPlatformNames);
	FString GameName = FApp::GetProjectName();
	if (FPaths::IsProjectFilePathSet())
	{
		GameName = FPaths::GetProjectFilePath();
	}

	FString EngineRelPath = FPaths::EngineDir();
	FString EngineRelPluginPath = FPaths::EnginePluginsDir();
	FString GameRelPath = FPaths::ProjectDir();
	FString GameRelPluginPath = FPaths::ProjectPluginsDir();

	TArray<FString> Directories;
	Directories.Add(EngineRelPath);
	Directories.Add(EngineRelPluginPath);
	Directories.Add(GameRelPath);
	Directories.Add(GameRelPluginPath);

	Payload << TargetPlatformNames;
	Payload << GameName;
	Payload << EngineRelPath;
	Payload << GameRelPath;
	Payload << Directories;
	Payload << ConnectionFlags;

	FString VersionInfo = GetVersionInfo();
	Payload << VersionInfo;
}

void FNetworkPlatformFile::ProcessServerInitialResponse(FArrayReader& InResponse, int32& OutServerPackageVersion, int32& OutServerPackageLicenseeVersion)
{
	// Receive the cooked version information.
	InResponse << OutServerPackageVersion;
	InResponse << OutServerPackageLicenseeVersion;

	// receive the server engine and game dir
	InResponse << ServerEngineDir;
	InResponse << ServerProjectDir;

	UE_LOG(LogNetworkPlatformFile, Display, TEXT("    Server EngineDir = %s"), *ServerEngineDir);
	UE_LOG(LogNetworkPlatformFile, Display, TEXT("     Local EngineDir = %s"), *FPaths::EngineDir());
	UE_LOG(LogNetworkPlatformFile, Display, TEXT("    Server ProjectDir   = %s"), *ServerProjectDir);
	UE_LOG(LogNetworkPlatformFile, Display, TEXT("     Local ProjectDir   = %s"), *FPaths::ProjectDir());

	// Receive a list of files and their timestamps.
	TMap<FString, FDateTime> ServerFileMap;
	InResponse << ServerFileMap;
	for (TMap<FString, FDateTime>::TIterator It(ServerFileMap); It; ++It)
	{
		FString ServerFile = It.Key();
		ConvertServerFilenameToClientFilename(ServerFile);
		ServerFiles.AddFileOrDirectory(ServerFile, It.Value());
	}
}


FString FNetworkPlatformFile::GetVersionInfo() const
{
	return FString("");
}

bool FNetworkPlatformFile::SendReadMessage(uint8* Destination, int64 BytesToRead)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	return true;
}

bool FNetworkPlatformFile::SendWriteMessage(const uint8* Source, int64 BytesToWrite)
{
//	FScopeLock ScopeLock(&SynchronizationObject);

	return true;
}

bool FNetworkPlatformFile::SendMessageToServer(const TCHAR* Message, IPlatformFile::IFileServerMessageHandler* Handler)
{
	// handle the recompile shaders message
	// @todo: Maybe we should just send the string message to the server, but then we'd have to 
	// handle the return from the server in a generic way
	if (FCString::Stricmp(Message, TEXT("RecompileShaders")) == 0)
	{
		FNetworkFileArchive Payload(NFS_Messages::RecompileShaders);

		// let the handler fill out the object
		Handler->FillPayload(Payload);

		FArrayReader Response;
		if (!SendPayloadAndReceiveResponse(Payload, Response))
		{
			return false;
		}

		// locally delete any files that were modified on the server, so that any read will recache the file
		// this has to be done in this class, not in the Handler (which can't access these members)
		TArray<FString> ModifiedFiles;
		Response << ModifiedFiles;

		if( InnerPlatformFile != NULL )
		{
			for (int32 Index = 0; Index < ModifiedFiles.Num(); Index++)
			{
				InnerPlatformFile->DeleteFile(*ModifiedFiles[Index]);
				CachedLocalFiles.Remove(ModifiedFiles[Index]);
				ServerFiles.AddFileOrDirectory(ModifiedFiles[Index], FDateTime::UtcNow());
			}
		}


		// let the handler process the response directly
		Handler->ProcessResponse(Response);
	}

	return true;
}


static FThreadSafeCounter OutstandingAsyncWrites;

class FAsyncNetworkWriteWorker : public FNonAbandonableTask
{
public:
	/** Filename To write to**/
	FString							Filename;
	/** An archive to read the file contents from */
	FArchive* FileArchive;
	/** timestamp for the file **/
	FDateTime ServerTimeStamp;
	IPlatformFile& InnerPlatformFile;
	FScopedEvent* Event;

	uint8 Buffer[128 * 1024];

	/** Constructor
	*/
	FAsyncNetworkWriteWorker(const TCHAR* InFilename, FArchive* InArchive, FDateTime InServerTimeStamp, IPlatformFile* InInnerPlatformFile, FScopedEvent* InEvent)
		: Filename(InFilename)
		, FileArchive(InArchive)
		, ServerTimeStamp(InServerTimeStamp)
		, InnerPlatformFile(*InInnerPlatformFile)
		, Event(InEvent)
	{
	}
		
	/** Write the file  */
	void DoWork()
	{
		if (InnerPlatformFile.FileExists(*Filename))
		{
			InnerPlatformFile.SetReadOnly(*Filename, false);
			InnerPlatformFile.DeleteFile(*Filename);
		}
		// Read FileSize first so that the correct amount of data is read from the archive
		// before exiting this worker.
		uint64 FileSize;
		*FileArchive << FileSize;
		if (ServerTimeStamp != FDateTime::MinValue())  // if the file didn't actually exist on the server, don't create a zero byte file
		{
			FString TempFilename = Filename + TEXT(".tmp");
			InnerPlatformFile.CreateDirectoryTree(*FPaths::GetPath(Filename));
			{
				TUniquePtr<IFileHandle> FileHandle;
				FileHandle.Reset(InnerPlatformFile.OpenWrite(*TempFilename));

				if (!FileHandle)
				{
					UE_LOG(LogNetworkPlatformFile, Fatal, TEXT("Could not open file for writing '%s'."), *TempFilename);
				}

				// now write the file from bytes pulled from the archive
				// read/write a chunk at a time
				uint64 RemainingData = FileSize;
				while (RemainingData)
				{
					// read next chunk from archive
					uint32 LocalSize = FPlatformMath::Min<uint32>(ARRAY_COUNT(Buffer), RemainingData);
					FileArchive->Serialize(Buffer, LocalSize);
					// write it out
					if (!FileHandle->Write(Buffer, LocalSize))
					{
						UE_LOG(LogNetworkPlatformFile, Fatal, TEXT("Could not write '%s'."), *TempFilename);
					}

					// decrement how much is left
					RemainingData -= LocalSize;
				}

				// delete async write archives
				if (Event)
				{
					delete FileArchive;
				}

				if (InnerPlatformFile.FileSize(*TempFilename) != FileSize)
				{
					UE_LOG(LogNetworkPlatformFile, Fatal, TEXT("Did not write '%s'."), *TempFilename);
				}
			}

			// rename from temp filename to real filename
			InnerPlatformFile.MoveFile(*Filename, *TempFilename);

			// now set the server's timestamp on the local file (so we can make valid comparisons)
			InnerPlatformFile.SetTimeStamp(*Filename, ServerTimeStamp);

			FDateTime CheckTime = InnerPlatformFile.GetTimeStamp(*Filename);
			if (CheckTime < ServerTimeStamp)
			{
				UE_LOG(LogNetworkPlatformFile, Fatal, TEXT("Could Not Set Timestamp '%s'  %s < %s."), *Filename, *CheckTime.ToString(), *ServerTimeStamp.ToString());
			}
		}
		if (Event)
		{
			if (OutstandingAsyncWrites.Decrement() == 0)
			{
				Event->Trigger(); // last file, fire trigger
			}
		}
	}

	FORCEINLINE TStatId GetStatId() const
	{
		return TStatId();
		//RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncNetworkWriteWorker, STATGROUP_ThreadPoolAsyncTasks);
	}
};

/**
 * Write a file async or sync, with the data coming from a TArray or an FArchive/Filesize
 */
void SyncWriteFile(FArchive* Archive, const FString& Filename, FDateTime ServerTimeStamp, IPlatformFile& InnerPlatformFile)
{
	FScopedEvent* NullEvent = NULL;
	(new FAutoDeleteAsyncTask<FAsyncNetworkWriteWorker>(*Filename, Archive, ServerTimeStamp, &InnerPlatformFile, NullEvent))->StartSynchronousTask();
}

void AsyncWriteFile(FArchive* Archive, const FString& Filename, FDateTime ServerTimeStamp, IPlatformFile& InnerPlatformFile, FScopedEvent* Event = NULL)
{
	(new FAutoDeleteAsyncTask<FAsyncNetworkWriteWorker>(*Filename, Archive, ServerTimeStamp, &InnerPlatformFile, Event))->StartBackgroundTask();
}

void AsyncReadUnsolicitedFiles(int32 InNumUnsolictedFiles, FNetworkPlatformFile& InNetworkFile, IPlatformFile& InInnerPlatformFile, FString& InServerEngineDir, FString& InServerProjectDir, FScopedEvent *InNetworkDoneEvent, FScopedEvent *InWritingDoneEvent)
{
	class FAsyncReadUnsolicitedFile : public FNonAbandonableTask
	{
	public:
		int32 NumUnsolictedFiles;
		FNetworkPlatformFile& NetworkFile;
		IPlatformFile& InnerPlatformFile;
		FString ServerEngineDir;
		FString ServerProjectDir;
		FScopedEvent* NetworkDoneEvent; // finished using the network
		FScopedEvent* WritingDoneEvent; // finished writing the files to disk

		FAsyncReadUnsolicitedFile(int32 In_NumUnsolictedFiles, FNetworkPlatformFile* In_NetworkFile, IPlatformFile* In_InnerPlatformFile, FString& In_ServerEngineDir, FString& In_ServerProjectDir, FScopedEvent *In_NetworkDoneEvent, FScopedEvent *In_WritingDoneEvent )
			: NumUnsolictedFiles(In_NumUnsolictedFiles)
			, NetworkFile(*In_NetworkFile)
			, InnerPlatformFile(*In_InnerPlatformFile)
			, ServerEngineDir(In_ServerEngineDir)
			, ServerProjectDir(In_ServerProjectDir)
			, NetworkDoneEvent(In_NetworkDoneEvent)
			, WritingDoneEvent(In_WritingDoneEvent)
		{
		}
		
		/** Write the file  */
		void DoWork()
		{
			OutstandingAsyncWrites.Add( NumUnsolictedFiles );
			for (int32 Index = 0; Index < NumUnsolictedFiles; Index++)
			{
				FArrayReader* UnsolictedResponse = new FArrayReader;
				if (!NetworkFile.ReceiveResponse(*UnsolictedResponse))
				{
					UE_LOG(LogNetworkPlatformFile, Fatal, TEXT("Receive failure!"));
					return;
				}
				FString UnsolictedReplyFile;
				*UnsolictedResponse << UnsolictedReplyFile;

				if (!UnsolictedReplyFile.IsEmpty())
				{
					FNetworkPlatformFile::ConvertServerFilenameToClientFilename(UnsolictedReplyFile, ServerEngineDir, ServerProjectDir);
					// get the server file timestamp
					FDateTime UnsolictedServerTimeStamp;
					*UnsolictedResponse << UnsolictedServerTimeStamp;

					// write the file by pulling out of the FArrayReader
					AsyncWriteFile(UnsolictedResponse, UnsolictedReplyFile, UnsolictedServerTimeStamp, InnerPlatformFile, WritingDoneEvent);
				}
			}
			NetworkDoneEvent->Trigger();
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncReadUnsolicitedFile, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

	(new FAutoDeleteAsyncTask<FAsyncReadUnsolicitedFile>(InNumUnsolictedFiles, &InNetworkFile, &InInnerPlatformFile, InServerEngineDir, InServerProjectDir, InNetworkDoneEvent, InWritingDoneEvent))->StartSynchronousTask();
}

bool FNetworkPlatformFile::IsMediaExtension(const TCHAR* Ext)
{
	if (*Ext != TEXT('.'))
	{
		return MP4Extension.EndsWith(Ext);
	}
	else
	{
		return MP4Extension == Ext;
	}
}

bool FNetworkPlatformFile::IsAdditionalCookedFileExtension(const TCHAR* Ext)
{
	if (*Ext != TEXT('.'))
	{
		return BulkFileExtension.EndsWith(Ext) || FontFileExtension.EndsWith(Ext) || ExpFileExtension.EndsWith(Ext);
	}
	else
	{
		return BulkFileExtension == Ext || FontFileExtension == Ext || ExpFileExtension == Ext;
	}
}

/**
 * Given a filename, make sure the file exists on the local filesystem
 */

void FNetworkPlatformFile::EnsureFileIsLocal(const FString& Filename)
{
	double StartTime;
	float ThisTime;
	StartTime = FPlatformTime::Seconds();

	UE_LOG(LogNetworkPlatformFile, Verbose, TEXT("Searching for %s locally "), *Filename);

	{
		FScopeLock ScopeLock(&SynchronizationObject);
		// have we already cached this file? 
		if (CachedLocalFiles.Find(Filename) != NULL)
		{
			return;
		}
	}

	bool bIncrimentedPackageWaits = false;
	if (FinishedAsyncNetworkReadUnsolicitedFiles)
	{
		if (FinishedAsyncNetworkReadUnsolicitedFiles->Get() == 0)
		{
			++UnsolicitedPackageWaits;
			bIncrimentedPackageWaits = true;
		}
		delete FinishedAsyncNetworkReadUnsolicitedFiles; // wait here for any async unsolicited files to finish reading being read from the network 
		FinishedAsyncNetworkReadUnsolicitedFiles = NULL;
	}
	if (FinishedAsyncWriteUnsolicitedFiles)
	{
		if (bIncrimentedPackageWaits == false && FinishedAsyncNetworkReadUnsolicitedFiles->Get() == 0)
		{
			++UnsolicitedPackageWaits;
		}
		delete FinishedAsyncWriteUnsolicitedFiles; // wait here for any async unsolicited files to finish writing to disk
		FinishedAsyncWriteUnsolicitedFiles = NULL;
	}

	FScopeLock ScopeLock(&SynchronizationObject);
	ThisTime = 1000.0f * float(FPlatformTime::Seconds() - StartTime);
	TotalWaitForAsyncUnsolicitedPackages += ThisTime;
	//UE_LOG(LogNetworkPlatformFile, Display, TEXT("Lock and wait for old async writes %6.2fms"), ThisTime);

	if (CachedLocalFiles.Find(Filename) != NULL)
	{
		++UnsolicitedPackagesHits;
		return;
	}

	UE_LOG(LogNetworkPlatformFile, Verbose, TEXT("Attempting to get %s from server"), *Filename);

	// even if an error occurs later, we still want to remember not to try again
	CachedLocalFiles.Add(Filename);
	UE_LOG(LogNetworkPlatformFile, Warning, TEXT("Cached file %s"), *Filename)
	StartTime = FPlatformTime::Seconds();

	// no need to read it if it already exists 
	// @todo: Handshake with server to delete files that are out of date
	if (InnerPlatformFile->FileExists(*Filename))
	{
		++TotalFilesFoundLocally;
		UE_LOG(LogNetworkPlatformFile, Verbose, TEXT("File %s exists locally but wasn't in cache"), *Filename);
		return;
	}

	++TotalFilesSynced;

	

	ThisTime = 1000.0f * float(FPlatformTime::Seconds() - StartTime);
	//UE_LOG(LogNetworkPlatformFile, Display, TEXT("Check for local file %6.2fms - %s"), ThisTime, *Filename);

	// this is a bit of a waste if we aren't doing cook on the fly, but we assume missing asset files are relatively rare
	FString Extension = FPaths::GetExtension(Filename, true);
	bool bIsCookable = GConfig && GConfig->IsReadyForUse() && (FPackageName::IsPackageExtension(*Extension) || IsMediaExtension(*Extension) || IsAdditionalCookedFileExtension(*Extension));

	// we only copy files that actually exist on the server, can greatly reduce network traffic for, say,
	// the INT file each package tries to load
	if (!bIsCookable && (ServerFiles.FindFile(Filename) == NULL))
	{
		// Uncomment this to have the server file list dumped
		// the first time a file requested is not found.

		UE_LOG(LogNetworkPlatformFile, Verbose, TEXT("Didn't find %s in server files list"), *Filename);

#if 0
		static bool sb_DumpedServer = false;
		if (sb_DumpedServer == false)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Dumping server files... %s not found\n"), *Filename);
			for (TMap<FString, FServerTOC::FDirectory*>::TIterator ServerDumpIt(ServerFiles.Directories); ServerDumpIt; ++ServerDumpIt)
			{
				FServerTOC::FDirectory& Directory = *ServerDumpIt.Value();
				for (FServerTOC::FDirectory::TIterator DirDumpIt(Directory); DirDumpIt; ++DirDumpIt)
				{
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%10s - %s\n"), *(DirDumpIt.Value().ToString()), *(DirDumpIt.Key()));
				}
			}
			sb_DumpedServer = true;
		}
#endif
		return;
	}

	// send the filename over (cast away const here because we know this << will not modify the string)
	FNetworkFileArchive Payload(NFS_Messages::SyncFile);
	Payload << (FString&)Filename;

	StartTime = FPlatformTime::Seconds();

	// allocate array reader on the heap, because the SyncWriteFile function will delete it
	FArrayReader Response;
	if (!SendPayloadAndReceiveResponse(Payload, Response))
	{
		UE_LOG(LogNetworkPlatformFile, Fatal, TEXT("Receive failure!"));
		return;
	}
	ThisTime = 1000.0f * float(FPlatformTime::Seconds() - StartTime);
	TotalNetworkSyncTime += ThisTime;
	//UE_LOG(LogNetworkPlatformFile, Display, TEXT("Send and receive %6.2fms"), ThisTime);

	StartTime = FPlatformTime::Seconds();

	FString ReplyFile;
	Response << ReplyFile;
	ConvertServerFilenameToClientFilename(ReplyFile);
	check(ReplyFile == Filename);

	// get the server file timestamp
	FDateTime ServerTimeStamp;
	Response << ServerTimeStamp;

	if (ServerTimeStamp != FDateTime::MinValue())  // if the file didn't actually exist on the server, don't create a zero byte file
	{
		UE_LOG(LogNetworkPlatformFile, Verbose, TEXT("Succeeded in getting %s from server"), *Filename);
	}
	else
	{
		UE_LOG(LogNetworkPlatformFile, Verbose, TEXT("File not found %s from server"), *Filename);
	}

	// write the file in chunks, synchronously
	SyncWriteFile(&Response, ReplyFile, ServerTimeStamp, *InnerPlatformFile);

	int32 NumUnsolictedFiles;
	Response << NumUnsolictedFiles;

	if (NumUnsolictedFiles)
	{
		TotalUnsolicitedPackages += NumUnsolictedFiles;
		check( FinishedAsyncNetworkReadUnsolicitedFiles == NULL );
		check( FinishedAsyncWriteUnsolicitedFiles == NULL );
		FinishedAsyncNetworkReadUnsolicitedFiles = new FScopedEvent;
		FinishedAsyncWriteUnsolicitedFiles = new FScopedEvent;
		AsyncReadUnsolicitedFiles(NumUnsolictedFiles, *this, *InnerPlatformFile, ServerEngineDir, ServerProjectDir, FinishedAsyncNetworkReadUnsolicitedFiles, FinishedAsyncWriteUnsolicitedFiles);
	}
	
	ThisTime = 1000.0f * float(FPlatformTime::Seconds() - StartTime);
	TotalWriteTime += ThisTime;
	//UE_LOG(LogNetworkPlatformFile, Display, TEXT("Write file to local %6.2fms"), ThisTime);
}

static FString NetworkPlatformFileEndChop(TEXT("/"));
void FNetworkPlatformFile::MakeStandardNetworkFilename(FString& Filename)
{
	FPaths::MakeStandardFilename(Filename);
	Filename.RemoveFromEnd(NetworkPlatformFileEndChop, ESearchCase::CaseSensitive);
}

bool FNetworkPlatformFile::IsInLocalDirectoryUnGuarded(const FString& Filename)
{
	// cache the directory of the input file
	FString Directory = FPaths::GetPath(Filename);

	// look if the file is in a local directory
	for (int32 DirIndex = 0; DirIndex < LocalDirectories.Num(); DirIndex++)
	{
		if (Directory.StartsWith(LocalDirectories[DirIndex]))
		{
			return true;
		}
	}

	// if not local, talk to the server
	return false;
}

bool FNetworkPlatformFile::IsInLocalDirectory(const FString& Filename)
{
	if (!bHasLoadedDDCDirectories)
	{
		// need to be careful here to avoid initializing the DDC from the wrong thread or using LocalDirectories while it is being initialized
		FScopeLock ScopeLock(&LocalDirectoriesCriticalSection);

		if (IsInGameThread() && GConfig && GConfig->IsReadyForUse())
		{
			// one time DDC directory initialization
			// add any DDC directories to our list of local directories (local = inner platform file, it may
			// actually live on a server, but it will use the platform's file system)
			if (GetDerivedDataCache())
			{
				TArray<FString> DdcDirectories;

				GetDerivedDataCacheRef().GetDirectories(DdcDirectories);

				LocalDirectories.Append(DdcDirectories);
			}

			FPlatformMisc::MemoryBarrier();
			bHasLoadedDDCDirectories = true;
		}

		return IsInLocalDirectoryUnGuarded(Filename);
	}

	// once the DDC is initialized, we don't need to lock a critical section anymore
	return IsInLocalDirectoryUnGuarded(Filename);
}

void FNetworkPlatformFile::PerformHeartbeat()
{
	// send the filename over (cast away const here because we know this << will not modify the string)
	FNetworkFileArchive Payload(NFS_Messages::Heartbeat);

	

	// send the filename over
	FArrayReader Response;
	if (!SendPayloadAndReceiveResponse(Payload, Response))
	{
		return;
	}

	// get any files that have been modified on the server - 
	TArray<FString> UpdatedFiles;
	Response << UpdatedFiles;

	// delete any outdated files from the client
	// @todo: This may need a critical section around all calls to LowLevel in the other functions
	// because we don't want to delete files while other threads are using them!

	TArray<FString> PackageNames;
	for (int32 FileIndex = 0; FileIndex < UpdatedFiles.Num(); FileIndex++)
	{
		// clean up the linkers for this package
		FString LocalFileName = UpdatedFiles[FileIndex];
		ConvertServerFilenameToClientFilename( LocalFileName );

		UE_LOG(LogNetworkPlatformFile, Log, TEXT("Server updated file '%s', deleting local copy %s"), *UpdatedFiles[FileIndex], *LocalFileName);

		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(LocalFileName, PackageName))
		{
			PackageNames.Add(PackageName);
		}
		else
		{
			UE_LOG(LogNetworkPlatformFile, Log, TEXT("Unable to convert filename to package name %s"), *LocalFileName);
		}

		OnFileUpdated(LocalFileName);
	}

	if ( PackageNames.Num() > 0 )
	{
		FCoreUObjectDelegates::NetworkFileRequestPackageReload.ExecuteIfBound(PackageNames);
	}
}

void FNetworkPlatformFile::OnFileUpdated(const FString& LocalFileName)
{
	if (InnerPlatformFile->FileExists(*LocalFileName) && InnerPlatformFile->DeleteFile(*LocalFileName) == false)
	{
		UE_LOG(LogNetworkPlatformFile, Error, TEXT("Failed to delete %s, someone is probably accessing without FNetworkPlatformFile, or we need better thread protection"), *LocalFileName);
	}
	CachedLocalFiles.Remove(LocalFileName);
	ServerFiles.AddFileOrDirectory(LocalFileName, FDateTime::UtcNow());
}

void FNetworkPlatformFile::ConvertServerFilenameToClientFilename(FString& FilenameToConvert, const FString& InServerEngineDir, const FString& InServerProjectDir)
{
	if (FilenameToConvert.StartsWith(InServerEngineDir))
	{
		FilenameToConvert = FilenameToConvert.Replace(*InServerEngineDir, *(FPaths::EngineDir()));
	}
	else if (FilenameToConvert.StartsWith(InServerProjectDir))
	{
		FilenameToConvert = FilenameToConvert.Replace(*InServerProjectDir, *(FPaths::ProjectDir()));
	}
}

void FNetworkPlatformFile::Tick() 
{
	// try send a heart beat every 5 seconds as long as we are not async loading
	static double StartTime = FPlatformTime::Seconds();

	bool bShouldPerformHeartbeat = true;
	if ((FPlatformTime::Seconds() - StartTime) > HeartbeatFrequency && HeartbeatFrequency >= 0 )
	{


		if (IsAsyncLoading() && bShouldPerformHeartbeat)
		{
			bShouldPerformHeartbeat = false;
		}

		{
			FScopeLock S(&SynchronizationObject);
			if (FinishedAsyncNetworkReadUnsolicitedFiles && bShouldPerformHeartbeat)
			{
				if ( FinishedAsyncNetworkReadUnsolicitedFiles->IsReady() )
				{
					delete FinishedAsyncNetworkReadUnsolicitedFiles;
					FinishedAsyncNetworkReadUnsolicitedFiles = nullptr;
				}
				else
				{
					bShouldPerformHeartbeat = false;
				}
			}
		}

		if ( bShouldPerformHeartbeat )
		{
			StartTime = FPlatformTime::Seconds();

			//DeleteLoaders();
			PerformHeartbeat();
		}
	}
}

bool FNetworkPlatformFile::Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd, TEXT("networkfile")))
	{
		if ( FParse::Command(&Cmd, TEXT("stats")))
		{

			Ar.Logf(TEXT("Network platform file %s stats\n"
				"TotalWriteTime \t%fms \n"
				"TotalNetworkSyncTime \t%fms \n"
				"TotalTimeSpentInUnsolicitedPackages \t%fms \n"
				"TotalWaitForAsyncUnsolicitedPackages \t%fms \n"
				"TotalFilesSynced \t%d \n"
				"TotalFilesFoundLocally \t%d\n"
				"TotalUnsolicitedPackages \t%d \n"
				"UnsolicitedPackagesHits \t%d \n"
				"UnsolicitedPackageWaits \t%d \n"),
				GetTypeName(),
				TotalWriteTime,
				TotalNetworkSyncTime,
				TotalTimeSpentInUnsolicitedPackages,
				TotalWaitForAsyncUnsolicitedPackages,
				TotalFilesSynced,
				TotalFilesFoundLocally,
				TotalUnsolicitedPackages, 
				UnsolicitedPackagesHits,
				UnsolicitedPackageWaits);

			// there could be multiple network platform files so let them all report their stats
			return false;
		}
	}
	return false;
}

/**
 * Module for the network file
 */
class FNetworkFileModule : public IPlatformFileModule
{
public:
	virtual IPlatformFile* GetPlatformFile() override
	{
		static TUniquePtr<IPlatformFile> AutoDestroySingleton = MakeUnique<FNetworkPlatformFile>();
		return AutoDestroySingleton.Get();
	}
};
IMPLEMENT_MODULE(FNetworkFileModule, NetworkFile);
