// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Bus/MessageRouter.h"
#include "HAL/PlatformProcess.h"
#include "Bus/MessageDispatchTask.h"
#include "IMessageSubscription.h"
#include "IMessageReceiver.h"
#include "IMessageInterceptor.h"


/* FMessageRouter structors
 *****************************************************************************/

FMessageRouter::FMessageRouter()
	: DelayedMessagesSequence(0)
	, Stopping(false)
	, Tracer(MakeShareable(new FMessageTracer()))
{
	ActiveSubscriptions.FindOrAdd(NAME_All);
	WorkEvent = FPlatformProcess::GetSynchEventFromPool(true);
}


FMessageRouter::~FMessageRouter()
{
	FPlatformProcess::ReturnSynchEventToPool(WorkEvent);
	WorkEvent = nullptr;
}


/* FRunnable interface
 *****************************************************************************/

bool FMessageRouter::Init()
{
	return true;
}


uint32 FMessageRouter::Run()
{
	CurrentTime = FDateTime::UtcNow();

	while (!Stopping)
	{
		if (WorkEvent->Wait(CalculateWaitTime()))
		{
			CurrentTime = FDateTime::UtcNow();
			CommandDelegate Command;

			while (Commands.Dequeue(Command))
			{
				Command.Execute();
			}

			WorkEvent->Reset();
		}

		ProcessDelayedMessages();
	}

	return 0;
}


void FMessageRouter::Stop()
{
	Tracer->Stop();
	Stopping = true;
	WorkEvent->Trigger();
}


void FMessageRouter::Exit()
{
	TArray<TWeakPtr<IMessageReceiver, ESPMode::ThreadSafe>> Recipients;

	// gather all subscribed and registered recipients
	for (const auto& RecipientPair : ActiveRecipients)
	{
		Recipients.AddUnique(RecipientPair.Value);
	}

	for (const auto& SubscriptionsPair : ActiveSubscriptions)
	{
		for (const auto& Subscription : SubscriptionsPair.Value)
		{
			Recipients.AddUnique(Subscription->GetSubscriber());
		}
	}
}


/* FMessageRouter implementation
 *****************************************************************************/

FTimespan FMessageRouter::CalculateWaitTime()
{
	FTimespan WaitTime = FTimespan::FromMilliseconds(100);

	if (DelayedMessages.Num() > 0)
	{
		FTimespan DelayedTime = DelayedMessages.HeapTop().Context->GetTimeSent() - CurrentTime;

		if (DelayedTime < WaitTime)
		{
			return DelayedTime;
		}
	}

	return WaitTime;
}


void FMessageRouter::DispatchMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (Context->IsValid())
	{
		TArray<TSharedPtr<IMessageReceiver, ESPMode::ThreadSafe>> Recipients;

		// get recipients, either from the context...
		const TArray<FMessageAddress>& RecipientList = Context->GetRecipients();

		if (RecipientList.Num() > 0)
		{
			for (const auto& RecipientAddress : RecipientList)
			{
				auto Recipient = ActiveRecipients.FindRef(RecipientAddress).Pin();

				if (Recipient.IsValid())
				{
					Recipients.AddUnique(Recipient);
				}
				else
				{
					ActiveRecipients.Remove(RecipientAddress);
				}
			}
		}
		// ... or from subscriptions
		else
		{
			FilterSubscriptions(ActiveSubscriptions.FindOrAdd(Context->GetMessageType()), Context, Recipients);
			FilterSubscriptions(ActiveSubscriptions.FindOrAdd(NAME_All), Context, Recipients);
		}

		// dispatch the message
		for (auto& Recipient : Recipients)
		{
			ENamedThreads::Type RecipientThread = Recipient->GetRecipientThread();

			if (RecipientThread == ENamedThreads::AnyThread)
			{
				Tracer->TraceDispatchedMessage(Context, Recipient.ToSharedRef(), false);
				Recipient->ReceiveMessage(Context);
				Tracer->TraceHandledMessage(Context, Recipient.ToSharedRef());
			}
			else
			{
				TGraphTask<FMessageDispatchTask>::CreateTask().ConstructAndDispatchWhenReady(RecipientThread, Context, Recipient, Tracer);
			}
		}
	}
}


void FMessageRouter::FilterSubscriptions(
	TArray<TSharedPtr<IMessageSubscription, ESPMode::ThreadSafe>>& Subscriptions,
	const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
	TArray<TSharedPtr<IMessageReceiver, ESPMode::ThreadSafe>>& OutRecipients
)
{
	EMessageScope MessageScope = Context->GetScope();

	for (int32 SubscriptionIndex = 0; SubscriptionIndex < Subscriptions.Num(); ++SubscriptionIndex)
	{
		const auto& Subscription = Subscriptions[SubscriptionIndex];

		if (!Subscription->IsEnabled() || !Subscription->GetScopeRange().Contains(MessageScope))
		{
			continue;
		}

		auto Subscriber = Subscription->GetSubscriber().Pin();

		if (Subscriber.IsValid())
		{
			if (MessageScope == EMessageScope::Thread)
			{
				ENamedThreads::Type RecipientThread = Subscriber->GetRecipientThread();
				ENamedThreads::Type SenderThread = Context->GetSenderThread();

				if (RecipientThread != SenderThread)
				{
					continue;
				}
			}

			OutRecipients.AddUnique(Subscriber);
		}
		else
		{
			Subscriptions.RemoveAtSwap(SubscriptionIndex);
			--SubscriptionIndex;
		}
	}
}


void FMessageRouter::ProcessDelayedMessages()
{
	FDelayedMessage DelayedMessage;

	while ((DelayedMessages.Num() > 0) && (DelayedMessages.HeapTop().Context->GetTimeSent() <= CurrentTime))
	{
		DelayedMessages.HeapPop(DelayedMessage);
		DispatchMessage(DelayedMessage.Context.ToSharedRef());
	}
}


/* FMessageRouter callbacks
 *****************************************************************************/

void FMessageRouter::HandleAddInterceptor(TSharedRef<IMessageInterceptor, ESPMode::ThreadSafe> Interceptor, FName MessageType)
{
	ActiveInterceptors.FindOrAdd(MessageType).AddUnique(Interceptor);
	Tracer->TraceAddedInterceptor(Interceptor, MessageType);
}


void FMessageRouter::HandleAddRecipient(FMessageAddress Address, TWeakPtr<IMessageReceiver, ESPMode::ThreadSafe> RecipientPtr)
{
	auto Recipient = RecipientPtr.Pin();

	if (Recipient.IsValid())
	{
		ActiveRecipients.FindOrAdd(Address) = Recipient;
		Tracer->TraceAddedRecipient(Address, Recipient.ToSharedRef());
	}
}


void FMessageRouter::HandleAddSubscriber(TSharedRef<IMessageSubscription, ESPMode::ThreadSafe> Subscription)
{
	ActiveSubscriptions.FindOrAdd(Subscription->GetMessageType()).AddUnique(Subscription);
	Tracer->TraceAddedSubscription(Subscription);
}


void FMessageRouter::HandleRemoveInterceptor(TSharedRef<IMessageInterceptor, ESPMode::ThreadSafe> Interceptor, FName MessageType)
{
	if (MessageType == NAME_All)
	{
		for (auto& InterceptorsPair : ActiveInterceptors)
		{
			InterceptorsPair.Value.Remove(Interceptor);
		}
	}
	else
	{
		auto& Interceptors = ActiveInterceptors.FindOrAdd(MessageType);
		Interceptors.Remove(Interceptor);
	}

	Tracer->TraceRemovedInterceptor(Interceptor, MessageType);
}


void FMessageRouter::HandleRemoveRecipient(FMessageAddress Address)
{
	auto Recipient = ActiveRecipients.FindRef(Address).Pin();

	if (Recipient.IsValid())
	{
		ActiveRecipients.Remove(Address);
	}

	Tracer->TraceRemovedRecipient(Address);
}


void FMessageRouter::HandleRemoveSubscriber(TWeakPtr<IMessageReceiver, ESPMode::ThreadSafe> SubscriberPtr, FName MessageType)
{
	auto Subscriber = SubscriberPtr.Pin();

	if (!Subscriber.IsValid())
	{
		return;
	}

	for (auto& SubscriptionsPair : ActiveSubscriptions)
	{
		if ((MessageType != NAME_All) && (MessageType != SubscriptionsPair.Key))
		{
			continue;
		}

		TArray<TSharedPtr<IMessageSubscription, ESPMode::ThreadSafe>>& Subscriptions = SubscriptionsPair.Value;

		for (int32 Index = 0; Index < Subscriptions.Num(); Index++)
		{
			const auto& Subscription = Subscriptions[Index];

			if (Subscription->GetSubscriber().Pin() == Subscriber)
			{
				Subscriptions.RemoveAtSwap(Index);
				Tracer->TraceRemovedSubscription(Subscription.ToSharedRef(), MessageType);

				break;
			}
		}
	}
}


void FMessageRouter::HandleRouteMessage(TSharedRef<IMessageContext, ESPMode::ThreadSafe> Context)
{
	Tracer->TraceRoutedMessage(Context);

	// intercept routing
	auto& Interceptors = ActiveInterceptors.FindOrAdd(Context->GetMessageType());

	for (auto& Interceptor : Interceptors)
	{
		if (Interceptor->InterceptMessage(Context))
		{
			Tracer->TraceInterceptedMessage(Context, Interceptor.ToSharedRef());

			return;
		}
	}

	// dispatch the message
	// @todo gmp: implement time synchronization between networked message endpoints
	if (false) //(Context->GetTimeSent() > CurrentTime)
	{
		DelayedMessages.HeapPush(FDelayedMessage(Context, ++DelayedMessagesSequence));
	}
	else
	{
		DispatchMessage(Context);
	}
}
