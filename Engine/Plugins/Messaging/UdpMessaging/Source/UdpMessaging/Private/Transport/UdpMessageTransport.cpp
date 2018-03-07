// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpMessageTransport.h"
#include "UdpMessagingPrivate.h"

#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "HAL/RunnableThread.h"
#include "IMessageContext.h"
#include "IMessageTransportHandler.h"
#include "Misc/Guid.h"
#include "Serialization/ArrayReader.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#include "Transport/UdpReassembledMessage.h"
#include "Transport/UdpDeserializedMessage.h"
#include "Transport/UdpSerializedMessage.h"
#include "Transport/UdpMessageProcessor.h"
#include "Transport/UdpSerializeMessageTask.h"


/* FUdpMessageTransport structors
 *****************************************************************************/

FUdpMessageTransport::FUdpMessageTransport(const FIPv4Endpoint& InUnicastEndpoint, const FIPv4Endpoint& InMulticastEndpoint, uint8 InMulticastTtl)
	: MessageProcessor(nullptr)
	, MessageProcessorThread(nullptr)
	, MulticastEndpoint(InMulticastEndpoint)
	, MulticastReceiver(nullptr)
	, MulticastSocket(nullptr)
	, MulticastTtl(InMulticastTtl)
	, SocketSubsystem(ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	, TransportHandler(nullptr)
	, UnicastEndpoint(InUnicastEndpoint)
#if PLATFORM_DESKTOP
	, UnicastReceiver(nullptr)
	, UnicastSocket(nullptr)
#endif
{ }


FUdpMessageTransport::~FUdpMessageTransport()
{
	StopTransport();
}


/* IMessageTransport interface
 *****************************************************************************/

FName FUdpMessageTransport::GetDebugName() const
{
	return "UdpMessageTransport";
}


bool FUdpMessageTransport::StartTransport(IMessageTransportHandler& Handler)
{
#if PLATFORM_DESKTOP
	// create & initialize unicast socket (only on multi-process platforms)
	UnicastSocket = FUdpSocketBuilder(TEXT("UdpMessageUnicastSocket"))
		.AsNonBlocking()
		.BoundToEndpoint(UnicastEndpoint)
		.WithMulticastLoopback()
		.WithReceiveBufferSize(UDP_MESSAGING_RECEIVE_BUFFER_SIZE);

	if (UnicastSocket == nullptr)
	{
		UE_LOG(LogUdpMessaging, Error, TEXT("StartTransport failed to create unicast socket on %s"), *UnicastEndpoint.ToString());

		return false;
	}
#endif

	// create & initialize multicast socket (optional)
	MulticastSocket = FUdpSocketBuilder(TEXT("UdpMessageMulticastSocket"))
		.AsNonBlocking()
		.AsReusable()
#if PLATFORM_WINDOWS
		// For 0.0.0.0, Windows will pick the default interface instead of all
		// interfaces. Here we allow to specify which interface to bind to. 
		// On all other platforms we bind to the wildcard IP address in order
		// to be able to also receive packets that were sent directly to the
		// interface IP instead of the multicast address.
		.BoundToAddress(UnicastEndpoint.Address)
#endif
		.BoundToPort(MulticastEndpoint.Port)
		.JoinedToGroup(MulticastEndpoint.Address)
		.WithMulticastLoopback()
		.WithMulticastTtl(MulticastTtl)
		.WithReceiveBufferSize(UDP_MESSAGING_RECEIVE_BUFFER_SIZE);

	if (MulticastSocket == nullptr)
	{
		UE_LOG(LogUdpMessaging, Warning, TEXT("StartTransport failed to create multicast socket on %s, joined to %s with TTL %i"), *UnicastEndpoint.ToString(), *MulticastEndpoint.ToString(), MulticastTtl);

#if !PLATFORM_DESKTOP
		return false;
#endif
	}

	TransportHandler = &Handler;

	// initialize threads
	FTimespan ThreadWaitTime = FTimespan::FromMilliseconds(100);

#if PLATFORM_DESKTOP
	MessageProcessor = new FUdpMessageProcessor(*UnicastSocket, FGuid::NewGuid(), MulticastEndpoint);
#else
	MessageProcessor = new FUdpMessageProcessor(*MulticastSocket, FGuid::NewGuid(), MulticastEndpoint);
#endif
	{
		MessageProcessor->OnMessageReassembled().BindRaw(this, &FUdpMessageTransport::HandleProcessorMessageReassembled);
		MessageProcessor->OnNodeDiscovered().BindRaw(this, &FUdpMessageTransport::HandleProcessorNodeDiscovered);
		MessageProcessor->OnNodeLost().BindRaw(this, &FUdpMessageTransport::HandleProcessorNodeLost);
	}

	if (MulticastSocket != nullptr)
	{
		MulticastReceiver = new FUdpSocketReceiver(MulticastSocket, ThreadWaitTime, TEXT("UdpMessageMulticastReceiver"));
		MulticastReceiver->OnDataReceived().BindRaw(this, &FUdpMessageTransport::HandleSocketDataReceived);
		MulticastReceiver->Start();
	}

#if PLATFORM_DESKTOP
	UnicastReceiver = new FUdpSocketReceiver(UnicastSocket, ThreadWaitTime, TEXT("UdpMessageUnicastReceiver"));
	{
		UnicastReceiver->OnDataReceived().BindRaw(this, &FUdpMessageTransport::HandleSocketDataReceived);
		UnicastReceiver->Start();
	}
#endif

	return true;
}


void FUdpMessageTransport::StopTransport()
{
	// shut down threads
	delete MulticastReceiver;
	MulticastReceiver = nullptr;

#if PLATFORM_DESKTOP
	delete UnicastReceiver;
	UnicastReceiver = nullptr;
#endif

	delete MessageProcessor;
	MessageProcessor = nullptr;

	// destroy sockets
	if (MulticastSocket != nullptr)
	{
		SocketSubsystem->DestroySocket(MulticastSocket);
		MulticastSocket = nullptr;
	}
	
#if PLATFORM_DESKTOP
	if (UnicastSocket != nullptr)
	{
		SocketSubsystem->DestroySocket(UnicastSocket);
		UnicastSocket = nullptr;
	}
#endif

	TransportHandler = nullptr;
}


bool FUdpMessageTransport::TransportMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, const TArray<FGuid>& Recipients)
{
	if (MessageProcessor == nullptr)
	{
		return false;
	}

	if (Context->GetRecipients().Num() > UDP_MESSAGING_MAX_RECIPIENTS)
	{
		return false;
	}

	TSharedRef<FUdpSerializedMessage, ESPMode::ThreadSafe> SerializedMessage = MakeShareable(new FUdpSerializedMessage());

	if (Recipients.Num() == 0)
	{
		// publish the message
		MessageProcessor->EnqueueOutboundMessage(SerializedMessage, FGuid());
	}
	else
	{
		// send the message
		for (const auto& Recipient : Recipients)
		{
			MessageProcessor->EnqueueOutboundMessage(SerializedMessage, Recipient);
		}
	}

	TGraphTask<FUdpSerializeMessageTask>::CreateTask().ConstructAndDispatchWhenReady(Context, SerializedMessage);

	return true;
}


/* FUdpMessageTransport event handlers
 *****************************************************************************/

void FUdpMessageTransport::HandleProcessorMessageReassembled(const FUdpReassembledMessage& ReassembledMessage, const TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe>& Attachment, const FGuid& NodeId)
{
	// @todo gmp: move message deserialization into an async task
	TSharedRef<FUdpDeserializedMessage, ESPMode::ThreadSafe> DeserializedMessage = MakeShareable(new FUdpDeserializedMessage(Attachment));

	if (DeserializedMessage->Deserialize(ReassembledMessage))
	{
		TransportHandler->ReceiveTransportMessage(DeserializedMessage, NodeId);
	}
}


void FUdpMessageTransport::HandleProcessorNodeDiscovered(const FGuid& DiscoveredNodeId)
{
	TransportHandler->DiscoverTransportNode(DiscoveredNodeId);
}


void FUdpMessageTransport::HandleProcessorNodeLost(const FGuid& LostNodeId)
{
	TransportHandler->ForgetTransportNode(LostNodeId);
}


void FUdpMessageTransport::HandleSocketDataReceived(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Data, const FIPv4Endpoint& Sender)
{
	if (MessageProcessor != nullptr)
	{
		MessageProcessor->EnqueueInboundSegment(Data, Sender);
	}
}
