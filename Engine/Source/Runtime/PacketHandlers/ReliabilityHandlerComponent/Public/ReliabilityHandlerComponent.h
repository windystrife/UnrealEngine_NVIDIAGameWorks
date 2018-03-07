// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "PacketHandler.h"

// Symmetric Stream cipher
class RELIABILITYHANDLERCOMPONENT_API ReliabilityHandlerComponent : public HandlerComponent
{
public:
	/* Initializes default data */
	ReliabilityHandlerComponent();

	virtual void Initialize() override;

	virtual bool IsValid() const override;

	virtual void Tick(float DeltaTime) override;

	virtual void Incoming(FBitReader& Packet) override;

	virtual void Outgoing(FBitWriter& Packet) override;

	virtual void IncomingConnectionless(FString Address, FBitReader& Packet) override
	{
	}

	/* Queues a packet for resending */
	void QueuePacketForResending(uint8* Packet, int32 CountBits);

	/**
	 * Queues a packet sent through SendHandlerPacket, for resending
	 *
	 * @param InComponent	The HandlerComponent the packet originated from
	 * @param Packet		The packet to be queued
	 * @param CountBits		The number of bits in the packet
	 */
	FORCEINLINE void QueueHandlerPacketForResending(HandlerComponent* InComponent, uint8* Packet, int32 CountBits)
	{
		QueuePacketForResending(Packet, CountBits);

		BufferedPackets[BufferedPackets.Num()-1]->FromComponent = InComponent;
	}

	virtual int32 GetReservedPacketBits() override;

protected:
	/* Buffered Packets in case they need to be resent */
	TArray<BufferedPacket*> BufferedPackets;

	/* Latest Packet ID */
	uint32 LocalPacketID;

	/* Latest Packet ID that was ACKED */
	uint32 LocalPacketIDACKED;

	/* Latest Remote Packet ID */
	uint32 RemotePacketID;

	/* Latest Remote Packet ID that was ACKED */
	uint32 RemotePacketIDACKED;

	/* How long to wait before resending an UNACKED packet */
	float ResendResolutionTime;

	/* Last time we resent UNACKED packets */
	float LastResendTime;
};

/* Reliability Module Interface */
class FReliabilityHandlerComponentModuleInterface : public FPacketHandlerComponentModuleInterface
{
public:
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};
