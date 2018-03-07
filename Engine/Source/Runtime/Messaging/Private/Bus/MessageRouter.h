// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMessageContext.h"
#include "Containers/Queue.h"
#include "IMessageTracer.h"
#include "Bus/MessageTracer.h"
#include "HAL/Runnable.h"

class IMessageInterceptor;
class IMessageReceiver;
class IMessageSubscription;

/**
 * Implements a topic-based message router.
 */
class FMessageRouter
	: public FRunnable
{
	DECLARE_DELEGATE(CommandDelegate)

public:

	/** Default constructor. */
	FMessageRouter();

	/** Destructor. */
	~FMessageRouter();

public:

	/**
	 * Adds a message interceptor.
	 *
	 * @param Interceptor The interceptor to add.
	 * @param MessageType The type of messages to intercept.
	 */
	FORCEINLINE void AddInterceptor(const TSharedRef<IMessageInterceptor, ESPMode::ThreadSafe>& Interceptor, const FName& MessageType)
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleAddInterceptor, Interceptor, MessageType));
	}

	/**
	 * Adds a recipient.
	 *
	 * @param Address The address of the recipient to add.
	 * @param Recipient The recipient.
	 */
	FORCEINLINE void AddRecipient(const FMessageAddress& Address, const TSharedRef<IMessageReceiver, ESPMode::ThreadSafe>& Recipient)
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleAddRecipient, Address, TWeakPtr<IMessageReceiver, ESPMode::ThreadSafe>(Recipient)));
	}

	/**
	 * Adds a subscription.
	 *
	 * @param Subscription The subscription to add.
	 */
	FORCEINLINE void AddSubscription(const TSharedRef<IMessageSubscription, ESPMode::ThreadSafe>& Subscription)
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleAddSubscriber, Subscription));
	}

	/**
	 * Gets the message tracer.
	 *
	 * @return Weak pointer to the message tracer.
	 */
	FORCEINLINE TSharedRef<IMessageTracer, ESPMode::ThreadSafe> GetTracer()
	{
		return Tracer;
	}

	/**
	 * Removes a message interceptor.
	 *
	 * @param Interceptor The interceptor to remove.
	 * @param MessageType The type of messages to stop intercepting.
	 */
	FORCEINLINE void RemoveInterceptor(const TSharedRef<IMessageInterceptor, ESPMode::ThreadSafe>& Interceptor, const FName& MessageType)
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleRemoveInterceptor, Interceptor, MessageType));
	}

	/**
	 * Removes a recipient.
	 *
	 * @param Address The address of the recipient to remove.
	 */
	FORCEINLINE void RemoveRecipient(const FMessageAddress& Address)
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleRemoveRecipient, Address));
	}

	/**
	 * Removes a subscription.
	 *
	 * @param Subscriber The subscriber to stop routing messages to.
	 * @param MessageType The type of message to unsubscribe from (NAME_None = all types).
	 */
	FORCEINLINE void RemoveSubscription(const TSharedRef<IMessageReceiver, ESPMode::ThreadSafe>& Subscriber, const FName& MessageType)
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleRemoveSubscriber, TWeakPtr<IMessageReceiver, ESPMode::ThreadSafe>(Subscriber), MessageType));
	}

	/**
	 * Routes a message to the specified recipients.
	 *
	 * @param Context The context of the message to route.
	 */
	FORCEINLINE void RouteMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
	{
		Tracer->TraceSentMessage(Context);
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleRouteMessage, Context));
	}

public:

	//~ FRunnable interface

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

protected:

	/**
	 * Calculates the time that the thread will wait for new work.
	 *
	 * @return Wait time.
	 */
	FTimespan CalculateWaitTime();

	/**
	 * Queues up a router command.
	 *
	 * @param Command The command to queue up.
	 * @return true if the command was enqueued, false otherwise.
	 */
	FORCEINLINE bool EnqueueCommand(CommandDelegate Command)
	{
		if (!Commands.Enqueue(Command))
		{
			return false;
		}

		WorkEvent->Trigger();

		return true;
	}

	/**
	 * Filters a collection of subscriptions using the given message context.
	 *
	 * @param Subscriptions The subscriptions to filter.
	 * @param Context The message context to filter by.
	 * @param Sender The message sender (may be nullptr if the sender has no subscriptions).
	 * @param OutRecipients Will hold the collection of recipients.
	 */
	void FilterSubscriptions(
		TArray<TSharedPtr<IMessageSubscription, ESPMode::ThreadSafe>>& Subscriptions,
		const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
		TArray<TSharedPtr<IMessageReceiver, ESPMode::ThreadSafe>>& OutRecipients);

	/**
	 * Dispatches a single message to its recipients.
	 *
	 * @param Message The message to dispatch.
	 */
	void DispatchMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Message);

	/** Processes all delayed messages. */
	void ProcessDelayedMessages();

private:

	/** Structure for delayed messages. */
	struct FDelayedMessage
	{
		/** Holds the context of the delayed message. */
		TSharedPtr<IMessageContext, ESPMode::ThreadSafe> Context;

		/** Holds a sequence number used by the delayed message queue. */
		int64 Sequence;


		/** Default constructor. */
		FDelayedMessage() { }

		/** Creates and initializes a new instance. */
		FDelayedMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext, int64 InSequence)
			: Context(InContext)
			, Sequence(InSequence)
		{ }

		/** Comparison operator for heap sorting. */
		bool operator<(const FDelayedMessage& Other) const
		{
			const FTimespan Difference = Other.Context->GetTimeSent() - Context->GetTimeSent();

			if (Difference.IsZero())
			{
				return (Sequence < Other.Sequence);
			}

			return (Difference > FTimespan::Zero());
		}
	};

private:

	/** Handles adding message interceptors. */
	void HandleAddInterceptor(TSharedRef<IMessageInterceptor, ESPMode::ThreadSafe> Interceptor, FName MessageType);

	/** Handles adding message recipients. */
	void HandleAddRecipient(FMessageAddress Address, TWeakPtr<IMessageReceiver, ESPMode::ThreadSafe> RecipientPtr);

	/** Handles adding of subscriptions. */
	void HandleAddSubscriber(TSharedRef<IMessageSubscription, ESPMode::ThreadSafe> Subscription);

	/** Handles the removal of message interceptors. */
	void HandleRemoveInterceptor(TSharedRef<IMessageInterceptor, ESPMode::ThreadSafe> Interceptor, FName MessageType);

	/** Handles the removal of message recipients. */
	void HandleRemoveRecipient(FMessageAddress Address);

	/** Handles the removal of subscribers. */
	void HandleRemoveSubscriber(TWeakPtr<IMessageReceiver, ESPMode::ThreadSafe> SubscriberPtr, FName MessageType);

	/** Handles the routing of messages. */
	void HandleRouteMessage(TSharedRef<IMessageContext, ESPMode::ThreadSafe> Context);

private:

	/** Maps message types to interceptors. */
	TMap<FName, TArray<TSharedPtr<IMessageInterceptor, ESPMode::ThreadSafe>>> ActiveInterceptors;

	/** Maps message addresses to recipients. */
	TMap<FMessageAddress, TWeakPtr<IMessageReceiver, ESPMode::ThreadSafe>> ActiveRecipients;

	/** Maps message types to subscriptions. */
	TMap<FName, TArray<TSharedPtr<IMessageSubscription, ESPMode::ThreadSafe>>> ActiveSubscriptions;

	/** Holds the router command queue. */
	TQueue<CommandDelegate, EQueueMode::Mpsc> Commands;

	/** Holds the current time. */
	FDateTime CurrentTime;

	/** Holds the collection of delayed messages. */
	TArray<FDelayedMessage> DelayedMessages;

	/** Holds a sequence number for delayed messages. */
	int64 DelayedMessagesSequence;

	/** Holds a flag indicating that the thread is stopping. */
	bool Stopping;

	/** Holds the message tracer. */
	TSharedRef<FMessageTracer, ESPMode::ThreadSafe> Tracer;

	/** Holds an event signaling that work is available. */
	FEvent* WorkEvent;
};
