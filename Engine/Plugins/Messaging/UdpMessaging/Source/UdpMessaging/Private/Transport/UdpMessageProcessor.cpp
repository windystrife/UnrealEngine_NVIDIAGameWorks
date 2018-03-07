// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpMessageProcessor.h"
#include "UdpMessagingPrivate.h"

#include "Common/UdpSocketSender.h"
#include "HAL/Event.h"
#include "HAL/RunnableThread.h"
#include "IMessageAttachment.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "Sockets.h"
#include "UObject/Class.h"

#include "Shared/UdpMessagingSettings.h"
#include "Transport/UdpMessageBeacon.h"
#include "Transport/UdpMessageSegmenter.h"
#include "Transport/UdpReassembledMessage.h"
#include "Transport/UdpSerializedMessage.h"


/* FUdpMessageHelloSender static initialization
 *****************************************************************************/

const int32 FUdpMessageProcessor::DeadHelloIntervals = 5;


/* FUdpMessageProcessor structors
 *****************************************************************************/

FUdpMessageProcessor::FUdpMessageProcessor(FSocket& InSocket, const FGuid& InNodeId, const FIPv4Endpoint& InMulticastEndpoint)
	: Beacon(nullptr)
	, LastSentMessage(-1)
	, LocalNodeId(InNodeId)
	, MulticastEndpoint(InMulticastEndpoint)
	, Socket(&InSocket)
	, SocketSender(nullptr)
	, Stopping(false)
{
	WorkEvent = FPlatformProcess::GetSynchEventFromPool();
	Thread = FRunnableThread::Create(this, TEXT("FUdpMessageProcessor"), 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());

	const UUdpMessagingSettings& Settings = *GetDefault<UUdpMessagingSettings>();

	for (auto& StaticEndpoint : Settings.StaticEndpoints)
	{
		FIPv4Endpoint Endpoint;

		if (FIPv4Endpoint::Parse(StaticEndpoint, Endpoint))
		{
			FNodeInfo& NodeInfo = StaticNodes.FindOrAdd(Endpoint);
			NodeInfo.Endpoint = Endpoint;
		}
		else
		{
			UE_LOG(LogUdpMessaging, Warning, TEXT("Invalid UDP Messaging Static Endpoint '%s'"), *StaticEndpoint);
		}
	}
}


FUdpMessageProcessor::~FUdpMessageProcessor()
{
	// shut down worker thread
	Thread->Kill(true);
	delete Thread;
	Thread = nullptr;

	// remove all transport nodes
	if (NodeLostDelegate.IsBound())
	{
		for (auto& KnownNodePair : KnownNodes)
		{
			NodeLostDelegate.Execute(KnownNodePair.Key);
		}
	}

	KnownNodes.Empty();

	// clean up 
	FPlatformProcess::ReturnSynchEventToPool(WorkEvent);
	WorkEvent = nullptr;
}


/* FUdpMessageProcessor interface
 *****************************************************************************/

bool FUdpMessageProcessor::EnqueueInboundSegment(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Data, const FIPv4Endpoint& InSender) 
{
	if (!InboundSegments.Enqueue(FInboundSegment(Data, InSender)))
	{
		return false;
	}

	WorkEvent->Trigger();

	return true;
}


bool FUdpMessageProcessor::EnqueueOutboundMessage(const TSharedRef<FUdpSerializedMessage, ESPMode::ThreadSafe>& SerializedMessage, const FGuid& Recipient)
{
	if (!OutboundMessages.Enqueue(FOutboundMessage(SerializedMessage, Recipient)))
	{
		return false;
	}

	SerializedMessage->OnStateChanged().BindRaw(this, &FUdpMessageProcessor::HandleSerializedMessageStateChanged);

	return true;
}


/* FRunnable interface
 *****************************************************************************/

bool FUdpMessageProcessor::Init()
{
	Beacon = new FUdpMessageBeacon(Socket, LocalNodeId, MulticastEndpoint);
	SocketSender = new FUdpSocketSender(Socket, TEXT("FUdpMessageProcessor.Sender"));

	return true;
}


uint32 FUdpMessageProcessor::Run()
{
	while (!Stopping)
	{
		if (WorkEvent->Wait(CalculateWaitTime()))
		{
			CurrentTime = FDateTime::UtcNow();

			ConsumeInboundSegments();
			ConsumeOutboundMessages();
			UpdateKnownNodes();
			UpdateStaticNodes();
		}
	}
	
	delete Beacon;
	Beacon = nullptr;

	delete SocketSender;
	SocketSender = nullptr;

	return 0;
}


void FUdpMessageProcessor::Stop()
{
	Stopping = true;
	WorkEvent->Trigger();
}


/* FUdpMessageProcessor implementation
 *****************************************************************************/

void FUdpMessageProcessor::AcknowledgeReceipt(int32 MessageId, const FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FHeader Header;
	{
		Header.RecipientNodeId = NodeInfo.NodeId;
		Header.SenderNodeId = LocalNodeId;
		Header.ProtocolVersion = UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION;
		Header.SegmentType = EUdpMessageSegments::Acknowledge;
	}

	FUdpMessageSegment::FAcknowledgeChunk AcknowledgeChunk;
	{
		AcknowledgeChunk.MessageId = MessageId;
	}

	FArrayWriter Writer;
	{
		Writer << Header;
		Writer << AcknowledgeChunk;
	}

	int32 OutSent;
	Socket->SendTo(Writer.GetData(), Writer.Num(), OutSent, *NodeInfo.Endpoint.ToInternetAddr());
}


FTimespan FUdpMessageProcessor::CalculateWaitTime() const
{
	return FTimespan::FromMilliseconds(10);
}


void FUdpMessageProcessor::ConsumeInboundSegments()
{
	FInboundSegment Segment;

	while (InboundSegments.Dequeue(Segment))
	{
		FUdpMessageSegment::FHeader Header;

		// quick hack for TTP# 247103
		if (!Segment.Data.IsValid())
		{
			continue;
		}

		*Segment.Data << Header;

		if (FilterSegment(Header, Segment.Data, Segment.Sender))
		{
			FNodeInfo& NodeInfo = KnownNodes.FindOrAdd(Header.SenderNodeId);

			if (!NodeInfo.NodeId.IsValid())
			{
				NodeInfo.NodeId = Header.SenderNodeId;
				NodeDiscoveredDelegate.ExecuteIfBound(NodeInfo.NodeId);
			}

			NodeInfo.Endpoint = Segment.Sender;

			switch (Header.SegmentType)
			{
			case EUdpMessageSegments::Abort:
				ProcessAbortSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Acknowledge:
				ProcessAcknowledgeSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Bye:
				ProcessByeSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Data:			
				ProcessDataSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Hello:
				ProcessHelloSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Retransmit:
				ProcessRetransmitSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Timeout:
				ProcessTimeoutSegment(Segment, NodeInfo);
				break;

			default:
				ProcessUnknownSegment(Segment, NodeInfo, (uint8)Header.SegmentType);
			}

			NodeInfo.LastSegmentReceivedTime = CurrentTime;
		}
	}
}


void FUdpMessageProcessor::ConsumeOutboundMessages()
{
	FOutboundMessage OutboundMessage;

	while (OutboundMessages.Dequeue(OutboundMessage))
	{
		if (OutboundMessage.SerializedMessage->TotalSize() > 1024 * 65536)
		{
			continue;
		}

		++LastSentMessage;

		FNodeInfo& RecipientNodeInfo = KnownNodes.FindOrAdd(OutboundMessage.RecipientId);

		if (!OutboundMessage.RecipientId.IsValid())
		{
			RecipientNodeInfo.Endpoint = MulticastEndpoint;

			for (auto& StaticNodeInfoPair : StaticNodes)
			{
				StaticNodeInfoPair.Value.Segmenters.Add(
					LastSentMessage,
					MakeShareable(new FUdpMessageSegmenter(OutboundMessage.SerializedMessage.ToSharedRef(), 1024))
				);
			}
		}

		RecipientNodeInfo.Segmenters.Add(
			LastSentMessage,
			MakeShareable(new FUdpMessageSegmenter(OutboundMessage.SerializedMessage.ToSharedRef(), 1024))
		);
	}
}


bool FUdpMessageProcessor::FilterSegment(const FUdpMessageSegment::FHeader& Header, const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Data, const FIPv4Endpoint& InSender)
{
	// filter unsupported protocol versions
	if (Header.ProtocolVersion != UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION)
	{
		return false;
	}

	// filter locally generated segments
	if (Header.SenderNodeId == LocalNodeId)
	{
		return false;
	}

	return true;
}


void FUdpMessageProcessor::ProcessAbortSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FAbortChunk AbortChunk;
	*Segment.Data << AbortChunk;

	NodeInfo.Segmenters.Remove(AbortChunk.MessageId);
}


void FUdpMessageProcessor::ProcessAcknowledgeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FAcknowledgeChunk AcknowledgeChunk;
	*Segment.Data << AcknowledgeChunk;

	NodeInfo.Segmenters.Remove(AcknowledgeChunk.MessageId);
}


void FUdpMessageProcessor::ProcessByeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;

	if (RemoteNodeId.IsValid() && (RemoteNodeId == NodeInfo.NodeId))
	{
		RemoveKnownNode(RemoteNodeId);
	}
}


void FUdpMessageProcessor::ProcessDataSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FDataChunk DataChunk;
	*Segment.Data << DataChunk;

	// Discard late segments for sequenced messages
	if ((DataChunk.Sequence != 0) && (DataChunk.Sequence < NodeInfo.Resequencer.GetNextSequence()))
	{
		return;
	}

	TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>& ReassembledMessage = NodeInfo.ReassembledMessages.FindOrAdd(DataChunk.MessageId);

	// Reassemble message
	if (!ReassembledMessage.IsValid())
	{
		ReassembledMessage = MakeShareable(new FUdpReassembledMessage(DataChunk.MessageSize, DataChunk.TotalSegments, DataChunk.Sequence, Segment.Sender));
	}

	ReassembledMessage->Reassemble(DataChunk.SegmentNumber, DataChunk.SegmentOffset, DataChunk.Data, CurrentTime);

	// Deliver or re-sequence message
	if (!ReassembledMessage->IsComplete())
	{
		return;
	}

	AcknowledgeReceipt(DataChunk.MessageId, NodeInfo);

	if (ReassembledMessage->GetSequence() == 0)
	{
		if (NodeInfo.NodeId.IsValid())
		{
			MessageReassembledDelegate.ExecuteIfBound(*ReassembledMessage, nullptr, NodeInfo.NodeId);
		}
	}
	else if (NodeInfo.Resequencer.Resequence(ReassembledMessage))
	{
		TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe> ResequencedMessage;

		while (NodeInfo.Resequencer.Pop(ResequencedMessage))
		{
			if (NodeInfo.NodeId.IsValid())
			{
				MessageReassembledDelegate.ExecuteIfBound(*ResequencedMessage, nullptr, NodeInfo.NodeId);
			}
		}
	}

	NodeInfo.ReassembledMessages.Remove(DataChunk.MessageId);
}


void FUdpMessageProcessor::ProcessHelloSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;

	if (RemoteNodeId.IsValid())
	{
		NodeInfo.ResetIfRestarted(RemoteNodeId);
	}
}


void FUdpMessageProcessor::ProcessRetransmitSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FRetransmitChunk RetransmitChunk;
	*Segment.Data << RetransmitChunk;

	TSharedPtr<FUdpMessageSegmenter> Segmenter = NodeInfo.Segmenters.FindRef(RetransmitChunk.MessageId);

	if (Segmenter.IsValid())
	{
		Segmenter->MarkForRetransmission(RetransmitChunk.Segments);
	}
}


void FUdpMessageProcessor::ProcessTimeoutSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FTimeoutChunk TimeoutChunk;
	*Segment.Data << TimeoutChunk;

	TSharedPtr<FUdpMessageSegmenter> Segmenter = NodeInfo.Segmenters.FindRef(TimeoutChunk.MessageId);

	if (Segmenter.IsValid())
	{
		Segmenter->MarkForRetransmission();
	}
}


void FUdpMessageProcessor::ProcessUnknownSegment(FInboundSegment& Segment, FNodeInfo& EndpointInfo, uint8 SegmentType)
{
	UE_LOG(LogUdpMessaging, Verbose, TEXT("Received unknown segment type '%i' from %s"), SegmentType, *Segment.Sender.ToText().ToString());
}


void FUdpMessageProcessor::RemoveKnownNode(const FGuid& NodeId)
{
	NodeLostDelegate.ExecuteIfBound(NodeId);
	KnownNodes.Remove(NodeId);
}


void FUdpMessageProcessor::UpdateKnownNodes()
{
	// remove dead remote endpoints
	FTimespan DeadHelloTimespan = DeadHelloIntervals * Beacon->GetBeaconInterval();
	TArray<FGuid> NodesToRemove;

	for (auto& KnownNodePair : KnownNodes)
	{
		FGuid& NodeId = KnownNodePair.Key;
		FNodeInfo& NodeInfo = KnownNodePair.Value;

		if ((NodeId.IsValid()) && ((NodeInfo.LastSegmentReceivedTime + DeadHelloTimespan) <= CurrentTime))
		{
			NodesToRemove.Add(NodeId);
		}
		else
		{
			UpdateSegmenters(NodeInfo);
		}
	}

	for (const auto& Node : NodesToRemove)
	{
		RemoveKnownNode(Node);
	}

	Beacon->SetEndpointCount(KnownNodes.Num() + 1);
}


void FUdpMessageProcessor::UpdateSegmenters(FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FHeader Header;
	{
		Header.RecipientNodeId = NodeInfo.NodeId;
		Header.SenderNodeId = LocalNodeId;
		Header.ProtocolVersion = UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION;
		Header.SegmentType = EUdpMessageSegments::Data;
	}

	for (TMap<int32, TSharedPtr<FUdpMessageSegmenter> >::TIterator It(NodeInfo.Segmenters); It; ++It)
	{
		TSharedPtr<FUdpMessageSegmenter>& Segmenter = It.Value();

		Segmenter->Initialize();

		if (Segmenter->IsInitialized())
		{
			FUdpMessageSegment::FDataChunk DataChunk;

			while (Segmenter->GetNextPendingSegment(DataChunk.Data, DataChunk.SegmentNumber))
			{
				DataChunk.MessageId = It.Key();
				DataChunk.MessageSize = Segmenter->GetMessageSize();
				DataChunk.SegmentOffset = 1024 * DataChunk.SegmentNumber;
				DataChunk.Sequence = 0; // @todo gmp: implement message sequencing
				DataChunk.TotalSegments = Segmenter->GetSegmentCount();

				TSharedRef<FArrayWriter, ESPMode::ThreadSafe> Writer = MakeShareable(new FArrayWriter);
				{
					*Writer << Header;
					*Writer << DataChunk;
				}

				if (!SocketSender->Send(Writer, NodeInfo.Endpoint))
				{
					return;
 				}

				Segmenter->MarkAsSent(DataChunk.SegmentNumber);
			}

			It.RemoveCurrent();
		}
		else if (Segmenter->IsInvalid())
		{
			It.RemoveCurrent();
		}
	}
}


void FUdpMessageProcessor::UpdateStaticNodes()
{
	for (auto& StaticNodePair : StaticNodes)
	{
		UpdateSegmenters(StaticNodePair.Value);
	}
}


/* FUdpMessageProcessor callbacks
 *****************************************************************************/

void FUdpMessageProcessor::HandleSerializedMessageStateChanged()
{
	WorkEvent->Trigger();
}
