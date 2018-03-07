// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "HAL/RunnableThread.h"
#include "Serialization/ArrayReader.h"
#include "Sockets.h"

/**
 * Temporary fix for concurrency crashes. This whole class will be redesigned.
 */
typedef TSharedPtr<FArrayReader, ESPMode::ThreadSafe> FArrayReaderPtr;

/**
 * Delegate type for received data.
 *
 * The first parameter is the received data.
 * The second parameter is sender's IP endpoint.
 */
DECLARE_DELEGATE_TwoParams(FOnSocketDataReceived, const FArrayReaderPtr&, const FIPv4Endpoint&);


/**
 * Asynchronously receives data from an UDP socket.
 */
class FUdpSocketReceiver
	: public FRunnable
{
public:

	/**
	 * Creates and initializes a new socket receiver.
	 *
	 * @param InSocket The UDP socket to receive data from.
	 * @param InWaitTime The amount of time to wait for the socket to be readable.
	 * @param InThreadName The receiver thread name (for debugging).
	 */
	FUdpSocketReceiver(FSocket* InSocket, const FTimespan& InWaitTime, const TCHAR* InThreadName)
		: Socket(InSocket)
		, Stopping(false)
		, Thread(nullptr)
		, ThreadName(InThreadName)
		, WaitTime(InWaitTime)
	{
		check(Socket != nullptr);
		check(Socket->GetSocketType() == SOCKTYPE_Datagram);

		SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	}

	/** Virtual destructor. */
	virtual ~FUdpSocketReceiver()
	{
		if (Thread != nullptr)
		{
			Thread->Kill(true);
			delete Thread;
		}
	}

public:

	/** Start the receiver thread. */
	void Start()
	{
		Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
	}

	/**
	 * Returns a delegate that is executed when data has been received.
	 *
	 * This delegate must be bound before the receiver thread is started with
	 * the Start() method. It cannot be unbound while the thread is running.
	 *
	 * @return The delegate.
	 */
	FOnSocketDataReceived& OnDataReceived()
	{
		check(Thread == nullptr);
		return DataReceivedDelegate;
	}

public:

	// FRunnable interface

	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();

		while (!Stopping)
		{
			if (!Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTime))
			{
				continue;
			}

			uint32 Size;

			while (Socket->HasPendingData(Size))
			{
				FArrayReaderPtr Reader = MakeShareable(new FArrayReader(true));
				Reader->SetNumUninitialized(FMath::Min(Size, 65507u));

				int32 Read = 0;
				
				if (Socket->RecvFrom(Reader->GetData(), Reader->Num(), Read, *Sender))
				{
					Reader->RemoveAt(Read, Reader->Num() - Read, false);
					DataReceivedDelegate.ExecuteIfBound(Reader, FIPv4Endpoint(Sender));
				}
			}
		}

		return 0;
	}

	virtual void Stop() override
	{
		Stopping = true;
	}

	virtual void Exit() override { }

private:

	/** Holds the network socket. */
	FSocket* Socket;

	/** Holds a pointer to the socket sub-system. */
	ISocketSubsystem* SocketSubsystem;

	/** Holds a flag indicating that the thread is stopping. */
	bool Stopping;

	/** Holds the thread object. */
	FRunnableThread* Thread;

	/** The receiver thread's name. */
	FString ThreadName;

	/** Holds the amount of time to wait for inbound packets. */
	FTimespan WaitTime;

private:

	/** Holds the data received delegate. */
	FOnSocketDataReceived DataReceivedDelegate;
};
