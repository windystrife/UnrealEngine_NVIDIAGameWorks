// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/UdpSocketReceiver.h"
#include "IMessageAttachment.h"
#include "IMessageContext.h"

/**
 * Holds a deserialized message.
 */
class FTcpDeserializedMessage
	: public IMessageContext
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InAttachment An optional message attachment.
	 */
	FTcpDeserializedMessage(const TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe>& InAttachment);

	/** Virtual destructor. */
	virtual ~FTcpDeserializedMessage() override;

public:

	/**
	 * Deserializes the given reassembled message.
	 *
	 * @param ReassembledMessage The reassembled message to deserialize.
	 * @return true on success, false otherwise.
	 */
	bool Deserialize(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Message);

public:

	// IMessageContext interface

	virtual const TMap<FName, FString>& GetAnnotations() const override;
	virtual TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe> GetAttachment() const override;
	virtual const FDateTime& GetExpiration() const override;
	virtual const void* GetMessage() const override;
	virtual const TWeakObjectPtr<UScriptStruct>& GetMessageTypeInfo() const override;
	virtual TSharedPtr<IMessageContext, ESPMode::ThreadSafe> GetOriginalContext() const override;
	virtual const TArray<FMessageAddress>& GetRecipients() const override;
	virtual EMessageScope GetScope() const override;
	virtual const FMessageAddress& GetSender() const override;
	virtual ENamedThreads::Type GetSenderThread() const override;
	virtual const FDateTime& GetTimeForwarded() const override;
	virtual const FDateTime& GetTimeSent() const override;

private:

	/** Holds the optional message annotations. */
	TMap<FName, FString> Annotations;

	/** Holds a pointer to attached binary data. */
	TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe> Attachment;

	/** Holds the expiration time. */
	FDateTime Expiration;

	/** Holds the message. */
	void* MessageData;

	/** Holds the message recipients. */
	TArray<FMessageAddress> Recipients;

	/** Holds the message's scope. */
	EMessageScope Scope;

	/** Holds the sender's identifier. */
	FMessageAddress Sender;

	/** Holds the time at which the message was sent. */
	FDateTime TimeSent;

	/** Holds the message's type information. */
	TWeakObjectPtr<UScriptStruct> TypeInfo;
};

