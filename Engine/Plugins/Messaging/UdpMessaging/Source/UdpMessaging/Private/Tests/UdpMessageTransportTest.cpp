// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/AutomationTest.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "IMessageContext.h"
#include "IMessageTransportHandler.h"
#include "Transport/UdpMessageTransport.h"
#include "Tests/UdpMessagingTestTypes.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUdpMessageTransportTest, "System.Core.Messaging.Transports.Udp.UdpMessageTransport (takes ~2 minutes!)", EAutomationTestFlags::Disabled | EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)


class FUdpMessageTransportTestState
	: public IMessageTransportHandler
{
public:

	FUdpMessageTransportTestState(FAutomationTestBase& InTest, const FIPv4Endpoint& UnicastEndpoint, const FIPv4Endpoint& MulticastEndpoint, uint8 MulticastTimeToLive)
		: NumReceivedMessages(0)
		, Test(InTest)
	{
		Transport = MakeShareable(new FUdpMessageTransport(UnicastEndpoint, MulticastEndpoint, MulticastTimeToLive));
	}

public:

	const TArray<FGuid>& GetDiscoveredNodes() const
	{
		return DiscoveredNodes;
	}

	const TArray<FGuid>& GetLostNodes() const
	{
		return LostNodes;
	}

	int32 GetNumReceivedMessages() const
	{
		return NumReceivedMessages;
	}

	bool Publish(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
	{
		return Transport->TransportMessage(Context, TArray<FGuid>());
	}

	bool Start()
	{
		return Transport->StartTransport(*this);
	}

	void Stop()
	{
		Transport->StopTransport();
	}

public:

	//~ IMessageTransportHandler interface

	virtual void DiscoverTransportNode(const FGuid& NodeId) override
	{
		DiscoveredNodes.Add(NodeId);
	}

	virtual void ForgetTransportNode(const FGuid& NodeId) override
	{
		LostNodes.Add(NodeId);
	}

	virtual void ReceiveTransportMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, const FGuid& NodeId) override
	{
		FPlatformAtomics::InterlockedIncrement(&NumReceivedMessages);
	}

private:

	TArray<FGuid> DiscoveredNodes;
	TArray<FGuid> LostNodes;
	int32 NumReceivedMessages;
	FAutomationTestBase& Test;
	TSharedPtr<IMessageTransport, ESPMode::ThreadSafe> Transport;
};


bool FUdpMessageTransportTest::RunTest(const FString& Parameters)
{
	const FIPv4Endpoint MulticastEndpoint(FIPv4Address(231, 0, 0, 1), 7777);
	const FIPv4Endpoint UnicastEndpoint = FIPv4Endpoint::Any;
	const uint8 MulticastTimeToLive = 0;
	const int32 NumTestMessages = 10000;
	const int32 MessageSize = 1280;

	// create transports
	FUdpMessageTransportTestState Transport1(*this, UnicastEndpoint, MulticastEndpoint, MulticastTimeToLive);
	const auto& DiscoveredNodes1 = Transport1.GetDiscoveredNodes();
	
	FUdpMessageTransportTestState Transport2(*this, UnicastEndpoint, MulticastEndpoint, MulticastTimeToLive);
	const auto& DiscoveredNodes2 = Transport2.GetDiscoveredNodes();

	// test transport node discovery
	Transport1.Start();
	FPlatformProcess::Sleep(3.0f);

	TestTrue(TEXT("A single message transport must not discover any remote nodes"), DiscoveredNodes1.Num() == 0);

	Transport2.Start();
	FPlatformProcess::Sleep(3.0f);

	if (DiscoveredNodes1.Num() == 0)
	{
		AddError(TEXT("The first transport did not discover any nodes"));

		return false;
	}

	if (DiscoveredNodes2.Num() == 0)
	{
		AddError(TEXT("The second transport did not discover any nodes"));

		return false;
	}

	TestTrue(TEXT("The first transport must discover exactly one node"), DiscoveredNodes1.Num() == 1);
	TestTrue(TEXT("The second transport must discover exactly one node"), DiscoveredNodes2.Num() == 1);
	TestTrue(TEXT("The discovered node IDs must be valid"), DiscoveredNodes1[0].IsValid() && DiscoveredNodes2[0].IsValid());
	TestTrue(TEXT("The discovered node IDs must be unique"), DiscoveredNodes1[0] != DiscoveredNodes2[0]);

	if (HasAnyErrors())
	{
		return false;
	}

	// stress test message sending
	FDateTime StartTime = FDateTime::UtcNow();

	for (int32 Count = 0; Count < NumTestMessages; ++Count)
	{
		FUdpMockMessage* Message = new FUdpMockMessage(MessageSize);
		TSharedRef<IMessageContext, ESPMode::ThreadSafe> Context = MakeShareable(new FUdpMockMessageContext(Message));

		Transport1.Publish(Context);
	}

	AddInfo(FString::Printf(TEXT("Sent %i messages in %s"), NumTestMessages, *(FDateTime::UtcNow() - StartTime).ToString()));

	while ((Transport2.GetNumReceivedMessages() < NumTestMessages) && ((FDateTime::UtcNow() - StartTime) < FTimespan::FromSeconds(120.0)))
	{
		FPlatformProcess::Sleep(0.0f);
	}

	AddInfo(FString::Printf(TEXT("Received %i messages in %s"), Transport2.GetNumReceivedMessages(), *(FDateTime::UtcNow() - StartTime).ToString()));
	TestTrue(TEXT("All sent messages must have been received"), Transport2.GetNumReceivedMessages() == NumTestMessages);

	return true;
}

void EmptyLinkFunctionForStaticInitializationUdpMessageTransportTest()
{
	// This function exists to prevent the object file containing this test from being excluded by the linker, because it has no publicly referenced symbols.
}
