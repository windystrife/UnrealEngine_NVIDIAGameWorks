// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Logging/MessageLog.h"
#include "Internationalization/Internationalization.h"
#include "Logging/IMessageLog.h"

FMessageLog::FGetLog FMessageLog::GetLog;

FMessageLog::FMessageSelectionChanged FMessageLog::MessageSelectionChanged;

#define LOCTEXT_NAMESPACE "MessageLog"

class FBasicMessageLog : public IMessageLog, public TSharedFromThis<FBasicMessageLog>
{
public:
	FBasicMessageLog( const FName& InLogName )
		: LogName( InLogName )
	{
	}

	/** Begin IMessageLog interface */
	virtual void AddMessage( const TSharedRef<FTokenizedMessage>& NewMessage, bool bMirrorToOutputLog ) override
	{
		AddMessageInternal(NewMessage, bMirrorToOutputLog);
	}

	virtual void AddMessages( const TArray< TSharedRef<FTokenizedMessage> >& NewMessages, bool bMirrorToOutputLog ) override
	{
		for(TArray< TSharedRef<FTokenizedMessage> >::TConstIterator It = NewMessages.CreateConstIterator(); It; It++)
		{
			AddMessageInternal(*It, bMirrorToOutputLog);
		}
	}

	virtual void NewPage( const FText& Title ) override
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("PageTitle"), Title);
		FMsg::Logf(__FILE__, __LINE__, LogName, ELogVerbosity::Log, TEXT("%s"), *FText::Format(LOCTEXT("BasicMessageLog_NewPage", "New Page: {PageTitle}"), Arguments).ToString());
	}

	virtual void NotifyIfAnyMessages( const FText& Message, EMessageSeverity::Type SeverityFilter, bool bForce ) override
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Message"), Message);
		FMsg::Logf(__FILE__, __LINE__, LogName, ELogVerbosity::Log, TEXT("%s"), *FText::Format(LOCTEXT("BasicMessageLog_Notify", "Notify: {Message}"), Arguments).ToString());
	}

	virtual void Open() override
	{
		FMsg::Logf(__FILE__, __LINE__, LogName, ELogVerbosity::Log, TEXT("%s"), *LOCTEXT("BasicMessageLog_Open", "Open Log").ToString());
	}

	virtual int32 NumMessages( EMessageSeverity::Type SeverityFilter ) override
	{
		return 0;
	}
	/** End IMessageLog interface */

private:
	void AddMessageInternal( const TSharedRef<FTokenizedMessage>& Message, bool bMirrorToOutputLog )
	{
		if (bMirrorToOutputLog)
		{
			const TCHAR* const LogColor = FMessageLog::GetLogColor(Message->GetSeverity());
			if (LogColor)
			{
				SET_WARN_COLOR(LogColor);
			}
			FMsg::Logf(__FILE__, __LINE__, LogName, FMessageLog::GetLogVerbosity(Message->GetSeverity()), TEXT("%s"), *Message->ToText().ToString());
			CLEAR_WARN_COLOR();
		}
	}

private:
	/** The name of this log */
	FName LogName;
};

FMessageLog::FMessageLog( const FName& InLogName )
	: bSuppressLoggingToOutputLog(false)
{
	if(GetLog.IsBound())
	{
		MessageLog = GetLog.Execute(InLogName);
	}
	else
	{
		MessageLog = MakeShareable( new FBasicMessageLog(InLogName) );
	}
}

FMessageLog::~FMessageLog()
{
	Flush();
}

const TSharedRef<FTokenizedMessage>& FMessageLog::AddMessage( const TSharedRef<FTokenizedMessage>& InMessage )
{
	Messages.Add(InMessage);

	return InMessage;
}

void FMessageLog::AddMessages( const TArray< TSharedRef<FTokenizedMessage> >& InMessages )
{
	Messages.Append(InMessages);
}

TSharedRef<FTokenizedMessage> FMessageLog::Message( EMessageSeverity::Type InSeverity, const FText& InMessage )
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(InSeverity, InMessage);
	Messages.Add(Message);
	return Message;
}

TSharedRef<FTokenizedMessage> FMessageLog::CriticalError( const FText& InMessage )
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::CriticalError, InMessage);
	Messages.Add(Message);
	return Message;
}

TSharedRef<FTokenizedMessage> FMessageLog::Error( const FText& InMessage )
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error, InMessage);
	Messages.Add(Message);
	return Message;
}

TSharedRef<FTokenizedMessage> FMessageLog::PerformanceWarning( const FText& InMessage )
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::PerformanceWarning, InMessage);
#if !PLATFORM_LINUX // @todo: these are too spammy for now on Linux
	Messages.Add(Message);
#endif // !PLATFORM_LINUX
	return Message;
}

TSharedRef<FTokenizedMessage> FMessageLog::Warning( const FText& InMessage )
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning, InMessage);
#if !PLATFORM_LINUX // @todo: these are too spammy for now on Linux
	Messages.Add(Message); // TODO These are too spammy for now
#endif // !PLATFORM_LINUX
	return Message;
}

TSharedRef<FTokenizedMessage> FMessageLog::Info( const FText& InMessage )
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Info, InMessage);
	Messages.Add(Message);
	return Message;
}

int32 FMessageLog::NumMessages( EMessageSeverity::Type InSeverityFilter )
{
	Flush();
	return MessageLog->NumMessages(InSeverityFilter);
}

void FMessageLog::Open( EMessageSeverity::Type InSeverityFilter, bool bOpenEvenIfEmpty )
{
	Flush();
	if(bOpenEvenIfEmpty)
	{
		MessageLog->Open();
	}
	else if(MessageLog->NumMessages(InSeverityFilter) > 0)
	{
		MessageLog->Open();
	}
}

void FMessageLog::Notify( const FText& InMessage, EMessageSeverity::Type InSeverityFilter, bool bForce )
{
	Flush();
	MessageLog->NotifyIfAnyMessages(InMessage, InSeverityFilter, bForce);
}

void FMessageLog::NewPage( const FText& InLabel )
{
	Flush();
	MessageLog->NewPage(InLabel);
}

FMessageLog& FMessageLog::SuppressLoggingToOutputLog(bool bShouldSuppress)
{
	bSuppressLoggingToOutputLog = bShouldSuppress;
	return *this;
}

void FMessageLog::Flush()
{
	if(Messages.Num() > 0)
	{
		MessageLog->AddMessages(Messages, !bSuppressLoggingToOutputLog);
		Messages.Empty();
	}
}

ELogVerbosity::Type FMessageLog::GetLogVerbosity( EMessageSeverity::Type InSeverity )
{
	switch(InSeverity)
	{
	case EMessageSeverity::CriticalError:
		return ELogVerbosity::Fatal;
	case EMessageSeverity::Error:
		return ELogVerbosity::Error;
	case EMessageSeverity::PerformanceWarning:
	case EMessageSeverity::Warning:
		return ELogVerbosity::Warning;
	case EMessageSeverity::Info:
	default:
		return ELogVerbosity::Log;
	}
}

const TCHAR* const FMessageLog::GetLogColor( EMessageSeverity::Type InSeverity )
{
#if !PLATFORM_DESKTOP
	return NULL;
#else
	switch(InSeverity)
	{
	case EMessageSeverity::CriticalError:
		return OutputDeviceColor::COLOR_RED;
	case EMessageSeverity::PerformanceWarning:
	case EMessageSeverity::Warning:
		return OutputDeviceColor::COLOR_YELLOW;
	case EMessageSeverity::Info:
	default:
		return NULL;
	}
#endif
}

#undef LOCTEXT_NAMESPACE
