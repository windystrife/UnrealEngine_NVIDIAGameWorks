// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemIOSPackage.h"

/**
 * GameCenter specific implementation of the unique net id
 */
class FUniqueNetIdGameCenter :
	public FUniqueNetId
{
PACKAGE_SCOPE:
	/** Holds the net id for a player */
	uint64 UniqueNetId;

	/** Hidden on purpose */
	FUniqueNetIdGameCenter() :
		UniqueNetId(0)
	{
	}

	/**
	 * Copy Constructor
	 *
	 * @param Src the id to copy
	 */
	explicit FUniqueNetIdGameCenter(const FUniqueNetIdGameCenter& Src) :
		UniqueNetId(Src.UniqueNetId)
	{
	}

public:
	/**
	 * Constructs this object with the specified net id
	 *
	 * @param InUniqueNetId the id to set ours to
	 */
	explicit FUniqueNetIdGameCenter(uint64 InUniqueNetId) :
		UniqueNetId(InUniqueNetId)
	{
	}

	/**
	 * Get the raw byte representation of this net id
	 * This data is platform dependent and shouldn't be manipulated directly
	 *
	 * @return byte array of size GetSize()
	 */
	virtual const uint8* GetBytes() const override
	{
		return (uint8*)&UniqueNetId;
	}

	/**
	 * Get the size of the id
	 *
	 * @return size in bytes of the id representation
	 */
	virtual int32 GetSize() const override
	{
		return sizeof(uint64);
	}

	/**
	 * Check the validity of the id 
	 *
	 * @return true if this is a well formed ID, false otherwise
	 */
	virtual bool IsValid() const override
	{
		return UniqueNetId != 0;
	}

	/** 
	 * Platform specific conversion to string representation of data
	 *
	 * @return data in string form 
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("%I64d"), UniqueNetId);
	}

	/**
	 * Get a human readable representation of the net id
	 * Shouldn't be used for anything other than logging/debugging
	 *
	 * @return id in string form 
	 */
	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("0%I64X"), UniqueNetId);
	}

	/** Needed for TMap::GetTypeHash() */
	friend uint32 GetTypeHash(const FUniqueNetIdGameCenter& A)
	{
		return (uint32)(A.UniqueNetId) + ((uint32)((A.UniqueNetId) >> 32 ) * 23);
	}
};
