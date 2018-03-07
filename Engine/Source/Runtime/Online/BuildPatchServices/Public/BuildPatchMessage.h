// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

namespace BuildPatchServices
{
	/**
	 * A message describing an event that occurred for a chunk source.
	 */
	struct FChunkSourceEvent
	{
		// Describes the event type.
		enum class EType : uint32
		{
			// Access was lost to the source.
			AccessLost = 0,
			// Access has been regained after being lost.
			AccessRegained,
		};

		// The type of event that occurred.
		EType Event;
		// The location context for the source, could be cloud root, install location, chunkdb file etc.
		FString Location;
	};

	/**
	 * Base class of a message handler, this should be inherited from and passed to an installer to receive messages that you want to handle.
	 */
	class FMessageHandler
	{
	public:
		FMessageHandler() {}
		virtual ~FMessageHandler() {}

		/**
		 * Handles a chunk source event message.
		 * @param Message   The message to be handled.
		 */
		virtual void HandleMessage(const FChunkSourceEvent& Message) {}
	};
}
