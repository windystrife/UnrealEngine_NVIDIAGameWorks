// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Logging/EventLogger.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceFile.h"
#include "SlateGlobals.h"
#include "Widgets/SWidget.h"


/* Local helper functions
 *****************************************************************************/

static FString PrettyPrint(EEventLog::Type Event, const FString& AdditionalContent = FString(), TSharedPtr<SWidget> Widget = TSharedPtr<SWidget>())
{
	FString WidgetInfo;
	FString Content;

	if (Widget.IsValid())
	{
		WidgetInfo = FString::Printf(TEXT(", %s %s"), *Widget->GetTypeAsString(), *Widget->GetReadableLocation());
	}

	if (AdditionalContent.Len())
	{
		Content = FString::Printf(TEXT(": %s"), *AdditionalContent);
	}

	return FString::Printf(TEXT("[%07.2f] %s%s%s"), FPlatformTime::Seconds() - GStartTime, *EEventLog::ToString(Event), *Content, *WidgetInfo);
}


/* FFileEventLogger interface
 *****************************************************************************/

FString FFileEventLogger::GetLog( ) const
{
	FString OutputLog = LINE_TERMINATOR LINE_TERMINATOR;
	
	for (int32 LoggedEventIndex = 0; LoggedEventIndex < LoggedEvents.Num(); ++LoggedEventIndex)
	{
		OutputLog += LoggedEvents[LoggedEventIndex] + LINE_TERMINATOR;
	}

	return OutputLog;
}


void FFileEventLogger::Log(EEventLog::Type Event, const FString& AdditionalContent, TSharedPtr<SWidget> Widget)
{
	if (Widget.IsValid())
	{
		LoggedEvents.Add( PrettyPrint(Event, AdditionalContent, Widget) );
	}
}

void FFileEventLogger::SaveToFile()
{
	FString LogFilePath;
	{
		const TCHAR LogFileName[] = TEXT("EventLog");
		LogFilePath = FPaths::CreateTempFilename(*FPaths::ProjectLogDir(), LogFileName, TEXT(".log"));
	}

	FOutputDeviceFile EventLogFile(*LogFilePath);

	for (int32 LoggedEventIndex = 0; LoggedEventIndex < LoggedEvents.Num(); ++LoggedEventIndex)
	{
		EventLogFile.Serialize(*LoggedEvents[LoggedEventIndex], ELogVerbosity::Log, NAME_None);
	}

	EventLogFile.Flush();
	EventLogFile.TearDown();
}


/* FConsoleEventLogger interface
 *****************************************************************************/

FString FConsoleEventLogger::GetLog( ) const
{
	return FString();
}

void FConsoleEventLogger::Log( EEventLog::Type Event, const FString& AdditionalContent, TSharedPtr<SWidget> Widget )
{
	UE_LOG(LogSlate, Log, TEXT("%s"), *PrettyPrint(Event, AdditionalContent, Widget));
}


/* FStabilityEventLogger interface
 *****************************************************************************/

/** Limit of how many items we should have in our stability log */
static const int32 STABILITY_LOG_MAX_SIZE = 100;

FString FStabilityEventLogger::GetLog() const
{
	FString OutputLog = LINE_TERMINATOR LINE_TERMINATOR;

	for (int32 LoggedEventIndex = 0; LoggedEventIndex < LoggedEvents.Num(); ++LoggedEventIndex)
	{
		OutputLog += LoggedEvents[LoggedEventIndex] + LINE_TERMINATOR;
	}

	return OutputLog;
}


void FStabilityEventLogger::Log(EEventLog::Type Event, const FString& AdditionalContent, TSharedPtr<SWidget> Widget)
{
	// filter out events that happen a lot
	if (Event == EEventLog::MouseMove ||
		Event == EEventLog::MouseEnter ||
		Event == EEventLog::MouseLeave ||
		Event == EEventLog::DragEnter ||
		Event == EEventLog::DragLeave ||
		Event == EEventLog::DragOver)
	{
		return;
	}

	if (Widget.IsValid())
	{
		LoggedEvents.Add(PrettyPrint(Event, AdditionalContent, Widget));
	}
	else
	{
		LoggedEvents.Add(PrettyPrint(Event, AdditionalContent));
	}

	if (LoggedEvents.Num() > STABILITY_LOG_MAX_SIZE)
	{
		LoggedEvents.RemoveAt(0, LoggedEvents.Num() - STABILITY_LOG_MAX_SIZE);
	}
}
