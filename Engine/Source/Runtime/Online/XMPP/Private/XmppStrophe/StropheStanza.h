// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_XMPP_STROPHE

class FXmppConnectionStrophe;
class FStropheContext;
class FXmppUserJid;

typedef struct _xmpp_ctx_t xmpp_ctx_t;
typedef struct _xmpp_conn_t xmpp_conn_t;
typedef struct _xmpp_stanza_t xmpp_stanza_t;

class FStropheStanza
{
	// For TArray<FStropheStanza> Emplace clone from xmp_stanza_t*
	friend class TArray<FStropheStanza>;
	// For Clone constructor access
	friend class FStropheError;
	// For GetStanzaPtr access
	friend class FStropheConnection;

	// For cloning stanzas from our socket
	friend int StropheStanzaEventHandler(xmpp_conn_t* const Connection, xmpp_stanza_t* const IncomingStanza, void* const UserData);

public:
	explicit FStropheStanza(const FXmppConnectionStrophe& Context, const FString& StanzaName = FString());
	FStropheStanza(const FStropheStanza& Other);
	FStropheStanza(FStropheStanza&& Other);
	~FStropheStanza();
	FStropheStanza& operator=(const FStropheStanza& Other);
	FStropheStanza& operator=(FStropheStanza&& Other);

	FStropheStanza Clone();

	void AddChild(const FStropheStanza& Child);
	TOptional<FStropheStanza> GetChild(const FString& ChildName);
	TOptional<const FStropheStanza> GetChild(const FString& ChildName) const;
	TOptional<FStropheStanza> GetChildByNameAndNamespace(const FString& ChildName, const FString& Namespace);
	TOptional<const FStropheStanza> GetChildByNameAndNamespace(const FString& ChildName, const FString& Namespace) const;
	bool HasChild(const FString& ChildName) const;
	bool HasChildByNameAndNamespace(const FString& ChildName, const FString& Namespace) const;

	TArray<FStropheStanza> GetChildren();
	const TArray<FStropheStanza> GetChildren() const;

	void SetNamespace(const FString& Namespace);
	FString GetNamespace() const;

	void SetAttribute(const FString& Key, const FString& Value);
	FString GetAttribute(const FString& Key) const;
	bool HasAttribute(const FString& Key) const;

	void SetName(const FString& Name);
	FString GetName() const;

	void SetText(const FString& Text);
	FString GetText() const;

	void SetType(const FString& Type);
	FString GetType() const;

	void SetId(const FString& Id);
	FString GetId() const;

	void SetTo(const FXmppUserJid& To);
	void SetTo(const FString& To);
	FXmppUserJid GetTo() const;

	void SetFrom(const FXmppUserJid& From);
	void SetFrom(const FString& From);
	FXmppUserJid GetFrom() const;

	// Helpers for Message stanzas */

	/** Add a child stanza of name Body with the requested text.  Fails if we already have a body stanza, or if we are a text stanza. */
	bool AddBodyWithText(const FString& Text);
	/** Get the text from a child Body stanza, if one exists */
	TOptional<FString> GetBodyText() const;

protected:
	/** Get the current stanza */
	xmpp_stanza_t* GetStanzaPtr() const { return XmppStanzaPtr; }

	/** Passed in stanzas will be cloned instead of copied here */
	explicit FStropheStanza(xmpp_stanza_t* const OtherStanzaPtr);

	/** Create a new stanza directly from a context */
	explicit FStropheStanza(xmpp_ctx_t* const StropheContextPtr);

protected:
	/** Ref-counted pointer to libstrophe stanza data*/
	xmpp_stanza_t* XmppStanzaPtr;
};

#endif
