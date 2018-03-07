// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonTypes.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "OnlineKeyValuePair.h"
#include "Interfaces/OnlineMessageInterface.h"
#include "NboSerializer.h"

void FOnlineMessagePayload::ToBytes(TArray<uint8>& OutBytes) const
{
	FNboSerializeToBuffer Ar(MaxPayloadSize);
	Ar << KeyValData;
	Ar.TrimBuffer();
	OutBytes = Ar.GetBuffer();
}

void FOnlineMessagePayload::FromBytes(const TArray<uint8>& InBytes)
{
	FNboSerializeFromBuffer Ar(InBytes.GetData(), InBytes.Num());
	Ar >> KeyValData;
}

void FOnlineMessagePayload::ToJson(FJsonObject& OutJsonObject) const
{
	TArray<TSharedPtr<FJsonValue> > JsonProperties;
	for (const auto It : KeyValData)
	{
		const FString& PropertyName = It.Key;
		const FVariantData& PropertyValue = It.Value;

		TSharedRef<FJsonObject> PropertyJson = PropertyValue.ToJson();
		PropertyJson->SetStringField(TEXT("Name"), *PropertyName);
		JsonProperties.Add(MakeShareable(new FJsonValueObject(PropertyJson)));
	}
	OutJsonObject.SetArrayField(TEXT("Properties"), JsonProperties);
}

FString FOnlineMessagePayload::ToJsonStr() const
{
	FString PayloadJsonStr;
	TSharedRef<FJsonObject> JsonObject(new FJsonObject());
	ToJson(*JsonObject);
	auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&PayloadJsonStr);
	FJsonSerializer::Serialize(JsonObject, JsonWriter);
	JsonWriter->Close();
	return PayloadJsonStr;
}

void FOnlineMessagePayload::FromJson(const FJsonObject& JsonObject)
{
	if (JsonObject.HasTypedField<EJson::Array>(TEXT("Properties")))
	{
		KeyValData.Empty();
		const TArray<TSharedPtr<FJsonValue> >& JsonProperties = JsonObject.GetArrayField(TEXT("Properties"));
		for (auto JsonPropertyValue : JsonProperties)
		{
			TSharedPtr<FJsonObject> JsonPropertyObject = JsonPropertyValue->AsObject();
			if (JsonPropertyObject.IsValid())
			{
				FString PropertyName;
				if (JsonPropertyObject->TryGetStringField(TEXT("Name"), PropertyName) &&
					!PropertyName.IsEmpty())
				{
					FVariantData PropertyData;
					if (PropertyData.FromJson(JsonPropertyObject.ToSharedRef()))
					{
						KeyValData.Add(PropertyName, PropertyData);
					}
				}
			}
		}
	}
}

void FOnlineMessagePayload::FromJsonStr(const FString& JsonStr)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonStr);
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) &&
		JsonObject.IsValid())
	{
		FromJson(*JsonObject);
	}
}

bool FOnlineMessagePayload::GetAttribute(const FString& AttrName, FVariantData& OutAttrValue) const
{
	const FVariantData* Value = KeyValData.Find(AttrName);
	if (Value != NULL)
	{
		OutAttrValue = *Value;
		return true;
	}
	return false;
}

void FOnlineMessagePayload::SetAttribute(const FString& AttrName, const FVariantData& AttrValue)
{
	KeyValData.Add(AttrName, AttrValue);
}
