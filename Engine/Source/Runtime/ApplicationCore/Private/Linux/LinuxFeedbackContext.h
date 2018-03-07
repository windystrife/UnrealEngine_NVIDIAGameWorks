// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceConsole.h"
#include "Misc/FeedbackContext.h"
#include <syslog.h>

class Error;

/**
 * Feedback context implementation for Linux.
 */
class FLinuxFeedbackContext : public FFeedbackContext
{
	/** Context information for warning and error messages */
	FContextSupplier*	Context;

public:
	// Variables.
	int32					SlowTaskCount;

	// Constructor.
	FLinuxFeedbackContext()
	: FFeedbackContext()
	, Context( NULL )
	, SlowTaskCount( 0 )
	{}

	void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
	{
		// if we set the color for warnings or errors, then reset at the end of the function
		// note, we have to set the colors directly without using the standard SET_WARN_COLOR macro
		if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Warning)
		{
			if (TreatWarningsAsErrors && Verbosity==ELogVerbosity::Warning)
			{
				Verbosity = ELogVerbosity::Error;
			}

			FString Prefix;
			if (Context)
			{
				Prefix = Context->GetContext() + TEXT(" : ");
			}
			FString Format = Prefix + FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V);

			if (Verbosity == ELogVerbosity::Error)
			{
				// Only store off the message if running a commandlet.
				if (IsRunningCommandlet())
				{
					AddError(Format);
				}

				// send errors (warnings are too spammy) to syslog too (for zabbix etc)
				syslog(LOG_ERR | LOG_USER, "%s", StringCast< char >(*Format).Get());
			}
			else
			{
				// Only store off the message if running a commandlet.
				if ( IsRunningCommandlet() )
				{
					AddWarning(Format);
				}
			}
		}

		if( GLogConsole && IsRunningCommandlet() && !GLog->IsRedirectingTo(GLogConsole) )
		{
			GLogConsole->Serialize( V, Verbosity, Category );
		}
		if( !GLog->IsRedirectingTo( this ) )
		{
			GLog->Serialize( V, Verbosity, Category );
		}
	}
	/** Ask the user a binary question, returning their answer */
	virtual bool YesNof( const FText& Question )
	{
		if( ( GIsClient || GIsEditor ) && ( ( GIsSilent != true ) && ( FApp::IsUnattended() != true ) ) )
		{
			//return( ::MessageBox( NULL, TempStr, *NSLOCTEXT("Core", "Question", "Question").ToString(), MB_YESNO|MB_TASKMODAL ) == IDYES);
			STUBBED("+++++++++++++++ LINUXPLATFORMFEEDBACKCONTEXTPRIVATE.H DIALOG BOX PROMPT +++++++++++++++++++");
			return true;
		}
		else
		{
			return false;
		}
	}

	void BeginSlowTask( const FText& Task, bool ShowProgressDialog, bool bShowCancelButton=false )
	{
		GIsSlowTask = ++SlowTaskCount>0;
	}
	void EndSlowTask()
	{
		check(SlowTaskCount>0);
		GIsSlowTask = --SlowTaskCount>0;
	}
	virtual bool StatusUpdate( int32 Numerator, int32 Denominator, const FText& StatusText )
	{
		return true;
	}
	FContextSupplier* GetContext() const
	{
		return Context;
	}
	void SetContext( FContextSupplier* InSupplier )
	{
		Context = InSupplier;
	}
};
