// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

enum class EIcmpResponseStatus
{
	/** We did receive a valid Echo reply back from the target host */
	Success,
	/** We did not receive any results within the time limit */
	Timeout,
	/** We got an unreachable error from another node on the way */
	Unreachable,
	/** We could not resolve the target address to a valid IP address */
	Unresolvable,
	/** Some internal error happened during setting up or sending the ping packet */
	InternalError,
	/** not implemented - used to indicate we haven't implemented ICMP ping on this platform */
	NotImplemented,

};

struct FIcmpEchoResult
{
	/** Status of the final response */
	EIcmpResponseStatus Status;
	/** Addressed resolved by GetHostName */
	FString ResolvedAddress;
	/** Reply received from this address */
	FString ReplyFrom;
	/** Total round trip time */
	float Time;

	FIcmpEchoResult()
		: Status(EIcmpResponseStatus::InternalError)
		, ResolvedAddress()
		, ReplyFrom()
		, Time(-1)
	{}

	FIcmpEchoResult(const FIcmpEchoResult& Other)
		: Status(Other.Status)
		, ResolvedAddress(Other.ResolvedAddress)
		, ReplyFrom(Other.ReplyFrom)
		, Time(Other.Time)
	{}
};

typedef TFunction<void(FIcmpEchoResult)> FIcmpEchoResultCallback;
DECLARE_DELEGATE_OneParam(FIcmpEchoResultDelegate, FIcmpEchoResult);

// Simple ping interface that sends an ICMP packet to the given address and returns timing info for the reply if reachable
class ICMP_API FIcmp
{
	public:

	/** Send an ICMP echo packet and wait for a reply.
	 *
	 * The name resolution and ping send/receive will happen on a separate thread.
	 * The third argument is a callback function that will be invoked on the game thread after the
	 * a reply has been received from the target address, the timeout has expired, or if there
	 * was an error resolving the address or delivering the ICMP message to it.
	 *
	 * Multiple pings can be issued concurrently and this function will ensure they're executed in
	 * turn in order not to mix ping replies from different nodes.
	 *
	 * @param TargetAddress the target address to ping
	 * @param Timeout max time to wait for a reply
	 * @param HandleResult a callback function that will be called when the result is ready
	 */
	static void IcmpEcho(const FString& TargetAddress, float Timeout, FIcmpEchoResultCallback HandleResult);

	/** Send an ICMP echo packet and wait for a reply.
	 *
	 * This is a wrapper around the above function, taking a delegate instead of a function argument.
	 *
	 * @param TargetAddress the target address to ping
	 * @param Timeout max time to wait for a reply
	 * @param ResultDelegate a delegate that will be called when the result is ready
	 */
	static void IcmpEcho(const FString& TargetAddress, float Timeout, FIcmpEchoResultDelegate ResultDelegate)
	{
		IcmpEcho(TargetAddress, Timeout, [ResultDelegate](FIcmpEchoResult Result)
		{
			ResultDelegate.ExecuteIfBound(Result);
		});
	}
};

// Simple ping interface that sends an ICMP packet over UDP to the given address and returns timing info for the reply if reachable
class ICMP_API FUDPPing
{
public:

	/** Send an ICMP echo packet and wait for a reply.
	 *
	 * The name resolution and ping send/receive will happen on a separate thread.
	 * The third argument is a callback function that will be invoked on the game thread after the
	 * a reply has been received from the target address, the timeout has expired, or if there
	 * was an error resolving the address or delivering the ICMP message to it.
	 *
	 * Multiple pings can be issued concurrently and this function will ensure they're executed in
	 * turn in order not to mix ping replies from different nodes.
	 *
	 * @param TargetAddress the target address to ping
	 * @param Timeout max time to wait for a reply
	 * @param HandleResult a callback function that will be called when the result is ready
	 */
	static void UDPEcho(const FString& TargetAddress, float Timeout, FIcmpEchoResultCallback HandleResult);

	/** Send an ICMP echo packet and wait for a reply.
	 *
	 * This is a wrapper around the above function, taking a delegate instead of a function argument.
	 *
	 * @param TargetAddress the target address to ping
	 * @param Timeout max time to wait for a reply
	 * @param ResultDelegate a delegate that will be called when the result is ready
	 */
	static void UDPEcho(const FString& TargetAddress, float Timeout, FIcmpEchoResultDelegate ResultDelegate)
	{
		UDPEcho(TargetAddress, Timeout, [ResultDelegate](FIcmpEchoResult Result)
		{
			ResultDelegate.ExecuteIfBound(Result);
		});
	}
};


#define EnumCase(Name) case EIcmpResponseStatus::Name : return TEXT(#Name)
static const TCHAR* ToString(EIcmpResponseStatus Status)
{
	switch (Status)
	{
		EnumCase(Success);
		EnumCase(Timeout);
		EnumCase(Unreachable);
		EnumCase(Unresolvable);
		EnumCase(InternalError);
		EnumCase(NotImplemented);
		default:
			return TEXT("Unknown");
	}
}

#undef EnumCase
