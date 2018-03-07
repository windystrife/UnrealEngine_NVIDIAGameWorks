// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/MessageDialog.h"
#include "Misc/CString.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/FeedbackContext.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"

namespace
{
	/**
	 * Singleton to only create error title text if needed (and after localization system is in place)
	 */
	const FText& GetDefaultMessageTitle()
	{
		// Will be initialised on first call
		static FText DefaultMessageTitle(NSLOCTEXT("MessageDialog", "DefaultMessageTitle", "Message"));
		return DefaultMessageTitle;
	}
}

void FMessageDialog::Debugf( const FText& Message, const FText* OptTitle )
{
	if( FApp::IsUnattended() == true )
	{
		GLog->Logf( TEXT("%s"), *Message.ToString() );
	}
	else
	{
		FText Title = OptTitle ? *OptTitle : GetDefaultMessageTitle();
		if ( GIsEditor && FCoreDelegates::ModalErrorMessage.IsBound() )
		{
			FCoreDelegates::ModalErrorMessage.Execute(EAppMsgType::Ok, Message, Title);
		}
		else
		{
			FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *Message.ToString(), *NSLOCTEXT("MessageDialog", "DefaultDebugMessageTitle", "ShowDebugMessagef").ToString() );
		}
	}
}

void FMessageDialog::ShowLastError()
{
	uint32 LastError = FPlatformMisc::GetLastError();

	TCHAR TempStr[MAX_SPRINTF]=TEXT("");
	TCHAR ErrorBuffer[1024];
	FCString::Sprintf( TempStr, TEXT("GetLastError : %d\n\n%s"), LastError, FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, 0) );
	if( FApp::IsUnattended() == true )
	{
		UE_LOG(LogOutputDevice, Fatal, TempStr);
	}
	else
	{
		FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, TempStr, *NSLOCTEXT("MessageDialog", "DefaultSystemErrorTitle", "System Error").ToString() );
	}
}

EAppReturnType::Type FMessageDialog::Open( EAppMsgType::Type MessageType, const FText& Message, const FText* OptTitle )
{
	if( FApp::IsUnattended() == true )
	{
		if (GWarn)
		{
			GWarn->Logf( TEXT("%s"), *Message.ToString() );
		}

		switch(MessageType)
		{
		case EAppMsgType::Ok:
			return EAppReturnType::Ok;
		case EAppMsgType::YesNo:
			return EAppReturnType::No;
		case EAppMsgType::OkCancel:
			return EAppReturnType::Cancel;
		case EAppMsgType::YesNoCancel:
			return EAppReturnType::Cancel;
		case EAppMsgType::CancelRetryContinue:
			return EAppReturnType::Cancel;
		case EAppMsgType::YesNoYesAllNoAll:
			return EAppReturnType::No;
		case EAppMsgType::YesNoYesAllNoAllCancel:
		default:
			return EAppReturnType::Yes;
			break;
		}
	}
	else
	{
		FText Title = OptTitle ? *OptTitle : GetDefaultMessageTitle();
		if ( GIsEditor && !IsRunningCommandlet() && FCoreDelegates::ModalErrorMessage.IsBound() )
		{
			return FCoreDelegates::ModalErrorMessage.Execute( MessageType, Message, Title );
		}
		else
		{
			return FPlatformMisc::MessageBoxExt( MessageType, *Message.ToString(), *Title.ToString() );
		}
	}
}
