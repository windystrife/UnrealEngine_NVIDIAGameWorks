// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/MessagePump.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockMessagePump
		: public IMessagePump
	{
	public:
		virtual void SendMessage(FChunkSourceEvent Message) override
		{
		}

		virtual void PumpMessages(const TArray<FMessageHandler*>& Handlers) override
		{
		}
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
