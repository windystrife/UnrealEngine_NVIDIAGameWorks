// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Transport/TcpSerializeMessageTask.h"
#include "Transport/TcpMessageTransportConnection.h"
#include "Backends/JsonStructSerializerBackend.h"
#include "StructSerializer.h"


/* FTcpSerializeMessageTask interface
 *****************************************************************************/

void FTcpSerializeMessageTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (MessageContext->IsValid())
	{
		// Note that some complex values are serialized manually here, so that we can ensure
		// a consistent wire format, if their implementations change. This allows us to sanity
		// check the values during deserialization. @see FTcpDeserializeMessage::Deserialize()

		// serialize context
		FArchive& Archive = SerializedMessage.Get();
		{
			const FName& MessageType = MessageContext->GetMessageType();
			Archive << const_cast<FName&>(MessageType);

			const FMessageAddress& Sender = MessageContext->GetSender();
			Archive << const_cast<FMessageAddress&>(Sender);

			const TArray<FMessageAddress>& Recipients = MessageContext->GetRecipients();
			Archive << const_cast<TArray<FMessageAddress>&>(Recipients);

			EMessageScope Scope = MessageContext->GetScope();
			Archive << Scope;

			const FDateTime& TimeSent = MessageContext->GetTimeSent();
			Archive << const_cast<FDateTime&>(TimeSent);

			const FDateTime& Expiration = MessageContext->GetExpiration();
			Archive << const_cast<FDateTime&>(Expiration);

			int32 NumAnnotations = MessageContext->GetAnnotations().Num();
			Archive << NumAnnotations;

			for (const auto& AnnotationPair : MessageContext->GetAnnotations())
			{
				Archive << const_cast<FName&>(AnnotationPair.Key);
				Archive << const_cast<FString&>(AnnotationPair.Value);
			}
		}

		// serialize message body
		FJsonStructSerializerBackend Backend(Archive);
		FStructSerializer::Serialize(MessageContext->GetMessage(), *MessageContext->GetMessageTypeInfo(), Backend);

		// enqueue to recipients
		for (auto& Connection : RecipientConnections)
		{
			Connection->Send(SerializedMessage);
		}
	}
}


ENamedThreads::Type FTcpSerializeMessageTask::GetDesiredThread()
{
	return ENamedThreads::AnyThread;
}


TStatId FTcpSerializeMessageTask::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FTcpSerializeMessageTask, STATGROUP_TaskGraphTasks);
}


ESubsequentsMode::Type FTcpSerializeMessageTask::GetSubsequentsMode() 
{ 
	return ESubsequentsMode::FireAndForget; 
}
