// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ShaderCompiler.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"

#if PLATFORM_WINDOWS

// --------------------------------------------------------------------------
//                          Legacy XGE Xml interface                         
// --------------------------------------------------------------------------
namespace XGEShaderCompilerVariables
{
	int32 Enabled = 0;
	FAutoConsoleVariableRef CVarXGEShaderCompile(
		TEXT("r.XGEShaderCompile"),
		Enabled,
		TEXT("Enables or disables the use of XGE to build shaders.\n")
		TEXT("0: Local builds only. \n")
		TEXT("1: Distribute builds using XGE (default)."),
		ECVF_Default);

	int32 Mode = 1;
	FAutoConsoleVariableRef CVarXGEShaderCompileMode(
		TEXT("r.XGEShaderCompile.Mode"),
		Mode,
		TEXT("Selects which dispatch mode to use.\n")
		TEXT("0: Use legacy xml dispatch mode. (default)\n")
		TEXT("1: Prefer interception mode if available (requires XGE controller support). Falls back to legacy mode otherwise.\n")
		TEXT("2: Force interception mode. Disables XGE shader compiling if XGE controller is not available.\n"),
		ECVF_Default);

	/** Xml Interface specific CVars */

	/** The maximum number of shaders to group into a single XGE task. */
	int32 BatchSize = 16;
	FAutoConsoleVariableRef CVarXGEShaderCompileBatchSize(
		TEXT("r.XGEShaderCompile.Xml.BatchSize"),
		BatchSize,
		TEXT("Specifies the number of shaders to batch together into a single XGE task.\n")
		TEXT("Default = 16\n"),
		ECVF_Default);

	/** The total number of batches to fill with shaders before creating another group of batches. */
	int32 BatchGroupSize = 128;
	FAutoConsoleVariableRef CVarXGEShaderCompileBatchGroupSize(
		TEXT("r.XGEShaderCompile.Xml.BatchGroupSize"),
		BatchGroupSize,
		TEXT("Specifies the number of batches to fill with shaders.\n")
		TEXT("Shaders are spread across this number of batches until all the batches are full.\n")
		TEXT("This allows the XGE compile to go wider when compiling a small number of shaders.\n")
		TEXT("Default = 128\n"),
		ECVF_Default);

	/**
	* The number of seconds to wait after a job is submitted before kicking off the XGE process.
	* This allows time for the engine to enqueue more shaders, so we get better batching.
	*/
	float JobTimeout = 0.5f;
	FAutoConsoleVariableRef CVarXGEShaderCompileJobTimeout(
		TEXT("r.XGEShaderCompile.Xml.JobTimeout"),
		JobTimeout,
		TEXT("The number of seconds to wait for additional shader jobs to be submitted before starting a build.\n")
		TEXT("Default = 0.5\n"),
		ECVF_Default);

	void Init()
	{
		static bool bInitialized = false;
		if (!bInitialized)
		{
			// Allow command line to override the value of the console variables.
			if (FParse::Param(FCommandLine::Get(), TEXT("xgeshadercompile")))
			{
				XGEShaderCompilerVariables::Enabled = 1;
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("noxgeshadercompile")))
			{
				XGEShaderCompilerVariables::Enabled = 0;
			}

			bInitialized = true;
		}
	}
}

static FString XGE_ConsolePath;
static const FString XGE_ScriptFileName(TEXT("xgscript.xml"));
static const FString XGE_InputFileName(TEXT("WorkerInput.in"));
static const FString XGE_OutputFileName(TEXT("WorkerOutput.out"));
static const FString XGE_SuccessFileName(TEXT("Success"));

bool FShaderCompileXGEThreadRunnable_XmlInterface::IsSupported()
{
	// List of possible paths to xgconsole.exe
	static const TCHAR* Paths[] =
	{
		TEXT("C:\\Program Files\\Xoreax\\IncrediBuild\\xgConsole.exe"),
		TEXT("C:\\Program Files (x86)\\Xoreax\\IncrediBuild\\xgConsole.exe")
	};

	XGEShaderCompilerVariables::Init();

	// Check for a valid installation of Incredibuild by seeing if xgconsole.exe exists.
	bool bXgeFound = false;
	if (XGEShaderCompilerVariables::Enabled == 1)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		for (int PathIndex = 0; PathIndex < ARRAY_COUNT(Paths); PathIndex++)
		{
			if (PlatformFile.FileExists(Paths[PathIndex]))
			{
				bXgeFound = true;
				XGE_ConsolePath = Paths[PathIndex];
				break;
			}
		}

		if (!bXgeFound)
		{
			UE_LOG(LogShaderCompilers, Warning, TEXT("Cannot use XGE Shader Compiler as Incredibuild is not installed on this machine."));
		}
	}

	return 
		(XGEShaderCompilerVariables::Enabled == 1) && // XGE is enabled by CVar or command line.
		(XGEShaderCompilerVariables::Mode    != 2) && // XGE xml mode is allowed (not force disabled).
		bXgeFound;                             // We've found the xgConsole executable.
}

/** Initialization constructor. */
FShaderCompileXGEThreadRunnable_XmlInterface::FShaderCompileXGEThreadRunnable_XmlInterface(class FShaderCompilingManager* InManager)
	: FShaderCompileThreadRunnableBase(InManager)
	, BuildProcessID(INDEX_NONE)
	, XGEWorkingDirectory(InManager->AbsoluteShaderBaseWorkingDirectory / TEXT("XGE"))
	, XGEDirectoryIndex(0)
	, LastAddTime(0)
	, StartTime(0)
	, BatchIndexToCreate(0)
	, BatchIndexToFill(0)
{
}

FShaderCompileXGEThreadRunnable_XmlInterface::~FShaderCompileXGEThreadRunnable_XmlInterface()
{
	if (BuildProcessHandle.IsValid())
	{
		// We still have a build in progress.
		// Kill it...
		FPlatformProcess::TerminateProc(BuildProcessHandle);
		FPlatformProcess::CloseProc(BuildProcessHandle);
	}

	// Clean up any intermediate files/directories we've got left over.
	IFileManager::Get().DeleteDirectory(*XGEWorkingDirectory, false, true);

	// Delete all the shader batch instances we have.
	for (FShaderBatch* Batch : ShaderBatchesIncomplete)
		delete Batch;

	for (FShaderBatch* Batch : ShaderBatchesInFlight)
		delete Batch;

	for (FShaderBatch* Batch : ShaderBatchesFull)
		delete Batch;

	ShaderBatchesIncomplete.Empty();
	ShaderBatchesInFlight.Empty();
	ShaderBatchesFull.Empty();
}

static FArchive* CreateFileHelper(const FString& Filename)
{
	// TODO: This logic came from FShaderCompileThreadRunnable::WriteNewTasks().
	// We can't avoid code duplication unless we refactored the local worker too.

	FArchive* File = nullptr;
	int32 RetryCount = 0;
	// Retry over the next two seconds if we can't write out the file.
	// Anti-virus and indexing applications can interfere and cause this to fail.
	while (File == nullptr && RetryCount < 200)
	{
		if (RetryCount > 0)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		File = IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_EvenIfReadOnly);
		RetryCount++;
	}
	if (File == nullptr)
	{
		File = IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_EvenIfReadOnly | FILEWRITE_NoFail);
	}
	checkf(File, TEXT("Failed to create file %s!"), *Filename);
	return File;
}

static void MoveFileHelper(const FString& To, const FString& From)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.FileExists(*From))
	{
		FString DirectoryName;
		int32 LastSlashIndex;
		if (To.FindLastChar('/', LastSlashIndex))
		{
			DirectoryName = To.Left(LastSlashIndex);
		}
		else
		{
			DirectoryName = To;
		}

		// TODO: This logic came from FShaderCompileThreadRunnable::WriteNewTasks().
		// We can't avoid code duplication unless we refactored the local worker too.

		bool Success = false;
		int32 RetryCount = 0;
		// Retry over the next two seconds if we can't move the file.
		// Anti-virus and indexing applications can interfere and cause this to fail.
		while (!Success && RetryCount < 200)
		{
			if (RetryCount > 0)
			{
				FPlatformProcess::Sleep(0.01f);
			}

			// MoveFile does not create the directory tree, so try to do that now...
			Success = PlatformFile.CreateDirectoryTree(*DirectoryName);
			if (Success)
			{
				Success = PlatformFile.MoveFile(*To, *From);
			}
			RetryCount++;
		}
		checkf(Success, TEXT("Failed to move file %s to %s!"), *From, *To);
	}
}

static void DeleteFileHelper(const FString& Filename)
{
	// TODO: This logic came from FShaderCompileThreadRunnable::WriteNewTasks().
	// We can't avoid code duplication unless we refactored the local worker too.

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*Filename))
	{
		bool bDeletedOutput = IFileManager::Get().Delete(*Filename, true, true);

		// Retry over the next two seconds if we couldn't delete it
		int32 RetryCount = 0;
		while (!bDeletedOutput && RetryCount < 200)
		{
			FPlatformProcess::Sleep(0.01f);
			bDeletedOutput = IFileManager::Get().Delete(*Filename, true, true);
			RetryCount++;
		}
		checkf(bDeletedOutput, TEXT("Failed to delete %s!"), *Filename);
	}
}

void FShaderCompileXGEThreadRunnable_XmlInterface::PostCompletedJobsForBatch(FShaderBatch* Batch)
{
	// Enter the critical section so we can access the input and output queues
	FScopeLock Lock(&Manager->CompileQueueSection);
	for (FShaderCommonCompileJob* Job : Batch->GetJobs())
	{
		FShaderMapCompileResults& ShaderMapResults = Manager->ShaderMapJobs.FindChecked(Job->Id);
		ShaderMapResults.FinishedJobs.Add(Job);
		ShaderMapResults.bAllJobsSucceeded = ShaderMapResults.bAllJobsSucceeded && Job->bSucceeded;
	}

	// Using atomics to update NumOutstandingJobs since it is read outside of the critical section
	FPlatformAtomics::InterlockedAdd(&Manager->NumOutstandingJobs, -Batch->NumJobs());
}

void FShaderCompileXGEThreadRunnable_XmlInterface::FShaderBatch::AddJob(FShaderCommonCompileJob* Job)
{
	// We can only add jobs to a batch which hasn't been written out yet.
	if (bTransferFileWritten)
	{
		UE_LOG(LogShaderCompilers, Fatal, TEXT("Attempt to add shader compile jobs to an XGE shader batch which has already been written to disk."));
	}
	else
	{
		Jobs.Add(Job);
	}
}

void FShaderCompileXGEThreadRunnable_XmlInterface::FShaderBatch::WriteTransferFile()
{
	// Write out the file that the worker app is waiting for, which has all the information needed to compile the shader.
	FArchive* TransferFile = CreateFileHelper(InputFileNameAndPath);
	FShaderCompileUtilities::DoWriteTasks(Jobs, *TransferFile);
	delete TransferFile;

	bTransferFileWritten = true;
}

void FShaderCompileXGEThreadRunnable_XmlInterface::FShaderBatch::SetIndices(int32 InDirectoryIndex, int32 InBatchIndex)
{
	DirectoryIndex = InDirectoryIndex;
	BatchIndex = InBatchIndex;

	WorkingDirectory = FString::Printf(TEXT("%s/%d/%d"), *DirectoryBase, DirectoryIndex, BatchIndex);

	InputFileNameAndPath = WorkingDirectory / InputFileName;
	OutputFileNameAndPath = WorkingDirectory / OutputFileName;
	SuccessFileNameAndPath = WorkingDirectory / SuccessFileName;
}

void FShaderCompileXGEThreadRunnable_XmlInterface::FShaderBatch::CleanUpFiles(bool keepInputFile)
{
	if (!keepInputFile)
	{
		DeleteFileHelper(InputFileNameAndPath);
	}

	DeleteFileHelper(OutputFileNameAndPath);
	DeleteFileHelper(SuccessFileNameAndPath);
}

static void WriteScriptFileHeader(FArchive* ScriptFile, const FString& WorkerName)
{
	static const TCHAR HeaderTemplate[] =
		TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\r\n")
		TEXT("<BuildSet FormatVersion=\"1\">\r\n")
		TEXT("\t<Environments>\r\n")
		TEXT("\t\t<Environment Name=\"Default\">\r\n")
		TEXT("\t\t\t<Tools>\r\n")
		TEXT("\t\t\t\t<Tool Name=\"ShaderCompiler\" Path=\"%s\" OutputFileMasks=\"%s,%s\" AllowRemote=\"true\" AllowRestartOnLocal=\"true\" />\r\n")
		TEXT("\t\t\t</Tools>\r\n")
		TEXT("\t\t</Environment>\r\n")
		TEXT("\t</Environments>\r\n")
		TEXT("\t<Project Env=\"Default\" Name=\"Shader Compilation Project\">\r\n")
		TEXT("\t\t<TaskGroup Name=\"Compiling Shaders\" Tool=\"ShaderCompiler\">\r\n");

	FString HeaderXML = FString::Printf(HeaderTemplate, *WorkerName, *XGE_OutputFileName, *XGE_SuccessFileName);
	ScriptFile->Serialize((void*)StringCast<ANSICHAR>(*HeaderXML, HeaderXML.Len()).Get(), sizeof(ANSICHAR) * HeaderXML.Len());
}

static void WriteScriptFileFooter(FArchive* ScriptFile)
{
	static const ANSICHAR HeaderFooter[] =
		"\t\t</TaskGroup>\r\n"
		"\t</Project>\r\n"
		"</BuildSet>\r\n";

	ScriptFile->Serialize((void*)HeaderFooter, sizeof(ANSICHAR) * (ARRAY_COUNT(HeaderFooter) - 1));
}

void FShaderCompileXGEThreadRunnable_XmlInterface::GatherResultsFromXGE()
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileManager& FileManager = IFileManager::Get();

	// Reverse iterate so we can remove batches that have completed as we go.
	for (int32 Index = ShaderBatchesInFlight.Num() - 1; Index >= 0; Index--)
	{
		FShaderBatch* Batch = ShaderBatchesInFlight[Index];

		// Check to see if the shader compile worker has finished for this batch. This will be indicated by a zero length file placed in the working directory.
		// We also check the timestamp of the success file to determine if it came from the current build and is not simply a leftover from a previous build.
		if (PlatformFile.FileExists(*Batch->SuccessFileNameAndPath) &&
			PlatformFile.GetTimeStamp(*Batch->SuccessFileNameAndPath) >= ScriptFileCreationTime)
		{
			// Perform the same checks on the worker output file to verify it came from this build.
			if (PlatformFile.FileExists(*Batch->OutputFileNameAndPath) &&
				PlatformFile.GetTimeStamp(*Batch->OutputFileNameAndPath) >= ScriptFileCreationTime)
			{
				FArchive* OutputFilePtr = FileManager.CreateFileReader(*Batch->OutputFileNameAndPath, FILEREAD_Silent);
				if (OutputFilePtr)
				{
					FArchive& OutputFile = *OutputFilePtr;
					FShaderCompileUtilities::DoReadTaskResults(Batch->GetJobs(), OutputFile);

					// Close the output file.
					delete OutputFilePtr;

					// Cleanup the worker files
					Batch->CleanUpFiles(false);			// (false = don't keep the input file)
					PostCompletedJobsForBatch(Batch);
					ShaderBatchesInFlight.RemoveAt(Index);
					delete Batch;
				}
			}
		}
	}
}

int32 FShaderCompileXGEThreadRunnable_XmlInterface::CompilingLoop()
{
	bool bWorkRemaining = false;

	// We can only run one XGE build at a time.
	// Check if a build is currently in progress.
	if (BuildProcessHandle.IsValid())
	{
		// Read back results from the current batches in progress.
		GatherResultsFromXGE();

		bool bDoExitCheck = false;
		if (FPlatformProcess::IsProcRunning(BuildProcessHandle))
		{
			if (ShaderBatchesInFlight.Num() == 0)
			{
				// We've processed all batches.
				// Wait for the XGE console process to exit
				FPlatformProcess::WaitForProc(BuildProcessHandle);
				bDoExitCheck = true;
			}
		}
		else
		{
			bDoExitCheck = true;
		}

		if (bDoExitCheck)
		{
			if (ShaderBatchesInFlight.Num() > 0)
			{
				// The build process has stopped.
				// Do one final pass over the output files to gather any remaining results.
				GatherResultsFromXGE();
			}

			// The build process is no longer running.
			// We need to check the return code for possible failure
			int32 ReturnCode = 0;
			FPlatformProcess::GetProcReturnCode(BuildProcessHandle, &ReturnCode);

			switch (ReturnCode)
			{
			case 0:
				// No error
				break;

			case 1:
				// One or more of the shader compile worker processes crashed.
				UE_LOG(LogShaderCompilers, Fatal, TEXT("An error occurred during an XGE shader compilation job. One or more of the shader compile worker processes exited unexpectedly (Code 1)."));
				break;

			case 2:
				// Fatal IncrediBuild error
				UE_LOG(LogShaderCompilers, Fatal, TEXT("An error occurred during an XGE shader compilation job. XGConsole.exe returned a fatal Incredibuild error (Code 2)."));
				break;

			case 3:
				// User canceled the build
				UE_LOG(LogShaderCompilers, Display, TEXT("The user terminated an XGE shader compilation job. Incomplete shader jobs will be redispatched in another XGE build."));
				break;

			default:
				UE_LOG(LogShaderCompilers, Fatal, TEXT("An unknown error occurred during an XGE shader compilation job (Code %d)."), ReturnCode);
				break;
			}

			// Reclaim jobs from the workers which did not succeed (if any).
			for (FShaderBatch* Batch : ShaderBatchesInFlight)
			{
				// Delete any output/success files, but keep the input file so we don't have to write it out again.
				Batch->CleanUpFiles(true);

				// We can't add any jobs to a shader batch which has already been written out to disk,
				// so put the batch back into the full batches list, even if the batch isn't full.
				ShaderBatchesFull.Add(Batch);

				// Reset the batch/directory indices and move the input file to the correct place.
				FString OldInputFilename = Batch->InputFileNameAndPath;
				Batch->SetIndices(XGEDirectoryIndex, BatchIndexToCreate++);
				MoveFileHelper(Batch->InputFileNameAndPath, OldInputFilename);
			}

			ShaderBatchesInFlight.Empty();
			FPlatformProcess::CloseProc(BuildProcessHandle);
		}

		bWorkRemaining |= ShaderBatchesInFlight.Num() > 0;
	}
	// No build process running. Check if we can kick one off now.
	else
	{
		// Determine if enough time has passed to allow a build to kick off.
		// Since shader jobs are added to the shader compile manager asynchronously by the engine, 
		// we want to give the engine enough time to queue up a large number of shaders.
		// Otherwise we will only be kicking off a small number of shader jobs at once.
		bool BuildDelayElapsed = (((FPlatformTime::Cycles() - LastAddTime) * FPlatformTime::GetSecondsPerCycle()) >= XGEShaderCompilerVariables::JobTimeout);
		bool HasJobsToRun = (ShaderBatchesIncomplete.Num() > 0 || ShaderBatchesFull.Num() > 0);

		if (BuildDelayElapsed && HasJobsToRun && ShaderBatchesInFlight.Num() == 0)
		{
			// Move all the pending shader batches into the in-flight list.
			ShaderBatchesInFlight.Reserve(ShaderBatchesIncomplete.Num() + ShaderBatchesFull.Num());

			for (FShaderBatch* Batch : ShaderBatchesIncomplete)
			{
				// Check we've actually got jobs for this batch.
				check(Batch->NumJobs() > 0);

				// Make sure we've written out the worker files for any incomplete batches.
				Batch->WriteTransferFile();
				ShaderBatchesInFlight.Add(Batch);
			}

			for (FShaderBatch* Batch : ShaderBatchesFull)
			{
				// Check we've actually got jobs for this batch.
				check(Batch->NumJobs() > 0);

				ShaderBatchesInFlight.Add(Batch);
			}

			ShaderBatchesFull.Empty();
			ShaderBatchesIncomplete.Empty(XGEShaderCompilerVariables::BatchGroupSize);

			FString ScriptFilename = XGEWorkingDirectory / FString::FromInt(XGEDirectoryIndex) / XGE_ScriptFileName;

			// Create the XGE script file.
			FArchive* ScriptFile = CreateFileHelper(ScriptFilename);
			WriteScriptFileHeader(ScriptFile, Manager->ShaderCompileWorkerName);

			// Write the XML task line for each shader batch
			for (FShaderBatch* Batch : ShaderBatchesInFlight)
			{
				FString WorkerAbsoluteDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*Batch->WorkingDirectory);
				FPaths::NormalizeDirectoryName(WorkerAbsoluteDirectory);

				FString WorkerParameters = FString::Printf(TEXT("&quot;%s/&quot; %d %d &quot;%s&quot; &quot;%s&quot; -xge_xml %s"),
					*WorkerAbsoluteDirectory,
					Manager->ProcessId,
					Batch->BatchIndex,
					*XGE_InputFileName,
					*XGE_OutputFileName,
					*FCommandLine::GetSubprocessCommandline());
				FString TaskXML = FString::Printf(TEXT("\t\t\t<Task Caption=\"Compiling %d Shaders (Batch %d)\" Params=\"%s\" />\r\n"), Batch->NumJobs(), Batch->BatchIndex, *WorkerParameters);

				ScriptFile->Serialize((void*)StringCast<ANSICHAR>(*TaskXML, TaskXML.Len()).Get(), sizeof(ANSICHAR) * TaskXML.Len());
			}

			// End the XML script file and close it.
			WriteScriptFileFooter(ScriptFile);
			delete ScriptFile;
			ScriptFile = nullptr;

			// Grab the timestamp from the script file.
			// We use this to ignore any left over files from previous builds by only accepting files created after the script file.
			ScriptFileCreationTime = IFileManager::Get().GetTimeStamp(*ScriptFilename);

			StartTime = FPlatformTime::Cycles();

			// Use stop on errors so we can respond to shader compile worker crashes immediately.
			// Regular shader compilation errors are not returned as worker errors.
			FString XGConsoleArgs = TEXT("/VIRTUALIZEDIRECTX /STOPONERRORS /BUILD \"") + ScriptFilename + TEXT("\"");

			// Kick off the XGE process...
			BuildProcessHandle = FPlatformProcess::CreateProc(*XGE_ConsolePath, *XGConsoleArgs, false, false, true, &BuildProcessID, 0, nullptr, nullptr);
			if (!BuildProcessHandle.IsValid())
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Failed to launch %s during shader compilation."), *XGE_ConsolePath);
			}

			// If the engine crashes, we don't get a chance to kill the build process.
			// Start up the build monitor process to monitor for engine crashes.
			uint32 BuildMonitorProcessID;
			FProcHandle BuildMonitorHandle = FPlatformProcess::CreateProc(*Manager->ShaderCompileWorkerName, *FString::Printf(TEXT("-xgemonitor %d %d"), Manager->ProcessId, BuildProcessID), true, false, false, &BuildMonitorProcessID, 0, nullptr, nullptr);
			FPlatformProcess::CloseProc(BuildMonitorHandle);

			// Reset batch counters and switch directories
			BatchIndexToFill = 0;
			BatchIndexToCreate = 0;
			XGEDirectoryIndex = 1 - XGEDirectoryIndex;

			bWorkRemaining = true;
		}
	}

	// Try to prepare more shader jobs (even if a build is in flight).
	TArray<FShaderCommonCompileJob*> JobQueue;
	{
		// Enter the critical section so we can access the input and output queues
		FScopeLock Lock(&Manager->CompileQueueSection);

		// Grab as many jobs from the job queue as we can.
		int32 NumNewJobs = Manager->CompileQueue.Num();
		if (NumNewJobs > 0)
		{
			int32 DestJobIndex = JobQueue.AddUninitialized(NumNewJobs);
			for (int32 SrcJobIndex = 0; SrcJobIndex < NumNewJobs; SrcJobIndex++, DestJobIndex++)
			{
				JobQueue[DestJobIndex] = Manager->CompileQueue[SrcJobIndex];
			}

			Manager->CompileQueue.RemoveAt(0, NumNewJobs);
		}
	}

	if (JobQueue.Num() > 0)
	{
		// We have new jobs in the queue.
		// Group the jobs into batches and create the worker input files.
		for (int32 JobIndex = 0; JobIndex < JobQueue.Num(); JobIndex++)
		{
			if (BatchIndexToFill >= ShaderBatchesIncomplete.GetMaxIndex() || !ShaderBatchesIncomplete.IsAllocated(BatchIndexToFill))
			{
				// There are no more incomplete shader batches available.
				// Create another one...
				ShaderBatchesIncomplete.Insert(BatchIndexToFill, new FShaderBatch(
					XGEWorkingDirectory,
					XGE_InputFileName,
					XGE_SuccessFileName,
					XGE_OutputFileName,
					XGEDirectoryIndex,
					BatchIndexToCreate));

				BatchIndexToCreate++;
			}

			// Add a single job to this batch
			FShaderBatch* CurrentBatch = ShaderBatchesIncomplete[BatchIndexToFill];
			CurrentBatch->AddJob(JobQueue[JobIndex]);

			// If the batch is now full...
			if (CurrentBatch->NumJobs() == XGEShaderCompilerVariables::BatchSize)
			{
				CurrentBatch->WriteTransferFile();

				// Move the batch to the full list.
				ShaderBatchesFull.Add(CurrentBatch);
				ShaderBatchesIncomplete.RemoveAt(BatchIndexToFill);
			}

			BatchIndexToFill++;
			BatchIndexToFill %= XGEShaderCompilerVariables::BatchGroupSize;
		}

		// Keep track of the last time we added jobs.
		LastAddTime = FPlatformTime::Cycles();

		bWorkRemaining = true;
	}

	if (Manager->bAllowAsynchronousShaderCompiling)
	{
		// Yield for a short while to stop this thread continuously polling the disk.
		FPlatformProcess::Sleep(0.01f);
	}

	return bWorkRemaining ? 1 : 0;
}

// --------------------------------------------------------------------------
//                         XGE Interception interface                        
// --------------------------------------------------------------------------

#if WITH_XGE_CONTROLLER
	#include "XGEControllerInterface.h"
#endif

bool FShaderCompileXGEThreadRunnable_InterceptionInterface::IsSupported()
{
#if WITH_XGE_CONTROLLER

	XGEShaderCompilerVariables::Init();

	return
		(XGEShaderCompilerVariables::Enabled == 1) && // XGE is enabled by CVar or command line.
		(XGEShaderCompilerVariables::Mode    != 0) && // XGE intercept mode is allowed.
		IXGEController::Get().IsSupported();   // XGE controller is supported.

#else
	return false;
#endif
}

#if WITH_XGE_CONTROLLER
class FXGEShaderCompilerTask
{
public:
	TFuture<FXGETaskResult> Future;
	TArray<FShaderCommonCompileJob*> ShaderJobs;
	FString InputFilePath;
	FString OutputFilePath;

	FXGEShaderCompilerTask(TFuture<FXGETaskResult>&& Future, TArray<FShaderCommonCompileJob*>&& ShaderJobs, FString&& InputFilePath, FString&& OutputFilePath)
		: Future(MoveTemp(Future))
		, ShaderJobs(MoveTemp(ShaderJobs))
		, InputFilePath(MoveTemp(InputFilePath))
		, OutputFilePath(MoveTemp(OutputFilePath))
	{}
};
#endif

/** Initialization constructor. */
FShaderCompileXGEThreadRunnable_InterceptionInterface::FShaderCompileXGEThreadRunnable_InterceptionInterface(class FShaderCompilingManager* InManager)
	: FShaderCompileThreadRunnableBase(InManager)
	, NumDispatchedJobs(0)
{
}

FShaderCompileXGEThreadRunnable_InterceptionInterface::~FShaderCompileXGEThreadRunnable_InterceptionInterface()
{
}

int32 FShaderCompileXGEThreadRunnable_InterceptionInterface::CompilingLoop()
{
#if WITH_XGE_CONTROLLER

	TArray<FShaderCommonCompileJob*> PendingJobs;

	// Try to prepare more shader jobs.
	{
		// Enter the critical section so we can access the input and output queues
		FScopeLock Lock(&Manager->CompileQueueSection);

		// Grab as many jobs from the job queue as we can.
		int32 NumNewJobs = Manager->CompileQueue.Num();
		if (NumNewJobs > 0)
		{
			for (int32 SrcJobIndex = 0; SrcJobIndex < NumNewJobs; ++SrcJobIndex)
			{
				PendingJobs.Add(Manager->CompileQueue[SrcJobIndex]);
			}

			Manager->CompileQueue.RemoveAt(0, NumNewJobs);
		}
	}

	if (PendingJobs.Num() > 0)
	{
		// Increase the batch size when more jobs are queued/in flight.
		const uint32 JobsPerBatch = FMath::Max(1, FMath::FloorToInt(FMath::LogX(2, PendingJobs.Num() + NumDispatchedJobs)));
		UE_LOG(LogShaderCompilers, Verbose, TEXT("Current jobs: %d, Batch size: %d, Num Already Dispatched: %d"), PendingJobs.Num(), JobsPerBatch, NumDispatchedJobs);

		for (auto JobsIter = PendingJobs.CreateIterator(); JobsIter; )
		{
			TArray<FShaderCommonCompileJob*> JobsToSerialize;

			for (uint32 NumThisBatch = 0; NumThisBatch < JobsPerBatch && JobsIter; ++JobsIter, ++NumThisBatch)
				JobsToSerialize.Add(*JobsIter);

			if (JobsToSerialize.Num())
			{
				FString InputFilePath = IXGEController::Get().CreateUniqueFilePath();
				FString OutputFilePath = IXGEController::Get().CreateUniqueFilePath();

				FString WorkingDirectory = FPaths::GetPath(InputFilePath);
				FString InputFileName = FPaths::GetCleanFilename(InputFilePath);
				FString OutputFileName = FPaths::GetCleanFilename(OutputFilePath);

				FString WorkerParameters = FString::Printf(TEXT("\"%s/\" %d 0 \"%s\" \"%s\" -xge_int %s"),
					*WorkingDirectory,
					Manager->ProcessId,
					*InputFileName,
					*OutputFileName,
					*FCommandLine::GetSubprocessCommandline());

				// Serialize the jobs to the input file
				FArchive* InputFileAr = IFileManager::Get().CreateFileWriter(*InputFilePath, FILEWRITE_EvenIfReadOnly | FILEWRITE_NoFail);
				FShaderCompileUtilities::DoWriteTasks(JobsToSerialize, *InputFileAr);
				delete InputFileAr;

				// Kick off the job
				NumDispatchedJobs += JobsToSerialize.Num();

				DispatchedTasks.Add(
					new FXGEShaderCompilerTask(
						IXGEController::Get().EnqueueTask(Manager->ShaderCompileWorkerName, WorkerParameters),
						MoveTemp(JobsToSerialize),
						MoveTemp(InputFilePath),
						MoveTemp(OutputFilePath)
					)
				);
			}
		}
	}

	for (auto Iter = DispatchedTasks.CreateIterator(); Iter; ++Iter)
	{
		FXGEShaderCompilerTask* Task = *Iter;
		if (!Task->Future.IsReady())
			continue;

		FXGETaskResult Result = Task->Future.Get();
		NumDispatchedJobs -= Task->ShaderJobs.Num();

		if (Result.ReturnCode != 0)
			UE_LOG(LogShaderCompilers, Error, TEXT("Shader compiler returned a non-zero error code (%d)."), Result.ReturnCode);

		if (Result.bCompleted)
		{
			// Open the output file, and serialize in the completed jobs.
			FArchive* OutputFileAr = IFileManager::Get().CreateFileReader(*Task->OutputFilePath, FILEREAD_NoFail);
			FShaderCompileUtilities::DoReadTaskResults(Task->ShaderJobs, *OutputFileAr);
			delete OutputFileAr;

			// Enter the critical section so we can access the input and output queues
			{
				FScopeLock Lock(&Manager->CompileQueueSection);
				for (FShaderCommonCompileJob* Job : Task->ShaderJobs)
				{
					FShaderMapCompileResults& ShaderMapResults = Manager->ShaderMapJobs.FindChecked(Job->Id);
					ShaderMapResults.FinishedJobs.Add(Job);
					ShaderMapResults.bAllJobsSucceeded = ShaderMapResults.bAllJobsSucceeded && Job->bSucceeded;
				}
			}

			// Using atomics to update NumOutstandingJobs since it is read outside of the critical section
			FPlatformAtomics::InterlockedAdd(&Manager->NumOutstandingJobs, -Task->ShaderJobs.Num());
		}
		else
		{
			// The compile job was canceled. Return the jobs to the manager's compile queue.
			FScopeLock Lock(&Manager->CompileQueueSection);
			Manager->CompileQueue.Append(Task->ShaderJobs);
		}

		// Delete input and output files, if they exist.
		while (!IFileManager::Get().Delete(*Task->InputFilePath, false, true, true)) FPlatformProcess::Sleep(0.01f);
		while (!IFileManager::Get().Delete(*Task->OutputFilePath, false, true, true)) FPlatformProcess::Sleep(0.01f);

		Iter.RemoveCurrent();
		delete Task;
	}

	// Yield for a short while to stop this thread continuously polling the disk.
	FPlatformProcess::Sleep(0.01f);

	// Return true if there is more work to be done.
	return FPlatformAtomics::InterlockedAdd(&Manager->NumOutstandingJobs, 0) > 0;

#else

	return 0;

#endif
}

#endif // PLATFORM_WINDOWS
