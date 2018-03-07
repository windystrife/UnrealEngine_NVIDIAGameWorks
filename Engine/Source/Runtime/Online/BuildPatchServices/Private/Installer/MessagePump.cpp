// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/MessagePump.h"
#include "Containers/Union.h"
#include "Containers/Queue.h"

namespace BuildPatchServices
{
	// Union of all possible message types.
	typedef TUnion<FChunkSourceEvent> FMessageUnion;

	// Queue type for messages.
	typedef TQueue<FMessageUnion, EQueueMode::Mpsc> FMessageQueue;
}

namespace MessagePumpHelpers
{
	template<typename Type>
	bool TryPump(const TArray<BuildPatchServices::FMessageHandler*>& Handlers, const BuildPatchServices::FMessageUnion& MessageUnion)
	{
		if (MessageUnion.HasSubtype<Type>())
		{
			for (BuildPatchServices::FMessageHandler* Handler : Handlers)
			{
				Handler->HandleMessage(MessageUnion.GetSubtype<Type>());
			}
			return true;
		}
		return false;
	}
}

namespace BuildPatchServices
{
	class FMessagePump
		: public IMessagePump
	{
	public:
		FMessagePump();
		~FMessagePump();

		// IMessagePump interface begin.
		virtual void SendMessage(FChunkSourceEvent Message) override;
		virtual void PumpMessages(const TArray<FMessageHandler*>& Handlers) override;
		// IMessagePump interface end.

	private:
		FMessageQueue MessageQueue;
	};

	FMessagePump::FMessagePump()
	{
	}

	FMessagePump::~FMessagePump()
	{
	}

	void FMessagePump::SendMessage(FChunkSourceEvent Message)
	{
		MessageQueue.Enqueue(FMessageUnion(MoveTemp(Message)));
	}

	void FMessagePump::PumpMessages(const TArray<FMessageHandler*>& Handlers)
	{
		FMessageUnion MessageUnion;
		while (MessageQueue.Dequeue(MessageUnion))
		{
			if (MessagePumpHelpers::TryPump<FChunkSourceEvent>(Handlers, MessageUnion))
			{
				continue;
			}
		}
	}

	IMessagePump* FMessagePumpFactory::Create()
	{
		return new FMessagePump();
	}
}
