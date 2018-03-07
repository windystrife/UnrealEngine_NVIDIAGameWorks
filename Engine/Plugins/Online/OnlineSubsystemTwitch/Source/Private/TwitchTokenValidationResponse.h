// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "JsonSerializerMacros.h"

/** 
 * Authorization JSON block from Twitch token validation
 */
struct FTwitchTokenValidationResponseAuthorization : public FJsonSerializable
{
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_ARRAY("scopes", Scopes);
	END_JSON_SERIALIZER

	/** List of scope fields the user has given permissions to in the token */
	TArray<FString> Scopes;
};

/** 
 * Token JSON block from Twitch token validation
 */
struct FTwitchTokenValidationResponse : public FJsonSerializable
{
	FTwitchTokenValidationResponse()
		: bTokenIsValid(false)
	{
	}

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("valid", bTokenIsValid);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("authorization", Authorization);
		JSON_SERIALIZE("user_name", UserName);
		JSON_SERIALIZE("user_id", UserId);
		JSON_SERIALIZE("client_id", ClientId);
	END_JSON_SERIALIZER

	/** Whether or not the token is still valid */
	bool bTokenIsValid;
	/** Json block containing scope fields, if the token is valid */
	FTwitchTokenValidationResponseAuthorization Authorization;
	/** Twitch user name, if the token is valid*/
	FString UserName;
	/** Twitch user Id, if the token is valid */
	FString UserId;
	/** Client Id the token was granted for, if the token is valid */
	FString ClientId;
};
