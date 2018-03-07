// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/FileAttribution.h"
#include "Common/FileSystem.h"
#include "BuildPatchProgress.h"

namespace BuildPatchServices
{
	class FFileAttribution : public IFileAttribution
	{
	public:
		FFileAttribution(IFileSystem* FileSystem, const FBuildPatchAppManifestRef& NewManifest, const FBuildPatchAppManifestPtr& OldManifest, TSet<FString> TouchedFiles, const FString& InstallDirectory, const FString& StagedFileDirectory, bool bUseStageDirectory, FBuildPatchProgress* BuildProgress);

		virtual ~FFileAttribution() {}

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override;
		virtual void Abort() override;
		// IControllable interface end.

		// IFileAttribution interface begin.
		virtual bool ApplyAttributes(bool bForce) override;
		// IFileAttribution interface end.

	private:
		FString SelectFullFilePath(const FString& BuildFile);
		bool HasSameAttributes(const FFileManifestData* NewFileManifest, const FFileManifestData* OldFileManifest);
		void SetupFileAttributes(const FString& FilePath, const FFileManifestData& FileManifest, bool bForce);

	private:
		IFileSystem* FileSystem;
		FBuildPatchAppManifestRef NewManifest;
		FBuildPatchAppManifestPtr OldManifest;
		TSet<FString> TouchedFiles;
		const FString InstallDirectory;
		const FString StagedFileDirectory;
		const bool bUseStageDirectory;
		FBuildPatchProgress* BuildProgress;
		FThreadSafeBool bIsPaused;
		FThreadSafeBool bShouldAbort;
	};

	FFileAttribution::FFileAttribution(IFileSystem* InFileSystem, const FBuildPatchAppManifestRef& InNewManifest, const FBuildPatchAppManifestPtr& InOldManifest, TSet<FString> InTouchedFiles, const FString& InInstallDirectory, const FString& InStagedFileDirectory, bool bInUseStageDirectory, FBuildPatchProgress* InBuildProgress)
		: FileSystem(InFileSystem)
		, NewManifest(InNewManifest)
		, OldManifest(InOldManifest)
		, TouchedFiles(MoveTemp(InTouchedFiles))
		, InstallDirectory(InInstallDirectory)
		, StagedFileDirectory(InStagedFileDirectory)
		, bUseStageDirectory(bInUseStageDirectory)
		, BuildProgress(InBuildProgress)
		, bIsPaused(false)
		, bShouldAbort(false)
	{
		BuildProgress->SetStateProgress(EBuildPatchState::SettingAttributes, 0.0f);
	}

	void FFileAttribution::SetPaused(bool bInIsPaused)
	{
		bIsPaused = bInIsPaused;
	}

	void FFileAttribution::Abort()
	{
		bShouldAbort = true;
	}

	bool FFileAttribution::ApplyAttributes(bool bForce)
	{
		// We need to set attributes for all files in the new build that require it
		TArray<FString> BuildFileList = NewManifest->GetBuildFileList();
		BuildProgress->SetStateProgress(EBuildPatchState::SettingAttributes, 0.0f);
		for (int32 BuildFileIdx = 0; BuildFileIdx < BuildFileList.Num() && !bShouldAbort; ++BuildFileIdx)
		{
			const FString& BuildFile = BuildFileList[BuildFileIdx];
			const FFileManifestData* NewFileManifest = NewManifest->GetFileManifest(BuildFile);
			const FFileManifestData* OldFileManifest = OldManifest.IsValid() ? OldManifest->GetFileManifest(BuildFile) : nullptr;
			bool bHasChanged = bForce || (TouchedFiles.Contains(BuildFile) && !HasSameAttributes(NewFileManifest, OldFileManifest));
			if (NewFileManifest != nullptr && bHasChanged)
			{
				SetupFileAttributes(SelectFullFilePath(BuildFile), *NewFileManifest, bForce);
			}
			BuildProgress->SetStateProgress(EBuildPatchState::SettingAttributes, BuildFileIdx / float(BuildFileList.Num()));
			// Wait while paused
			while (bIsPaused && !bShouldAbort)
			{
				FPlatformProcess::Sleep(0.5f);
			}
		}
		BuildProgress->SetStateProgress(EBuildPatchState::SettingAttributes, 1.0f);

		// We don't fail on this step currently
		return true;
	}

	FString FFileAttribution::SelectFullFilePath(const FString& BuildFile)
	{
		FString InstallFilename;
		if (bUseStageDirectory)
		{
			InstallFilename = StagedFileDirectory / BuildFile;
			int64 FileSize;
			if (FileSystem->GetFileSize(*InstallFilename, FileSize) && FileSize != INDEX_NONE)
			{
				return InstallFilename;
			}
		}
		InstallFilename = InstallDirectory / BuildFile;
		return InstallFilename;
	}

	bool FFileAttribution::HasSameAttributes(const FFileManifestData* NewFileManifest, const FFileManifestData* OldFileManifest)
	{
		// Currently it is not supported to rely on this, as the update process always makes new files when a file changes.
		// This can be reconsidered when the patching process changes.
		return false;

		// return (NewFileManifest != nullptr && OldFileManifest != nullptr)
		//     && (NewFileManifest->bIsUnixExecutable == OldFileManifest->bIsUnixExecutable)
		//     && (NewFileManifest->bIsReadOnly == OldFileManifest->bIsReadOnly)
		//     && (NewFileManifest->bIsCompressed == OldFileManifest->bIsCompressed);
	}

	void FFileAttribution::SetupFileAttributes(const FString& FilePath, const FFileManifestData& FileManifest, bool bForce)
	{
		EFileAttributes FileAttributes = EFileAttributes::None;

		// First check file attributes as it's much faster to read and do nothing
		bool bKnownAttributes = FileSystem->GetFileAttributes(*FilePath, FileAttributes);
		bool bIsReadOnly = (FileAttributes & EFileAttributes::ReadOnly) != EFileAttributes::None;
		const bool bFileExists = (FileAttributes & EFileAttributes::Exists) != EFileAttributes::None;
		const bool bIsCompressed = (FileAttributes & EFileAttributes::Compressed) != EFileAttributes::None;
		const bool bIsUnixExecutable = (FileAttributes & EFileAttributes::Executable) != EFileAttributes::None;

		// If we know the file is missing, skip out
		if (bKnownAttributes && !bFileExists)
		{
			return;
		}

		// If we are forcing, say we don't know existing so that all calls are made
		if (bForce)
		{
			bKnownAttributes = false;
		}

		// Set compression attribute
		if (!bKnownAttributes || bIsCompressed != FileManifest.bIsCompressed)
		{
			// Must make not readonly if required
			if (!bKnownAttributes || bIsReadOnly)
			{
				bIsReadOnly = false;
				FileSystem->SetReadOnly(*FilePath, false);
			}
			FileSystem->SetCompressed(*FilePath, FileManifest.bIsCompressed);
		}

		// Set executable attribute
		if (!bKnownAttributes || bIsUnixExecutable != FileManifest.bIsUnixExecutable)
		{
			// Must make not readonly if required
			if (!bKnownAttributes || bIsReadOnly)
			{
				bIsReadOnly = false;
				FileSystem->SetReadOnly(*FilePath, false);
			}
			FileSystem->SetExecutable(*FilePath, FileManifest.bIsUnixExecutable);
		}

		// Set readonly attribute
		if (!bKnownAttributes || bIsReadOnly != FileManifest.bIsReadOnly)
		{
			FileSystem->SetReadOnly(*FilePath, FileManifest.bIsReadOnly);
		}
	}

	IFileAttribution* FFileAttributionFactory::Create(IFileSystem* FileSystem, const FBuildPatchAppManifestRef& NewManifest, const FBuildPatchAppManifestPtr& OldManifest, TSet<FString> TouchedFiles, const FString& InstallDirectory, const FString& StagedFileDirectory, FBuildPatchProgress* BuildProgress)
	{
		check(BuildProgress != nullptr);
		return new FFileAttribution(FileSystem, NewManifest, OldManifest, MoveTemp(TouchedFiles), InstallDirectory, StagedFileDirectory, !StagedFileDirectory.IsEmpty(), BuildProgress);
	}
}