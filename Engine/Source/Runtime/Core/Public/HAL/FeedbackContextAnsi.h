// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/FeedbackContext.h"

class Error;

/*-----------------------------------------------------------------------------
	FFeedbackContextAnsi.
-----------------------------------------------------------------------------*/

//
// Feedback context.
//
class FFeedbackContextAnsi : public FFeedbackContext
{
public:
	// Variables.
	FContextSupplier*	Context;
	FOutputDevice*		AuxOut;

	// Local functions.
	void LocalPrint( const TCHAR* Str )
	{
#if PLATFORM_APPLE || PLATFORM_LINUX
		printf("%s", TCHAR_TO_ANSI(Str));
#elif PLATFORM_WINDOWS
		wprintf(TEXT("%ls"), Str);
#else
		// If this function ever gets more complicated, we could make a PlatformMisc::Printf, and each platform can then 
		// do the right thing. For instance, LocalPrint is OutputDebugString on Windows, which messes up a lot of stuff
		FPlatformMisc::LocalPrint(Str);
#endif
	}

	// Constructor.
	FFeedbackContextAnsi()
	: FFeedbackContext()
	, Context(nullptr)
	, AuxOut(nullptr)
	{}
	void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override
	{
		// When -stdout is specified then FOutputDeviceStdOutput will be installed and pipe logging to stdout. 
		// If so don't use LocalPrint or else duplicate messages will be written to stdout
		static bool bUsingStdOut = FParse::Param(FCommandLine::Get(), TEXT("stdout"));

		if (bUsingStdOut == false &&
			(Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Warning || Verbosity == ELogVerbosity::Display))
		{
			if( TreatWarningsAsErrors && Verbosity==ELogVerbosity::Warning )
			{
				Verbosity = ELogVerbosity::Error;
			}

			FString Prefix;
			if( Context )
			{
				Prefix = Context->GetContext() + TEXT(" : ");
			}
			FString Format = Prefix + FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V);

			// Only store off the message if running a commandlet.
			if ( IsRunningCommandlet() )
			{
				if (Verbosity == ELogVerbosity::Error)
				{
					AddError(Format);
				}
				else if (Verbosity == ELogVerbosity::Warning)
				{
					AddWarning(Format);
				}
			}
			LocalPrint(*Format);
			LocalPrint( TEXT("\n") );
		}
		else if (Verbosity == ELogVerbosity::SetColor)
		{
		}
		if( !GLog->IsRedirectingTo( this ) )
			GLog->Serialize( V, Verbosity, Category );
		if( AuxOut )
			AuxOut->Serialize( V, Verbosity, Category );
		fflush( stdout );
	}

	virtual bool YesNof( const FText& Question ) override
	{
		if( (GIsClient || GIsEditor) )
		{
			LocalPrint( *Question.ToString() );
			LocalPrint( TEXT(" (Y/N): ") );
			if( ( GIsSilent == true ) || ( FApp::IsUnattended() == true ) )
			{
				LocalPrint( TEXT("Y") );
				return true;
			}
			else
			{
				char InputText[256];
				if (fgets( InputText, sizeof(InputText), stdin ) != nullptr)
				{
					return (InputText[0]=='Y' || InputText[0]=='y');
				}
			}
		}
		return true;
	}

	void SetContext( FContextSupplier* InSupplier ) override
	{
		Context = InSupplier;
	}
};

