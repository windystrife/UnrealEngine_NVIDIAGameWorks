// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TestDirectoryWatcher.h"
#include "IDirectoryWatcher.h"
#include "DirectoryWatcherModule.h"
#include "LaunchEngineLoop.h"	// GEngineLoop
#include "ModuleManager.h"
#include "TestPALLog.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"

struct FChangeDetector
{
	void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
	{
		UE_LOG(LogTestPAL, Display, TEXT("  -- %d change(s) detected"), static_cast<int32>(FileChanges.Num()));

		int ChangeIdx = 0;
		for (const auto& ThisEntry : FileChanges)
		{
			UE_LOG(LogTestPAL, Display, TEXT("      Change %d: %s was %s"),
				++ChangeIdx,
				*ThisEntry.Filename,
				ThisEntry.Action == FFileChangeData::FCA_Added ? TEXT("added") :
					(ThisEntry.Action == FFileChangeData::FCA_Removed ? TEXT("removed") :
						(ThisEntry.Action == FFileChangeData::FCA_Modified ? TEXT("modified") : TEXT("??? (unknown)")
						)
					)
			);
		}
	}
};

/**
 * Kicks directory watcher test/
 */
int32 DirectoryWatcherTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running directory watcher test."));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString TestDir = FString::Printf(TEXT("%sDirectoryWatcherTest%d"), FPlatformProcess::UserTempDir(), FPlatformProcess::GetCurrentProcessId());

	if (PlatformFile.CreateDirectory(*TestDir) && PlatformFile.CreateDirectory(*(TestDir + TEXT("/subtest"))))
	{
		FChangeDetector Detector;
		FDelegateHandle DirectoryChangedHandle;

		IDirectoryWatcher* DirectoryWatcher = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")).Get();
		if (DirectoryWatcher)
		{
			auto Callback = IDirectoryWatcher::FDirectoryChanged::CreateRaw(&Detector, &FChangeDetector::OnDirectoryChanged);
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(TestDir, Callback, DirectoryChangedHandle);
			UE_LOG(LogTestPAL, Display, TEXT("Registered callback for changes in '%s'"), *TestDir);
		}
		else
		{
			UE_LOG(LogTestPAL, Fatal, TEXT("Could not get DirectoryWatcher module"));
		}

		FPlatformProcess::Sleep(1.0f);
		DirectoryWatcher->Tick(1.0f);

		// create and remove directory
		UE_LOG(LogTestPAL, Display, TEXT("Creating DIRECTORY '%s'"), *(TestDir + TEXT("/test")));
		verify(PlatformFile.CreateDirectory(*(TestDir + TEXT("/test"))));
		DirectoryWatcher->Tick(1.0f);
		FPlatformProcess::Sleep(1.0f);
		DirectoryWatcher->Tick(1.0f);

		UE_LOG(LogTestPAL, Display, TEXT("Deleting DIRECTORY '%s'"), *(TestDir + TEXT("/test")));
		verify(PlatformFile.DeleteDirectory(*(TestDir + TEXT("/test"))));
		DirectoryWatcher->Tick(1.0f);
		FPlatformProcess::Sleep(1.0f);
		DirectoryWatcher->Tick(1.0f);

		// create and remove in a sub directory
		UE_LOG(LogTestPAL, Display, TEXT("Creating DIRECTORY '%s'"), *(TestDir + TEXT("/subtest/blah")));
		verify(PlatformFile.CreateDirectory(*(TestDir + TEXT("/subtest/blah"))));
		DirectoryWatcher->Tick(1.0f);
		FPlatformProcess::Sleep(1.0f);
		DirectoryWatcher->Tick(1.0f);

		UE_LOG(LogTestPAL, Display, TEXT("Deleting DIRECTORY '%s'"), *(TestDir + TEXT("/subtest/blah")));
		verify(PlatformFile.DeleteDirectory(*(TestDir + TEXT("/subtest/blah"))));
		DirectoryWatcher->Tick(1.0f);
		FPlatformProcess::Sleep(1.0f);
		DirectoryWatcher->Tick(1.0f);

		{
			// create file
			FString DummyFileName = TestDir + TEXT("/test file.bin");
			UE_LOG(LogTestPAL, Display, TEXT("Creating FILE '%s'"), *DummyFileName);
			IFileHandle* DummyFile = PlatformFile.OpenWrite(*DummyFileName);
			check(DummyFile);
			DirectoryWatcher->Tick(1.0f);
			FPlatformProcess::Sleep(1.0f);
			DirectoryWatcher->Tick(1.0f);

			// modify file
			UE_LOG(LogTestPAL, Display, TEXT("Modifying FILE '%s'"), *DummyFileName);
			uint8 Contents = 0;
			DummyFile->Write(&Contents, sizeof(Contents));
			DirectoryWatcher->Tick(1.0f);
			FPlatformProcess::Sleep(1.0f);
			DirectoryWatcher->Tick(1.0f);

			// close the file
			UE_LOG(LogTestPAL, Display, TEXT("Closing FILE '%s'"), *DummyFileName);
			delete DummyFile;
			DummyFile = nullptr;
			DirectoryWatcher->Tick(1.0f);
			FPlatformProcess::Sleep(1.0f);
			DirectoryWatcher->Tick(1.0f);

			// delete file
			UE_LOG(LogTestPAL, Display, TEXT("Deleting FILE '%s'"), *DummyFileName);
			verify(PlatformFile.DeleteFile(*DummyFileName));
			DirectoryWatcher->Tick(1.0f);
			FPlatformProcess::Sleep(1.0f);
			DirectoryWatcher->Tick(1.0f);
		}

		// now the same in a grandchild directory
		{
			FString GrandChildDir = TestDir + TEXT("/subtest/grandchild");

			UE_LOG(LogTestPAL, Display, TEXT("Creating DIRECTORY '%s'"), *GrandChildDir);
			verify(PlatformFile.CreateDirectory(*GrandChildDir));
			DirectoryWatcher->Tick(1.0f);
			FPlatformProcess::Sleep(1.0f);
			DirectoryWatcher->Tick(1.0f);

			{
				// create file
				FString DummyFileName = GrandChildDir + TEXT("/test file.bin");
				UE_LOG(LogTestPAL, Display, TEXT("Creating FILE '%s'"), *DummyFileName);
				IFileHandle* DummyFile = PlatformFile.OpenWrite(*DummyFileName);
				check(DummyFile);
				DirectoryWatcher->Tick(1.0f);
				FPlatformProcess::Sleep(1.0f);
				DirectoryWatcher->Tick(1.0f);

				// modify file
				UE_LOG(LogTestPAL, Display, TEXT("Modifying FILE '%s'"), *DummyFileName);
				uint8 Contents = 0;
				DummyFile->Write(&Contents, sizeof(Contents));
				DirectoryWatcher->Tick(1.0f);
				FPlatformProcess::Sleep(1.0f);
				DirectoryWatcher->Tick(1.0f);

				// close the file
				UE_LOG(LogTestPAL, Display, TEXT("Closing FILE '%s'"), *DummyFileName);
				delete DummyFile;
				DummyFile = nullptr;
				DirectoryWatcher->Tick(1.0f);
				FPlatformProcess::Sleep(1.0f);
				DirectoryWatcher->Tick(1.0f);

				// delete file
				UE_LOG(LogTestPAL, Display, TEXT("Deleting FILE '%s'"), *DummyFileName);
				PlatformFile.DeleteFile(*DummyFileName);
				DirectoryWatcher->Tick(1.0f);
				FPlatformProcess::Sleep(1.0f);
				DirectoryWatcher->Tick(1.0f);
			}

			UE_LOG(LogTestPAL, Display, TEXT("Deleting DIRECTORY '%s'"), *GrandChildDir);
			verify(PlatformFile.DeleteDirectory(*GrandChildDir));
			DirectoryWatcher->Tick(1.0f);
			FPlatformProcess::Sleep(1.0f);
			DirectoryWatcher->Tick(1.0f);
		}


		// clean up
		verify(DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(TestDir, DirectoryChangedHandle));
		// remove dirs as well
		verify(PlatformFile.DeleteDirectory(*(TestDir + TEXT("/subtest"))));
		verify(PlatformFile.DeleteDirectory(*TestDir));

		UE_LOG(LogTestPAL, Display, TEXT("End of test"));
	}
	else
	{
		UE_LOG(LogTestPAL, Fatal, TEXT("Could not create test directory %s."), *TestDir);
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}
