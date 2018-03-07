// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemNullTypes.h"
#include "NboSerializer.h"

/**
 * Serializes data in network byte order form into a buffer
 */
class FNboSerializeToBufferNull : public FNboSerializeToBuffer
{
public:
	/** Default constructor zeros num bytes*/
	FNboSerializeToBufferNull() :
		FNboSerializeToBuffer(512)
	{
	}

	/** Constructor specifying the size to use */
	FNboSerializeToBufferNull(uint32 Size) :
		FNboSerializeToBuffer(Size)
	{
	}

	/**
	 * Adds Null session info to the buffer
	 */
 	friend inline FNboSerializeToBufferNull& operator<<(FNboSerializeToBufferNull& Ar, const FOnlineSessionInfoNull& SessionInfo)
 	{
		check(SessionInfo.HostAddr.IsValid());
		// Skip SessionType (assigned at creation)
		Ar << SessionInfo.SessionId;
		Ar << *SessionInfo.HostAddr;
		return Ar;
 	}

	/**
	 * Adds Null Unique Id to the buffer
	 */
	friend inline FNboSerializeToBufferNull& operator<<(FNboSerializeToBufferNull& Ar, const FUniqueNetIdString& UniqueId)
	{
		Ar << UniqueId.UniqueNetIdStr;
		return Ar;
	}
};

/**
 * Class used to write data into packets for sending via system link
 */
class FNboSerializeFromBufferNull : public FNboSerializeFromBuffer
{
public:
	/**
	 * Initializes the buffer, size, and zeros the read offset
	 */
	FNboSerializeFromBufferNull(uint8* Packet,int32 Length) :
		FNboSerializeFromBuffer(Packet,Length)
	{
	}

	/**
	 * Reads Null session info from the buffer
	 */
 	friend inline FNboSerializeFromBufferNull& operator>>(FNboSerializeFromBufferNull& Ar, FOnlineSessionInfoNull& SessionInfo)
 	{
		check(SessionInfo.HostAddr.IsValid());
		// Skip SessionType (assigned at creation)
		Ar >> SessionInfo.SessionId; 
		Ar >> *SessionInfo.HostAddr;
		return Ar;
 	}

	/**
	 * Reads Null Unique Id from the buffer
	 */
	friend inline FNboSerializeFromBufferNull& operator>>(FNboSerializeFromBufferNull& Ar, FUniqueNetIdString& UniqueId)
	{
		Ar >> UniqueId.UniqueNetIdStr;
		return Ar;
	}
};
