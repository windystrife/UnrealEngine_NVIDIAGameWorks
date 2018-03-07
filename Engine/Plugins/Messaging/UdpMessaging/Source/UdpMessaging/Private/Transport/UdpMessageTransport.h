// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "IMessageTransport.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "IMessageAttachment.h"
#include "Templates/SharedPointer.h"

class FArrayReader;
class FUdpReassembledMessage;
class FRunnableThread;
class FSocket;
class FUdpMessageProcessor;
class FUdpSocketReceiver;
class IMessageAttachment;
class IMessageContext;
class IMessageTransportHandler;
class ISocketSubsystem;

struct FGuid;


/**
 * Implements a message transport technology using an UDP network connection.
 *
 * On platforms that support multiple processes, the transport is using two sockets,
 * one for per-process unicast sending/receiving, and one for multicast receiving.
 * Console and mobile platforms use a single multicast socket for all sending and receiving.
 */
class FUdpMessageTransport
	: public IMessageTransport
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InLocalEndpoint The local IP endpoint to receive messages on.
	 * @param InMulticastEndpoint The multicast group endpoint to transport messages to.
	 * @param InMulticastTtl The multicast time-to-live.
	 */
	FUdpMessageTransport(const FIPv4Endpoint& InLocalEndpoint, const FIPv4Endpoint& InMulticastEndpoint, uint8 InMulticastTtl);

	/** Virtual destructor. */
	virtual ~FUdpMessageTransport();

public:

	//~ IMessageTransport interface

	virtual FName GetDebugName() const override;
	virtual bool StartTransport(IMessageTransportHandler& Handler) override;
	virtual void StopTransport() override;
	virtual bool TransportMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, const TArray<FGuid>& Recipients) override;

private:

	/** Handles received transport messages. */
	void HandleProcessorMessageReassembled(const FUdpReassembledMessage& ReassembledMessage, const TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe>& Attachment, const FGuid& NodeId);

	/** Handles discovered transport endpoints. */
	void HandleProcessorNodeDiscovered(const FGuid& DiscoveredNodeId);

	/** Handles lost transport endpoints. */
	void HandleProcessorNodeLost(const FGuid& LostNodeId);

	/** Handles received socket data. */
	void HandleSocketDataReceived(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Data, const FIPv4Endpoint& Sender);

private:

	/** Holds the message processor. */
	FUdpMessageProcessor* MessageProcessor;

	/** Holds the message processor thread. */
	FRunnableThread* MessageProcessorThread;

	/** Holds the multicast endpoint. */
	FIPv4Endpoint MulticastEndpoint;

	/** Holds the multicast socket receiver. */
	FUdpSocketReceiver* MulticastReceiver;

	/** Holds the multicast socket. */
	FSocket* MulticastSocket;

	/** Holds the multicast time to live. */
	uint8 MulticastTtl;

	/** Holds a pointer to the socket sub-system. */
	ISocketSubsystem* SocketSubsystem;

	/** Message transport handler. */
	IMessageTransportHandler* TransportHandler;

	/** Holds the local endpoint to receive messages on. */
	FIPv4Endpoint UnicastEndpoint;

#if PLATFORM_DESKTOP
	/** Holds the unicast socket receiver. */
	FUdpSocketReceiver* UnicastReceiver;

	/** Holds the unicast socket. */
	FSocket* UnicastSocket;
#endif
};
