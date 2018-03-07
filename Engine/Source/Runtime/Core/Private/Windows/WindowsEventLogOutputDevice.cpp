// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WindowsEventLogOutputDevice.h"
#include "WindowsHWrapper.h"
#include "Containers/UnrealString.h"
#include "Misc/Parse.h"
#include "Misc/App.h"
#include "CoreGlobals.h"

FWindowsEventLogOutputDevice::FWindowsEventLogOutputDevice()
	: EventLog(NULL)
{
	FString InstanceName;
	FString ServerName;
	// Build a name to uniquely identify this instance
	if (FParse::Value(FCommandLine::Get(),TEXT("-Login="),ServerName))
	{
		InstanceName = FApp::GetProjectName();
		InstanceName += ServerName;
	}
	else
	{
		uint32 ProcID = GetCurrentProcessId();
		InstanceName = FString::Printf(TEXT("%s-PID%d"),FApp::GetProjectName(),ProcID);
	}
	// Open the event log using the name built above
	EventLog = RegisterEventSource(NULL,*InstanceName);
	if (EventLog == NULL)
	{
		UE_LOG(LogWindows, Error,TEXT("Failed to open the Windows Event Log for writing (%d)"),GetLastError());
	}
}

FWindowsEventLogOutputDevice::~FWindowsEventLogOutputDevice()
{
	TearDown();
}


void FWindowsEventLogOutputDevice::Serialize(const TCHAR* Buffer, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if (EventLog != NULL)
	{
		// Only forward errors and warnings to the event log
		switch (Verbosity)
		{
		case ELogVerbosity::Error:
			{
				ReportEvent(EventLog,
					EVENTLOG_ERROR_TYPE,
					NULL,
					0xC0000001L,
					NULL,
					1,
					0,
					&Buffer,
					NULL);
				break;
			}
		case ELogVerbosity::Warning:
			{
				ReportEvent(EventLog,
					EVENTLOG_WARNING_TYPE,
					NULL,
					0x80000002L,
					NULL,
					1,
					0,
					&Buffer,
					NULL);
				break;
			}
		}
	}
}
	
void FWindowsEventLogOutputDevice::Flush(void)
{
}

void FWindowsEventLogOutputDevice::TearDown(void)
{
	if (EventLog != NULL)
	{
		DeregisterEventSource(EventLog);
		EventLog = NULL;
	}
}
