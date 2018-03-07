// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/NetConnection.h"
#include "DemoNetConnection.generated.h"

class APlayerController;
class FObjectReplicator;
class UDemoNetDriver;

struct FQueuedDemoPacket
{
	/** The packet data to send */
	TArray<uint8> Data;

	/** The size of the packet in bits */
	int32 SizeBits;

public:
	FORCEINLINE FQueuedDemoPacket(uint8* InData, int32 InSizeBytes, int32 InSizeBits)
		: Data()
		, SizeBits(InSizeBits)
	{
		Data.AddUninitialized(InSizeBytes);
		FMemory::Memcpy(Data.GetData(), InData, InSizeBytes);
	}
};


/**
 * Simulated network connection for recording and playing back game sessions.
 */
UCLASS(transient, config=Engine)
class ENGINE_API UDemoNetConnection
	: public UNetConnection
{
	GENERATED_UCLASS_BODY()

public:

	// UNetConnection interface.

	virtual void InitConnection( class UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed = 0, int32 InMaxPacket=0) override;
	virtual FString LowLevelGetRemoteAddress( bool bAppendPort = false ) override;
	virtual FString LowLevelDescribe() override;
	virtual void LowLevelSend(void* Data, int32 CountBytes, int32 CountBits) override;
	virtual int32 IsNetReady( bool Saturate ) override;
	virtual void FlushNet( bool bIgnoreSimulation = false ) override;
	virtual void HandleClientPlayer( APlayerController* PC, class UNetConnection* NetConnection ) override;
	virtual bool ClientHasInitializedLevelFor( const UObject* TestObject ) const override;
	virtual TSharedPtr<FObjectReplicator> CreateReplicatorForNewActorChannel(UObject* Object);

	virtual void FlushDormancy( class AActor* Actor ) override;

public:

	/** @return The DemoRecording driver object */
	FORCEINLINE class UDemoNetDriver* GetDriver()
	{
		return (UDemoNetDriver*)Driver;
	}

	/** @return The DemoRecording driver object */
	FORCEINLINE const class UDemoNetDriver* GetDriver() const
	{
		return (UDemoNetDriver*)Driver;
	}

	TArray<FQueuedDemoPacket> QueuedDemoPackets;
	TArray<FQueuedDemoPacket> QueuedCheckpointPackets;

private:
	void TrackSendForProfiler(const void* Data, int32 NumBytes);
};
