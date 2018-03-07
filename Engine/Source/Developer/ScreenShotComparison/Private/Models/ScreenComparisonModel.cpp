// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ScreenComparisonModel.h"
#include "ISourceControlModule.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogScreenshotComparison, Log, All);

FScreenComparisonModel::FScreenComparisonModel(const FComparisonReport& InReport)
	: Report(InReport)
	, bComplete(false)
{
	const FImageComparisonResult& ComparisonResult = Report.Comparison;

	FString IncomingImage = Report.ReportFolder / ComparisonResult.ReportIncomingFile;
	FString IncomingMetadata = FPaths::ChangeExtension(IncomingImage, TEXT("json"));

	FileImports.Add(FFileMapping(IncomingImage, ComparisonResult.IncomingFile));
	FileImports.Add(FFileMapping(IncomingMetadata, FPaths::ChangeExtension(ComparisonResult.IncomingFile, TEXT("json"))));
}

bool FScreenComparisonModel::IsComplete() const
{
	return bComplete;
}

void FScreenComparisonModel::Complete()
{
	FString RelativeReportFolder = Report.ReportFolder;
	if ( FPaths::MakePathRelativeTo(RelativeReportFolder, *Report.ReportRootDirectory) )
	{
		for (;;)
		{
			FString ParentFolder = FPaths::GetPath(RelativeReportFolder);
			if ( ParentFolder.IsEmpty() )
			{
				break;
			}
			RelativeReportFolder = ParentFolder;
		}

		FString ReportTopFolder = Report.ReportRootDirectory / RelativeReportFolder;
		if ( IFileManager::Get().DeleteDirectory(*ReportTopFolder, false, true) )
		{
			bComplete = true;
			OnComplete.Broadcast();
		}
	}
}

TOptional<FAutomationScreenshotMetadata> FScreenComparisonModel::GetMetadata()
{
	// Load it.
	if ( !Metadata.IsSet() )
	{
		FString IncomingImage = Report.ReportFolder / Report.Comparison.ReportIncomingFile;
		FString IncomingMetadata = FPaths::ChangeExtension(IncomingImage, TEXT("json"));

		if ( !IncomingMetadata.IsEmpty() )
		{
			FString Json;
			if ( FFileHelper::LoadFileToString(Json, *IncomingMetadata) )
			{
				FAutomationScreenshotMetadata LoadedMetadata;
				if ( FJsonObjectConverter::JsonObjectStringToUStruct(Json, &LoadedMetadata, 0, 0) )
				{
					Metadata = LoadedMetadata;
				}
			}
		}
	}

	return Metadata;
}

bool FScreenComparisonModel::AddNew(IScreenShotManagerPtr ScreenshotManager)
{
	// Copy the files from the reports location to the destination location
	TArray<FString> SourceControlFiles;
	for ( const FFileMapping& Import : FileImports )
	{
		FString DestFilePath = ScreenshotManager->GetLocalApprovedFolder() / Import.DestinationFile;
		IFileManager::Get().Copy(*DestFilePath, *Import.SourceFile, true, true);

		SourceControlFiles.Add(DestFilePath);
	}

	// Add the files to source control
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SourceControlFiles) == ECommandResult::Failed )
	{
		// TODO Error
	}

	Complete();

	return true;
}

bool FScreenComparisonModel::Replace(IScreenShotManagerPtr ScreenshotManager)
{
	// Delete all the existing files in this area
	RemoveExistingApproved(ScreenshotManager);

	// Copy files to the approved
	const FString& LocalApprovedFolder = ScreenshotManager->GetLocalApprovedFolder();
	const FString ImportIncomingRoot = Report.ReportFolder;

	TArray<FString> SourceControlFiles;

	for ( const FFileMapping& Import : FileImports )
	{
		FString DestFilePath = LocalApprovedFolder / Import.DestinationFile;
		SourceControlFiles.Add(DestFilePath);
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), SourceControlFiles) == ECommandResult::Failed )
	{
		//TODO Error
	}

	SourceControlFiles.Reset();

	for ( const FFileMapping& Import : FileImports )
	{
		FString DestFilePath = LocalApprovedFolder / Import.DestinationFile;
		IFileManager::Get().Copy(*DestFilePath, *Import.SourceFile, true, true);

		SourceControlFiles.Add(DestFilePath);
	}

	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SourceControlFiles) == ECommandResult::Failed )
	{
		//TODO Error
	}
	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), SourceControlFiles) == ECommandResult::Failed )
	{
		//TODO Error
	}

	Complete();

	return true;
}

bool FScreenComparisonModel::RemoveExistingApproved(IScreenShotManagerPtr ScreenshotManager)
{
	FString RelativeReportFolder = Report.ReportFolder;
	if (FPaths::MakePathRelativeTo(RelativeReportFolder, *Report.ReportRootDirectory))
	{
		TArray<FString> SourceControlFiles;

		const FString& LocalApprovedFolder = ScreenshotManager->GetLocalApprovedFolder() / RelativeReportFolder;
		IFileManager::Get().FindFilesRecursive(SourceControlFiles, *LocalApprovedFolder, TEXT("*.*"), true, false, false);

		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		if (SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), SourceControlFiles) == ECommandResult::Failed)
		{
			//TODO Error
		}

		if (SourceControlProvider.Execute(ISourceControlOperation::Create<FDelete>(), SourceControlFiles) == ECommandResult::Failed)
		{
			//TODO Error
		}

		for (const FString& File : SourceControlFiles)
		{
			IFileManager::Get().Delete(*File, false, true, false);
		}

		return true;
	}

	return false;
}

bool FScreenComparisonModel::AddAlternative(IScreenShotManagerPtr ScreenshotManager)
{
	// Copy files to the approved
	const FString& LocalApprovedFolder = ScreenshotManager->GetLocalApprovedFolder();
	const FString ImportIncomingRoot = Report.ReportFolder;

	TArray<FString> SourceControlFiles;

	for ( const FFileMapping& Import : FileImports )
	{
		FString DestFilePath = LocalApprovedFolder / Import.DestinationFile;
		SourceControlFiles.Add(DestFilePath);
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), SourceControlFiles) == ECommandResult::Failed )
	{
		//TODO Error
	}

	for ( const FFileMapping& Import : FileImports )
	{
		FString DestFilePath = LocalApprovedFolder / Import.DestinationFile;
		if ( IFileManager::Get().Copy(*DestFilePath, *Import.SourceFile, false, true) == COPY_OK )
		{
			SourceControlFiles.Add(DestFilePath);
		}
		else
		{
			// TODO Error
		}
	}

	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SourceControlFiles) == ECommandResult::Failed )
	{
		//TODO Error
	}
	if ( SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), SourceControlFiles) == ECommandResult::Failed )
	{
		//TODO Error
	}

	Complete();

	return true;
}