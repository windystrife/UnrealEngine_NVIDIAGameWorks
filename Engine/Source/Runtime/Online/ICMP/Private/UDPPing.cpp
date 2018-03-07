// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Ticker.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "IcmpPrivate.h"
#include "Icmp.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

#include "Sockets.h"

extern uint16 NtoHS(uint16 val);
extern uint16 HtoNS(uint16 val);
extern uint32 NtoHL(uint32 val);
extern uint32 HtoNL(uint32 val);

#define MAGIC_HIGH 0xaaaaaaaa
#define MAGIC_LOW 0xbbbbbbbb

namespace UDPPing
{
	// 2 uint32 magic numbers + 64bit timecode
	const SIZE_T PayloadSize = 4 * sizeof(uint32);
}

FIcmpEchoResult UDPEchoImpl(ISocketSubsystem* SocketSub, const FString& TargetAddress, float Timeout)
{
	struct FUDPPingHeader
	{
		uint16 Id;
		uint16 Sequence;
		uint16 Checksum;
	};

	// Size of the udp header sent/received
	static const SIZE_T UDPPingHeaderSize = sizeof(FUDPPingHeader);

	// The packet we send is just the header plus our payload
	static const SIZE_T PacketSize = UDPPingHeaderSize + UDPPing::PayloadSize;

	// The result read back is just the header plus our payload;
	static const SIZE_T ResultPacketSize = PacketSize;

	// Location of the timecode in the buffer
	static const SIZE_T TimeCodeOffset = UDPPingHeaderSize;

	// Location of the payload in the buffer
	static const SIZE_T MagicNumberOffset = TimeCodeOffset + sizeof(uint64);

	static int PingSequence = 0;

	FIcmpEchoResult Result;
	Result.Status = EIcmpResponseStatus::InternalError;

	FString PortStr;

	TArray<FString> IpParts;
	int32 NumTokens = TargetAddress.ParseIntoArray(IpParts, TEXT(":"));

	FString Address = TargetAddress;
	if (NumTokens == 2)
	{
		Address = IpParts[0];
		PortStr = IpParts[1];
	}

	FString ResolvedAddress;
	if (!ResolveIp(SocketSub, Address, ResolvedAddress))
	{
		Result.Status = EIcmpResponseStatus::Unresolvable;
		return Result;
	}

	int32 Port = 0;
	Lex::FromString(Port, *PortStr);

	//ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSub)
	{
		FSocket* Socket = SocketSub->CreateSocket(NAME_DGram, TEXT("UDPPing"), false);
		if (Socket)
		{
			uint8 SendBuffer[PacketSize];
			// Clear packet buffer
			FMemory::Memset(SendBuffer, 0, PacketSize);

			Result.ResolvedAddress = ResolvedAddress;

			TSharedRef<FInternetAddr> ToAddr = SocketSub->CreateInternetAddr();

			bool bIsValid = false;
			ToAddr->SetIp(*ResolvedAddress, bIsValid);
			ToAddr->SetPort(Port);
			if (bIsValid)
			{
				uint16 SentId = (uint16)FPlatformProcess::GetCurrentProcessId();
				uint16 SentSeq = PingSequence++;

				FUDPPingHeader* PacketHeader = reinterpret_cast<FUDPPingHeader*>(SendBuffer);
				PacketHeader->Id = HtoNS(SentId);
				PacketHeader->Sequence = HtoNS(SentSeq);
				PacketHeader->Checksum = 0;

				// Put some data into the packet payload
				uint32* PayloadStart = (uint32*)(SendBuffer + MagicNumberOffset);
				PayloadStart[0] = HtoNL(MAGIC_HIGH);
				PayloadStart[1] = HtoNL(MAGIC_LOW);

				// Calculate the time packet is to be sent
				uint64* TimeCodeStart = (uint64*)(SendBuffer + TimeCodeOffset);
				FDateTime TimeCode = FDateTime::UtcNow();
				TimeCodeStart[0] = TimeCode.GetTicks();

				// Calculate the packet checksum
				PacketHeader->Checksum = CalculateChecksum(SendBuffer, PacketSize);

				uint8 ResultBuffer[ResultPacketSize];

				double TimeLeft = Timeout;
				double StartTime = FPlatformTime::Seconds();

				int32 BytesSent = 0;
				if (Socket->SendTo(SendBuffer, PacketSize, BytesSent, *ToAddr))
				{
					bool bDone = false;
					while (!bDone)
					{
						if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(TimeLeft)))
						{
							double EndTime = FPlatformTime::Seconds();
							TimeLeft = FPlatformMath::Max(0.0, (double)Timeout - (EndTime - StartTime));

							int32 BytesRead = 0;
							TSharedRef<FInternetAddr> RecvAddr = SocketSub->CreateInternetAddr();
							if (Socket->RecvFrom(ResultBuffer, ResultPacketSize, BytesRead, *RecvAddr) && BytesRead > 0)
							{
								FDateTime NowTime = FDateTime::UtcNow();

								Result.ReplyFrom = RecvAddr->ToString(false);
								FUDPPingHeader* RecvHeader = reinterpret_cast<FUDPPingHeader*>(ResultBuffer);

								// Validate the packet checksum
								const uint16 RecvChecksum = RecvHeader->Checksum;
								RecvHeader->Checksum = 0;
								const uint16 LocalChecksum = (uint16)CalculateChecksum((uint8*)RecvHeader, PacketSize);

								if (RecvChecksum == LocalChecksum)
								{
									// Convert values back from network byte order
									RecvHeader->Id = NtoHS(RecvHeader->Id);
									RecvHeader->Sequence = NtoHS(RecvHeader->Sequence);

									uint32* MagicNumberPtr = (uint32*)(ResultBuffer + MagicNumberOffset);
									if (MagicNumberPtr[0] == MAGIC_HIGH && MagicNumberPtr[1] == MAGIC_LOW)
									{
										// Estimate elapsed time
										uint64* TimeCodePtr = (uint64*)(ResultBuffer + TimeCodeOffset);
										FDateTime PrevTime(*TimeCodePtr);
										double DeltaTime = (NowTime - PrevTime).GetTotalSeconds();

										if (Result.ReplyFrom == Result.ResolvedAddress &&
											RecvHeader->Id == SentId && RecvHeader->Sequence == SentSeq &&
											DeltaTime >= 0.0 && DeltaTime < (60.0 * 1000.0))
										{
											Result.Time = DeltaTime;
											Result.Status = EIcmpResponseStatus::Success;
										}
									}
								}

								bDone = true;
							}
						}
						else
						{
							// timeout
							Result.Status = EIcmpResponseStatus::Timeout;
							Result.ReplyFrom.Empty();
							Result.Time = Timeout;

							bDone = true;
						}
					}
				}
			}

			SocketSub->DestroySocket(Socket);
		}
	}

	return Result;
}

class FUDPPingAsyncResult
	: public FTickerObjectBase
{
public:

	FUDPPingAsyncResult(ISocketSubsystem* InSocketSub, const FString& TargetAddress, float Timeout, uint32 StackSize, FIcmpEchoResultCallback InCallback)
		: FTickerObjectBase(0)
		, SocketSub(InSocketSub)
		, Callback(InCallback)
		, bThreadCompleted(false)
	{
		if (SocketSub)
		{
			bThreadCompleted = false;
			TFunction<FIcmpEchoResult()> Task = [this, TargetAddress, Timeout]()
			{
				auto Result = UDPEchoImpl(SocketSub, TargetAddress, Timeout);
				bThreadCompleted = true;
				return Result;
			};

			FutureResult = AsyncThread(Task, StackSize);
		}
		else
		{
			bThreadCompleted = true;
		}
	}

	virtual ~FUDPPingAsyncResult()
	{
		check(IsInGameThread());

		if (FutureResult.IsValid())
		{
			FutureResult.Wait();
		}
	}

private:
	virtual bool Tick(float DeltaTime) override
	{
		if (bThreadCompleted)
		{
			FIcmpEchoResult Result;
			if (FutureResult.IsValid())
			{
				Result = FutureResult.Get();
			}

			Callback(Result);

			delete this;
			return false;
		}
		return true;
	}

	/** Reference to the socket subsystem */
	ISocketSubsystem* SocketSub;
	/** Callback when the ping result returns */
	FIcmpEchoResultCallback Callback;
	/** Thread task complete */
	FThreadSafeBool bThreadCompleted;
	/** Async result future */
	TFuture<FIcmpEchoResult> FutureResult;
};

void FUDPPing::UDPEcho(const FString& TargetAddress, float Timeout, FIcmpEchoResultCallback HandleResult)
{
	int32 StackSize = 0;

#if PING_ALLOWS_CUSTOM_THREAD_SIZE
	GConfig->GetInt(TEXT("Ping"), TEXT("StackSize"), StackSize, GEngineIni);

	// Sanity clamp
	if (StackSize != 0)
	{
		StackSize = FMath::Max<int32>(FMath::Min<int32>(StackSize, 2 * 1024 * 1024), 32 * 1024);
	}
#endif

	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	new FUDPPingAsyncResult(SocketSub, TargetAddress, Timeout, StackSize, HandleResult);
}
