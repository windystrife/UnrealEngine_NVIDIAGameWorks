// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "IcmpPrivate.h"
#include "Icmp.h"

#if PLATFORM_USES_POSIX_IMCP


#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <poll.h>

#ifndef n_short
typedef u_int16_t n_short; /* short as received from the net */
#endif

namespace IcmpPosix
{
	// 32 bytes is the default size for the windows ping utility, and windows has problems with sending < 18 bytes.
	const SIZE_T IcmpPayloadSize = 32;
	const uint8 IcmpPayload[IcmpPayloadSize] = ">>>>This string is 32 bytes<<<<";

	// A critical section that ensures we only have a single ping in flight at once.
	FCriticalSection gPingCS;

	// Returns the ip address as string
	FString IpToString(void* Address)
	{
		ANSICHAR Buffer[INET_ADDRSTRLEN];
		return ANSI_TO_TCHAR(inet_ntop(AF_INET, Address, Buffer, INET_ADDRSTRLEN));
	}
}

uint16 NtoHS(uint16 val)
{
	return ntohs(val);
}

uint16 HtoNS(uint16 val)
{
	return htons(val);
}

uint32 NtoHL(uint32 val)
{
	return ntohl(val);
}

uint32 HtoNL(uint32 val)
{
	return htonl(val);
}

FIcmpEchoResult IcmpEchoImpl(ISocketSubsystem* SocketSub, const FString& TargetAddress, float Timeout)
{
	static const SIZE_T IpHeaderSize = sizeof(struct ip);
	static const SIZE_T IcmpHeaderSize = sizeof(struct icmp);

	// The packet we send is just the ICMP header plus our payload
	static const SIZE_T PacketSize = IcmpHeaderSize + IcmpPosix::IcmpPayloadSize;

	// The result read back will need a room for the IP header as well the icmp echo reply packet
	static const SIZE_T ResultPacketSize = IpHeaderSize + PacketSize;
	static int PingSequence = 0;

	FIcmpEchoResult Result;
	Result.Status = EIcmpResponseStatus::InternalError;

	struct sockaddr_in Address = {0};
	uint8 Packet[PacketSize];

	int IcmpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	if (IcmpSocket < 0)
	{
		return Result;
	}
	// Clear packet buffer
	FMemory::Memset(Packet, 0, PacketSize);

	FString ResolvedAddress;
	if (!ResolveIp(SocketSub, TargetAddress, ResolvedAddress))
	{
		Result.Status = EIcmpResponseStatus::Unresolvable;
		return Result;
	}
	Result.ResolvedAddress = ResolvedAddress;
	Address.sin_family = AF_INET;
	inet_aton(TCHAR_TO_UTF8(*ResolvedAddress), &Address.sin_addr);  // should be inet_pton check error code

	// Build the ICMP header
	struct icmp *PacketHeader = reinterpret_cast<struct icmp*>(Packet);
	PacketHeader->icmp_type = ICMP_ECHO;
	PacketHeader->icmp_code = 0;
	PacketHeader->icmp_cksum = 0; // chksum must be zero when calculating it

	n_short SentId = PacketHeader->icmp_id = getpid() & 0xFFFF;
	n_short SentSeq = PacketHeader->icmp_seq = PingSequence++;

	// Put some data into the packet payload
	uint8* Payload = Packet + IcmpHeaderSize;
	FMemory::Memcpy(Payload, IcmpPosix::IcmpPayload, IcmpPosix::IcmpPayloadSize);

	// Calculate the IP checksum
	PacketHeader->icmp_cksum = CalculateChecksum(Packet, PacketSize);

	uint8 ResultBuffer[ResultPacketSize];

	// We can only have one ping in flight at once, as otherwise we risk swapping echo replies between requests
	{
		FScopeLock PingLock(&IcmpPosix::gPingCS);
		double TimeLeft = Timeout;
		double StartTime = FPlatformTime::Seconds();
		if (0 < sendto(IcmpSocket, Packet, PacketSize, 0, reinterpret_cast<struct sockaddr*>(&Address), sizeof(Address)))
		{
			struct pollfd PollData[1];
			PollData[0].fd = IcmpSocket;
			PollData[0].events = POLLIN;
			bool bDone = false;

			while (!bDone)
			{
				int NumReady = poll(PollData, 1, int(TimeLeft * 1000.0));
				if (NumReady == 0)
				{
					// timeout - if we've received an 'Unreachable' result earlier, return that result instead.
					if (Result.Status != EIcmpResponseStatus::Unreachable)
					{
						Result.Status = EIcmpResponseStatus::Timeout;
						Result.Time = Timeout;
						Result.ReplyFrom.Empty();
					}
					bDone = true;
				}
				else if (NumReady == 1)
				{
					int ReadSize = recv(IcmpSocket, ResultBuffer, ResultPacketSize, 0);

					double EndTime = FPlatformTime::Seconds();

					// Estimate elapsed time
					Result.Time = EndTime - StartTime;

					TimeLeft = FPlatformMath::Max(0.0, (double)Timeout - Result.Time);

					if (ReadSize > IpHeaderSize)
					{
						struct ip* IpHeader = reinterpret_cast<struct ip*>(ResultBuffer);
						if (IpHeader->ip_p != IPPROTO_ICMP)
						{
							// We got a non-ICMP packet back??!
						}
						else
						{
							PacketHeader = reinterpret_cast<struct icmp*>(ResultBuffer + IpHeaderSize);
							Result.ReplyFrom = IcmpPosix::IpToString(&IpHeader->ip_src.s_addr);
							switch (PacketHeader->icmp_type)
							{
								case ICMP_ECHOREPLY:
									if (Result.ReplyFrom == ResolvedAddress &&
										PacketHeader->icmp_id == SentId && PacketHeader->icmp_seq == SentSeq)
									{
										Result.Status = EIcmpResponseStatus::Success;
										bDone = true;
									}
									break;
								case ICMP_UNREACH:
									Result.Status = EIcmpResponseStatus::Unreachable;
									// If there is still time left, try waiting for another result.
									// If we run out of time, we'll return Unreachable instead of Timeout.
									break;
								default:
									break;
							}
						}
					}
				}
			}
		}

		close(IcmpSocket);
	}

	return Result;
}

#endif //PLATFORM_USES_POSIX_IMCP
