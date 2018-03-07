// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IProfilerClientModule.h"
#include "ProfilerClientManager.h"
#include "IMessagingModule.h"

class IMessageBus;

/**
 * Implements the ProfilerClient module
 */
class FProfilerClientModule
	: public IProfilerClientModule
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		MessageBusPtr = IMessagingModule::Get().GetDefaultBus();
	}

	virtual void ShutdownModule() override
	{
		// do nothing
	}

public:

	//~ IProfilerClientModule interface

	virtual TSharedPtr<IProfilerClient> CreateProfilerClient() override
	{
		auto MessageBus = MessageBusPtr.Pin();

		if (!MessageBus.IsValid())
		{
			return nullptr;
		}
		
		return MakeShareable(new FProfilerClientManager(MessageBus.ToSharedRef()));
	}

private:

	/** Holds a weak pointer to the message bus. */
	TWeakPtr<IMessageBus, ESPMode::ThreadSafe> MessageBusPtr;
};


IMPLEMENT_MODULE(FProfilerClientModule, ProfilerClient);
