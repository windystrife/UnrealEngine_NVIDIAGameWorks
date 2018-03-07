// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"
#include "Containers/TripleBuffer.h"
#include "Math/RandomStream.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTripleBufferTest, "System.Core.Misc.TripleBuffer", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)


bool FTripleBufferTest::RunTest(const FString& Parameters)
{
	// uninitialized buffer
	{
		TTripleBuffer<int32> Buffer(NoInit);

		TestFalse(TEXT("Uninitialized triple buffer must not be dirty"), Buffer.IsDirty());
	}

	// initialized buffer
	{
		TTripleBuffer<int32> Buffer(1);

		TestFalse(TEXT("Initialized triple buffer must not be dirty"), Buffer.IsDirty());
		TestEqual(TEXT("Initialized triple buffer must have correct read buffer value"), Buffer.Read(), 1);

		Buffer.SwapReadBuffers();

		TestEqual(TEXT("Initialized triple buffer must have correct temp buffer value"), Buffer.Read(), 1);

		Buffer.SwapWriteBuffers();

		TestTrue(TEXT("Write buffer swap must set dirty flag"), Buffer.IsDirty());

		Buffer.SwapReadBuffers();

		TestFalse(TEXT("Read buffer swap must clear dirty flag"), Buffer.IsDirty());
		TestEqual(TEXT("Initialized triple buffer must have correct temp buffer value"), Buffer.Read(), 1);
	}

	// pre-set buffer
	{
		int32 Array[3] = { 1, 2, 3 };
		TTripleBuffer<int32> Buffer(Array);

		int32 Read = Buffer.Read();
		TestEqual(TEXT("Pre-set triple buffer must have correct Read buffer value"), Read, 3);

		Buffer.SwapReadBuffers();

		int32 Temp = Buffer.Read();
		TestEqual(TEXT("Pre-set triple buffer must have correct Temp buffer value"), Temp, 1);

		Buffer.SwapWriteBuffers();
		Buffer.SwapReadBuffers();

		int32 Write = Buffer.Read();
		TestEqual(TEXT("Pre-set triple buffer must have correct Write buffer value"), Write, 2);
	}

	// operations
	{
		TTripleBuffer<int32> Buffer;

		for (int32 Index = 0; Index < 6; ++Index)
		{
			int32& Write = Buffer.GetWriteBuffer(); Write = Index; Buffer.SwapWriteBuffers();
			Buffer.SwapReadBuffers();
			TestEqual(*FString::Printf(TEXT("Triple buffer must read correct value (%i)"), Index), Buffer.Read(), Index);
		}

		FRandomStream Rand;
		int32 LastRead = -1;

		for (int32 Index = 0; Index < 100; ++Index)
		{
			int32 Writes = Rand.GetUnsignedInt() % 4;

			while (Writes > 0)
			{
				int32& Write = Buffer.GetWriteBuffer(); Write = Index; Buffer.SwapWriteBuffers();
				--Writes;
			}
			
			int32 Reads = Rand.GetUnsignedInt() % 4;

			while (Reads > 0)
			{
				if (!Buffer.IsDirty())
				{
					break;
				}

				Buffer.SwapReadBuffers();
				int32 Read = Buffer.Read();
				TestTrue(TEXT("Triple buffer must read in increasing order"), Read > LastRead);
				LastRead = Read;
				--Reads;
			}
		}
	}

	return true;
}
