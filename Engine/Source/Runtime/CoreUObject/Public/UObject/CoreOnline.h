// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#define GameSessionName NAME_GameSession
#define PartySessionName NAME_PartySession
#define GamePort NAME_GamePort
#define BeaconPort NAME_BeaconPort

#if !CPP
// Circular dependency on Core vs UHT means we have to noexport these structs so tools can build
USTRUCT(noexport)
struct FJoinabilitySettings
{
	//GENERATED_BODY()

	/** Name of session these settings affect */
	UPROPERTY()
	FName SessionName;
	/** Is this session now publicly searchable */
	UPROPERTY()
	bool bPublicSearchable;
	/** Does this session allow invites */
	UPROPERTY()
	bool bAllowInvites;
	/** Does this session allow public join via presence */
	UPROPERTY()
	bool bJoinViaPresence;
	/** Does this session allow friends to join via presence */
	UPROPERTY()
	bool bJoinViaPresenceFriendsOnly;
	/** Current max players in this session */
	UPROPERTY()
	int32 MaxPlayers;
	/** Current max party size in this session */
	UPROPERTY()
	int32 MaxPartySize;
};

USTRUCT(noexport)
struct FUniqueNetIdWrapper
{
	//GENERATED_BODY()
};

#endif

struct FJoinabilitySettings
{
	/** Name of session these settings affect */
	FName SessionName;
	/** Is this session now publicly searchable */
	bool bPublicSearchable;
	/** Does this session allow invites */
	bool bAllowInvites;
	/** Does this session allow public join via presence */
	bool bJoinViaPresence;
	/** Does this session allow friends to join via presence */
	bool bJoinViaPresenceFriendsOnly;
	/** Current max players in this session */
	int32 MaxPlayers;
	/** Current max party size in this session */
	int32 MaxPartySize;

	FJoinabilitySettings() :
		SessionName(NAME_None),
		bPublicSearchable(false),
		bAllowInvites(false),
		bJoinViaPresence(false),
		bJoinViaPresenceFriendsOnly(false),
		MaxPlayers(0),
		MaxPartySize(0)
	{
	}
};

/**
 * Abstraction of a profile service online Id
 * The class is meant to be opaque
 */
class FUniqueNetId : public TSharedFromThis<FUniqueNetId>
{
protected:

	/** Hidden on purpose */
	FUniqueNetId()
	{
	}

	/** Hidden on purpose */
	FUniqueNetId(const FUniqueNetId& Src)
	{
	}

	/** Hidden on purpose */
	FUniqueNetId& operator=(const FUniqueNetId& Src)
	{
		return *this;
	}

	virtual bool Compare(const FUniqueNetId& Other) const
	{
		return (GetSize() == Other.GetSize()) &&
			(FMemory::Memcmp(GetBytes(), Other.GetBytes(), GetSize()) == 0);
	}

public:

	/**
	 *	Comparison operator
	 */
	bool operator==(const FUniqueNetId& Other) const
	{
		return Other.Compare(*this);
	}

	bool operator!=(const FUniqueNetId& Other) const
	{
		return !(FUniqueNetId::operator==(Other));
	}

	/** 
	 * Get the raw byte representation of this opaque data
	 * This data is platform dependent and shouldn't be manipulated directly
	 *
	 * @return byte array of size GetSize()
	 */
	virtual const uint8* GetBytes() const = 0;

	/** 
	 * Get the size of the opaque data
	 *
	 * @return size in bytes of the data representation
	 */
	virtual int32 GetSize() const = 0;

	/**
	 * Check the validity of the opaque data
	 *
	 * @return true if this is well formed data, false otherwise
	 */
	virtual bool IsValid() const = 0;

	/**
	 * Platform specific conversion to string representation of data
	 *
	 * @return data in string form
	 */
	virtual FString ToString() const = 0;

	/**
	 * Get a human readable representation of the opaque data
	 * Shouldn't be used for anything other than logging/debugging
	 *
	 * @return data in string form
	 */
	virtual FString ToDebugString() const = 0;

	/**
	 * @return hex encoded string representation of unique id
	 */
	FString GetHexEncodedString() const
	{
		if (GetSize() > 0 && GetBytes() != NULL)
		{
			return BytesToHex(GetBytes(), GetSize());
		}
		return FString();
	}

	virtual ~FUniqueNetId() {}
};

struct FUniqueNetIdWrapper
{
public:

	FUniqueNetIdWrapper() 
		: UniqueNetId()
	{
	}

	FUniqueNetIdWrapper(const TSharedRef<const FUniqueNetId>& InUniqueNetId)
		: UniqueNetId(InUniqueNetId)
	{
	}

	FUniqueNetIdWrapper(const TSharedPtr<const FUniqueNetId>& InUniqueNetId)
		: UniqueNetId(InUniqueNetId)
	{
	}

	FUniqueNetIdWrapper(const FUniqueNetIdWrapper& InUniqueNetId)
		: UniqueNetId(InUniqueNetId.UniqueNetId)
	{
	}

	virtual ~FUniqueNetIdWrapper()
	{
	}

	/** Assignment operator */
	FUniqueNetIdWrapper& operator=(const FUniqueNetIdWrapper& Other)
	{
		if (&Other != this)
		{
			UniqueNetId = Other.UniqueNetId;
		}
		return *this;
	}

	/** Comparison operator */
	bool operator==(FUniqueNetIdWrapper const& Other) const
	{
		// Both invalid structs or both valid and deep comparison equality
		bool bBothValid = IsValid() && Other.IsValid();
		bool bBothInvalid = !IsValid() && !Other.IsValid();
		return (bBothInvalid || (bBothValid && (*UniqueNetId == *Other.UniqueNetId)));
	}

	/** Comparison operator */
	bool operator!=(FUniqueNetIdWrapper const& Other) const
	{
		return !(*this == Other);
	}

	/** Convert this value to a string */
	FString ToString() const
	{
		return IsValid() ? UniqueNetId->ToString() : TEXT("INVALID");
	}

	/** Convert this value to a string with additional information */
	virtual FString ToDebugString() const
	{
		return IsValid() ? UniqueNetId->ToDebugString() : TEXT("INVALID");
	}

	/** Is the FUniqueNetId wrapped in this object valid */
	bool IsValid() const
	{
		return UniqueNetId.IsValid() && UniqueNetId->IsValid();
	}
	
	/** 
	 * Assign a unique id to this wrapper object
	 *
	 * @param InUniqueNetId id to associate
	 */
	void SetUniqueNetId(const TSharedPtr<const FUniqueNetId>& InUniqueNetId)
	{
		UniqueNetId = InUniqueNetId;
	}

	/** @return unique id associated with this wrapper object */
	const TSharedPtr<const FUniqueNetId>& GetUniqueNetId() const
	{
		return UniqueNetId;
	}

	/**
	 * Dereference operator returns a reference to the FUniqueNetId
	 */
	const FUniqueNetId& operator*() const
	{
		return *UniqueNetId;
	}

	/**
	 * Arrow operator returns a pointer to this FUniqueNetId
	 */
	const FUniqueNetId* operator->() const
	{
		return UniqueNetId.Get();
	}

protected:

	// Actual unique id
	TSharedPtr<const FUniqueNetId> UniqueNetId;
};
