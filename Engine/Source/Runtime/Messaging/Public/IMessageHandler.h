// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class IMessageContext;


/**
 * Interface for message handlers.
 */
class IMessageHandler
{
public:

	/**
	 * Handles the specified message.
	 *
	 * @param Context The context of the message to handle.
	 */
	virtual void HandleMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context) = 0;

public:

	/** Virtual destructor. */
	virtual ~IMessageHandler() { }
};
