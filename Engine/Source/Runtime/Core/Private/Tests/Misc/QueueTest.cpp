// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/Queue.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueTest, "System.Core.Misc.Queue", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)


bool FQueueTest::RunTest( const FString& Parameters )
{
	// empty queues
	{
		TQueue<int32> Queue;
		int32 OutItem = 0;

		TestTrue(TEXT("A new queue must be empty"), Queue.IsEmpty());

		TestFalse(TEXT("A new queue must not dequeue anything"), Queue.Dequeue(OutItem));
		TestFalse(TEXT("A new queue must not peek anything"), Queue.Peek(OutItem));
	}

	// insertion & removal
	{
		TQueue<int32> Queue;
		int32 Item1 = 1;
		int32 Item2 = 2;
		int32 Item3 = 3;
		int32 OutItem = 0;

		TestTrue(TEXT("Inserting into a new queue must succeed"), Queue.Enqueue(Item1));
		TestTrue(TEXT("Peek must succeed on a queue with one item"), Queue.Peek(OutItem));
		TestEqual(TEXT("Peek must return the first value"), OutItem, Item1);

		TestTrue(TEXT("Inserting into a non-empty queue must succeed"), Queue.Enqueue(Item2));
		TestTrue(TEXT("Peek must succeed on a queue with two items"), Queue.Peek(OutItem));
		TestEqual(TEXT("Peek must return the first item"), OutItem, Item1);

		Queue.Enqueue(Item3);

		TestTrue(TEXT("Dequeue must succeed on a queue with three items"), Queue.Dequeue(OutItem));
		TestEqual(TEXT("Dequeue must return the first item"), OutItem, Item1);
		TestTrue(TEXT("Dequeue must succeed on a queue with two items"), Queue.Dequeue(OutItem));
		TestEqual(TEXT("Dequeue must return the second item"), OutItem, Item2);
		TestTrue(TEXT("Dequeue must succeed on a queue with one item"), Queue.Dequeue(OutItem));
		TestEqual(TEXT("Dequeue must return the third item"), OutItem, Item3);

		TestTrue(TEXT("After removing all items, the queue must be empty"), Queue.IsEmpty());
	}

	// emptying
	{
		TQueue<int32> Queue;

		Queue.Enqueue(1);
		Queue.Enqueue(2);
		Queue.Enqueue(3);
		Queue.Empty();

		TestTrue(TEXT("An emptied queue must be empty"), Queue.IsEmpty());
	}

	return true;
}


#endif //WITH_DEV_AUTOMATION_TESTS
