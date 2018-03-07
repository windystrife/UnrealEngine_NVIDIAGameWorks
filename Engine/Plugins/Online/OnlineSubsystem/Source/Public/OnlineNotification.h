// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreOnline.h"
#include "JsonObjectWrapper.h"

class FJsonValue;

/** Notification object, used to send messages between systems */
struct ONLINESUBSYSTEM_API FOnlineNotification
{
	/**
	 * Default constructor
	 */
	FOnlineNotification()
	{
	}

	/**
	 * Constructor from type and FJsonValue
	 * System message unless ToUserId is specified; FromUserId optional
	 */
	FOnlineNotification(
		const FString& InTypeStr,
		const TSharedPtr<FJsonValue>& InPayload,
		TSharedPtr<const FUniqueNetId> InToUserId = nullptr,
		TSharedPtr<const FUniqueNetId> InFromUserId = nullptr
	);

	/**
	 * Constructor from type and FJsonObject
	 * System message unless ToUserId is specified; FromUserId optional
	 */
	FOnlineNotification(
		const FString& InTypeStr,
		const TSharedPtr<FJsonObject>& InPayload,
		TSharedPtr<const FUniqueNetId> InToUserId = nullptr,
		TSharedPtr<const FUniqueNetId> InFromUserId = nullptr
	)
	: TypeStr(InTypeStr)
	, Payload(InPayload)
	, ToUserId(InToUserId)
	, FromUserId(InFromUserId)
	{
	}


	/**
	 * Parse a payload and assume there is a static const TypeStr member to use
	 */
	template <class FStruct>
	bool ParsePayload(FStruct& PayloadOut) const
	{
		return ParsePayload(FStruct::StaticStruct(), &PayloadOut);
	}

	/**
	 * Parse out Payload into the provided UStruct
	 */
	bool ParsePayload(UStruct* StructType, void* StructPtr) const;

	/**
	 * Does this notification have a valid payload?
	 */
	explicit operator bool() const
	{
		return Payload.IsValid();
	}

	/**
	 * Set up the type string for the case where the type is embedded in the payload
	 */
	void SetTypeFromPayload();

	/** A string defining the type of this notification, used to determine how to parse the payload */
	FString TypeStr;

	/** The payload of this notification */
	TSharedPtr<FJsonObject> Payload;

	/** User to deliver the notification to.  Can be null for system notifications. */
	TSharedPtr<const FUniqueNetId> ToUserId;

	/** User who sent the notification, optional. */
	TSharedPtr<const FUniqueNetId> FromUserId;
};
