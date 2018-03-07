// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ScreenShotManager.h"

#include "AutomationWorkerMessages.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "MessageEndpointBuilder.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Misc/FilterCollection.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"


class FScreenshotComparisons
{
public:
	FString ApprovedFolder;
	FString UnapprovedFolder;

	TArray<FString> Existing;
	TArray<FString> New;
	TArray<FString> Missing;
};

FScreenShotManager::FScreenShotManager()
{
	FModuleManager::Get().LoadModuleChecked(FName("ImageWrapper"));

	ScreenshotUnapprovedFolder = FPaths::ProjectSavedDir() / TEXT("Automation/Incoming/");
	ScreenshotDeltaFolder = FPaths::ProjectSavedDir() / TEXT("Automation/Delta/");
	ScreenshotResultsFolder = FPaths::ProjectSavedDir() / TEXT("Automation/");
	ScreenshotApprovedFolder = FPaths::ProjectDir() / TEXT("Test/Screenshots/");

	ComparisonResultsFolder = FPaths::ProjectSavedDir() / TEXT("Automation/Comparisons");

	// Clear the incoming directory when we initialize, we don't care about previous runs.
	//IFileManager::Get().DeleteDirectory(*ScreenshotUnapprovedFolder, false, true);

	// Clear previous comparison results from the local comparison folder.
	//IFileManager::Get().DeleteDirectory(*ComparisonResultsRoot, false, true);
}

FString FScreenShotManager::GetLocalUnapprovedFolder() const
{
	return FPaths::ConvertRelativePathToFull(ScreenshotUnapprovedFolder);
}

FString FScreenShotManager::GetLocalApprovedFolder() const
{
	return FPaths::ConvertRelativePathToFull(ScreenshotApprovedFolder);
}

FString FScreenShotManager::GetLocalComparisonFolder() const
{
	return FPaths::ConvertRelativePathToFull(ScreenshotDeltaFolder);
}

/* IScreenShotManager event handlers
 *****************************************************************************/

TFuture<FImageComparisonResult> FScreenShotManager::CompareScreensotAsync(FString RelativeImagePath)
{
	return Async<FImageComparisonResult>(EAsyncExecution::Thread, [&] () { return CompareScreensot(RelativeImagePath); });
}

FImageComparisonResult FScreenShotManager::CompareScreensot(FString ExistingImage)
{
	FString Existing = FPaths::GetPath(ExistingImage);

	FImageComparer Comparer;
	Comparer.ImageRootA = ScreenshotApprovedFolder;
	Comparer.ImageRootB = ScreenshotUnapprovedFolder;
	Comparer.DeltaDirectory = ScreenshotDeltaFolder;

	// If the metadata for the screenshot does not provide tolerance rules, use these instead.
	FImageTolerance DefaultTolerance = FImageTolerance::DefaultIgnoreLess;
	DefaultTolerance.IgnoreAntiAliasing = true;

	FString TestApprovedFolder = FPaths::Combine(ScreenshotApprovedFolder, Existing);
	FString TestUnapprovedFolder =  FPaths::Combine(ScreenshotUnapprovedFolder, Existing);

	TArray<FString> ApprovedDeviceShots;
	IFileManager::Get().FindFilesRecursive(ApprovedDeviceShots, *TestApprovedFolder, TEXT("*.png"), true, false);

	FImageComparisonResult ComparisonResult;

	// We can't find a ground truth, so it's a new comparison.
	if ( ApprovedDeviceShots.Num() > 0 )
	{
		// Load the metadata for the incoming unapproved image.
		FString UnapprovedFileName = FPaths::GetCleanFilename(ExistingImage);
		FString UnapprovedFullPath = FPaths::Combine(TestUnapprovedFolder, UnapprovedFileName);

		TOptional<FAutomationScreenshotMetadata> ExistingMetadata;

		{
			// Always read the metadata file from the unapproved location, as we may have introduced new comparison rules.
			FString MetadataFile = FPaths::ChangeExtension(UnapprovedFullPath, ".json");

			FString Json;
			if ( FFileHelper::LoadFileToString(Json, *MetadataFile) )
			{
				FAutomationScreenshotMetadata Metadata;
				if ( FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Metadata, 0, 0) )
				{
					ExistingMetadata = Metadata;
				}
			}
		}

		FString NearestExistingApprovedImage;
		TOptional<FAutomationScreenshotMetadata> NearestExistingApprovedImageMetadata;

		if ( ExistingMetadata.IsSet() )
		{
			int32 MatchScore = -1;

			for ( FString ApprovedShot : ApprovedDeviceShots )
			{
				FString ApprovedShotFile = FPaths::GetCleanFilename(ApprovedShot);
				FString ApprovedShotFileFull = FPaths::Combine(TestApprovedFolder, ApprovedShotFile);

				FString ApprovedMetadataFile = FPaths::ChangeExtension(ApprovedShotFileFull, ".json");

				FString Json;
				if ( FFileHelper::LoadFileToString(Json, *ApprovedMetadataFile) )
				{
					FAutomationScreenshotMetadata Metadata;
					if ( FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Metadata, 0, 0) )
					{
						int32 Comparison = Metadata.Compare(ExistingMetadata.GetValue());
						if ( Comparison > MatchScore )
						{
							MatchScore = Comparison;
							NearestExistingApprovedImage = ApprovedShotFile;
							NearestExistingApprovedImageMetadata = Metadata;
						}
					}
				}
			}
		}
		else
		{
			// TODO no metadata how do I pick a good shot?
			NearestExistingApprovedImage = FPaths::GetCleanFilename(ApprovedDeviceShots[0]);
		}

		FString ApprovedFullPath = FPaths::Combine(TestApprovedFolder, NearestExistingApprovedImage);

		FImageTolerance Tolerance = DefaultTolerance;

		if ( ExistingMetadata.IsSet() && ExistingMetadata->bHasComparisonRules )
		{
			Tolerance.Red = ExistingMetadata->ToleranceRed;
			Tolerance.Green = ExistingMetadata->ToleranceGreen;
			Tolerance.Blue = ExistingMetadata->ToleranceBlue;
			Tolerance.Alpha = ExistingMetadata->ToleranceAlpha;
			Tolerance.MinBrightness = ExistingMetadata->ToleranceMinBrightness;
			Tolerance.MaxBrightness = ExistingMetadata->ToleranceMaxBrightness;
			Tolerance.IgnoreAntiAliasing = ExistingMetadata->bIgnoreAntiAliasing;
			Tolerance.IgnoreColors = ExistingMetadata->bIgnoreColors;
			Tolerance.MaximumLocalError = ExistingMetadata->MaximumLocalError;
			Tolerance.MaximumGlobalError = ExistingMetadata->MaximumGlobalError;
		}

		// TODO Think about using SSIM, but needs local SSIM as well as Global SSIM, same as the basic comparison.
		//double SSIM = Comparer.CompareStructuralSimilarity(ApprovedFullPath, UnapprovedFullPath, FImageComparer::EStructuralSimilarityComponent::Luminance);
		//printf("%f\n", SSIM);

		ComparisonResult = Comparer.Compare(ApprovedFullPath, UnapprovedFullPath, Tolerance);
	}
	else
	{
		ComparisonResult.IncomingFile = ExistingImage;
	}

	// Generate and save a report of the comparison if it's new or the results are not similar
	if ( ComparisonResult.IsNew() || !ComparisonResult.AreSimilar() )
	{
		FString ReportFolder = ComparisonResultsFolder / Existing;

		FString ApprovedFile = ( ScreenshotApprovedFolder / ComparisonResult.ApprovedFile );
		FString ApprovedMetadataFile = FPaths::ChangeExtension(ApprovedFile, ".json");

		FString IncomingFile = ( ScreenshotUnapprovedFolder / ComparisonResult.IncomingFile );
		FString IncomingMetadataFile = FPaths::ChangeExtension(IncomingFile, ".json");

		if ( IFileManager::Get().Copy(*( ReportFolder / TEXT("Approved.png")), *ApprovedFile, true, true) == COPY_OK )
		{
			IFileManager::Get().Copy(*( ReportFolder / TEXT("Approved.json")), *ApprovedMetadataFile, true, true);
			ComparisonResult.ReportApprovedFile = TEXT("Approved.png");
		}

		if ( IFileManager::Get().Copy(*( ReportFolder / TEXT("Incoming.png")), *IncomingFile, true, true) == COPY_OK )
		{
			IFileManager::Get().Copy(*( ReportFolder / TEXT("Incoming.json")), *IncomingMetadataFile, true, true);
			ComparisonResult.ReportIncomingFile = TEXT("Incoming.png");
		}

		if ( IFileManager::Get().Copy(*( ReportFolder / TEXT("Delta.png")), *( ScreenshotDeltaFolder / ComparisonResult.ComparisonFile ), true, true) == COPY_OK )
		{
			ComparisonResult.ReportComparisonFile = TEXT("Delta.png");
		}

		FString Json;
		if ( FJsonObjectConverter::UStructToJsonObjectString(ComparisonResult, Json) )
		{
			FString ComparisonReportFile = ReportFolder / TEXT("Report.json");
			FFileHelper::SaveStringToFile(Json, *ComparisonReportFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
	}

	return ComparisonResult;
}

TFuture<FScreenshotExportResults> FScreenShotManager::ExportComparisonResultsAsync(FString ExportPath)
{
	return Async<FScreenshotExportResults>(EAsyncExecution::Thread, [&] () { return ExportComparisonResults(ExportPath); });
}

FScreenshotExportResults FScreenShotManager::ExportComparisonResults(FString RootExportFolder)
{
	FPaths::NormalizeDirectoryName(RootExportFolder);

	if ( RootExportFolder.IsEmpty() )
	{
		RootExportFolder = GetDefaultExportDirectory();
	}

	FScreenshotExportResults Results;
	Results.Success = false;
	Results.ExportPath = RootExportFolder / FString::FromInt(FEngineVersion::Current().GetChangelist());

	if ( !IFileManager::Get().MakeDirectory(*Results.ExportPath, /*Tree =*/true) )
	{
		return Results;
	}

	// Wait for file operations to complete.
	FPlatformProcess::Sleep(1.0f);

	CopyDirectory(Results.ExportPath, ComparisonResultsFolder);

	Results.Success = true;
	return Results;
}

bool FScreenShotManager::OpenComparisonReports(FString ImportPath, TArray<FComparisonReport>& OutReports)
{
	OutReports.Reset();

	FPaths::NormalizeDirectoryName(ImportPath);
	ImportPath += TEXT("/");

	TArray<FString> ComparisonReportPaths;
	IFileManager::Get().FindFilesRecursive(ComparisonReportPaths, *ImportPath, TEXT("Report.json"), /*Files=*/true, /*Directories=*/false, /*bClearFileNames=*/ false);

	for ( const FString& ReportPath : ComparisonReportPaths )
	{
		FString JsonString;
		if ( FFileHelper::LoadFileToString(JsonString, *ReportPath) )
		{
			TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);

			TSharedPtr<FJsonObject> JsonComparisonReport;
			if ( !FJsonSerializer::Deserialize(JsonReader, JsonComparisonReport) )
			{
				return false;
			}

			FImageComparisonResult ComparisonResult;
			if ( FJsonObjectConverter::JsonObjectToUStruct(JsonComparisonReport.ToSharedRef(), &ComparisonResult, 0, 0) )
			{
				FComparisonReport Report(ImportPath, ReportPath);
				Report.Comparison = ComparisonResult;

				OutReports.Add(Report);
			}
		}
	}

	return true;
}

FString FScreenShotManager::GetDefaultExportDirectory() const
{
	return FPaths::ProjectSavedDir() / TEXT("Exported");
}

void FScreenShotManager::CopyDirectory(const FString& DestDir, const FString& SrcDir)
{
	TArray<FString> FilesToCopy;

	IFileManager::Get().FindFilesRecursive(FilesToCopy, *SrcDir, TEXT("*"), /*Files=*/true, /*Directories=*/true);
	for ( const FString& File : FilesToCopy )
	{
		const FString& SourceFilePath = File;
		FString DestFilePath = DestDir / SourceFilePath.RightChop(SrcDir.Len());

		IFileManager::Get().Copy(*DestFilePath, *File, true, true);
	}
}
