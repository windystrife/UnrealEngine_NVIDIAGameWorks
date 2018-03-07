// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

class Error;

/**
 * Implements a fluent builder for UDP sockets.
 */
class FUdpSocketBuilder
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InDescription Debug description for the socket.
	 */
	FUdpSocketBuilder(const FString& InDescription)
		: AllowBroadcast(false)
		, Blocking(false)
		, Bound(false)
		, BoundEndpoint(FIPv4Address::Any, 0)
		, Description(InDescription)
		, MulticastLoopback(false)
		, MulticastTtl(1)
		, ReceiveBufferSize(0)
		, Reusable(false)
		, SendBufferSize(0)
	{ }

public:

	/**
	 * Sets socket operations to be blocking.
	 *
	 * @return This instance (for method chaining).
	 * @see AsNonBlocking, AsReusable
	 */
	FUdpSocketBuilder AsBlocking()
	{
		Blocking = true;

		return *this;
	}

	/**
	 * Sets socket operations to be non-blocking.
	 *
	 * @return This instance (for method chaining).
	 * @see AsBlocking, AsReusable
	 */
	FUdpSocketBuilder AsNonBlocking()
	{
		Blocking = false;

		return *this;
	}

	/**
	 * Makes the bound address reusable by other sockets.
	 *
	 * @return This instance (for method chaining).
	 * @see AsBlocking, AsNonBlocking
	 */
	FUdpSocketBuilder AsReusable()
	{
		Reusable = true;

		return *this;
	}

	/**
 	 * Sets the local address to bind the socket to.
	 *
	 * Unless specified in a subsequent call to BoundToPort(), a random
	 * port number will be assigned by the underlying provider.
	 *
	 * @param Address The IP address to bind the socket to.
	 * @return This instance (for method chaining).
	 * @see BoundToEndpoint, BoundToPort
	 */
	FUdpSocketBuilder BoundToAddress(const FIPv4Address& Address)
	{
		BoundEndpoint = FIPv4Endpoint(Address, BoundEndpoint.Port);
		Bound = true;

		return *this;
	}

	/**
 	 * Sets the local endpoint to bind the socket to.
	 *
	 * @param Endpoint The IP endpoint to bind the socket to.
	 * @return This instance (for method chaining).
	 * @see BoundToAddress, BoundToPort
	 */
	FUdpSocketBuilder BoundToEndpoint(const FIPv4Endpoint& Endpoint)
	{
		BoundEndpoint = Endpoint;
		Bound = true;

		return *this;
	}

	/**
	 * Sets the local port to bind the socket to.
	 *
	 * Unless specified in a subsequent call to BoundToAddress(), the local
	 * address will be determined automatically by the underlying provider.
	 *
	 * @param Port The local port number to bind the socket to.
	 * @return This instance (for method chaining).
	 * @see BoundToAddress
	 */
	FUdpSocketBuilder BoundToPort(int32 Port)
	{
		BoundEndpoint = FIPv4Endpoint(BoundEndpoint.Address, Port);
		Bound = true;

		return *this;
	}

	/**
	 * Joins the socket to the specified multicast group.
	 *
	 * @param GroupAddress The IP address of the multicast group to join.
	 * @return This instance (for method chaining).
	 * @see WithMulticastLoopback, WithMulticastTtl
	 */
	FUdpSocketBuilder JoinedToGroup(const FIPv4Address& GroupAddress)
	{
		JoinedGroups.Add(GroupAddress);

		return *this;
	}

	/**
	 * Enables broadcasting.
	 *
	 * @return This instance (for method chaining).
	 */
	FUdpSocketBuilder WithBroadcast()
	{
		AllowBroadcast = true;

		return *this;
	}

	/**
	 * Enables multicast loopback.
	 *
	 * @param AllowLoopback Whether to allow multicast loopback.
	 * @param TimeToLive The time to live.
	 * @return This instance (for method chaining).
	 * @see JoinedToGroup, WithMulticastTtl
	 */
	FUdpSocketBuilder WithMulticastLoopback()
	{
		MulticastLoopback = true;

		return *this;
	}

	/**
	 * Sets the multicast time-to-live.
	 *
	 * @param TimeToLive The time to live.
	 * @return This instance (for method chaining).
	 * @see JoinedToGroup, WithMulticastLoopback
	 */
	FUdpSocketBuilder WithMulticastTtl(uint8 TimeToLive)
	{
		MulticastTtl = TimeToLive;

		return *this;
	}

	/**
	 * Specifies the desired size of the receive buffer in bytes (0 = default).
	 *
	 * The socket creation will not fail if the desired size cannot be set or
	 * if the actual size is less than the desired size.
	 *
	 * @param SizeInBytes The size of the buffer.
	 * @return This instance (for method chaining).
	 * @see WithSendBufferSize
	 */
	FUdpSocketBuilder WithReceiveBufferSize(int32 SizeInBytes)
	{
		ReceiveBufferSize = SizeInBytes;

		return *this;
	}

	/**
	 * Specifies the desired size of the send buffer in bytes (0 = default).
	 *
	 * The socket creation will not fail if the desired size cannot be set or
	 * if the actual size is less than the desired size.
	 *
	 * @param SizeInBytes The size of the buffer.
	 * @return This instance (for method chaining).
	 * @see WithReceiveBufferSize
	 */
	FUdpSocketBuilder WithSendBufferSize(int32 SizeInBytes)
	{
		SendBufferSize = SizeInBytes;

		return *this;
	}

public:

	/**
	 * Implicit conversion operator that builds the socket as configured.
	 *
	 * @return The built socket.
	 */
	operator FSocket*() const
	{
		return Build();
	}

	/**
	 * Builds the socket as configured.
	 *
	 * @return The built socket.
	 */
	FSocket* Build() const
	{
		// load socket subsystem
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

		if (SocketSubsystem == nullptr)
		{
			GLog->Log(TEXT("FUdpSocketBuilder: Failed to load socket subsystem"));
			return nullptr;
		}

		// create socket
		FSocket* Socket = SocketSubsystem->CreateSocket(NAME_DGram, *Description, true);

		if (Socket == nullptr)
		{
			GLog->Logf(TEXT("FUdpSocketBuilder: Failed to create socket %s"), *Description);
			return nullptr;
		}

		// configure socket
		bool Error =
			!Socket->SetNonBlocking(!Blocking) ||
			!Socket->SetReuseAddr(Reusable) ||
			!Socket->SetBroadcast(AllowBroadcast) ||
			!Socket->SetRecvErr();

		// bind socket
		if (Error)
		{
			GLog->Logf(TEXT("FUdpSocketBuilder: Failed to configure %s (blocking: %i, reusable: %i, broadcast: %i)"), *Description, Blocking, Reusable, AllowBroadcast);
		}
		else
		{
			Error = Bound && !Socket->Bind(*BoundEndpoint.ToInternetAddr());
		}

		// configure multicast
		if (Error)
		{
			GLog->Logf(TEXT("FUdpSocketBuilder: Failed to bind %s to %s"), *Description, *BoundEndpoint.ToString());
		}
		else
		{
			Error = !Socket->SetMulticastLoopback(MulticastLoopback) || !Socket->SetMulticastTtl(MulticastTtl);
		}

		// join multicast groups
		if (Error)
		{
			GLog->Logf(TEXT("FUdpSocketBuilder: Failed to configure multicast for %s (loopback: %i, ttl: %i)"), *Description, MulticastLoopback, MulticastTtl);
		}
		else
		{
			for (const auto& Group : JoinedGroups)
			{
				if (!Socket->JoinMulticastGroup(*FIPv4Endpoint(Group, 0).ToInternetAddr()))
				{
					GLog->Logf(TEXT("FUdpSocketBuilder: Failed to subscribe %s to multicast group %s"), *Description, *Group.ToString());
					Error = true;

					break;
				}
			}
		}

		// set buffer sizes
		if (!Error)
		{
			int32 OutNewSize;

			if (ReceiveBufferSize > 0)
			{
				Socket->SetReceiveBufferSize(ReceiveBufferSize, OutNewSize);
			}

			if (SendBufferSize > 0)
			{
				Socket->SetSendBufferSize(SendBufferSize, OutNewSize);
			}
		}

		if (Error)
		{
			GLog->Logf(TEXT("FUdpSocketBuilder: Failed to create and initialize socket %s (last error: %i)"), *Description, (int32)SocketSubsystem->GetLastErrorCode());
			SocketSubsystem->DestroySocket(Socket);
			Socket = nullptr;
		}

		return Socket;
	}

private:

	/** Holds a flag indicating whether broadcasts will be enabled. */
	bool AllowBroadcast;

	/** Holds a flag indicating whether socket operations are blocking. */
	bool Blocking;

	/** Holds a flag indicating whether the socket should be bound. */
	bool Bound;

	/** Holds the IP address (and port) that the socket will be bound to. */
	FIPv4Endpoint BoundEndpoint;

	/** Holds the socket's debug description text. */
	FString Description;

	/** Holds the list of joined multicast groups. */
	TArray<FIPv4Address> JoinedGroups;

	/** Holds a flag indicating whether multicast loopback will be enabled. */
	bool MulticastLoopback;

	/** Holds the multicast time to live. */
	uint8 MulticastTtl;

	/** The desired size of the receive buffer in bytes (0 = default). */
	int32 ReceiveBufferSize;

	/** Holds a flag indicating whether the bound address can be reused by other sockets. */
	bool Reusable;

	/** The desired size of the send buffer in bytes (0 = default). */
	int32 SendBufferSize;
};
