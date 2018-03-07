// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Async/TaskGraphInterfaces.h"
#include "Transport/UdpSerializedMessage.h"
#include "IMessageContext.h"
#include "Transport/UdpSerializeMessageTask.h"
#include "Tests/UdpMessagingTestTypes.h"



IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUdpSerializeMessageTaskTest, "System.Core.Messaging.Transports.Udp.UdpSerializedMessage", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)


namespace UdpSerializeMessageTaskTest
{
	const int32 NumMessages = 100000;
	const FTimespan MaxWaitTime(0, 0, 5);

	int32 CompletedMessages = 0;
	int32 FailedMessages = 0;
	TSharedPtr<FUdpSerializedMessage, ESPMode::ThreadSafe> ReferenceMessage;
}


void HandleSerializedMessageStateChanged(TSharedRef<FUdpSerializedMessage, ESPMode::ThreadSafe> SerializedMessage)
{
	using namespace UdpSerializeMessageTaskTest;

	FPlatformAtomics::InterlockedIncrement(&CompletedMessages);

	const TArray<uint8>& ReferenceDataArray = ReferenceMessage->GetDataArray();
	const TArray<uint8>& SerializedDataArray = SerializedMessage->GetDataArray();

	if (ReferenceDataArray.Num() != SerializedDataArray.Num())
	{
		FPlatformAtomics::InterlockedIncrement(&FailedMessages);
	}
	else
	{
		const void* ReferenceData = ReferenceDataArray.GetData();
		const void* SerializedData = SerializedDataArray.GetData();

		if (FMemory::Memcmp(ReferenceData, SerializedData, ReferenceDataArray.Num()) != 0)
		{
			FPlatformAtomics::InterlockedIncrement(&FailedMessages);
		}
	}
}


bool FUdpSerializeMessageTaskTest::RunTest(const FString& Parameters)
{
	using namespace UdpSerializeMessageTaskTest;

	CompletedMessages = 0;
	FailedMessages = 0;
	ReferenceMessage = MakeShareable(new FUdpSerializedMessage);

	TSharedRef<IMessageContext, ESPMode::ThreadSafe> Context = MakeShareable(new FUdpMockMessageContext(new FUdpMockMessage));

	// synchronous reference serialization
	FUdpSerializeMessageTask ReferenceTask(Context, ReferenceMessage.ToSharedRef());
	{
		ReferenceTask.DoTask(FTaskGraphInterface::Get().GetCurrentThreadIfKnown(), FGraphEventRef());
	}

	// stress test
	for (int32 TestIndex = 0; TestIndex < NumMessages; ++TestIndex)
	{
		TSharedRef<FUdpSerializedMessage, ESPMode::ThreadSafe> SerializedMessage = MakeShareable(new FUdpSerializedMessage);
		{
			SerializedMessage->OnStateChanged().BindStatic(&HandleSerializedMessageStateChanged, SerializedMessage);
		}

		TGraphTask<FUdpSerializeMessageTask>::CreateTask().ConstructAndDispatchWhenReady(Context, SerializedMessage);
	}

	FDateTime StartTime = FDateTime::UtcNow();

	while ((CompletedMessages < NumMessages) && ((FDateTime::UtcNow() - StartTime) < MaxWaitTime))
	{
		FPlatformProcess::Sleep(0.0f);
	}

	TestEqual(TEXT("The number of completed messages must equal the total number of messages"), CompletedMessages, NumMessages);
	TestEqual(TEXT("There must be no failed messages"), FailedMessages, (int32)0);

	return ((CompletedMessages == NumMessages) && (FailedMessages == 0));
}

void EmptyLinkFunctionForStaticInitializationUdpSerializeMessageTaskTest()
{
	// This function exists to prevent the object file containing this test from being excluded by the linker, because it has no publically referenced symbols.
}
