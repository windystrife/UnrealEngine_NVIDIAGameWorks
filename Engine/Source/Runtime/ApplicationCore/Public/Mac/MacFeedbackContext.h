// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FeedbackContext.h"
#include "Misc/OutputDeviceConsole.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/OutputDeviceRedirector.h"

/**
 * Feedback context implementation for Mac.
 */
class FMacFeedbackContext : public FFeedbackContext
{
	/** Context information for warning and error messages */
	FContextSupplier*	Context;

public:
	// Constructor.
	FMacFeedbackContext()
		: FFeedbackContext()
		, Context( NULL )
	{}

	void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
	{
		// if we set the color for warnings or errors, then reset at the end of the function
		// note, we have to set the colors directly without using the standard SET_WARN_COLOR macro
		bool bNeedToResetColor = false;
		if( Verbosity==ELogVerbosity::Error || Verbosity==ELogVerbosity::Warning )
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

			if(Verbosity == ELogVerbosity::Error)
			{
				Serialize(COLOR_RED, ELogVerbosity::SetColor, Category); bNeedToResetColor = true;
				// Only store off the message if running a commandlet.
				if ( IsRunningCommandlet() )
				{
					AddError(Format);
				}
			}
			else
			{
				Serialize(COLOR_YELLOW, ELogVerbosity::SetColor, Category); bNeedToResetColor = true;
				// Only store off the message if running a commandlet.
				if ( IsRunningCommandlet() )
				{
					AddWarning(Format);
				}
			}
		}

		if( GLogConsole && IsRunningCommandlet() && !GLog->IsRedirectingTo(GLogConsole) )
			GLogConsole->Serialize( V, Verbosity, Category );
		if( !GLog->IsRedirectingTo( this ) )
			GLog->Serialize( V, Verbosity, Category );

		if (bNeedToResetColor)
		{
			Serialize(COLOR_NONE, ELogVerbosity::SetColor, Category);
		}
	}

	bool YesNof( const FText& Question )
	{
		return FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *Question.ToString(), TEXT("")) == EAppReturnType::Yes;
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

