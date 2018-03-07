// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "OnlineJsonSerializer.h"
#include "OnlineSubsystemFacebookPackage.h"

#define PICTURE_DATA "data"
#define PICTURE_DATA_SILHOUETTE "is_silhouette"
#define PICTURE_DATA_URL "url"

#if 0
/**
 * Facebook specific implementation of the unique net id
 */
class FUniqueNetIdFacebook :
	public FUniqueNetId
{
PACKAGE_SCOPE:
	/** Holds the net id for a player */
	uint64 UniqueNetId;

	/** Hidden on purpose */
	FUniqueNetIdFacebook() :
		UniqueNetId(0)
	{
	}

	/**
	 * Copy Constructor
	 *
	 * @param Src the id to copy
	 */
	explicit FUniqueNetIdFacebook(const FUniqueNetIdFacebook& Src) :
		UniqueNetId(Src.UniqueNetId)
	{
	}

public:
	/**
	 * Constructs this object with the specified net id
	 *
	 * @param InUniqueNetId the id to set ours to
	 */
	explicit FUniqueNetIdFacebook(uint64 InUniqueNetId) :
		UniqueNetId(InUniqueNetId)
	{
	}

	//~ Begin FUniqueNetId Interface
	virtual const uint8* GetBytes() const override
	{
		return (uint8*)&UniqueNetId;
	}


	virtual int32 GetSize() const override
	{
		return sizeof(uint64);
	}


	virtual bool IsValid() const override
	{
		return UniqueNetId != 0;
	}


	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("%I64d"), UniqueNetId);
	}


	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("0%I64X"), UniqueNetId);
	}

	//~ End FUniqueNetId Interface


public:
	/** Needed for TMap::GetTypeHash() */
	friend uint32 GetTypeHash(const FUniqueNetIdFacebook& A)
	{
		return (uint32)(A.UniqueNetId) + ((uint32)((A.UniqueNetId) >> 32 ) * 23);
	}
};
#endif

/**
 * Facebook error from JSON payload
 */
class FErrorFacebook :
	public FOnlineJsonSerializable
{
public:

	/**
	 * Constructor
	 */
	FErrorFacebook()
	{
	}

	class FErrorBody :
		public FOnlineJsonSerializable
	{

	public:
		/** Facebook error message */
		FString Message;
		/** Type of error reported by Facebook  */
		FString Type;
		/** Facebook error code */
		int32 Code;
		/** Facebook error sub code */
		int32 ErrorSubCode;
		/** Facebook trace id */
		FString FBTraceId;

		FErrorBody() {}

		// FJsonSerializable
		BEGIN_ONLINE_JSON_SERIALIZER
			ONLINE_JSON_SERIALIZE("message", Message);
			ONLINE_JSON_SERIALIZE("type", Type);
			ONLINE_JSON_SERIALIZE("code", Code);
			ONLINE_JSON_SERIALIZE("error_subcode", ErrorSubCode);
			ONLINE_JSON_SERIALIZE("fbtrace_id", FBTraceId);
		END_ONLINE_JSON_SERIALIZER
	};

	/** Main error body */
	FErrorBody Error;

	/** @return debug output for logs */
	FString ToDebugString() const { return FString::Printf(TEXT("%s [Type:%s Code:%d SubCode:%d Trace:%s]"), *Error.Message, *Error.Type, Error.Code, Error.ErrorSubCode, *Error.FBTraceId); }

	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE_OBJECT_SERIALIZABLE("error", Error);
	END_ONLINE_JSON_SERIALIZER
};

/** Facebook profile picture */
class FUserOnlineFacebookPicture :
	public FOnlineJsonSerializable
{
public:

	struct FPictureData :
		public FOnlineJsonSerializable
	{
		/** Is this picture the default silhouette */
		bool bIsSilhouette;
		/** URL to picture content */
		FString PictureURL;

		BEGIN_ONLINE_JSON_SERIALIZER
			ONLINE_JSON_SERIALIZE(PICTURE_DATA_SILHOUETTE, bIsSilhouette);
			ONLINE_JSON_SERIALIZE(PICTURE_DATA_URL, PictureURL);
		END_ONLINE_JSON_SERIALIZER
	};

	/** User picture */
	FPictureData PictureData;

	// FJsonSerializable
	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE_OBJECT_SERIALIZABLE(PICTURE_DATA, PictureData);
	END_ONLINE_JSON_SERIALIZER
};
