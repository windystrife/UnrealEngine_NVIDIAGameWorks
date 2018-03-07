// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MacErrorReport.h"
#include "CrashReportClientApp.h"
#include "../CrashReportUtil.h"
#include "CrashDebugHelperModule.h"
#include "CrashDebugHelper.h"
#include "FileHelper.h"
#include "PlatformFilemanager.h"

#define LOCTEXT_NAMESPACE "CrashReportClient"

namespace
{
	/** Pointer to dynamically loaded crash diagnosis module */
	FCrashDebugHelperModule* CrashHelperModule;
}

FMacErrorReport::FMacErrorReport(const FString& Directory)
	: FGenericErrorReport(Directory)
{
}

void FMacErrorReport::Init()
{
	CrashHelperModule = &FModuleManager::LoadModuleChecked<FCrashDebugHelperModule>(FName("CrashDebugHelper"));
}

void FMacErrorReport::ShutDown()
{
	CrashHelperModule->ShutdownModule();
}

FString FMacErrorReport::FindCrashedAppPath() const
{
	TArray<uint8> Data;
	if(FFileHelper::LoadFileToArray(Data, *(ReportDirectory / TEXT("Report.wer"))))
	{
		CFStringRef CFString = CFStringCreateWithBytes(NULL, Data.GetData(), Data.Num(), kCFStringEncodingUTF16LE, true);
		FString FileData((NSString*)CFString);
		CFRelease(CFString);
		
		static const TCHAR AppPathLineStart[] = TEXT("AppPath=");
		static const int AppPathIdLength = ARRAY_COUNT(AppPathLineStart) - 1;
		int32 AppPathStart = FileData.Find(AppPathLineStart);
		if(AppPathStart >= 0)
		{
			FString PathData = FileData.Mid(AppPathStart + AppPathIdLength);
			int32 LineEnd = -1;
			if(PathData.FindChar( TCHAR('\r'), LineEnd ))
			{
				PathData = PathData.Left(LineEnd);
			}
			if(PathData.FindChar( TCHAR('\n'), LineEnd ))
			{
				PathData = PathData.Left(LineEnd);
			}
			return PathData;
		}
	}
	else
	{
		UE_LOG(LogStreaming, Error,	TEXT("Failed to read file '%s' error."),*(ReportDirectory / TEXT("Report.wer")));
	}
	return "";
}

void FMacErrorReport::FindMostRecentErrorReports(TArray<FString>& ErrorReportPaths, const FTimespan& MaxCrashReportAge)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FDateTime MinCreationTime = FDateTime::UtcNow() - MaxCrashReportAge;
	auto ReportFinder = MakeDirectoryVisitor([&](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory)
		{
			auto TimeStamp = PlatformFile.GetTimeStamp(FilenameOrDirectory);
			if (TimeStamp > MinCreationTime)
			{
				ErrorReportPaths.Add(FilenameOrDirectory);
			}
		}
		return true;
	});

	FString AllReportsDirectory = FPaths::GameAgnosticSavedDir() / TEXT("Crashes");

	PlatformFile.IterateDirectory(
		*AllReportsDirectory,
		ReportFinder);

	ErrorReportPaths.Sort([&](const FString& L, const FString& R)
	{
		auto TimeStampL = PlatformFile.GetTimeStamp(*L);
		auto TimeStampR = PlatformFile.GetTimeStamp(*R);

		return TimeStampL > TimeStampR;
	});
}

FText FMacErrorReport::DiagnoseReport() const
{
	// Should check if there are local PDBs before doing anything
	ICrashDebugHelper* CrashDebugHelper = CrashHelperModule ? CrashHelperModule->Get() : nullptr;
	if (!CrashDebugHelper)
	{
		// Not localized: should never be seen
		return FText::FromString(TEXT("Failed to load CrashDebugHelper."));
	}
	
	FString DumpFilename;
	if (!FindFirstReportFileWithExtension(DumpFilename, TEXT(".dmp")))
	{
		if (!FindFirstReportFileWithExtension(DumpFilename, TEXT(".mdmp")))
		{
			return FText::FromString("No minidump found for this crash.");
		}
	}
	
	FCrashDebugInfo DebugInfo;
	if (!CrashDebugHelper->ParseCrashDump(ReportDirectory / DumpFilename, DebugInfo))
	{
		return FText::FromString("No minidump found for this crash.");
	}
	
	if ( !CrashDebugHelper->CreateMinidumpDiagnosticReport(ReportDirectory / DumpFilename) )
	{
		return LOCTEXT("NoDebuggingSymbols", "You do not have any debugging symbols required to display the callstack for this crash.");
	}
	else
	{
		FString CrashDump;
		FString DiagnosticsPath = ReportDirectory / FCrashReportClientConfig::Get().GetDiagnosticsFilename();
		CrashDebugHelper->CrashInfo.GenerateReport( DiagnosticsPath );
		if ( FFileHelper::LoadFileToString( CrashDump, *(ReportDirectory / FCrashReportClientConfig::Get().GetDiagnosticsFilename() ) ) )
		{
			return FText::FromString(CrashDump);
		}
		else
		{
			return FText::FromString("Failed to create diagnosis information.");
		}
	}
}

#undef LOCTEXT_NAMESPACE
