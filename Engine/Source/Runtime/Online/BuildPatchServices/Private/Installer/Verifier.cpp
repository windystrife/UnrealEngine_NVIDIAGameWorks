// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/Verifier.h"
#include "Common/StatsCollector.h"
#include "Common/FileSystem.h"
#include "BuildPatchVerify.h"
#include "BuildPatchUtil.h"
#include "HAL/ThreadSafeBool.h"

namespace BuildPatchServices
{
	class FVerification : public IVerifier
	{
	public:
		FVerification(IFileSystem* FileSystem, IVerifierStat* InVerificationStat, EVerifyMode InVerifyMode, TSet<FString> InTouchedFiles, TSet<FString> InInstallTags, FBuildPatchAppManifestRef InManifest, FString InVerifyDirectory, FString InStagedFileDirectory);
		~FVerification() {}

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override;
		virtual void Abort() override;
		// IControllable interface end.

		// IVerifier interface begin.
		virtual bool Verify(TArray<FString>& OutDatedFiles) override;
		// IVerifier interface end.

	private:
		FString SelectFullFilePath(const FString& BuildFile);
		bool VerfiyFileSha(const FString& BuildFile, int64 BuildFileSize);
		bool VerfiyFileSize(const FString& BuildFile, int64 BuildFileSize);

	private:
		IFileSystem* FileSystem;
		IVerifierStat* VerifierStat;
		EVerifyMode VerifyMode;
		TSet<FString> RequiredFiles;
		TSet<FString> InstallTags;
		FBuildPatchAppManifestRef Manifest;
		const FString VerifyDirectory;
		const FString StagedFileDirectory;
		FThreadSafeBool bIsPaused;
		FThreadSafeBool bShouldAbort;
		int64 ProcessedBytes;
	};

	FVerification::FVerification(IFileSystem* InFileSystem, IVerifierStat* InVerificationStat, EVerifyMode InVerifyMode, TSet<FString> InTouchedFiles, TSet<FString> InInstallTags, FBuildPatchAppManifestRef InManifest, FString InVerifyDirectory, FString InStagedFileDirectory)
		: FileSystem(InFileSystem)
		, VerifierStat(InVerificationStat)
		, VerifyMode(InVerifyMode)
		, RequiredFiles(MoveTemp(InTouchedFiles))
		, InstallTags(MoveTemp(InInstallTags))
		, Manifest(MoveTemp(InManifest))
		, VerifyDirectory(MoveTemp(InVerifyDirectory))
		, StagedFileDirectory(MoveTemp(InStagedFileDirectory))
		, bIsPaused(false)
		, bShouldAbort(false)
		, ProcessedBytes(0)
	{}

	void FVerification::SetPaused(bool bInIsPaused)
	{
		bIsPaused = bInIsPaused;
	}

	void FVerification::Abort()
	{
		bShouldAbort = true;
	}

	bool FVerification::Verify(TArray<FString>& OutDatedFiles)
	{
		bool bAllCorrect = true;
		OutDatedFiles.Empty();
		if (VerifyMode == EVerifyMode::FileSizeCheckAllFiles || VerifyMode == EVerifyMode::ShaVerifyAllFiles)
		{
			Manifest->GetTaggedFileList(InstallTags, RequiredFiles);
		}

		// Setup progress tracking.
		VerifierStat->OnTotalRequiredUpdated(Manifest->GetFileSize(RequiredFiles));

		// Select verify function.
		bool bVerifySha = VerifyMode == EVerifyMode::ShaVerifyAllFiles || VerifyMode == EVerifyMode::ShaVerifyTouchedFiles;

		// For each required file, perform the selected verification.
		ProcessedBytes = 0;
		for (const FString& BuildFile : RequiredFiles)
		{
			// Break if quitting
			if (bShouldAbort)
			{
				break;
			}

			// Get file details.
			int64 BuildFileSize = Manifest->GetFileSize(BuildFile);

			// Verify the file.
			VerifierStat->OnFileStarted(BuildFile, BuildFileSize);
			bool bFileOk = bVerifySha ? VerfiyFileSha(BuildFile, BuildFileSize) : VerfiyFileSize(BuildFile, BuildFileSize);
			VerifierStat->OnFileCompleted(BuildFile, bFileOk);
			if (bFileOk == false)
			{
				bAllCorrect = false;
				OutDatedFiles.Add(BuildFile);
			}
			ProcessedBytes += BuildFileSize;
			VerifierStat->OnProcessedDataUpdated(ProcessedBytes);
		}

		return bAllCorrect && !bShouldAbort;
	}

	FString FVerification::SelectFullFilePath(const FString& BuildFile)
	{
		FString FullFilePath;
		if (StagedFileDirectory.IsEmpty() == false)
		{
			FullFilePath = StagedFileDirectory / BuildFile;
			int64 FileSize;
			if (FileSystem->GetFileSize(*FullFilePath, FileSize) && FileSize != INDEX_NONE)
			{
				return FullFilePath;
			}
		}
		FullFilePath = VerifyDirectory / BuildFile;
		return FullFilePath;
	}

	bool FVerification::VerfiyFileSha(const FString& BuildFile, int64 BuildFileSize)
	{
		FSHAHashData BuildFileHash;
		bool bFoundHash = Manifest->GetFileHash(BuildFile, BuildFileHash);
		checkf(bFoundHash, TEXT("Missing file hash from manifest."));
		TFunction<void(float)> FileProgress = [this, &BuildFileSize, &BuildFile](float Progress)
		{
			int64 FileProcessed = FMath::Min<int64>(BuildFileSize * Progress, BuildFileSize);
			VerifierStat->OnFileProgress(BuildFile, FileProcessed);
			VerifierStat->OnProcessedDataUpdated(ProcessedBytes + FileProcessed);
		};
		TFunction<bool()> IsPaused = [this]() -> bool
		{
			return bIsPaused;
		};
		TFunction<bool()> ShouldAbort = [this]() -> bool
		{
			return bShouldAbort;
		};
		bool bSuccess = FBuildPatchUtils::VerifyFile(
			FileSystem,
			SelectFullFilePath(BuildFile),
			BuildFileHash,
			BuildFileHash,
			FBuildPatchFloatDelegate::CreateLambda(MoveTemp(FileProgress)),
			FBuildPatchBoolRetDelegate::CreateLambda(MoveTemp(IsPaused)),
			FBuildPatchBoolRetDelegate::CreateLambda(MoveTemp(ShouldAbort))) != 0;
		VerifierStat->OnFileProgress(BuildFile, BuildFileSize);
		return bSuccess;
	}

	bool FVerification::VerfiyFileSize(const FString& BuildFile, int64 BuildFileSize)
	{
		// Pause if necessary.
		const double PrePauseTime = FStatsCollector::GetSeconds();
		double PostPauseTime = PrePauseTime;
		while (bIsPaused && !bShouldAbort)
		{
			FPlatformProcess::Sleep(0.1f);
			PostPauseTime = FStatsCollector::GetSeconds();
		}
		VerifierStat->OnFileProgress(BuildFile, 0);
		int64 FileSize;
		bool bSuccess = false;
		if (FileSystem->GetFileSize(*SelectFullFilePath(BuildFile), FileSize))
		{
			bSuccess = FileSize == BuildFileSize;
		}
		VerifierStat->OnFileProgress(BuildFile, BuildFileSize);
		return bSuccess;
	}

	IVerifier* FVerifierFactory::Create(IFileSystem* FileSystem, IVerifierStat* VerifierStat, EVerifyMode VerifyMode, TSet<FString> TouchedFiles, TSet<FString> InstallTags, FBuildPatchAppManifestRef Manifest, FString VerifyDirectory, FString StagedFileDirectory)
	{
		check(FileSystem != nullptr);
		check(VerifierStat != nullptr);
		return new FVerification(FileSystem, VerifierStat, VerifyMode, MoveTemp(TouchedFiles), MoveTemp(InstallTags), MoveTemp(Manifest), MoveTemp(VerifyDirectory), MoveTemp(StagedFileDirectory));
	}
}
