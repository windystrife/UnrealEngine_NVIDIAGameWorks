// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppStrophe/XmppStrophe.h"
#include "XmppStrophe/XmppConnectionStrophe.h"
#include "XmppStrophe/StropheContext.h"

#if WITH_XMPP_STROPHE

THIRD_PARTY_INCLUDES_START
#include "strophe.h"
THIRD_PARTY_INCLUDES_END

void FXmppStrophe::Init()
{
	// Init libstrophe
	xmpp_initialize();
}

void FXmppStrophe::Cleanup()
{
	// Cleanup libstrophe
	xmpp_shutdown();
}

TSharedRef<IXmppConnection> FXmppStrophe::CreateConnection()
{
	return MakeShareable(new FXmppConnectionStrophe());
}

FString FXmppStrophe::JidToString(const FXmppUserJid& UserJid)
{
	return UserJid.GetFullPath();
}

FXmppUserJid FXmppStrophe::JidFromString(const FString& JidString)
{
	FString User;
	FString Domain;
	FString Resource;

	FString DomainAndResource;
	if (JidString.Split(TEXT("@"), &User, &DomainAndResource, ESearchCase::CaseSensitive, ESearchDir::FromStart))
	{
		if (!DomainAndResource.Split(TEXT("/"), &Domain, &Resource, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			// If we don't have a resource, the domain is all of DomainAndResource
			Domain = MoveTemp(DomainAndResource);
		}
	}
	else
	{
		if (!JidString.Split(TEXT("/"), &User, &Resource), ESearchCase::CaseSensitive, ESearchDir::FromEnd)
		{
			// If we don't have a resource, we only have the user in the JidString
			User = JidString;
		}
	}

	return FXmppUserJid(MoveTemp(User), MoveTemp(Domain), MoveTemp(Resource));
}

FXmppUserJid FXmppStrophe::JidFromStropheString(const char* StropheJidString)
{
	return JidFromString(FString(UTF8_TO_TCHAR(StropheJidString)));
}

#endif
