// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Online/Stomp/Private/StompFrame.h"

#if WITH_STOMP

#include "IStompMessage.h"

class FStompClient;
class FStompFrame;

class FStompMessage
	: public IStompMessage
{
public:

	virtual ~FStompMessage()
	{ }

	virtual void Ack(const FStompHeader& Header=FStompHeader(), const FStompRequestCompleted& CompletionCallback = FStompRequestCompleted()) const override;
	virtual void Nack(const FStompHeader& Header=FStompHeader(), const FStompRequestCompleted& CompletionCallback = FStompRequestCompleted()) const override;

	virtual const FStompHeader& GetHeader() const override;
	virtual FString GetBodyAsString() const override;
	virtual const uint8* GetRawBody() const override;
	virtual SIZE_T GetRawBodyLength() const override;

	virtual FStompSubscriptionId GetSubscriptionId() const override;
	virtual FString GetDestination() const override;
	virtual FString GetMessageId() const override;
	virtual FString GetAckId() const override;

private:
	FStompMessage(const TSharedRef<FStompClient>& InClient, const TSharedRef<FStompFrame>& InFrame)
		: ClientPtr(InClient)
		, Frame(InFrame)
	{ }

	TWeakPtr<FStompClient> ClientPtr;
	TSharedRef<FStompFrame> Frame;
	friend class FStompClient;

};

#endif
