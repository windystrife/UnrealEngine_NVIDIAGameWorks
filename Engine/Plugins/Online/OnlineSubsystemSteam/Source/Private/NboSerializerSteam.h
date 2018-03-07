// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemSteamTypes.h"
#include "NboSerializer.h"

/**
 * Serializes data in network byte order form into a buffer
 */
class FNboSerializeToBufferSteam : public FNboSerializeToBuffer
{
public:
	/** Default constructor zeros num bytes*/
	FNboSerializeToBufferSteam() :
		FNboSerializeToBuffer(512)
	{
	}

	/** Constructor specifying the size to use */
	FNboSerializeToBufferSteam(uint32 Size) :
		FNboSerializeToBuffer(Size)
	{
	}

	/**
	 * Adds Steam session info to the buffer
	 */
 	friend inline FNboSerializeToBufferSteam& operator<<(FNboSerializeToBufferSteam& Ar, const FOnlineSessionInfoSteam& SessionInfo)
 	{
		check(SessionInfo.HostAddr.IsValid());
		// Skip SessionType (assigned at creation)
		Ar << *SessionInfo.HostAddr;
		Ar << SessionInfo.SessionId;
		return Ar;
 	}

	/**
	 * Adds Steam Unique Id to the buffer
	 */
	friend inline FNboSerializeToBufferSteam& operator<<(FNboSerializeToBufferSteam& Ar, const FUniqueNetIdSteam& UniqueId)
	{
		Ar << UniqueId.UniqueNetId;
		return Ar;
	}
};

/**
 * Class used to write data into packets for sending via system link
 */
class FNboSerializeFromBufferSteam : public FNboSerializeFromBuffer
{
public:
	/**
	 * Initializes the buffer, size, and zeros the read offset
	 */
	FNboSerializeFromBufferSteam(uint8* Packet,int32 Length) :
		FNboSerializeFromBuffer(Packet,Length)
	{
	}

	/**
	 * Reads Steam session info from the buffer
	 */
 	friend inline FNboSerializeFromBufferSteam& operator>>(FNboSerializeFromBufferSteam& Ar, FOnlineSessionInfoSteam& SessionInfo)
 	{
		check(SessionInfo.HostAddr.IsValid());
		// Skip SessionType (assigned at creation)
		Ar >> *SessionInfo.HostAddr;
		Ar >> SessionInfo.SessionId; 
		return Ar;
 	}

	/**
	 * Reads Steam Unique Id from the buffer
	 */
	friend inline FNboSerializeFromBufferSteam& operator>>(FNboSerializeFromBufferSteam& Ar, FUniqueNetIdSteam& UniqueId)
	{
		Ar >> UniqueId.UniqueNetId;
		return Ar;
	}
};
