// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NetworkFileServerConnection.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/BufferArchive.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "IPlatformFileSandboxWrapper.h"
#include "NetworkMessage.h"
#include "ProjectDescriptor.h"
#include "NetworkFileSystemLog.h"
#include "Misc/PackageName.h"
#include "Interfaces/ITargetPlatform.h"
#include "HAL/PlatformTime.h"


/**
 * Helper function for resolving engine and game sandbox paths
 */
void GetSandboxRootDirectories(FSandboxPlatformFile* Sandbox, FString& SandboxEngine, FString& SandboxProject, const FString& LocalEngineDir, const FString& LocalProjectDir)
{
	SandboxEngine = Sandbox->ConvertToSandboxPath(*LocalEngineDir);
	if (SandboxEngine.EndsWith(TEXT("/"), ESearchCase::CaseSensitive) == false)
	{
		SandboxEngine += TEXT("/");
	}

	// we need to add an extra bit to the game path to make the sandbox convert it correctly (investigate?)
	// @todo: double check this
	SandboxProject = Sandbox->ConvertToSandboxPath(*(LocalProjectDir + TEXT("a.txt"))).Replace(TEXT("a.txt"), TEXT(""));
}


/* FNetworkFileServerClientConnection structors
 *****************************************************************************/

FNetworkFileServerClientConnection::FNetworkFileServerClientConnection( const FNetworkFileDelegateContainer* InNetworkFileDelegates, const TArray<ITargetPlatform*>& InActiveTargetPlatforms )
	: LastHandleId(0)
	, Sandbox(NULL)
	, NetworkFileDelegates(InNetworkFileDelegates)
	, ActiveTargetPlatforms(InActiveTargetPlatforms)
{	
	//stats
	FileRequestDelegateTime = 0.0;
	PackageFileTime = 0.0;
	UnsolicitedFilesTime = 0.0;

	FileRequestCount = 0;
	UnsolicitedFilesCount = 0;
	PackageRequestsSucceeded = 0;
	PackageRequestsFailed = 0;
	FileBytesSent = 0;

	if ( NetworkFileDelegates && NetworkFileDelegates->OnFileModifiedCallback )
	{
		NetworkFileDelegates->OnFileModifiedCallback->AddRaw(this, &FNetworkFileServerClientConnection::FileModifiedCallback);
	}

	LocalEngineDir = FPaths::EngineDir();
	LocalProjectDir = FPaths::ProjectDir();
	if (FPaths::IsProjectFilePathSet())
	{
		LocalProjectDir = FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/");
	}
}


FNetworkFileServerClientConnection::~FNetworkFileServerClientConnection( )
{
	if (NetworkFileDelegates && NetworkFileDelegates->OnFileModifiedCallback)
	{
		NetworkFileDelegates->OnFileModifiedCallback->RemoveAll(this);
	}

	// close all the files the client had opened through us when the client disconnects
	for (TMap<uint64, IFileHandle*>::TIterator It(OpenFiles); It; ++It)
	{
		delete It.Value();
	}

	delete Sandbox;
	Sandbox = NULL;	
}

/* FStreamingNetworkFileServerConnection implementation
 *****************************************************************************/
void FNetworkFileServerClientConnection::ConvertClientFilenameToServerFilename(FString& FilenameToConvert)
{
	if (FilenameToConvert.StartsWith(ConnectedEngineDir))
	{
		FilenameToConvert = FilenameToConvert.Replace(*ConnectedEngineDir, *(FPaths::EngineDir()));
	}
	else if (FilenameToConvert.StartsWith(ConnectedProjectDir))
	{
		if ( FPaths::IsProjectFilePathSet() )
		{
			FilenameToConvert = FilenameToConvert.Replace(*ConnectedProjectDir, *(FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/")));
		}
		else
		{
#if !IS_PROGRAM
			// UnrealFileServer has a ProjectDir of ../../../Engine/Programs/UnrealFileServer.
			// We do *not* want to replace the directory in that case.
			FilenameToConvert = FilenameToConvert.Replace(*ConnectedProjectDir, *(FPaths::ProjectDir()));
#endif
		}
	}
}


/**
 * Fixup sandbox paths to match what package loading will request on the client side.  e.g.
 * Sandbox path: "../../../Elemental/Content/Elemental/Effects/FX_Snow_Cracks/Crack_02/Materials/M_SnowBlast.uasset ->
 * client path: "../../../Samples/Showcases/Elemental/Content/Elemental/Effects/FX_Snow_Cracks/Crack_02/Materials/M_SnowBlast.uasset"
 * This ensures that devicelocal-cached files will be properly timestamp checked before deletion.
 */
TMap<FString, FDateTime> FNetworkFileServerClientConnection::FixupSandboxPathsForClient(const TMap<FString, FDateTime>& SandboxPaths)
{
	TMap<FString,FDateTime> FixedFiletimes;

	// since the sandbox remaps from A/B/C to C, and the client has no idea of this, we need to put the files
	// into terms of the actual LocalProjectDir, which is all that the client knows about
	for (TMap<FString, FDateTime>::TConstIterator It(SandboxPaths); It; ++It)
	{
		FixedFiletimes.Add(FixupSandboxPathForClient(It.Key()), It.Value());
	}
	return FixedFiletimes;
}

/**
 * Fixup sandbox paths to match what package loading will request on the client side.  e.g.
 * Sandbox path: "../../../Elemental/Content/Elemental/Effects/FX_Snow_Cracks/Crack_02/Materials/M_SnowBlast.uasset ->
 * client path: "../../../Samples/Showcases/Elemental/Content/Elemental/Effects/FX_Snow_Cracks/Crack_02/Materials/M_SnowBlast.uasset"
 * This ensures that devicelocal-cached files will be properly timestamp checked before deletion.
 */
FString FNetworkFileServerClientConnection::FixupSandboxPathForClient(const FString& Filename)
{
	FString Fixed = Sandbox->ConvertToSandboxPath(*Filename);
	Fixed = Fixed.Replace(*SandboxEngine, *LocalEngineDir);
	Fixed = Fixed.Replace(*SandboxProject, *LocalProjectDir);

	if (bSendLowerCase)
	{
		Fixed = Fixed.ToLower();
	}
	return Fixed;
}

/*void FNetworkFileServerClientConnection::ConvertServerFilenameToClientFilename(FString& FilenameToConvert)
{

	if (FilenameToConvert.StartsWith(FPaths::EngineDir()))
	{
		FilenameToConvert = FilenameToConvert.Replace(*(FPaths::EngineDir()), *ConnectedEngineDir);
	}
	else if (FPaths::IsProjectFilePathSet())
	{
		if (FilenameToConvert.StartsWith(FPaths::GetPath(FPaths::GetProjectFilePath())))
		{
			FilenameToConvert = FilenameToConvert.Replace(*(FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/")), *ConnectedProjectDir);
		}
	}
#if !IS_PROGRAM
	else if (FilenameToConvert.StartsWith(FPaths::ProjectDir()))
	{
			// UnrealFileServer has a ProjectDir of ../../../Engine/Programs/UnrealFileServer.
			// We do *not* want to replace the directory in that case.
			FilenameToConvert = FilenameToConvert.Replace(*(FPaths::ProjectDir()), *ConnectedProjectDir);
	}
#endif
}*/

static FCriticalSection SocketCriticalSection;

bool FNetworkFileServerClientConnection::ProcessPayload(FArchive& Ar)
{
	FBufferArchive Out;
	bool Result = true;

	// first part of the payload is always the command
	uint32 Cmd;
	Ar << Cmd;

	UE_LOG(LogFileServer, Verbose, TEXT("Processing payload with Cmd %d"), Cmd);

	// what type of message is this?
	NFS_Messages::Type Msg = NFS_Messages::Type(Cmd);

	// make sure the first thing is GetFileList which initializes the game/platform
	checkf(Msg == NFS_Messages::GetFileList || Msg == NFS_Messages::Heartbeat || Sandbox != NULL, TEXT("The first client message MUST be GetFileList, not %d"), (int32)Msg);

	// process the message!
	bool bSendUnsolicitedFiles = false;

	{
		FScopeLock SocketLock(&SocketCriticalSection);

		switch (Msg)
		{
		case NFS_Messages::OpenRead:
			ProcessOpenFile(Ar, Out, false);
			break;

		case NFS_Messages::OpenWrite:
			ProcessOpenFile(Ar, Out, true);
			break;

		case NFS_Messages::Read:
			ProcessReadFile(Ar, Out);
			break;

		case NFS_Messages::Write:
			ProcessWriteFile(Ar, Out);
			break;

		case NFS_Messages::Seek:
			ProcessSeekFile(Ar, Out);
			break;

		case NFS_Messages::Close:
			ProcessCloseFile(Ar, Out);
			break;

		case NFS_Messages::MoveFile:
			ProcessMoveFile(Ar, Out);
			break;

		case NFS_Messages::DeleteFile:
			ProcessDeleteFile(Ar, Out);
			break;

		case NFS_Messages::GetFileInfo:
			ProcessGetFileInfo(Ar, Out);
			break;

		case NFS_Messages::CopyFile:
			ProcessCopyFile(Ar, Out);
			break;

		case NFS_Messages::SetTimeStamp:
			ProcessSetTimeStamp(Ar, Out);
			break;

		case NFS_Messages::SetReadOnly:
			ProcessSetReadOnly(Ar, Out);
			break;

		case NFS_Messages::CreateDirectory:
			ProcessCreateDirectory(Ar, Out);
			break;

		case NFS_Messages::DeleteDirectory:
			ProcessDeleteDirectory(Ar, Out);
			break;

		case NFS_Messages::DeleteDirectoryRecursively:
			ProcessDeleteDirectoryRecursively(Ar, Out);
			break;

		case NFS_Messages::ToAbsolutePathForRead:
			ProcessToAbsolutePathForRead(Ar, Out);
			break;

		case NFS_Messages::ToAbsolutePathForWrite:
			ProcessToAbsolutePathForWrite(Ar, Out);
			break;

		case NFS_Messages::ReportLocalFiles:
			ProcessReportLocalFiles(Ar, Out);
			break;

		case NFS_Messages::GetFileList:
			Result = ProcessGetFileList(Ar, Out);
			break;

		case NFS_Messages::Heartbeat:
			ProcessHeartbeat(Ar, Out);
			break;

		case NFS_Messages::SyncFile:
			ProcessSyncFile(Ar, Out);
			bSendUnsolicitedFiles = true;
			break;

		case NFS_Messages::RecompileShaders:
			ProcessRecompileShaders(Ar, Out);
			break;

		default:

			UE_LOG(LogFileServer, Error, TEXT("Bad incomming message tag (%d)."), (int32)Msg);
		}
	}


	// send back a reply if the command wrote anything back out
	if (Out.Num() && Result )
	{
		int32 NumUnsolictedFiles = 0;


		if (bSendUnsolicitedFiles)
		{
			int64 MaxMemoryAllowed = 50 * 1024 * 1024;
			for (const auto& Filename : UnsolictedFiles)
			{
				// get file timestamp and send it to client
				FDateTime ServerTimeStamp = Sandbox->GetTimeStamp(*Filename);

				TArray<uint8> Contents;
				// open file
				int64 FileSize = Sandbox->FileSize(*Filename);

				if (MaxMemoryAllowed > FileSize)
				{
					MaxMemoryAllowed -= FileSize;
					++NumUnsolictedFiles;
				}
			}
			Out << NumUnsolictedFiles;
		}
		
		UE_LOG(LogFileServer, Verbose, TEXT("Returning payload with %d bytes"), Out.Num());

		// send back a reply
		Result &= SendPayload( Out );

		TArray<FString> UnprocessedUnsolictedFiles;
		UnprocessedUnsolictedFiles.Empty(NumUnsolictedFiles);

		if (bSendUnsolicitedFiles && Result )
		{
			double StartTime;
			StartTime = FPlatformTime::Seconds();
			for (int32 Index = 0; Index < NumUnsolictedFiles; Index++)
			{
				FBufferArchive OutUnsolicitedFile;
				PackageFile(UnsolictedFiles[Index], OutUnsolicitedFile);

				UE_LOG(LogFileServer, Display, TEXT("Returning unsolicited file %s with %d bytes"), *UnsolictedFiles[Index], OutUnsolicitedFile.Num());

				Result &= SendPayload(OutUnsolicitedFile);
				++UnsolicitedFilesCount;
			}
			UnsolictedFiles.RemoveAt(0, NumUnsolictedFiles);


			UnsolicitedFilesTime += 1000.0f * float(FPlatformTime::Seconds() - StartTime);
		}
	}

	UE_LOG(LogFileServer, Verbose, TEXT("Done Processing payload with Cmd %d Total Size sending %d "), Cmd,Out.TotalSize());

	return Result;
}


void FNetworkFileServerClientConnection::ProcessOpenFile( FArchive& In, FArchive& Out, bool bIsWriting )
{
	// Get filename
	FString Filename;
	In << Filename;

	bool bAppend = false;
	bool bAllowRead = false;

	if (bIsWriting)
	{
		In << bAppend;
		In << bAllowRead;
	}

	// todo: clients from the same ip address "could" be trying to write to the same file in the same sandbox (for example multiple windows clients)
	//			should probably have the sandbox write to separate files for each client
	//			not important for now

	ConvertClientFilenameToServerFilename(Filename);

	if (bIsWriting)
	{
		// Make sure the directory exists...
		Sandbox->CreateDirectoryTree(*(FPaths::GetPath(Filename)));
	}

	TArray<FString> NewUnsolictedFiles;
	NetworkFileDelegates->FileRequestDelegate.ExecuteIfBound(Filename, ConnectedPlatformName, NewUnsolictedFiles);

	FDateTime ServerTimeStamp = Sandbox->GetTimeStamp(*Filename);
	int64 ServerFileSize = 0;
	IFileHandle* File = bIsWriting ? Sandbox->OpenWrite(*Filename, bAppend, bAllowRead) : Sandbox->OpenRead(*Filename);
	if (!File)
	{
		UE_LOG(LogFileServer, Display, TEXT("Open request for %s failed for file %s."), bIsWriting ? TEXT("Writing") : TEXT("Reading"), *Filename);
		ServerTimeStamp = FDateTime::MinValue(); // if this was a directory, this will make sure it is not confused with a zero byte file
	}
	else
	{
		ServerFileSize = File->Size();
	}

	uint64 HandleId = ++LastHandleId;
	OpenFiles.Add( HandleId, File );
	
	Out << HandleId;
	Out << ServerTimeStamp;
	Out << ServerFileSize;
}


void FNetworkFileServerClientConnection::ProcessReadFile( FArchive& In, FArchive& Out )
{
	// Get Handle ID
	uint64 HandleId = 0;
	In << HandleId;

	int64 BytesToRead = 0;
	In << BytesToRead;

	int64 BytesRead = 0;
	IFileHandle* File = FindOpenFile(HandleId);

	if (File)
	{
		uint8* Dest = (uint8*)FMemory::Malloc(BytesToRead);		

		if (File->Read(Dest, BytesToRead))
		{
			BytesRead = BytesToRead;
			Out << BytesRead;
			Out.Serialize(Dest, BytesRead);
		}
		else
		{
			Out << BytesRead;
		}

		FMemory::Free(Dest);
	}
	else
	{
		Out << BytesRead;
	}
}


void FNetworkFileServerClientConnection::ProcessWriteFile( FArchive& In, FArchive& Out )
{
	// Get Handle ID
	uint64 HandleId = 0;
	In << HandleId;

	int64 BytesWritten = 0;
	IFileHandle* File = FindOpenFile(HandleId);

	if (File)
	{
		int64 BytesToWrite = 0;
		In << BytesToWrite;

		uint8* Source = (uint8*)FMemory::Malloc(BytesToWrite);
		In.Serialize(Source, BytesToWrite);

		if (File->Write(Source, BytesToWrite))
		{
			BytesWritten = BytesToWrite;
		}

		FMemory::Free(Source); 
	}
		
	Out << BytesWritten;
}


void FNetworkFileServerClientConnection::ProcessSeekFile( FArchive& In, FArchive& Out )
{
	// Get Handle ID
	uint64 HandleId = 0;
	In << HandleId;

	int64 NewPosition;
	In << NewPosition;

	int64 SetPosition = -1;
	IFileHandle* File = FindOpenFile(HandleId);

	if (File && File->Seek(NewPosition))
	{
		SetPosition = File->Tell();
	}

	Out << SetPosition;
}


void FNetworkFileServerClientConnection::ProcessCloseFile( FArchive& In, FArchive& Out )
{
	// Get Handle ID
	uint64 HandleId = 0;
	In << HandleId;

	uint32 Closed = 0;
	IFileHandle* File = FindOpenFile(HandleId);

	if (File)
	{
		Closed = 1;
		OpenFiles.Remove(HandleId);

		delete File;
	}
		
	Out << Closed;
}


void FNetworkFileServerClientConnection::ProcessGetFileInfo( FArchive& In, FArchive& Out )
{
	// Get filename
	FString Filename;
	In << Filename;

	ConvertClientFilenameToServerFilename(Filename);

	FFileInfo Info;
	Info.FileExists = Sandbox->FileExists(*Filename);

	// if the file exists, cook it if necessary (the FileExists flag won't change value based on this callback)
	// without this, the server can return the uncooked file size, which can cause reads off the end
	if (Info.FileExists)
	{
		TArray<FString> NewUnsolictedFiles;
		NetworkFileDelegates->FileRequestDelegate.ExecuteIfBound(Filename, ConnectedPlatformName, NewUnsolictedFiles);
	}

	// get the rest of the info
	Info.ReadOnly = Sandbox->IsReadOnly(*Filename);
	Info.Size = Sandbox->FileSize(*Filename);
	Info.TimeStamp = Sandbox->GetTimeStamp(*Filename);
	Info.AccessTimeStamp = Sandbox->GetAccessTimeStamp(*Filename);

	Out << Info.FileExists;
	Out << Info.ReadOnly;
	Out << Info.Size;
	Out << Info.TimeStamp;
	Out << Info.AccessTimeStamp;
}


void FNetworkFileServerClientConnection::ProcessMoveFile( FArchive& In, FArchive& Out )
{
	FString From;
	In << From;
	FString To;
	In << To;

	ConvertClientFilenameToServerFilename(From);
	ConvertClientFilenameToServerFilename(To);

	uint32 Success = Sandbox->MoveFile(*To, *From);
	Out << Success;
}


void FNetworkFileServerClientConnection::ProcessDeleteFile( FArchive& In, FArchive& Out )
{
	FString Filename;
	In << Filename;

	ConvertClientFilenameToServerFilename(Filename);

	uint32 Success = Sandbox->DeleteFile(*Filename);
	Out << Success;
}


void FNetworkFileServerClientConnection::ProcessReportLocalFiles( FArchive& In, FArchive& Out )
{
	// get the list of files on the other end
	TMap<FString, FDateTime> ClientFileTimes;
	In << ClientFileTimes;

	// go over them and compare times to this side
	TArray<FString> OutOfDateFiles;

	for (TMap<FString, FDateTime>::TIterator It(ClientFileTimes); It; ++It)
	{
		FString ClientFile = It.Key();
		ConvertClientFilenameToServerFilename(ClientFile);
		// get the local timestamp
		FDateTime Timestamp = Sandbox->GetTimeStamp(*ClientFile);

		// if it's newer than the client/remote timestamp, it's newer here, so tell the other side it's out of date
		if (Timestamp > It.Value())
		{
			OutOfDateFiles.Add(ClientFile);
		}
	}

	UE_LOG(LogFileServer, Display, TEXT("There were %d out of date files"), OutOfDateFiles.Num());
}


/** Copies file. */
void FNetworkFileServerClientConnection::ProcessCopyFile( FArchive& In, FArchive& Out )
{
	FString To;
	FString From;
	In << To;	
	In << From;

	ConvertClientFilenameToServerFilename(To);
	ConvertClientFilenameToServerFilename(From);

	bool Success = Sandbox->CopyFile(*To, *From);
	Out << Success;
}


void FNetworkFileServerClientConnection::ProcessSetTimeStamp( FArchive& In, FArchive& Out )
{
	FString Filename;
	FDateTime Timestamp;
	In << Filename;	
	In << Timestamp;

	ConvertClientFilenameToServerFilename(Filename);

	Sandbox->SetTimeStamp(*Filename, Timestamp);

	// Need to sends something back otherwise the response won't get sent at all.
	bool Success = true;
	Out << Success;
}


void FNetworkFileServerClientConnection::ProcessSetReadOnly( FArchive& In, FArchive& Out )
{
	FString Filename;
	bool bReadOnly;
	In << Filename;	
	In << bReadOnly;

	ConvertClientFilenameToServerFilename(Filename);

	bool Success = Sandbox->SetReadOnly(*Filename, bReadOnly);
	Out << Success;
}


void FNetworkFileServerClientConnection::ProcessCreateDirectory( FArchive& In, FArchive& Out ) 
{
	FString Directory;
	In << Directory;

	ConvertClientFilenameToServerFilename(Directory);

	bool bSuccess = Sandbox->CreateDirectory(*Directory);
	Out << bSuccess;
}


void FNetworkFileServerClientConnection::ProcessDeleteDirectory( FArchive& In, FArchive& Out )
{
	FString Directory;
	In << Directory;

	ConvertClientFilenameToServerFilename(Directory);

	bool bSuccess = Sandbox->DeleteDirectory(*Directory);
	Out << bSuccess;
}


void FNetworkFileServerClientConnection::ProcessDeleteDirectoryRecursively( FArchive& In, FArchive& Out )
{
	FString Directory;
	In << Directory;

	ConvertClientFilenameToServerFilename(Directory);

	bool bSuccess = Sandbox->DeleteDirectoryRecursively(*Directory);
	Out << bSuccess;
}


void FNetworkFileServerClientConnection::ProcessToAbsolutePathForRead( FArchive& In, FArchive& Out )
{
	FString Filename;
	In << Filename;

	ConvertClientFilenameToServerFilename(Filename);

	Filename = Sandbox->ConvertToAbsolutePathForExternalAppForRead(*Filename);
	Out << Filename;
}


void FNetworkFileServerClientConnection::ProcessToAbsolutePathForWrite( FArchive& In, FArchive& Out )
{
	FString Filename;
	In << Filename;

	ConvertClientFilenameToServerFilename(Filename);

	Filename = Sandbox->ConvertToAbsolutePathForExternalAppForWrite(*Filename);
	Out << Filename;
}


bool FNetworkFileServerClientConnection::ProcessGetFileList( FArchive& In, FArchive& Out )
{
	// get the list of directories to process
 	TArray<FString> TargetPlatformNames;
	FString GameName;
	FString EngineRelativePath;
	FString GameRelativePath;
	TArray<FString> RootDirectories;
	
	EConnectionFlags ConnectionFlags;

	FString ClientVersionInfo;

	In << TargetPlatformNames;
	In << GameName;
	In << EngineRelativePath;
	In << GameRelativePath;
	In << RootDirectories;
	In << ConnectionFlags;
	In << ClientVersionInfo;

	if ( NetworkFileDelegates->NewConnectionDelegate.IsBound() )
	{
		bool bIsValidVersion = true;
		for ( const FString& TargetPlatform : TargetPlatformNames )
		{
			bIsValidVersion &= NetworkFileDelegates->NewConnectionDelegate.Execute(ClientVersionInfo, TargetPlatform );
		}
		if ( bIsValidVersion == false )
		{
			return false;
		}
	}

	const bool bIsStreamingRequest = (ConnectionFlags & EConnectionFlags::Streaming) == EConnectionFlags::Streaming;
	const bool bIsPrecookedIterativeRequest = (ConnectionFlags & EConnectionFlags::PreCookedIterative) == EConnectionFlags::PreCookedIterative;

	ConnectedPlatformName = TEXT("");

	// if we didn't find one (and this is a dumb server - no active platforms), then just use what was sent
	if (ActiveTargetPlatforms.Num() == 0)
	{
		ConnectedPlatformName = TargetPlatformNames[0];
	}
	// we only need to care about validating the connected platform if there are active targetplatforms
	else
	{
		// figure out the best matching target platform for the set of valid ones
		for (int32 TPIndex = 0; TPIndex < TargetPlatformNames.Num() && ConnectedPlatformName == TEXT(""); TPIndex++)
		{
			UE_LOG(LogFileServer, Display, TEXT("    Possible Target Platform from client: %s"), *TargetPlatformNames[TPIndex]);

			// look for a matching target platform
			for (int32 ActiveTPIndex = 0; ActiveTPIndex < ActiveTargetPlatforms.Num(); ActiveTPIndex++)
			{
				UE_LOG(LogFileServer, Display, TEXT("   Checking against: %s"), *ActiveTargetPlatforms[ActiveTPIndex]->PlatformName());
				if (ActiveTargetPlatforms[ActiveTPIndex]->PlatformName() == TargetPlatformNames[TPIndex])
				{
					bSendLowerCase = ActiveTargetPlatforms[ActiveTPIndex]->SendLowerCaseFilePaths();
					ConnectedPlatformName = ActiveTargetPlatforms[ActiveTPIndex]->PlatformName();
					break;
				}
			}
		}

		// if we didn't find one, reject client and also print some warnings
		if (ConnectedPlatformName == TEXT(""))
		{
			// reject client we can't cook/compile shaders for you!
			UE_LOG(LogFileServer, Warning, TEXT("Unable to find target platform for client, terminating client connection!"));

			for (int32 TPIndex = 0; TPIndex < TargetPlatformNames.Num() && ConnectedPlatformName == TEXT(""); TPIndex++)
			{
				UE_LOG(LogFileServer, Warning, TEXT("    Target platforms from client: %s"), *TargetPlatformNames[TPIndex]);
			}
			for (int32 ActiveTPIndex = 0; ActiveTPIndex < ActiveTargetPlatforms.Num(); ActiveTPIndex++)
			{
				UE_LOG(LogFileServer, Warning, TEXT("    Active target platforms on server: %s"), *ActiveTargetPlatforms[ActiveTPIndex]->PlatformName());
			}
			return false;
		}
	}

	ConnectedEngineDir = EngineRelativePath;
	ConnectedProjectDir = GameRelativePath;

	UE_LOG(LogFileServer, Display, TEXT("    Connected EngineDir = %s"), *ConnectedEngineDir);
	UE_LOG(LogFileServer, Display, TEXT("        Local EngineDir = %s"), *LocalEngineDir);
	UE_LOG(LogFileServer, Display, TEXT("    Connected ProjectDir = %s"), *ConnectedProjectDir);
	UE_LOG(LogFileServer, Display, TEXT("        Local ProjectDir = %s"), *LocalProjectDir);

	// Remap the root directories requested...
	for (int32 RootDirIdx = 0; RootDirIdx < RootDirectories.Num(); RootDirIdx++)
	{
		FString CheckRootDir = RootDirectories[RootDirIdx];
		ConvertClientFilenameToServerFilename(CheckRootDir);
		RootDirectories[RootDirIdx] = CheckRootDir;
	}

	// figure out the sandbox directory
	// @todo: This should use FPlatformMisc::SavedDirectory(GameName)
	FString SandboxDirectory;
	if (NetworkFileDelegates->SandboxPathOverrideDelegate.IsBound() )
	{
		SandboxDirectory = NetworkFileDelegates->SandboxPathOverrideDelegate.Execute();
		// if the sandbox directory delegate returns a path with the platform name in it then replace it :)
		SandboxDirectory.ReplaceInline(TEXT("[Platform]"), *ConnectedPlatformName);
	}
	else if ( FPaths::IsProjectFilePathSet() )
	{
		FString ProjectDir = FPaths::GetPath(FPaths::GetProjectFilePath());
		SandboxDirectory = FPaths::Combine(*ProjectDir, TEXT("Saved"), TEXT("Cooked"), *ConnectedPlatformName);

		// this is a workaround because the cooker and the networkfile server don't have access to eachother and therefore don't share the same Sandbox
		// the cooker in cook in editor saves to the EditorCooked directory
		if ( GIsEditor && !IsRunningCommandlet())
		{
			SandboxDirectory = FPaths::Combine(*ProjectDir, TEXT("Saved"), TEXT("EditorCooked"), *ConnectedPlatformName);
		}
		if( bIsStreamingRequest )
		{
			RootDirectories.Add(ProjectDir);
		}
	}
	else
	{
		if (FPaths::GetExtension(GameName) == FProjectDescriptor::GetExtension())
		{
			SandboxDirectory = FPaths::Combine(*FPaths::GetPath(GameName), TEXT("Saved"), TEXT("Cooked"), *ConnectedPlatformName);
		}
		else
		{
			//@todo: This assumes the game is located in the UE4 Root directory
			SandboxDirectory = FPaths::Combine(*FPaths::GetRelativePathToRoot(), *GameName, TEXT("Saved"), TEXT("Cooked"), *ConnectedPlatformName);
		}
	}
	// Convert to full path so that the sandbox wrapper doesn't re-base to Saved/Sandboxes
	SandboxDirectory = FPaths::ConvertRelativePathToFull(SandboxDirectory);

	// delete any existing one first, in case game name somehow changed and client is re-asking for files (highly unlikely)
	delete Sandbox;
	Sandbox = new FSandboxPlatformFile(false);
	Sandbox->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=\"%s\""), *SandboxDirectory));

	GetSandboxRootDirectories(Sandbox, SandboxEngine, SandboxProject, LocalEngineDir, LocalProjectDir);


	// make sure the global shaders are up to date before letting the client read any shaders
	// @todo: This will probably add about 1/2 second to the boot-up time of the client while the server does this
	// @note: We assume the delegate will write to the proper sandbox directory, should we pass in SandboxDirectory, or Sandbox?
	FShaderRecompileData RecompileData;
	RecompileData.PlatformName = ConnectedPlatformName;
	// All target platforms
	RecompileData.ShaderPlatform = -1;
	RecompileData.ModifiedFiles = NULL;
	RecompileData.MeshMaterialMaps = NULL;
	NetworkFileDelegates->RecompileShadersDelegate.ExecuteIfBound(RecompileData);

	UE_LOG(LogFileServer, Display, TEXT("Getting files for %d directories, game = %s, platform = %s"), RootDirectories.Num(), *GameName, *ConnectedPlatformName);
	UE_LOG(LogFileServer, Display, TEXT("    Sandbox dir = %s"), *SandboxDirectory);

	for (int32 DumpIdx = 0; DumpIdx < RootDirectories.Num(); DumpIdx++)
	{
		UE_LOG(LogFileServer, Display, TEXT("\t%s"), *(RootDirectories[DumpIdx]));
	}

	TArray<FString> DirectoriesToAlwaysStageAsUFS;
	if ( GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("DirectoriesToAlwaysStageAsUFS"), DirectoriesToAlwaysStageAsUFS, GGameIni) )
	{
		for ( const auto& DirectoryToAlwaysStage : DirectoriesToAlwaysStageAsUFS )
		{
			RootDirectories.Add( DirectoryToAlwaysStage );
		}
	}

	// list of directories to skip
	TArray<FString> DirectoriesToSkip;
	TArray<FString> DirectoriesToNotRecurse;
	// @todo: This should really be FPlatformMisc::GetSavedDirForGame(ClientGameName), etc
	for (int32 DirIndex = 0; DirIndex < RootDirectories.Num(); DirIndex++)
	{
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Saved/Backup")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Saved/Config")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Saved/Logs")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Saved/Sandboxes")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Saved/Cooked")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Saved/EditorCooked")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Saved/ShaderDebugInfo")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Saved/StagedBuilds")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Intermediate")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Documentation")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Extras")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Binaries")));
		DirectoriesToSkip.Add(FString(RootDirectories[DirIndex] / TEXT("Source")));
		DirectoriesToNotRecurse.Add(FString(RootDirectories[DirIndex] / TEXT("DerivedDataCache")));
	}

	// use the timestamp grabbing visitor (include directories)
	FLocalTimestampDirectoryVisitor Visitor(*Sandbox, DirectoriesToSkip, DirectoriesToNotRecurse, true);
	for (int32 DirIndex = 0; DirIndex < RootDirectories.Num(); DirIndex++)
	{
		Sandbox->IterateDirectory(*RootDirectories[DirIndex], Visitor);
	}

	// report the package version information
	// The downside of this is that ALL cooked data will get tossed on package version changes
	int32 PackageFileUE4Version = GPackageFileUE4Version;
	Out << PackageFileUE4Version;
	int32 PackageFileLicenseeUE4Version = GPackageFileLicenseeUE4Version;
	Out << PackageFileLicenseeUE4Version;

	// Send *our* engine and game dirs
	Out << LocalEngineDir;
	Out << LocalProjectDir;

	// return the files and their timestamps
	TMap<FString, FDateTime> FixedTimes = FixupSandboxPathsForClient(Visitor.FileTimes);
	Out << FixedTimes;

#if 0 // dump the list of files
	for ( const auto& FileTime : Visitor.FileTimes)
	{
		UE_LOG(LogFileServer, Display, TEXT("Server list of files  %s time %d"), *FileTime.Key, *FileTime.Value.ToString() );
	}
#endif
	// Do it again, preventing access to non-cooked files
	if( bIsStreamingRequest == false )
	{
		TArray<FString> RootContentPaths;
		FPackageName::QueryRootContentPaths(RootContentPaths); 
		TArray<FString> ContentFolders;
		for (const auto& RootPath : RootContentPaths)
		{
			const FString& ContentFolder = FPackageName::LongPackageNameToFilename(RootPath);

			FString ConnectedContentFolder = ContentFolder;
			ConnectedContentFolder.ReplaceInline(*LocalEngineDir, *ConnectedEngineDir);

			int32 ReplaceCount = 0;

			// If one path is relative and the other isn't, convert both to absolute paths before trying to replace
			if (FPaths::IsRelative(LocalProjectDir) != FPaths::IsRelative(ConnectedContentFolder))
			{
				FString AbsoluteLocalGameDir = FPaths::ConvertRelativePathToFull(LocalProjectDir);
				FString AbsoluteConnectedContentFolder = FPaths::ConvertRelativePathToFull(ConnectedContentFolder);
				ReplaceCount = AbsoluteConnectedContentFolder.ReplaceInline(*AbsoluteLocalGameDir, *ConnectedProjectDir);
				if (ReplaceCount > 0)
				{
					ConnectedContentFolder = AbsoluteConnectedContentFolder;
				}
			}
			else
			{
				ReplaceCount = ConnectedContentFolder.ReplaceInline(*LocalProjectDir, *ConnectedProjectDir);
			}
			
			if (ReplaceCount == 0)
			{
				int32 GameDirOffset = ConnectedContentFolder.Find(ConnectedProjectDir, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (GameDirOffset != INDEX_NONE)
				{
					ConnectedContentFolder = ConnectedContentFolder.RightChop(GameDirOffset);
				}
			}

			ContentFolders.Add(ConnectedContentFolder);
		}
		Out << ContentFolders;

		// Do it again, preventing access to non-cooked files
		const int32 NUM_EXCLUSION_WILDCARDS = 2;
		FString ExclusionWildcard[NUM_EXCLUSION_WILDCARDS];
		ExclusionWildcard[0] = FString(TEXT("*")) + FPackageName::GetAssetPackageExtension(); 
		ExclusionWildcard[1] = FString(TEXT("*")) + FPackageName::GetMapPackageExtension();

		for (int32 i=0; i < NUM_EXCLUSION_WILDCARDS; ++i)
		{
			Sandbox->AddExclusion(*ExclusionWildcard[i]);
			UE_LOG(LogFileServer, Display, TEXT("Excluding %s from non-sandboxed directories"), 
				   *ExclusionWildcard[i]);
		}
	
		FLocalTimestampDirectoryVisitor VisitorForCacheDates(*Sandbox, DirectoriesToSkip, DirectoriesToNotRecurse, true);

		for (int32 DirIndex = 0; DirIndex < RootDirectories.Num(); DirIndex++)
		{
			Sandbox->IterateDirectory(*RootDirectories[DirIndex], VisitorForCacheDates);
		}
	
		// return the cached files and their timestamps
		FixedTimes = FixupSandboxPathsForClient(VisitorForCacheDates.FileTimes);
		Out << FixedTimes;
	}



	if ( bIsPrecookedIterativeRequest )
	{
		TMap<FString, FDateTime> PrecookedList;
		NetworkFileDelegates->InitialPrecookedListDelegate.ExecuteIfBound(ConnectedPlatformName, PrecookedList);

		FixedTimes = FixupSandboxPathsForClient(PrecookedList);
		Out << FixedTimes;
	}

	return true;
}

void FNetworkFileServerClientConnection::FileModifiedCallback( const FString& Filename)
{
	FScopeLock Lock(&ModifiedFilesSection);

	// do we care about this file???

	// translation here?
	ModifiedFiles.AddUnique(Filename);
}

void FNetworkFileServerClientConnection::ProcessHeartbeat( FArchive& In, FArchive& Out )
{
	TArray<FString> FixedupModifiedFiles;
	// Protect the array
	if (Sandbox)
	{
		FScopeLock Lock(&ModifiedFilesSection);
		
		for (const auto& ModifiedFile : ModifiedFiles)
		{
			FixedupModifiedFiles.Add(FixupSandboxPathForClient(ModifiedFile));
		}
		ModifiedFiles.Empty();
	}
	// return the list of modified files
	Out << FixedupModifiedFiles;

	

	// @todo: note the last received time, and toss clients that don't heartbeat enough!

	// @todo: Right now, there is no directory watcher adding to ModifiedFiles. It had to be pulled from this thread (well, the ModuleManager part)
	// We should have a single directory watcher that pushes the changes to all the connections - or possibly pass in a shared DirectoryWatcher
	// and have each connection set up a delegate (see p4 history for HandleDirectoryWatcherDirectoryChanged)
}


/* FStreamingNetworkFileServerConnection callbacks
 *****************************************************************************/

bool FNetworkFileServerClientConnection::PackageFile( FString& Filename, FArchive& Out )
{
	// get file timestamp and send it to client
	FDateTime ServerTimeStamp = Sandbox->GetTimeStamp(*Filename);

	TArray<uint8> Contents;
	// open file
	IFileHandle* File = Sandbox->OpenRead(*Filename);


	if (!File)
	{
		++PackageRequestsFailed;

		UE_LOG(LogFileServer, Warning, TEXT("Opening file %s failed"), *Filename);
		ServerTimeStamp = FDateTime::MinValue(); // if this was a directory, this will make sure it is not confused with a zero byte file
	}
	else
	{
		++PackageRequestsSucceeded;

		if (!File->Size())
		{
			UE_LOG(LogFileServer, Warning, TEXT("Sending empty file %s...."), *Filename);
		}
		else
		{
			FileBytesSent += File->Size();
			// read it
			Contents.AddUninitialized(File->Size());
			File->Read(Contents.GetData(), Contents.Num());
		}

		// close it
		delete File;

		UE_LOG(LogFileServer, Display, TEXT("Read %s, %d bytes"), *Filename, Contents.Num());
	}

	Out << Filename;
	Out << ServerTimeStamp;
	uint64 FileSize = Contents.Num();
	Out << FileSize;
	Out.Serialize(Contents.GetData(), FileSize);
	return true;
}


void FNetworkFileServerClientConnection::ProcessRecompileShaders( FArchive& In, FArchive& Out )
{
	TArray<FString> RecompileModifiedFiles;
	TArray<uint8> MeshMaterialMaps;
	FShaderRecompileData RecompileData;
	RecompileData.PlatformName = ConnectedPlatformName;
	RecompileData.ModifiedFiles = &RecompileModifiedFiles;
	RecompileData.MeshMaterialMaps = &MeshMaterialMaps;

	// tell other side all the materials to load, by pathname
	In << RecompileData.MaterialsToLoad;
	In << RecompileData.ShaderPlatform;
	In << RecompileData.SerializedShaderResources;
	In << RecompileData.bCompileChangedShaders;

	NetworkFileDelegates->RecompileShadersDelegate.ExecuteIfBound(RecompileData);

	// tell other side what to do!
	Out << RecompileModifiedFiles;
	Out << MeshMaterialMaps;
}


void FNetworkFileServerClientConnection::ProcessSyncFile( FArchive& In, FArchive& Out )
{

	double StartTime;
	StartTime = FPlatformTime::Seconds();

	// get filename
	FString Filename;
	In << Filename;
	
	UE_LOG(LogFileServer, Verbose, TEXT("Try sync file %s"), *Filename);

	ConvertClientFilenameToServerFilename(Filename);
	
	//FString AbsFile(FString(*Sandbox->ConvertToAbsolutePathForExternalApp(*Filename)).MakeStandardFilename());
	// ^^ we probably in general want that filename, but for cook on the fly, we want the un-sandboxed name

	TArray<FString> NewUnsolictedFiles;

	NetworkFileDelegates->FileRequestDelegate.ExecuteIfBound(Filename, ConnectedPlatformName, NewUnsolictedFiles);

	FileRequestDelegateTime += 1000.0f * float(FPlatformTime::Seconds() - StartTime);
	StartTime = FPlatformTime::Seconds();
	

	for (int32 Index = 0; Index < NewUnsolictedFiles.Num(); Index++)
	{
		if (NewUnsolictedFiles[Index] != Filename)
		{
			UnsolictedFiles.AddUnique(NewUnsolictedFiles[Index]);
		}
	}

	PackageFile(Filename, Out);

	PackageFileTime += 1000.0f * float(FPlatformTime::Seconds() - StartTime);
}

FString FNetworkFileServerClientConnection::GetDescription() const 
{
	return FString("Client For " ) + ConnectedPlatformName;
}


bool FNetworkFileServerClientConnection::Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) 
{
	if (FParse::Command(&Cmd, TEXT("networkserverconnection")))
	{
		if (FParse::Command(&Cmd, TEXT("stats")))
		{

			Ar.Logf(TEXT("Network server connection %s stats\n"
				"FileRequestDelegateTime \t%fms \n"
				"PackageFileTime \t%fms \n"
				"UnsolicitedFilesTime \t%fms \n"
				"FileRequestCount \t%d \n"
				"UnsolicitedFilesCount \t%d \n"
				"PackageRequestsSucceeded \t%d \n"
				"PackageRequestsFailed \t%d \n"
				"FileBytesSent \t%d \n"),
				*GetDescription(),
				FileRequestDelegateTime,
				PackageFileTime,
				UnsolicitedFilesTime,
				FileRequestCount,
				UnsolicitedFilesCount,
				PackageRequestsSucceeded,
				PackageRequestsFailed,
				FileBytesSent);

			// there could be multiple network platform files so let them all report their stats
			return false;
		}
	}
	return false;
}

