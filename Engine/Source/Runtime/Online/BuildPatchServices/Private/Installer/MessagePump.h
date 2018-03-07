// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BuildPatchMessage.h"

namespace BuildPatchServices
{
	/**
	 * Interface for a message pump which allows systems to bubble up event information to the Installer's public API.
	 */
	class IMessagePump
	{
	public:
		/**
		 * Virtual destructor.
		 */
		virtual ~IMessagePump() { }

		/**
		 * Sends a chunk source event message.
		 * @param Message   The message to be sent.
		 */
		virtual void SendMessage(FChunkSourceEvent Message) = 0;

		/**
		 * Dequeues received messages, pushing them to the provided handlers.
		 * @param Handlers      The array of handlers.
		 */
		virtual void PumpMessages(const TArray<FMessageHandler*>& Handlers) = 0;
	};

	/**
	 * A factory for creating an IMessagePump instance.
	 */
	class FMessagePumpFactory
	{
	public:
		/**
		 * Creates an instance of IMessagePump.
		 * @return a pointer to the new IMessagePump instance created.
		 */
		static IMessagePump* Create();
	};

}
