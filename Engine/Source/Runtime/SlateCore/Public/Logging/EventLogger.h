// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/IEventLogger.h"

class SWidget;

/**
 * Implements an event logger that forwards events to two other event loggers.
 */

class FMultiTargetLogger
	: public IEventLogger
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InLoggerA The first logger to forward events to.
	 * @param InLoggerB The second logger to forward events to.
	 */
	FMultiTargetLogger( const TSharedRef<IEventLogger>& InLoggerA, const TSharedRef<IEventLogger>& InLoggerB )
		: LoggerA(InLoggerA)
		, LoggerB(InLoggerB)
	{ }

public:

	// IEventLogger interface

	virtual FString GetLog( ) const override
	{
		return LoggerA->GetLog() + TEXT("\n") + LoggerB->GetLog();
	}

	virtual void Log(EEventLog::Type Event, const FString& AdditionalContent, TSharedPtr<SWidget> Widget) override
	{
		LoggerA->Log(Event, AdditionalContent, Widget);
		LoggerB->Log(Event, AdditionalContent, Widget);
	}

	virtual void SaveToFile( ) override
	{
		LoggerA->SaveToFile();
		LoggerB->SaveToFile();
	}

private:

	TSharedRef<IEventLogger> LoggerA;
	TSharedRef<IEventLogger> LoggerB;
};


/**
 * Implements an event logger that logs to a file.
 */
class SLATECORE_API FFileEventLogger
	: public IEventLogger
{
public:

	// IEventLogger interface

	virtual FString GetLog( ) const override;
	virtual void Log( EEventLog::Type Event, const FString& AdditionalContent, TSharedPtr<SWidget> Widget ) override;
	virtual void SaveToFile( ) override;

private:

	// Holds the collection of logged events.
	TArray<FString> LoggedEvents;
};


class SLATECORE_API FConsoleEventLogger
	: public IEventLogger
{
public:

	// IEventLogger interface

	virtual FString GetLog( ) const override;
	virtual void Log( EEventLog::Type Event, const FString& AdditionalContent, TSharedPtr<SWidget> Widget ) override;
	virtual void SaveToFile( ) override { }
};


/**
 * Implements an event logger that logs events related to assertions and ensures.
 */
class SLATECORE_API FStabilityEventLogger
	: public IEventLogger
{
public:

	// IEventLogger interface
	
	virtual FString GetLog( ) const override;
	virtual void Log( EEventLog::Type Event, const FString& AdditionalContent, TSharedPtr<SWidget> Widget ) override;
	virtual void SaveToFile( ) override { }

private:

	// Holds the collection of logged events.
	TArray<FString> LoggedEvents;
};
