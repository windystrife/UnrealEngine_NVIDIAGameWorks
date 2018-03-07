// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Templates/SharedPointer.h"

class FArchive;
class FUdpSerializedMessage;


/**
 * Implements a message segmenter.
 *
 * This class breaks up a message into smaller sized segments that fit into UDP datagrams.
 * It also tracks the segments that still need to be sent.
 */
class FUdpMessageSegmenter
{
public:

	/** Default constructor. */
	FUdpMessageSegmenter()
		: MessageReader(nullptr)
	{ }

	/**
	 * Creates and initializes a new message segmenter.
	 *
	 * @param InMessage The serialized message to segment.
	 */
	FUdpMessageSegmenter(const TSharedRef<FUdpSerializedMessage, ESPMode::ThreadSafe>& InSerializedMessage, uint16 InSegmentSize)
		: MessageReader(nullptr)
		, SegmentSize(InSegmentSize)
		, SerializedMessage(InSerializedMessage)
	{ }

	/** Destructor. */
	~FUdpMessageSegmenter();

public:

	/**
	 * Gets the total size of the message in bytes.
	 *
	 * @return Message size.
	 */
	int64 GetMessageSize() const;

	/**
	 * Gets the next pending segment.
	 *
	 * @param OutData Will hold the segment data.
	 * @param OutSegment Will hold the segment number.
	 * @return true if a segment was returned, false if there are no more pending segments.
	 */
	bool GetNextPendingSegment(TArray<uint8>& OutData, uint16& OutSegment) const;

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
	 * Gets the total number of segments that make up the message.
	 *
	 * @return Segment count.
	 */
	uint16 GetSegmentCount() const
	{
		return PendingSegments.Num();
	}

	/** Initializes the segmenter. */
	void Initialize();

	/**
	 * Checks whether all segments have been sent.
	 *
	 * @return true if all segments were sent, false otherwise.
	 */
	bool IsComplete() const
	{
		return (PendingSegmentsCount == 0);
	}

	/**
	 * Checks whether this segmenter has been initialized.
	 *
	 * @return true if it is initialized, false otherwise.
	 */
	bool IsInitialized() const
	{
		return (MessageReader != nullptr);
	}

	/**
	 * Checks whether this segmenter is invalid.
	 *
	 * @return true if the segmenter is invalid, false otherwise.
	 */
	bool IsInvalid() const;

	/**
	 * Marks the specified segment as sent.
	 *
	 * @param Segment The sent segment.
	 */
	void MarkAsSent(uint16 Segment);

	/**
	 * Marks the entire message for retransmission.
	 */
	void MarkForRetransmission()
	{
		PendingSegments.Init(true, PendingSegments.Num());
	}

	/**
	 * Marks the specified segments for retransmission.
	 *
	 * @param Segments The data segments to retransmit.
	 */
	void MarkForRetransmission(const TArray<uint16>& Segments);

private:

	/** temp hack to support new transport API. */
	FArchive* MessageReader;

	/** Holds an array of bits that indicate which segments still need to be sent. */
	TBitArray<> PendingSegments;

	/** Holds the number of segments that haven't been sent yet. */
	uint16 PendingSegmentsCount;

	/** Holds the segment size. */
	uint16 SegmentSize;

	/** Holds the message. */
	TSharedPtr<FUdpSerializedMessage, ESPMode::ThreadSafe> SerializedMessage;
};
