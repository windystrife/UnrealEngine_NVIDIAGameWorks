// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemOculusPackage.h"
#include "OVR_Platform.h"

class FUniqueNetIdOculus : public FUniqueNetId {
private:
	ovrID ID;

protected:
	bool Compare(const FUniqueNetId& Other) const override
	{
		if (Other.GetSize() != sizeof(ovrID))
		{
			return false;
		}

		return ID == static_cast<const FUniqueNetIdOculus&>(Other).ID;
	}

public:
	/** Default constructor */
	FUniqueNetIdOculus()
	{
		ID = 0;
	}

	FUniqueNetIdOculus(const ovrID& id)
	{
		ID = id;
	}

	FUniqueNetIdOculus(const FString& id)
	{
		ovrID_FromString(&ID, TCHAR_TO_ANSI(*id));
	}

	/**
	* Copy Constructor
	*
	* @param Src the id to copy
	*/
	explicit FUniqueNetIdOculus(const FUniqueNetId& Src)
	{
		if (Src.GetSize() == sizeof(ovrID))
		{
			ID = static_cast<const FUniqueNetIdOculus&>(Src).ID;
		}
	}

	// IOnlinePlatformData

	virtual const uint8* GetBytes() const override
	{
		return reinterpret_cast<const uint8*>(&ID);
	}

	virtual int32 GetSize() const override
	{
		return sizeof(ID);
	}

	virtual bool IsValid() const override
	{
		// Not completely accurate, but safe to assume numbers below this is invalid
		return ID > 100000;
	}

	ovrID GetID() const
	{
		return ID;
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("%llu"), ID);
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("ovrID: %llu"), ID);
	}

	/** Needed for TMap::GetTypeHash() */
	friend uint32 GetTypeHash(const FUniqueNetIdOculus& A)
	{
		return GetTypeHash((uint64)A.ID);
	}
};

/**
* Implementation of session information
*/
class FOnlineSessionInfoOculus : public FOnlineSessionInfo
{
protected:

	/** Hidden on purpose */
	FOnlineSessionInfoOculus(const FOnlineSessionInfoOculus& Src)
	{
	}

	/** Hidden on purpose */
	FOnlineSessionInfoOculus& operator=(const FOnlineSessionInfoOculus& Src)
	{
		return *this;
	}

PACKAGE_SCOPE:

	FOnlineSessionInfoOculus(ovrID RoomId);

	/** Unique Id for this session */
	FUniqueNetIdOculus SessionId;

public:

	virtual ~FOnlineSessionInfoOculus() {}

	bool operator==(const FOnlineSessionInfoOculus& Other) const
	{
		return Other.SessionId == SessionId;
	}

	virtual const uint8* GetBytes() const override
	{
		return nullptr;
	}

	virtual int32 GetSize() const override
	{
		return 0;
	}

	virtual bool IsValid() const override
	{
		return true;
	}

	virtual FString ToString() const override
	{
		return SessionId.ToString();
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("SessionId: %s"), *SessionId.ToDebugString());
	}

	virtual const FUniqueNetId& GetSessionId() const override
	{
		return SessionId;
	}
};
