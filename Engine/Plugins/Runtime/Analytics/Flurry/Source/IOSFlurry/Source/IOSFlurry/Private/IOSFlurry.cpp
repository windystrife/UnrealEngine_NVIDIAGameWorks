// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSFlurry.h"
#include "IOSFlurryProvider.h"

#if WITH_FLURRY
#import "Flurry.h"
#endif

#include "Analytics.h"

IMPLEMENT_MODULE( FAnalyticsIOSFlurry, IOSFlurry )

TSharedPtr<IAnalyticsProvider> FAnalyticsProviderFlurry::Provider;

void FAnalyticsIOSFlurry::StartupModule()
{
}

void FAnalyticsIOSFlurry::ShutdownModule()
{
	FAnalyticsProviderFlurry::Destroy();
}

TSharedPtr<IAnalyticsProvider> FAnalyticsIOSFlurry::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		const FString Key = GetConfigValue.Execute(TEXT("FlurryApiKey"), true);
		return FAnalyticsProviderFlurry::Create(Key);
	}
	else
	{
		UE_LOG(LogAnalytics, Warning, TEXT("IOSFlurry::CreateAnalyticsProvider called with an unbound config delegate"));
	}
	return nullptr;
}

// Provider

FAnalyticsProviderFlurry::FAnalyticsProviderFlurry(const FString Key) :
	ApiKey(Key)
{
#if WITH_FLURRY

	// Support more config options here

#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

FAnalyticsProviderFlurry::~FAnalyticsProviderFlurry()
{
	if (bHasSessionStarted)
	{
		EndSession();
	}
}

bool FAnalyticsProviderFlurry::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
#if WITH_FLURRY
	if (!bHasSessionStarted)
	{
		NSString* ApiKeyStr = [NSString stringWithFString : ApiKey];
		const int32 AttrCount = Attributes.Num();

		[Flurry startSession : ApiKeyStr];
		if (AttrCount > 0)
		{
			RecordEvent(TEXT("SessionAttributes"), Attributes);
		}

		UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::StartSession(%d attributes)"), AttrCount);
		bHasSessionStarted = true;
	}
	return bHasSessionStarted;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
	return false;
#endif
}

void FAnalyticsProviderFlurry::EndSession()
{
#if WITH_FLURRY
	// Flurry doesn't support ending a session
	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::EndSession - ignoring call"));
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::FlushEvents()
{
#if WITH_FLURRY
	// Flurry doesn't support flushing a session
	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::FlushEvents - ignoring call"));
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::SetUserID(const FString& InUserID)
{
#if WITH_FLURRY
	// Cache the string in case GetUserId is called
	UserId = InUserID;
	NSString* UserIdStr = [NSString stringWithFString : UserId];

	[Flurry setUserID:UserIdStr];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::SetUserID(%s)"), *UserId);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

FString FAnalyticsProviderFlurry::GetUserID() const
{
#if WITH_FLURRY
	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::GetUserID - returning cached id '%s'"), *UserId);

	return UserId;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
	return FString();
#endif
}

void FAnalyticsProviderFlurry::SetGender(const FString& InGender)
{
#if WITH_FLURRY
	NSString* ConvertedStr = [NSString stringWithFString : InGender];

	[Flurry setGender:ConvertedStr];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::SetGender(%s)"), *InGender);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::SetAge(const int32 InAge)
{
#if WITH_FLURRY
	[Flurry setAge:InAge];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::SetAge(%d)"), InAge);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::SetLocation(const FString& InLocation)
{
#if WITH_FLURRY
	FString Lat, Long;
	InLocation.Split(TEXT(","), &Lat, &Long);

	double Latitude = FCString::Atod(*Lat);
	double Longitude = FCString::Atod(*Long);

	[Flurry setLatitude:Latitude longitude:Longitude horizontalAccuracy:0.0 verticalAccuracy:0.0];

	UE_LOG(LogAnalytics, Display, TEXT("Parsed \"lat, long\" string in IOSFlurry::SetLocation(%s) as \"%f, %f\""), *InLocation, Latitude, Longitude);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

FString FAnalyticsProviderFlurry::GetSessionID() const
{
#if WITH_FLURRY
	NSString* Id = [Flurry getSessionID];
	FString ConvertedId(Id);

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::GetSessionID - returning the id as '%s'"), *ConvertedId);

	return ConvertedId;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
	return FString();
#endif
}

bool FAnalyticsProviderFlurry::SetSessionID(const FString& InSessionID)
{
#if WITH_FLURRY
	// Ignored
	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::SetSessionID - ignoring call"));
	return false;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
	return false;
#endif
}

void FAnalyticsProviderFlurry::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
#if WITH_FLURRY
	if (EventName.Len() > 0)
	{
		NSString* ConvertedEventName = [NSString stringWithFString : EventName];
		const int32 AttrCount = Attributes.Num();
		if (AttrCount > 0)
		{
			// Convert the event attributes into a dictionary object
			NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:AttrCount];
			for	(auto Attr : Attributes)
			{
				NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
				NSString* AttrValue = [NSString stringWithFString : Attr.ToString()];
				[AttributesDict setValue:AttrValue forKey:AttrName];
			}
			[Flurry logEvent:ConvertedEventName withParameters:AttributesDict];
		}
		else
		{
			// Log an event with no payload
			[Flurry logEvent:ConvertedEventName];
		}
		UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::RecordEvent('%s', %d attributes)"), *EventName, AttrCount);
	}
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
#if WITH_FLURRY
	NSString* EventName = @"Item Purchase";
	// Build the dictionary
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:4];
	[AttributesDict setValue:[NSString stringWithFString:ItemId] forKey:@"ItemId"];
	[AttributesDict setValue:[NSString stringWithFString:Currency] forKey:@"Currency"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", PerItemCost] forKey:@"PerItemCost"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", ItemQuantity] forKey:@"ItemQuantity"];
	// Send the event
	[Flurry logEvent:EventName withParameters:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::RecordItemPurchase('%s', '%s', %d, %d)"), *ItemId, *Currency, PerItemCost, ItemQuantity);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
#if WITH_FLURRY
	NSString* EventName = @"Currency Purchase";
	// Build the dictionary
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:5];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	[AttributesDict setValue:[NSString stringWithFString:RealCurrencyType] forKey:@"RealCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%.02f", RealMoneyCost] forKey:@"RealMoneyCost"];
	[AttributesDict setValue:[NSString stringWithFString:PaymentProvider] forKey:@"PaymentProvider"];
	// Send the event
	[Flurry logEvent:EventName withParameters:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::RecordCurrencyPurchase('%s', %d, '%s', %.02f, %s)"), *GameCurrencyType, GameCurrencyAmount, *RealCurrencyType, RealMoneyCost, *PaymentProvider);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount)
{
#if WITH_FLURRY
	NSString* EventName = @"Currency Given";
	// Build the dictionary
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:2];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	// Send the event
	[Flurry logEvent:EventName withParameters:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::RecordCurrencyGiven('%s', %d)"), *GameCurrencyType, GameCurrencyAmount);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordItemPurchase(const FString& ItemId, int ItemQuantity, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Item Purchase";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 2;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:ItemId] forKey:@"ItemId"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", ItemQuantity] forKey:@"Quantity"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.ToString()];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	[Flurry logEvent:EventName withParameters:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::RecordItemPurchase('%s', %d, %d)"), *ItemId, ItemQuantity, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Currency Purchase";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 2;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.ToString()];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	[Flurry logEvent:EventName withParameters:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::RecordCurrencyPurchase('%s', %d, %d)"), *GameCurrencyType, GameCurrencyAmount, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Currency Given";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 2;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.ToString()];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	[Flurry logEvent:EventName withParameters:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::RecordCurrencyGiven('%s', %d, %d)"), *GameCurrencyType, GameCurrencyAmount, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordError(const FString& Error, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Error";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 1;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:Error] forKey:@"Error"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.ToString()];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	[Flurry logEvent:EventName withParameters:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::RecordError('%s', %d)"), *Error, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordProgress(const FString& ProgressType, const FString& ProgressHierarchy, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Progress";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 2;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:ProgressType] forKey:@"ProgressType"];
	[AttributesDict setValue:[NSString stringWithFString:ProgressHierarchy] forKey:@"ProgressHierarchy"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.ToString()];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	[Flurry logEvent:EventName withParameters:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSFlurry::RecordProgress('%s', %s, %d)"), *ProgressType, *ProgressHierarchy, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}
