// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Misc/Guid.h"
#include "Serialization/Archive.h"


/**
 * Enumerates message segment types.
 */
enum class EUdpMessageSegments : uint8
{
	/** None. */
	None,

	/** Request to abort the sending of a message. */
	Abort,

	/** Acknowledges that the message was received successfully. */
	Acknowledge,

	/** Notifies the bus that an endpoint has left. */
	Bye,

	/** A message data segment. */
	Data,

	/** Notifies the bus that an endpoint has joined. */
	Hello,

	/** Request to retransmit selected data segments. */
	Retransmit,

	/** Notification that an inbound message timed out. */
	Timeout
};


namespace FUdpMessageSegment
{
	/**
	 * Structure for the header of all segments.
	 */
	struct FHeader
	{
		/** Holds the protocol version. */
		uint8 ProtocolVersion;

		/** Holds the recipient's node identifier (empty = multicast). */
		FGuid RecipientNodeId;

		/** Holds the sender's node identifier. */
		FGuid SenderNodeId;

		/**
		 * Holds the segment type.
		 *
		 * @see EUdpMessageSegments
		 */
		EUdpMessageSegments SegmentType;

	public:

		/**
		 * Serializes the given header from or into the specified archive.
		 *
		 * @param Ar The archive to serialize from or into.
		 * @param DateTime The header to serialize.
		 * @return The archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FHeader& Header)
		{
			return Ar << Header.ProtocolVersion << Header.RecipientNodeId << Header.SenderNodeId << Header.SegmentType;
		}
	};


	/**
	 * Structure for the sub-header of Abort segments.
	 */
	struct FAbortChunk
	{
		/** Holds the identifier of the message to abort. */
		int32 MessageId;

	public:

		/**
		 * Serializes the given header from or to the specified archive.
		 *
		 * @param Ar The archive to serialize from or into.
		 * @param DateTime The header to serialize.
		 * @return The archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FAbortChunk& Header)
		{
			return Ar << Header.MessageId;
		}
	};


	/**
	 * Structure for the header of Acknowledge segments.
	 */
	struct FAcknowledgeChunk
	{
		/** Holds the identifier of the message that was received successfully. */
		int32 MessageId;

	public:

		/**
		 * Serializes the given header from or into the specified archive.
		 *
		 * @param Ar The archive to serialize from or into.
		 * @param DateTime The header to serialize.
		 * @return The archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FAcknowledgeChunk& Header)
		{
			return Ar << Header.MessageId;
		}
	};


	/**
	 * Structure for the header of Data segments.
	 */
	struct FDataChunk
	{
		/** Holds the identifier of the message that the data belongs to. */
		int32 MessageId;

		/** Holds the total size of the message. */
		int32 MessageSize;

		/** Holds the sequence number of this segment. */
		uint16 SegmentNumber;

		/** Holds the segment's offset within the message. */
		uint32 SegmentOffset;

		/** Holds the message sequence number (0 = not sequential). */
		uint64 Sequence;

		/** Holds the total number of data segments being sent. */
		uint16 TotalSegments;

		/** Holds the segment data. */
		TArray<uint8> Data;

	public:

		/**
		 * Serializes the given header from or into the specified archive.
		 *
		 * @param Ar The archive to serialize from or into.
		 * @param DateTime The header to serialize.
		 * @return The archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FDataChunk& Chunk)
		{
			return Ar
				<< Chunk.MessageId
				<< Chunk.MessageSize
				<< Chunk.SegmentNumber
				<< Chunk.SegmentOffset
				<< Chunk.Sequence
				<< Chunk.TotalSegments
				<< Chunk.Data;
		}
	};


	/**
	 * Structure for the sub-header of Retransmit segments.
	 *
	 * Retransmit segments are sent from a message recipient to a message sender in order
	 * to indicate that selected message segments are to be retransmitted, i.e. if they
	 * got lost on the network or if the recipient was unable to handle them previously.
	 */
	struct FRetransmitChunk
	{
		/** Holds the identifier of the message for which data needs to be retransmitted. */
		int32 MessageId;

		/** Holds the list of data segments that need to be retransmitted. */
		TArray<uint16> Segments;

	public:

		/**
		 * Serializes the given header from or into the specified archive.
		 *
		 * @param Ar The archive to serialize from or into.
		 * @param DateTime The header to serialize.
		 * @return The archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FRetransmitChunk& Header)
		{
			return Ar << Header.MessageId << Header.Segments;
		}
	};


	/**
	 * Structure for the header of Timeout packets.
	 */
	struct FTimeoutChunk
	{
		/** Holds the identifier of the message that timed out. */
		int32 MessageId;

	public:

		/**
		 * Serializes the given header from or into the specified archive.
		 *
		 * @param Ar The archive to serialize from or into.
		 * @param DateTime The header to serialize.
		 * @return The archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FTimeoutChunk& Header)
		{
			return Ar << Header.MessageId;
		}
	};
};
