// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Bridge/MessageBridge.h"

#include "IMessageBus.h"
#include "IMessageSubscription.h"
#include "IMessageTransport.h"


/* FMessageBridge structors
 *****************************************************************************/

FMessageBridge::FMessageBridge(
	const FMessageAddress InAddress,
	const TSharedRef<IMessageBus, ESPMode::ThreadSafe>& InBus,
	const TSharedRef<IMessageTransport, ESPMode::ThreadSafe>& InTransport
)
	: Address(InAddress)
	, Bus(InBus)
	, Enabled(false)
	, Id(FGuid::NewGuid())
	, Transport(InTransport)
{
	Bus->OnShutdown().AddRaw(this, &FMessageBridge::HandleMessageBusShutdown);
}


FMessageBridge::~FMessageBridge()
{
	Disable();

	if (Bus.IsValid())
	{
		Bus->OnShutdown().RemoveAll(this);
		Bus->Unregister(Address);

		TArray<FMessageAddress> RemovedAddresses;
		AddressBook.RemoveAll(RemovedAddresses);

		for (const auto& RemovedAddress : RemovedAddresses)
		{
			Bus->Unregister(RemovedAddress);
		}
	}
}


/* IMessageBridge interface
 *****************************************************************************/

void FMessageBridge::Disable() 
{
	if (!Enabled)
	{
		return;
	}

	// disable subscription & transport
	if (MessageSubscription.IsValid())
	{
		MessageSubscription->Disable();
	}

	if (Transport.IsValid())
	{
		Transport->StopTransport();
	}

	Enabled = false;
}


void FMessageBridge::Enable()
{
	if (Enabled || !Bus.IsValid() || !Transport.IsValid())
	{
		return;
	}

	// enable subscription & transport
	if (!Transport->StartTransport(*this))
	{
		return;
	}

	Bus->Register(Address, AsShared());

	if (MessageSubscription.IsValid())
	{
		MessageSubscription->Enable();
	}
	else
	{
		MessageSubscription = Bus->Subscribe(AsShared(), NAME_All, FMessageScopeRange::AtLeast(EMessageScope::Network));
	}

	Enabled = true;
}


bool FMessageBridge::IsEnabled() const
{
	return Enabled;
}


/* IMessageReceiver interface
 *****************************************************************************/

FName FMessageBridge::GetDebugName() const
{
	return *FString::Printf(TEXT("FMessageBridge (%s)"), *Transport->GetDebugName().ToString());
}


const FGuid& FMessageBridge::GetRecipientId() const
{
	return Id;
}


ENamedThreads::Type FMessageBridge::GetRecipientThread() const
{
	return ENamedThreads::AnyThread;
}


bool FMessageBridge::IsLocal() const
{
	return false;
}


void FMessageBridge::ReceiveMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (!Enabled)
	{
		return;
	}

	// get remote nodes
	TArray<FGuid> RemoteNodes;

	if (Context->GetRecipients().Num() > 0)
	{
		RemoteNodes = AddressBook.GetNodesFor(Context->GetRecipients());

		if (RemoteNodes.Num() == 0)
		{
			return;
		}
	}

	// forward message to remote nodes
	Transport->TransportMessage(Context, RemoteNodes);
}


/* IMessageSender interface
 *****************************************************************************/

FMessageAddress FMessageBridge::GetSenderAddress()
{
	return Address;
}


void FMessageBridge::NotifyMessageError(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, const FString& Error)
{
	// deprecated
}


/* IMessageTransportHandler interface
 *****************************************************************************/

void FMessageBridge::DiscoverTransportNode(const FGuid& NodeId)
{
	// do nothing (address book is updated in ReceiveTransportMessage)
}


void FMessageBridge::ForgetTransportNode(const FGuid& NodeId)
{
	TArray<FMessageAddress> RemovedAddresses;

	// update address book
	AddressBook.RemoveNode(NodeId, RemovedAddresses);

	// unregister endpoints
	if (Bus.IsValid())
	{
		for (const auto& RemovedAddress : RemovedAddresses)
		{
			Bus->Unregister(RemovedAddress);
		}
	}
}


void FMessageBridge::ReceiveTransportMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, const FGuid& NodeId)
{
	if (!Enabled || !Bus.IsValid())
	{
		return;
	}

	// discard expired messages
	if (Context->GetExpiration() < FDateTime::UtcNow())
	{
		return;
	}

	// register newly discovered endpoints
	if (!AddressBook.Contains(Context->GetSender()))
	{
		AddressBook.Add(Context->GetSender(), NodeId);
		Bus->Register(Context->GetSender(), AsShared());
	}

	// forward message to local bus
	Bus->Forward(Context, Context->GetRecipients(), FTimespan::Zero(), AsShared());
}


/* FMessageBridge callbacks
 *****************************************************************************/

void FMessageBridge::HandleMessageBusShutdown()
{
	Disable();
	Bus.Reset();
}
