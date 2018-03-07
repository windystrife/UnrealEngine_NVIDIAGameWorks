// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchFileConstructor.cpp: Implements the BuildPatchFileConstructor class
	that handles creating files in a manifest from the chunks that make it.
=============================================================================*/

#include "BuildPatchFileConstructor.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "BuildPatchServicesPrivate.h"
#include "Interfaces/IBuildInstaller.h"
#include "Data/ChunkData.h"
#include "Installer/ChunkSource.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/InstallerError.h"
#include "Installer/InstallerAnalytics.h"
#include "BuildPatchUtil.h"

using namespace BuildPatchServices;

// This define the number of bytes on a half-finished file that we ignore from the end
// incase of previous partial write.
#define NUM_BYTES_RESUME_IGNORE     1024

// Helper functions wrapping common code.
namespace FileConstructorHelpers
{
	void WaitWhilePaused(FThreadSafeBool& bIsPaused, FThreadSafeBool& bShouldAbort)
	{
		// Wait while paused
		while (bIsPaused && !bShouldAbort)
		{
			FPlatformProcess::Sleep(0.5f);
		}
	}

	bool CheckAndReportRemainingDiskSpaceError(IInstallerError* InstallerError, const FString& InstallDirectory, uint64 RemainingBytesRequired)
	{
		bool bContinueConstruction = true;
		uint64 TotalSize = 0;
		uint64 AvailableSpace = 0;
		if (FPlatformMisc::GetDiskTotalAndFreeSpace(InstallDirectory, TotalSize, AvailableSpace))
		{
			if (AvailableSpace < RemainingBytesRequired)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Out of HDD space. Needs %llu bytes, Free %llu bytes"), RemainingBytesRequired, AvailableSpace);
				InstallerError->SetError(
					EBuildPatchInstallError::OutOfDiskSpace,
					DiskSpaceErrorCodes::InitialSpaceCheck,
					BuildPatchServices::GetDiskSpaceMessage(InstallDirectory, RemainingBytesRequired, AvailableSpace));
				bContinueConstruction = false;
			}
		}
		return bContinueConstruction;
	}
}

/**
 * This struct handles loading and saving of simple resume information, that will allow us to decide which
 * files should be resumed from. It will also check that we are creating the same version and app as we expect to be.
 */
struct FResumeData
{
public:
	// Save the staging directory
	const FString StagingDir;

	// The filename to the resume data information
	const FString ResumeDataFile;

	// A string determining the app and version we are installing
	const FString PatchVersion;

	// The set of files that were started
	TSet<FString> FilesStarted;

	// The set of files that were completed, determined by expected filesize
	TSet<FString> FilesCompleted;

	// The manifest for the app we are installing
	FBuildPatchAppManifestRef BuildManifest;

	// Whether we have resume data for this install
	bool bHasResumeData;

	// Whether we have resume data for a different install.
	bool bHasIncompatibleResumeData;

public:
	/**
	 * Constructor - reads in the resume data
	 * @param InStagingDir      The install staging directory
	 * @param InBuildManifest   The manifest we are installing from
	 */
	FResumeData(const FString& InStagingDir, const FBuildPatchAppManifestRef& InBuildManifest)
		: StagingDir(InStagingDir)
		, ResumeDataFile(InStagingDir / TEXT("$resumeData"))
		, PatchVersion(InBuildManifest->GetAppName() + InBuildManifest->GetVersionString())
		, BuildManifest(InBuildManifest)
		, bHasResumeData(false)
		, bHasIncompatibleResumeData(false)
	{
		// Load data from previous resume file
		bHasResumeData = FPlatformFileManager::Get().GetPlatformFile().FileExists(*ResumeDataFile);
		GLog->Logf(TEXT("BuildPatchResumeData file found %d"), bHasResumeData);
		if (bHasResumeData)
		{
			FString PrevResumeData;
			TArray< FString > ResumeDataLines;
			FFileHelper::LoadFileToString(PrevResumeData, *ResumeDataFile);
			PrevResumeData.ParseIntoArray(ResumeDataLines, TEXT("\n"), true);
			// Line 1 will be the previously attempted version
			FString PreviousVersion = (ResumeDataLines.Num() > 0) ? MoveTemp(ResumeDataLines[0]) : TEXT("");
			bHasResumeData = PreviousVersion == PatchVersion;
			bHasIncompatibleResumeData = !bHasResumeData;
			GLog->Logf(TEXT("BuildPatchResumeData version matched %d %s == %s"), bHasResumeData, *PreviousVersion, *PatchVersion);
		}
	}

	/**
	 * Saves out the resume data
	 */
	void SaveOut()
	{
		// Save out the patch version
		FFileHelper::SaveStringToFile(PatchVersion + TEXT("\n"), *ResumeDataFile);
	}

	/**
	 * Checks whether the file was completed during last install attempt and adds it to FilesCompleted if so
	 * @param Filename    The filename to check
	 */
	void CheckFile( const FString& Filename )
	{
		// If we had resume data, check file size is correct
		if(bHasResumeData)
		{
			const FString FullFilename = StagingDir / Filename;
			const int64 DiskFileSize = IFileManager::Get().FileSize( *FullFilename );
			const int64 CompleteFileSize = BuildManifest->GetFileSize( Filename );
			if (DiskFileSize > 0 && DiskFileSize <= CompleteFileSize)
			{
				FilesStarted.Add(Filename);
			}
			if( DiskFileSize == CompleteFileSize )
			{
				FilesCompleted.Add( Filename );
			}
		}
	}
};

/* FBuildPatchFileConstructor implementation
 *****************************************************************************/
FBuildPatchFileConstructor::FBuildPatchFileConstructor(FBuildPatchAppManifestRef InBuildManifest, FString InInstallDirectory, FString InStageDirectory, TArray<FString> InConstructList, IChunkSource* InChunkSource, IChunkReferenceTracker* InChunkReferenceTracker, IInstallerError* InInstallerError, IInstallerAnalytics* InInstallerAnalytics, IFileConstructorStat* InFileConstructorStat)
	: Thread(nullptr)
	, bIsRunning(false)
	, bIsInited(false)
	, bInitFailed(false)
	, bIsDownloadStarted(false)
	, bInitialDiskSizeCheck(false)
	, bIsPaused(false)
	, bShouldAbort(false)
	, ThreadLock()
	, BuildManifest(MoveTemp(InBuildManifest))
	, InstallDirectory(MoveTemp(InInstallDirectory))
	, StagingDirectory(MoveTemp(InStageDirectory))
	, FilesToConstruct(MoveTemp(InConstructList))
	, ChunkSource(InChunkSource)
	, ChunkReferenceTracker(InChunkReferenceTracker)
	, InstallerError(InInstallerError)
	, InstallerAnalytics(InInstallerAnalytics)
	, FileConstructorStat(InFileConstructorStat)
	, TotalJobSize(0)
	, ByteProcessed(0)
	, FilesConstructed()
{
	// Count initial job size
	for (const FString& FileToConstruct : FilesToConstruct)
	{
		TotalJobSize += BuildManifest->GetFileSize(*FileToConstruct);
	}
	// Start thread!
	const TCHAR* ThreadName = TEXT("FileConstructorThread");
	Thread = FRunnableThread::Create(this, ThreadName);
}

FBuildPatchFileConstructor::~FBuildPatchFileConstructor()
{
	// Wait for and deallocate the thread
	if( Thread != nullptr )
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool FBuildPatchFileConstructor::Init()
{
	// We are ready to go if our delegates are bound and directories successfully created
	bool bStageDirExists = IFileManager::Get().DirectoryExists(*StagingDirectory);
	if (!bStageDirExists)
	{
		UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Stage directory missing %s"), *StagingDirectory);
		InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingStageDirectory);
	}
	SetInitFailed(!bStageDirExists);
	return bStageDirExists;
}

uint32 FBuildPatchFileConstructor::Run()
{
	SetRunning( true );
	SetInited( true );
	const bool bIsFileData = BuildManifest->IsFileDataManifest();
	FileConstructorStat->OnTotalRequiredUpdated(TotalJobSize);

	// Save the list of completed files
	TArray< FString > ConstructedFiles;

	// Check for resume data
	FResumeData ResumeData( StagingDirectory, BuildManifest );

	// If we found incompatible resume data, we need to clean out the staging folder
	// We don't delete the folder itself though as we should presume it was created with desired attributes
	if (ResumeData.bHasIncompatibleResumeData)
	{
		GLog->Logf(TEXT("BuildPatchServices: Deleting incompatible stage files"));
		DeleteDirectoryContents(StagingDirectory);
	}

	// Save for started version
	ResumeData.SaveOut();

	// Start resume progress at zero or one
	FileConstructorStat->OnResumeStarted();

	// While we have files to construct, run
	FString FileToConstruct;
	while( GetFileToConstruct( FileToConstruct ) && !bShouldAbort)
	{
		int64 FileSize = BuildManifest->GetFileSize(FileToConstruct);
		FileConstructorStat->OnFileStarted(FileToConstruct, FileSize);
		// Check resume status, currently we are only supporting sequential resume, so once we start downloading, we can't resume any more.
		// this only comes up if the resume data has been changed externally.
		ResumeData.CheckFile( FileToConstruct );
		const bool bFilePreviouslyComplete = !bIsDownloadStarted && ResumeData.FilesCompleted.Contains( FileToConstruct );
		const bool bFilePreviouslyStarted = !bIsDownloadStarted && ResumeData.FilesStarted.Contains( FileToConstruct );

		// Construct or skip the file
		bool bFileSuccess;
		if (bFilePreviouslyComplete)
		{
			bFileSuccess = true;
			CountBytesProcessed(FileSize);
			GLog->Logf(TEXT("FBuildPatchFileConstructor::SkipFile %s"), *FileToConstruct);
			// Get the file manifest.
			const FFileManifestData* FileManifest = BuildManifest->GetFileManifest(FileToConstruct);
			// Go through each chunk part, and dereference it from the reference tracker.
			for (const FChunkPartData& ChunkPart : FileManifest->FileChunkParts)
			{
				bFileSuccess = ChunkReferenceTracker->PopReference(ChunkPart.Guid) && bFileSuccess;
			}
		}
		else
		{
			bFileSuccess = ConstructFileFromChunks(FileToConstruct, bFilePreviouslyStarted);
		}

		// If the file succeeded, add to lists
		if( bFileSuccess )
		{
			ConstructedFiles.Add( FileToConstruct );
		}
		else
		{
			// This will only record and log if a failure was not already registered
			bShouldAbort = true;
			InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::UnknownFail);
		}
		FileConstructorStat->OnFileCompleted(FileToConstruct, bFileSuccess);

		// Wait while paused
		FileConstructorHelpers::WaitWhilePaused(bIsPaused, bShouldAbort);
	}

	// Mark resume complete if we didn't have work to do.
	if (!bIsDownloadStarted)
	{
		FileConstructorStat->OnResumeCompleted();
	}

	// Set constructed files
	ThreadLock.Lock();
	FilesConstructed = MoveTemp(ConstructedFiles);
	ThreadLock.Unlock();

	SetRunning( false );
	return 0;
}

void FBuildPatchFileConstructor::Wait()
{
	if( Thread != nullptr )
	{
		Thread->WaitForCompletion();
	}
}

bool FBuildPatchFileConstructor::IsComplete()
{
	FScopeLock Lock( &ThreadLock );
	return ( !bIsRunning && bIsInited ) || bInitFailed;
}

void FBuildPatchFileConstructor::SetRunning( bool bRunning )
{
	FScopeLock Lock( &ThreadLock );
	bIsRunning = bRunning;
}

void FBuildPatchFileConstructor::SetInited( bool bInited )
{
	FScopeLock Lock( &ThreadLock );
	bIsInited = bInited;
}

void FBuildPatchFileConstructor::SetInitFailed( bool bFailed )
{
	FScopeLock Lock( &ThreadLock );
	bInitFailed = bFailed;
}

void FBuildPatchFileConstructor::CountBytesProcessed( const int64& ByteCount )
{
	ByteProcessed += ByteCount;
	FileConstructorStat->OnProcessedDataUpdated(ByteProcessed);
}

bool FBuildPatchFileConstructor::GetFileToConstruct(FString& Filename)
{
	FScopeLock Lock(&ThreadLock);
	const bool bFileAvailable = FilesToConstruct.IsValidIndex(0);
	if (bFileAvailable)
	{
		Filename = FilesToConstruct[0];
		FilesToConstruct.RemoveAt(0);
	}
	return bFileAvailable;
}

int64 FBuildPatchFileConstructor::GetRemainingBytes()
{
	FScopeLock Lock(&ThreadLock);
	return BuildManifest->GetFileSize(FilesToConstruct);
}

bool FBuildPatchFileConstructor::ConstructFileFromChunks( const FString& Filename, bool bResumeExisting )
{
	const bool bIsFileData = BuildManifest->IsFileDataManifest();
	bResumeExisting = bResumeExisting && !bIsFileData;
	bool bSuccess = true;
	FString NewFilename = StagingDirectory / Filename;

	// Calculate the hash as we write the data
	FSHA1 HashState;
	FSHAHashData HashValue;

	// First make sure we can get the file manifest
	const FFileManifestData* FileManifest = BuildManifest->GetFileManifest(Filename);
	bSuccess = FileManifest != nullptr;
	if( bSuccess )
	{
		if( !FileManifest->SymlinkTarget.IsEmpty() )
		{
#if PLATFORM_MAC
			bSuccess = symlink(TCHAR_TO_UTF8(*FileManifest->SymlinkTarget), TCHAR_TO_UTF8(*NewFilename)) == 0;
#else
			const bool bSymlinkNotImplemented = false;
			check(bSymlinkNotImplemented);
			bSuccess = false;
#endif
			return bSuccess;
		}

		// Check for resuming of existing file
		int64 StartPosition = 0;
		int32 StartChunkPart = 0;
		if (bResumeExisting)
		{
			// We have to read in the existing file so that the hash check can still be done.
			TUniquePtr<FArchive> NewFileReader(IFileManager::Get().CreateFileReader(*NewFilename));
			if (NewFileReader.IsValid())
			{
				// Read buffer
				TArray<uint8> ReadBuffer;
				ReadBuffer.Empty(BuildPatchServices::ChunkDataSize);
				ReadBuffer.SetNumUninitialized(BuildPatchServices::ChunkDataSize);
				// Reuse a certain amount of the file
				StartPosition = FMath::Max<int64>(0, NewFileReader->TotalSize() - NUM_BYTES_RESUME_IGNORE);
				// We'll also find the correct chunkpart to start writing from
				int64 ByteCounter = 0;
				for (int32 ChunkPartIdx = StartChunkPart; ChunkPartIdx < FileManifest->FileChunkParts.Num() && !bShouldAbort; ++ChunkPartIdx)
				{
					const FChunkPartData& ChunkPart = FileManifest->FileChunkParts[ChunkPartIdx];
					const int64 NextBytePosition = ByteCounter + ChunkPart.Size;
					if (NextBytePosition <= StartPosition)
					{
						// Read data for hash check
						NewFileReader->Serialize(ReadBuffer.GetData(), ChunkPart.Size);
						HashState.Update(ReadBuffer.GetData(), ChunkPart.Size);
						// Count bytes read from file
						ByteCounter = NextBytePosition;
						// Set to resume from next chunk part
						StartChunkPart = ChunkPartIdx + 1;
						// Inform the reference tracker of the chunk part skip
						bSuccess = ChunkReferenceTracker->PopReference(ChunkPart.Guid) && bSuccess;
						CountBytesProcessed(ChunkPart.Size);
						// Wait if paused
						FileConstructorHelpers::WaitWhilePaused(bIsPaused, bShouldAbort);
					}
					else
					{
						// No more parts on disk
						break;
					}
				}
				// Set start position to the byte we got up to
				StartPosition = ByteCounter;
				// Close file
				NewFileReader->Close();
			}
		}

		// If we haven't done so yet, make the initial disk space check
		if (!bInitialDiskSizeCheck)
		{
			bInitialDiskSizeCheck = true;
			const uint64 RequiredSpace = (FileManifest->GetFileSize() - StartPosition) + GetRemainingBytes();
			if (!FileConstructorHelpers::CheckAndReportRemainingDiskSpaceError(InstallerError, InstallDirectory, RequiredSpace))
			{
				return false;
			}
		}

		// Now we can make sure the chunk cache knows to start downloading chunks
		if (!bIsDownloadStarted)
		{
			bIsDownloadStarted = true;
			FileConstructorStat->OnResumeCompleted();
		}

		// Attempt to create the file
		FArchive* NewFile = IFileManager::Get().CreateFileWriter( *NewFilename, bResumeExisting ? ::EFileWrite::FILEWRITE_Append : 0 );
		uint32 LastError = FPlatformMisc::GetLastError();
		bSuccess = NewFile != nullptr;
		if( bSuccess )
		{
			// Seek to file write position
			NewFile->Seek( StartPosition );

			// For each chunk, load it, and place it's data into the file
			for( int32 ChunkPartIdx = StartChunkPart; ChunkPartIdx < FileManifest->FileChunkParts.Num() && bSuccess && !bShouldAbort; ++ChunkPartIdx )
			{
				const FChunkPartData& ChunkPart = FileManifest->FileChunkParts[ChunkPartIdx];
				bSuccess = InsertChunkData( ChunkPart, *NewFile, HashState );
				FileConstructorStat->OnFileProgress(Filename, NewFile->Tell());
				if( bSuccess )
				{
					CountBytesProcessed(ChunkPart.Size);
					// Wait while paused
					FileConstructorHelpers::WaitWhilePaused(bIsPaused, bShouldAbort);
				}
				else
				{
					// Only report or log if the first error
					if (InstallerError->HasError() == false)
					{
						InstallerAnalytics->RecordConstructionError(Filename, INDEX_NONE, TEXT("Missing Chunk"));
						UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed %s due to chunk %s"), *Filename, *ChunkPart.Guid.ToString());
					}
					InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingChunkData);
				}
			}

			// Close the file writer
			NewFile->Close();
			delete NewFile;
		}
		else
		{
			// Check if drive space was the issue here
			const uint64 RequiredSpace = FileManifest->GetFileSize() + GetRemainingBytes();
			bool bError = !FileConstructorHelpers::CheckAndReportRemainingDiskSpaceError(InstallerError, InstallDirectory, RequiredSpace);

			// Otherwise we just couldn't make the file
			if (!bError)
			{
				// Only report or log if the first error
				if (InstallerError->HasError() == false)
				{
					InstallerAnalytics->RecordConstructionError(Filename, LastError, TEXT("Could Not Create File"));
					UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Could not create %s"), *Filename);
				}
				// Always set
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::FileCreateFail);
			}
		}
	}
	else
	{
		// Only report or log if the first error
		if (InstallerError->HasError() == false)
		{
			InstallerAnalytics->RecordConstructionError(Filename, INDEX_NONE, TEXT("Missing File Manifest"));
			UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Missing file manifest for %s"), *Filename);
		}
		// Always set
		InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingFileInfo);
	}

	// Verify the hash for the file that we created
	if( bSuccess )
	{
		HashState.Final();
		HashState.GetHash( HashValue.Hash );
		bSuccess = HashValue == FileManifest->FileHash;
		if( !bSuccess )
		{
			// Only report or log if the first error
			if (InstallerError->HasError() == false)
			{
				InstallerAnalytics->RecordConstructionError(Filename, INDEX_NONE, TEXT("Serialised Verify Fail"));
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Verify failed after constructing %s"), *Filename);
			}
			// Always set
			InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::OutboundCorrupt);
		}
	}

#if PLATFORM_MAC
	if( bSuccess && FileManifest->bIsUnixExecutable )
	{
		// Enable executable permission bit
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NewFilename), &FileInfo) == 0)
		{
			bSuccess = chmod(TCHAR_TO_UTF8(*NewFilename), FileInfo.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) == 0;
		}
	}
#endif
	
#if PLATFORM_ANDROID || PLATFORM_ANDROIDESDEFERRED
	if (bSuccess)
	{
		IFileManager::Get().SetTimeStamp(*NewFilename, FDateTime::UtcNow());
	}
#endif

	// Delete the staging file if unsuccessful by means of construction fail (i.e. keep if canceled or download issue)
	if( !bSuccess && InstallerError->GetErrorType() == EBuildPatchInstallError::FileConstructionFail )
	{
		IFileManager::Get().Delete( *NewFilename, false, true );
	}

	return bSuccess;
}

bool FBuildPatchFileConstructor::InsertChunkData(const FChunkPartData& ChunkPart, FArchive& DestinationFile, FSHA1& HashState)
{
	uint8* Data;
	uint8* DataStart;
	IChunkDataAccess* ChunkDataAccess = ChunkSource->Get(ChunkPart.Guid);
	bool bSuccess = ChunkDataAccess != nullptr && !bShouldAbort;
	if (bSuccess)
	{
		ChunkDataAccess->GetDataLock(&Data, nullptr);
		DataStart = &Data[ChunkPart.Offset];
		HashState.Update(DataStart, ChunkPart.Size);
		DestinationFile.Serialize(DataStart, ChunkPart.Size);
		ChunkDataAccess->ReleaseDataLock();
		bSuccess = ChunkReferenceTracker->PopReference(ChunkPart.Guid);
	}
	return bSuccess;
}

void FBuildPatchFileConstructor::DeleteDirectoryContents(const FString& RootDirectory)
{
	TArray<FString> SubDirNames;
	IFileManager::Get().FindFiles(SubDirNames, *(RootDirectory / TEXT("*")), false, true);
	for (const FString& DirName : SubDirNames)
	{
		IFileManager::Get().DeleteDirectory(*(RootDirectory / DirName), false, true);
	}

	TArray<FString> SubFileNames;
	IFileManager::Get().FindFiles(SubFileNames, *(RootDirectory / TEXT("*")), true, false);
	for (const FString& FileName : SubFileNames)
	{
		IFileManager::Get().Delete(*(RootDirectory / FileName), false, true);
	}
}
