// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineNotification.h"
#include "Serialization/JsonTypes.h"
#include "JsonObjectConverter.h"
#include "OnlineSubsystem.h"

FOnlineNotification::FOnlineNotification(
	const FString& InTypeStr,
	const TSharedPtr<FJsonValue>& InPayload,
	TSharedPtr<const FUniqueNetId> InToUserId,
	TSharedPtr<const FUniqueNetId> InFromUserId
)
: TypeStr(InTypeStr)
, Payload(InPayload.IsValid() ? InPayload->AsObject() : nullptr)
, ToUserId(InToUserId)
, FromUserId(InFromUserId)
{
}

bool FOnlineNotification::ParsePayload(UStruct* StructType, void* StructPtr) const
{
	check(StructType && StructPtr);
	return Payload.IsValid() && FJsonObjectConverter::JsonObjectToUStruct(Payload.ToSharedRef(), StructType, StructPtr, 0, 0);
}

void FOnlineNotification::SetTypeFromPayload()
{
	// Lazy init of type, if supplied in payload
	if (Payload.IsValid() && TypeStr.IsEmpty())
	{
		if (!Payload->TryGetStringField(TEXT("Type"), TypeStr))
		{
			UE_LOG(LogOnline, Error, TEXT("No type in notification JSON object"));
			TypeStr = TEXT("<no type>");
		}
	}
}
