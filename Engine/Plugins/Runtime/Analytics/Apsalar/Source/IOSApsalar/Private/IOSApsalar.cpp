// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSApsalar.h"
#include "IOSApsalarProvider.h"
#include "Modules/ModuleManager.h"

#if WITH_APSALAR
#import "Apsalar.h"
#endif

#include "Analytics.h"

IMPLEMENT_MODULE( FAnalyticsIOSApsalar, IOSApsalar )

TSharedPtr<IAnalyticsProvider> FAnalyticsProviderApsalar::Provider;

void FAnalyticsIOSApsalar::StartupModule()
{
}

void FAnalyticsIOSApsalar::ShutdownModule()
{
	FAnalyticsProviderApsalar::Destroy();
}

TSharedPtr<IAnalyticsProvider> FAnalyticsIOSApsalar::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		const FString Key = GetConfigValue.Execute(TEXT("ApiKey"), true);
		const FString Secret = GetConfigValue.Execute(TEXT("ApiSecret"), true);
		const FString SendInterval = GetConfigValue.Execute(TEXT("SendInterval"), false);
		const FString MaxBufferSize = GetConfigValue.Execute(TEXT("MaxBufferSize"), false);
		const FString ManuallyReportRevenue = GetConfigValue.Execute(TEXT("ManuallyReportRevenue"), false);
		const bool bWantsManualRevenueReporting = ManuallyReportRevenue.Compare(TEXT("true"), ESearchCase::IgnoreCase) == 0;
		return FAnalyticsProviderApsalar::Create(Key, Secret, FCString::Atoi(*SendInterval), FCString::Atoi(*MaxBufferSize), bWantsManualRevenueReporting);
	}
	else
	{
		UE_LOG(LogAnalytics, Warning, TEXT("IOSApsalar::CreateAnalyticsProvider called with an unbound delegate"));
	}
	return nullptr;
}

#if !UE_BUILD_SHIPPING
	/**
	 * Verifies that the event name matches Apsalar's 32 char limit and warns if it doesn't
	 */
	inline void CheckEventNameLen(const FString& EventName)
	{
		const int32 Length = EventName.Len();
		if (Length > 32)
		{
			UE_LOG(LogAnalytics, Warning, TEXT("Apsalar event name is too long: %s and will be truncated by Apsalar. 32 character max limit."), *EventName);
		}
		else if (Length == 0)
		{
			UE_LOG(LogAnalytics, Warning, TEXT("Apsalar event name is empty!"));
		}
	}

	#define WarnIfEventNameIsWrongLength(x) CheckEventNameLen(x)
#else
	// Have these compile out
	#define WarnIfEventNameIsWrongLength(x)
#endif

// Provider

FAnalyticsProviderApsalar::FAnalyticsProviderApsalar(const FString Key, const FString Secret, const int32 SendInterval, const int32 MaxBufferSize, const bool bWantsManualRevenueReporting) :
	ApiKey(Key),
	ApiSecret(Secret)
{
#if WITH_APSALAR
	// If this is zero, then use the Apsalar default buffer size
	if (MaxBufferSize > 0)
	{
		[Apsalar setBufferLimit:MaxBufferSize];
	}

	// If this is zero, then use the Apsalar default time period
	if (SendInterval > 0)
	{
		[Apsalar setBatchInterval:(int)SendInterval];
	}

	// Disable the auto reporting of revenue if they want to manually report it
	if (bWantsManualRevenueReporting)
	{
		[Apsalar setAllowAutoIAPComplete:NO];
	}
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
#endif
}

FAnalyticsProviderApsalar::~FAnalyticsProviderApsalar()
{
	if (bHasSessionStarted)
	{
		EndSession();
	}
}

bool FAnalyticsProviderApsalar::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
#if WITH_APSALAR
	NSString* ApiKeyStr = [NSString stringWithFString:ApiKey];
	NSString* ApiSecretStr = [NSString stringWithFString:ApiSecret];
	const int32 AttrCount = Attributes.Num();

	if (!bHasSessionStarted)
	{
		[Apsalar startSession:ApiKeyStr withKey:ApiSecretStr];
		if (AttrCount > 0)
		{
			RecordEvent(TEXT("SessionAttributes"), Attributes);
		}

		UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::StartSession(%d attributes)"), AttrCount);
	}
	else
	{
		[Apsalar reStartSession:ApiKeyStr withKey:ApiSecretStr];
		if (AttrCount > 0)
		{
			RecordEvent(TEXT("SessionAttributes"), Attributes);
		}

		UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::RestartSession(%d attributes)"), AttrCount);
	}
	bHasSessionStarted = ([Apsalar sessionStarted] == YES) ? true : false;
	return bHasSessionStarted;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
	return false;
#endif
}

void FAnalyticsProviderApsalar::EndSession()
{
#if WITH_APSALAR
	[Apsalar endSession];
	bHasSessionStarted = false;

	UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::EndSession"));
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderApsalar::FlushEvents()
{
#if WITH_APSALAR
	[Apsalar sendAllBatches];

	UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::FlushEvents"));
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderApsalar::SetUserID(const FString& InUserID)
{
#if WITH_APSALAR
	// Ignored
	UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::SetUserID - ignoring call"));
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
#endif
}

FString FAnalyticsProviderApsalar::GetUserID() const
{
#if WITH_APSALAR
	NSString* ApsalarId = [Apsalar apsalarID];
	const FString ConvertedId(ApsalarId);

	UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::GetUserID - returning the id as '%s'"), *ConvertedId);

	return ConvertedId;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
	return FString();
#endif
}

FString FAnalyticsProviderApsalar::GetSessionID() const
{
#if WITH_APSALAR
	NSString* Id = [Apsalar sessionID];
	FString ConvertedId(Id);

	UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::GetSessionID - returning the id as '%s'"), *ConvertedId);

	return ConvertedId;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
	return FString();
#endif
}

bool FAnalyticsProviderApsalar::SetSessionID(const FString& InSessionID)
{
#if WITH_APSALAR
	// Ignored
	UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::SetSessionID - ignoring call"));
	return false;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
	return false;
#endif
}

void FAnalyticsProviderApsalar::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
#if WITH_APSALAR
	WarnIfEventNameIsWrongLength(EventName);

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
			[Apsalar event:ConvertedEventName withArgs:AttributesDict];
		}
		else
		{
			// Log an event with no payload
			[Apsalar event:ConvertedEventName];
		}
		UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::RecordEvent('%s', %d attributes)"), *EventName, AttrCount);
	}
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderApsalar::RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
#if WITH_APSALAR
	NSString* EventName = @"Item Purchase";
	// Build the dictionary
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:4];
	[AttributesDict setValue:[NSString stringWithFString:ItemId] forKey:@"ItemId"];
	[AttributesDict setValue:[NSString stringWithFString:Currency] forKey:@"Currency"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", PerItemCost] forKey:@"PerItemCost"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", ItemQuantity] forKey:@"ItemQuantity"];
	// Send the event
	[Apsalar event:EventName withArgs:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::RecordItemPurchase('%s', '%s', %d, %d)"), *ItemId, *Currency, PerItemCost, ItemQuantity);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderApsalar::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
#if WITH_APSALAR
	NSString* EventName = @"Currency Purchase";
	// Build the dictionary
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:5];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	[AttributesDict setValue:[NSString stringWithFString:RealCurrencyType] forKey:@"RealCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%.02f", RealMoneyCost] forKey:@"RealMoneyCost"];
	[AttributesDict setValue:[NSString stringWithFString:PaymentProvider] forKey:@"PaymentProvider"];
	// Send the event
	[Apsalar event:EventName withArgs:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::RecordCurrencyPurchase('%s', %d, '%s', %.02f, %s)"), *GameCurrencyType, GameCurrencyAmount, *RealCurrencyType, RealMoneyCost, *PaymentProvider);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderApsalar::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount)
{
#if WITH_APSALAR
	NSString* EventName = @"Currency Given";
	// Build the dictionary
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:2];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	// Send the event
	[Apsalar event:EventName withArgs:AttributesDict];

	UE_LOG(LogAnalytics, Display, TEXT("IOSApsalar::RecordCurrencyGiven('%s', %d)"), *GameCurrencyType, GameCurrencyAmount);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_APSALAR=0. Are you missing the SDK?"));
#endif
}
