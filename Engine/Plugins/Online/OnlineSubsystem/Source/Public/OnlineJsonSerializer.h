// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/JsonSerializerMacros.h"
#include "OnlineKeyValuePair.h"

#define BEGIN_ONLINE_JSON_SERIALIZER												BEGIN_JSON_SERIALIZER
#define END_ONLINE_JSON_SERIALIZER													END_JSON_SERIALIZER
#define ONLINE_JSON_SERIALIZE(JsonName, JsonValue)									JSON_SERIALIZE(JsonName, JsonValue)
#define ONLINE_JSON_SERIALIZE_ARRAY(JsonName, JsonArray)							JSON_SERIALIZE_ARRAY(JsonName, JsonArray) 
#define ONLINE_JSON_SERIALIZE_MAP(JsonName, JsonMap)								JSON_SERIALIZE_MAP(JsonName, JsonMap)
#define ONLINE_JSON_SERIALIZE_SERIALIZABLE(JsonName, JsonValue)						JSON_SERIALIZE_SERIALIZABLE(JsonName, JsonValue) 
#define ONLINE_JSON_SERIALIZE_ARRAY_SERIALIZABLE(JsonName, JsonArray, ElementType)	JSON_SERIALIZE_ARRAY_SERIALIZABLE(JsonName, JsonArray, ElementType)
#define ONLINE_JSON_SERIALIZE_MAP_SERIALIZABLE(JsonName, JsonMap, ElementType)		JSON_SERIALIZE_MAP_SERIALIZABLE(JsonName, JsonMap, ElementType) 
#define ONLINE_JSON_SERIALIZE_OBJECT_SERIALIZABLE(JsonName, JsonSerializableObject)	JSON_SERIALIZE_OBJECT_SERIALIZABLE(JsonName, JsonSerializableObject) 

typedef FJsonSerializable FOnlineJsonSerializable;
typedef FJsonSerializerWriter<> FOnlineJsonSerializerWriter;

typedef TMap<FString, FVariantData> FJsonSerializeableKeyValueMapVariant;

