// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpMessageBeacon.h"
#include "UdpMessagingPrivate.h"

#include "HAL/Event.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "IPAddress.h"
#include "Serialization/ArrayWriter.h"
#include "Sockets.h"


/* FUdpMessageHelloSender static initialization
 *****************************************************************************/

const FTimespan FUdpMessageBeacon::IntervalPerEndpoint = FTimespan::FromMilliseconds(200);
const FTimespan FUdpMessageBeacon::MinimumInterval = FTimespan::FromMilliseconds(1000);


/* FUdpMessageHelloSender structors
 *****************************************************************************/

FUdpMessageBeacon::FUdpMessageBeacon(FSocket* InSocket, const FGuid& InSocketId, const FIPv4Endpoint& InMulticastEndpoint)
	: BeaconInterval(MinimumInterval)
	, LastEndpointCount(1)
	, LastHelloSent(FDateTime::MinValue())
	, NextHelloTime(FDateTime::UtcNow())
	, NodeId(InSocketId)
	, Socket(InSocket)
	, Stopping(false)
{
	EndpointLeftEvent = FPlatformProcess::GetSynchEventFromPool(false);
	MulticastAddress = InMulticastEndpoint.ToInternetAddr();

	Thread = FRunnableThread::Create(this, TEXT("FUdpMessageBeacon"), 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}


FUdpMessageBeacon::~FUdpMessageBeacon()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
	}

	MulticastAddress = nullptr;

	FPlatformProcess::ReturnSynchEventToPool(EndpointLeftEvent);
	EndpointLeftEvent = nullptr;
}


/* FUdpMessageHelloSender interface
 *****************************************************************************/

void FUdpMessageBeacon::SetEndpointCount(int32 EndpointCount)
{
	check(EndpointCount > 0);

	if (EndpointCount < LastEndpointCount)
	{
		FDateTime CurrentTime = FDateTime::UtcNow();

		// adjust the send interval for reduced number of endpoints
		NextHelloTime = CurrentTime + (EndpointCount / LastEndpointCount) * (NextHelloTime - CurrentTime);
		LastHelloSent = CurrentTime - (EndpointCount / LastEndpointCount) * (CurrentTime - LastHelloSent);
		LastEndpointCount = EndpointCount;

		EndpointLeftEvent->Trigger();
	}
}


/* FRunnable interface
 *****************************************************************************/

bool FUdpMessageBeacon::Init()
{
	return true;
}


uint32 FUdpMessageBeacon::Run()
{
	while (!Stopping)
	{
		FDateTime CurrentTime = FDateTime::UtcNow();

		if (CurrentTime >= NextHelloTime)
		{
			// calculate the next send interval
			BeaconInterval = FMath::Max(MinimumInterval, IntervalPerEndpoint * LastEndpointCount);
			NextHelloTime = CurrentTime + BeaconInterval;

			SendSegment(EUdpMessageSegments::Hello);
		}

		EndpointLeftEvent->Wait(NextHelloTime - CurrentTime);
	}

	SendSegment(EUdpMessageSegments::Bye);

	return 0;
}


void FUdpMessageBeacon::Stop()
{
	Stopping = true;
}


/* FUdpMessageHelloSender implementation
 *****************************************************************************/

void FUdpMessageBeacon::SendSegment(EUdpMessageSegments SegmentType)
{
	FUdpMessageSegment::FHeader Header;
	{
		Header.SenderNodeId = NodeId;
		Header.ProtocolVersion = UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION;
		Header.SegmentType = SegmentType;
	}

	FArrayWriter Writer;
	{
		Writer << Header;
		Writer << NodeId;
	}

	int32 Sent;

	Socket->Wait(ESocketWaitConditions::WaitForWrite, BeaconInterval);
	Socket->SendTo(Writer.GetData(), Writer.Num(), Sent, *MulticastAddress);
}
