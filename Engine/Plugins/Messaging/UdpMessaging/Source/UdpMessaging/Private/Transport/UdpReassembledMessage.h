// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/DateTime.h"


/**
 * Implements a reassembled message.
 */
class FUdpReassembledMessage
{
public:

	/** Default constructor. */
	FUdpReassembledMessage() { }

	/**
	 * Creates and initializes a new inbound message info.
	 *
	 * @param MessageSize The total size of the message in bytes.
	 * @param SegmentCount The total number of segments that need to be received for this message.
	 * @param InSequence The message sequence number.
	 * @param InSender The IPv4 endpoint of the sender.
	 */
	FUdpReassembledMessage(int32 MessageSize, int32 SegmentCount, uint64 InSequence, const FIPv4Endpoint& InSender)
		: PendingSegments(true, SegmentCount)
		, PendingSegmentsCount(SegmentCount)
		, ReceivedBytes(0)
		, Sender(InSender)
		, Sequence(InSequence)
	{
		Data.AddUninitialized(MessageSize);
	}

	/** Virtual destructor. */
	virtual ~FUdpReassembledMessage() { }

public:

	/**
	 * Gets the message data.
	 *
	 * @return Message data.
	 */
	virtual const TArray<uint8>& GetData() const
	{
		return Data;
	}

	/**
	 * Gets the time at which the last segment was received.
	 *
	 * @return Last receive time.
	 */
	FDateTime GetLastSegmentTime() const
	{
		return LastSegmentTime;
	}

	/**
	 * Gets the list of segments that haven't been received yet.
	 *
	 * @return List of pending segment numbers.
	 */
	TArray<uint16> GetPendingSegments() const
	{
		TArray<uint16> Result;

		if (PendingSegmentsCount > 0)
		{
			for (TConstSetBitIterator<> It(PendingSegments); It; ++It)
			{
				Result.Add(It.GetIndex());
			}
		}

		return Result;
	}

	/**
	 * Gets the number of segments that haven't been received yet.
	 *
	 * @return Number of pending segments.
	 */
	uint16 GetPendingSegmentsCount() const
	{
		return PendingSegmentsCount;
	}

	/**
	 * Gets the number of retransmit requests that were sent for this payload.
	 *
	 * @return Number of retransmit requests.
	 */
	int32 GetRetransmitRequestsCount() const
	{
		return RetransmitRequestsCount;
	}

	/**
	 * Gets the message's sequence number.
	 *
	 * @return Sequence number.
	 */
	uint64 GetSequence() const
	{
		return Sequence;
	}

	/**
	 * Checks whether this message is complete.
	 *
	 * @return true if the message is complete, false otherwise.
	 */
	bool IsComplete() const
	{
		return (PendingSegmentsCount == 0);
	}

	/**
	 * Checks whether this message has been initialized.
	 *
	 * @return true if the message is initialized, false otherwise.
	 */
	bool IsInitialized() const
	{
		return (Data.Num() < 0);
	}

	/**
	 * Reassembles a segment into the specified message.
	 *
	 * @param SegmentNumber The number of the message segment.
	 * @param SegmentOffset The segment's offset within the message.
	 * @param SegmentData The segment data.
	 * @param CurrentTime The current time.
	 */
	void Reassemble(int32 SegmentNumber, int32 SegmentOffset, const TArray<uint8>& SegmentData, const FDateTime& CurrentTime)
	{
		if (SegmentNumber >= PendingSegments.Num())
		{
			// temp hack sanity check
			return;
		}

		LastSegmentTime = CurrentTime;

		if (PendingSegments[SegmentNumber])
		{
			if (SegmentOffset + SegmentData.Num() <= Data.Num())
			{
				FMemory::Memcpy(Data.GetData() + SegmentOffset, SegmentData.GetData(), SegmentData.Num());

				PendingSegments[SegmentNumber] = false;
				--PendingSegmentsCount;

				ReceivedBytes += SegmentData.Num();
			}
		}
	}

private:

	/** Holds the message data. */
	TArray<uint8> Data;

	/** Holds the time at which the last segment was received. */
	FDateTime LastSegmentTime;

	/** Holds an array of bits that indicate which segments still need to be received. */
	TBitArray<> PendingSegments;

	/** Holds the number of segments that haven't been received yet. */
	uint16 PendingSegmentsCount;

	/** Holds the number of bytes received so far. */
	int32 ReceivedBytes;

	/** Holds the number of retransmit requests that were sent since the last segment was received. */
	int32 RetransmitRequestsCount;

	/** Holds the sender. */
	FIPv4Endpoint Sender;

	/** Holds the message sequence. */
	uint64 Sequence;
};
