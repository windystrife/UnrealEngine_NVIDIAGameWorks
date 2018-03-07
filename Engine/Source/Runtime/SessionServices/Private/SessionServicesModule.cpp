// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "IMessagingModule.h"
#include "ISessionManager.h"
#include "SessionManager.h"
#include "ISessionService.h"
#include "SessionService.h"
#include "ISessionServicesModule.h"


/**
 * Implements the SessionServices module.
 *
 * @todo gmp: needs better error handling in case MessageBusPtr is invalid
 */
class FSessionServicesModule
	: public FSelfRegisteringExec
	, public ISessionServicesModule
{
public:

	/** Default constructor. */
	FSessionServicesModule()
		: SessionManager(nullptr)
		, SessionService(nullptr)
	{ }

public:

	//~ FSelfRegisteringExec interface

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (!FParse::Command(&Cmd, TEXT("SESSION")))
		{
			return false;
		}

		if (FParse::Command(&Cmd, TEXT("AUTH")))
		{
			FApp::AuthorizeUser(FParse::Token(Cmd, false));
		}
		else if (FParse::Command(&Cmd, TEXT("DENY")))
		{
			FApp::DenyUser(FParse::Token(Cmd, false));
		}
		else if (FParse::Command(&Cmd, TEXT("DENYALL")))
		{
			FApp::DenyAllUsers();
		}
		else if (FParse::Command(&Cmd, TEXT("STATUS")))
		{
			Ar.Logf(TEXT("Instance ID: %s"), *FApp::GetInstanceId().ToString());
			Ar.Logf(TEXT("Instance Name: %s"), *FApp::GetInstanceName());
			Ar.Logf(TEXT("Session ID: %s"), *FApp::GetSessionId().ToString());
			Ar.Logf(TEXT("Session Name: %s"), *FApp::GetSessionName());
			Ar.Logf(TEXT("Session Owner: %s"), *FApp::GetSessionOwner());
			Ar.Logf(TEXT("Standalone: %s"), FApp::IsStandalone() ? *GYes.ToString() : *GNo.ToString());
		}
		else if (FParse::Command(&Cmd, TEXT("SETNAME")))
		{
			FApp::SetSessionName(FParse::Token(Cmd, false));
		}
		else if (FParse::Command(&Cmd, TEXT("SETOWNER")))
		{
			FApp::SetSessionOwner(FParse::Token(Cmd, false));
		}
		else
		{
			// show usage
			Ar.Log(TEXT("Usage: SESSION <Command>"));
			Ar.Log(TEXT(""));
			Ar.Log(TEXT("Command"));
			Ar.Log(TEXT("    AUTH <UserName> = Authorize a user to interact with this instance"));
			Ar.Log(TEXT("    DENY <UserName> = Unauthorize a user from interacting with this instance"));
			Ar.Log(TEXT("    DENYALL = Removes all authorized session users"));
			Ar.Log(TEXT("    SETNAME <Name> = Sets the name of this instance"));
			Ar.Log(TEXT("    SETOWNER <Owner> = Sets the name of the owner of this instance"));
			Ar.Log(TEXT("    STATUS = Displays the status of this application session"));
		}

		return true;
	}

public:

	//~ ISessionServicesModule interface

	virtual TSharedRef<ISessionManager> GetSessionManager() override
	{
		if (!SessionManager.IsValid())
		{
			SessionManager = MakeShareable(new FSessionManager(MessageBusPtr.Pin().ToSharedRef()));
		}

		return SessionManager.ToSharedRef();
	}

	virtual TSharedRef<ISessionService> GetSessionService() override
	{
		if (!SessionService.IsValid())
		{
			SessionService = MakeShareable(new FSessionService(MessageBusPtr.Pin().ToSharedRef()));
		}

		return SessionService.ToSharedRef();
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		MessageBusPtr = IMessagingModule::Get().GetDefaultBus();
		check(MessageBusPtr.IsValid());
	}

	virtual void ShutdownModule() override
	{
		SessionManager.Reset();
		SessionService.Reset();
	}

private:
	
	/** Holds a weak pointer to the message bus. */
	TWeakPtr<IMessageBus, ESPMode::ThreadSafe> MessageBusPtr;

	/** Holds the session manager singleton. */
	TSharedPtr<ISessionManager> SessionManager;

	/** Holds the session service singleton. */
	TSharedPtr<ISessionService> SessionService;
};


IMPLEMENT_MODULE(FSessionServicesModule, SessionServices);
