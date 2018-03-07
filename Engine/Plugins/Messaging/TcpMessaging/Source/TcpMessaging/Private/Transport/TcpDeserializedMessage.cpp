// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Transport/TcpDeserializedMessage.h"
#include "UObject/Package.h"
#include "TcpMessagingPrivate.h"
#include "Backends/JsonStructDeserializerBackend.h"
#include "StructDeserializer.h"


/* FTcpDeserializedMessage structors
*****************************************************************************/

FTcpDeserializedMessage::FTcpDeserializedMessage(const TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe>& InAttachment)
	: Attachment(InAttachment)
	, MessageData(nullptr)
{ }


FTcpDeserializedMessage::~FTcpDeserializedMessage()
{
	if (MessageData != nullptr)
	{
		if (TypeInfo.IsValid())
		{
			TypeInfo->DestroyStruct(MessageData);
		}

		FMemory::Free(MessageData);
		MessageData = nullptr;
	}
}


/* FTcpDeserializedMessage interface
 *****************************************************************************/

bool FTcpDeserializedMessage::Deserialize(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Message)
{
	FArrayReader& MessageReader = Message.ToSharedRef().Get();

	// Note that some complex values are deserialized manually here, so that we
	// can sanity check their values. @see FTcpSerializeMessageTask::DoTask()
	MessageReader.ArMaxSerializeSize = NAME_SIZE;

	// message type info
	{
		FName MessageType;
		MessageReader << MessageType;

		// @todo gmp: cache message types for faster lookup
		TypeInfo = FindObjectSafe<UScriptStruct>(ANY_PACKAGE, *MessageType.ToString());

		if (!TypeInfo.IsValid(false, true))
		{
			return false;
		}
	}

	// sender address
	{
		MessageReader << Sender;
	}

	// recipient addresses
	{
		int32 NumRecipients = 0;
		MessageReader << NumRecipients;

		if ((NumRecipients < 0) || (NumRecipients > TCP_MESSAGING_MAX_RECIPIENTS))
		{
			return false;
		}

		Recipients.Empty(NumRecipients);

		while (0 < NumRecipients--)
		{
			MessageReader << *::new(Recipients) FMessageAddress;
		}
	}

	// message scope
	{
		MessageReader << Scope;

		if (Scope > EMessageScope::All)
		{
			return false;
		}
	}

	// time sent & expiration
	{
		MessageReader << TimeSent;
		MessageReader << Expiration;
	}

	// annotations
	{
		int32 NumAnnotations = 0;
		MessageReader << NumAnnotations;

		if (NumAnnotations > TCP_MESSAGING_MAX_ANNOTATIONS)
		{
			return false;
		}

		while (0 < NumAnnotations--)
		{
			FName Key;
			FString Value;

			MessageReader << Key;
			MessageReader << Value;

			Annotations.Add(Key, Value);
		}
	}

	// create message body
	MessageData = FMemory::Malloc(TypeInfo->GetStructureSize());
	TypeInfo->InitializeStruct(MessageData);

	// deserialize message body
	FJsonStructDeserializerBackend Backend(MessageReader);

	return FStructDeserializer::Deserialize(MessageData, *TypeInfo, Backend);
}


/* IMessageContext interface
 *****************************************************************************/

const TMap<FName, FString>& FTcpDeserializedMessage::GetAnnotations() const
{
	return Annotations;
}


TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe> FTcpDeserializedMessage::GetAttachment() const
{
	return Attachment;
}


const FDateTime& FTcpDeserializedMessage::GetExpiration() const
{
	return Expiration;
}


const void* FTcpDeserializedMessage::GetMessage() const
{
	return MessageData;
}


const TWeakObjectPtr<UScriptStruct>& FTcpDeserializedMessage::GetMessageTypeInfo() const
{
	return TypeInfo;
}


TSharedPtr<IMessageContext, ESPMode::ThreadSafe> FTcpDeserializedMessage::GetOriginalContext() const
{
	return nullptr;
}


const TArray<FMessageAddress>& FTcpDeserializedMessage::GetRecipients() const
{
	return Recipients;
}


EMessageScope FTcpDeserializedMessage::GetScope() const
{
	return Scope;
}


const FMessageAddress& FTcpDeserializedMessage::GetSender() const
{
	return Sender;
}


ENamedThreads::Type FTcpDeserializedMessage::GetSenderThread() const
{
	return ENamedThreads::AnyThread;
}


const FDateTime& FTcpDeserializedMessage::GetTimeForwarded() const
{
	return TimeSent;
}


const FDateTime& FTcpDeserializedMessage::GetTimeSent() const
{
	return TimeSent;
}
