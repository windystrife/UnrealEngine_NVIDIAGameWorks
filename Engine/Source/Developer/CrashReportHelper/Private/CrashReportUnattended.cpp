// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CrashReportUnattended.h"
#include "Containers/Ticker.h"
#include "CrashReportConfig.h"
#include "CrashDescription.h"
#include "CrashReportAnalytics.h"

FCrashReportUnattended::FCrashReportUnattended(FPlatformErrorReport& InErrorReport, bool InDeleteFiles)
	: ReceiverUploader(FCrashReportConfig::Get().GetReceiverAddress())
	, DataRouterUploader(FCrashReportConfig::Get().GetDataRouterURL())
	, ErrorReport(InErrorReport)
    , bDeleteReportFiles(InDeleteFiles)
{
	ErrorReport.TryReadDiagnosticsFile();

	// Process the report synchronously
	ErrorReport.DiagnoseReport();

	// Update properties for the crash.
	ErrorReport.SetPrimaryCrashProperties( *FPrimaryCrashProperties::Get() );

    FCrashReportAnalytics::Initialize();
    
	StartTicker();
}

bool FCrashReportUnattended::Tick(float UnusedDeltaTime)
{
	if (!FCrashUploadBase::IsInitialized())
	{
		FCrashUploadBase::StaticInitialize(ErrorReport);
	}

	if (ReceiverUploader.IsEnabled())
	{
		if (!ReceiverUploader.IsUploadCalled())
		{
			// Can be called only when we have all files.
			ReceiverUploader.BeginUpload(ErrorReport);
		}

		// IsWorkDone will always return true here (since ReceiverUploader can't finish until the diagnosis has been sent), but it
		//  has the side effect of joining the worker thread.
		if (!ReceiverUploader.IsFinished())
		{
			// More ticks, please
			return true;
		}
	}

	if (DataRouterUploader.IsEnabled())
	{
		if (!DataRouterUploader.IsUploadCalled())
		{
			// Can be called only when we have all files.
			DataRouterUploader.BeginUpload(ErrorReport);
		}

		// IsWorkDone will always return true here (since DataRouterUploader can't finish until the diagnosis has been sent), but it
		//  has the side effect of joining the worker thread.
		if (!DataRouterUploader.IsFinished())
		{
			// More ticks, please
			return true;
		}
	}

    if (bDeleteReportFiles)
    {
        ErrorReport.DeleteFiles();
    }
    
	// Shutdown analytics.
	FCrashReportAnalytics::Shutdown();

	FPrimaryCrashProperties::Shutdown();
	FPlatformErrorReport::ShutDown();

	return false;
}

void FCrashReportUnattended::StartTicker()
{
	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCrashReportUnattended::Tick), 1.f);
}
