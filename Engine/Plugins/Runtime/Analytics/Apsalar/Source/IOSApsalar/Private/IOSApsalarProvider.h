// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAnalyticsProvider.h"

class FAnalyticsProviderApsalar :
	public IAnalyticsProvider
{
	/** The API key given to you by Apsalar */
	FString ApiKey;
	/** The API secret generated for you by Apsalar */
	FString ApiSecret;
	/** Tracks whether we need to start the session or restart it */
	bool bHasSessionStarted;

	/** Singleton for analytics */
	static TSharedPtr<IAnalyticsProvider> Provider;
	FAnalyticsProviderApsalar(const FString Key, const FString Secret, const int32 SendInterval, const int32 MaxBufferSize, const bool bWantsManualRevenueReporting);

public:
	static TSharedPtr<IAnalyticsProvider> Create(const FString Key, const FString Secret, const int32 SendInterval, const int32 MaxBufferSize, const bool bWantsManualRevenueReporting)
	{
		if (!Provider.IsValid())
		{
			Provider = TSharedPtr<IAnalyticsProvider>(new FAnalyticsProviderApsalar(Key, Secret, SendInterval, MaxBufferSize, bWantsManualRevenueReporting));
		}
		return Provider;
	}
	static void Destroy()
	{
		Provider.Reset();
	}

	virtual ~FAnalyticsProviderApsalar();

	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void EndSession() override;
	virtual void FlushEvents() override;

	virtual void SetUserID(const FString& InUserID) override;
	virtual FString GetUserID() const override;

	virtual FString GetSessionID() const override;
	virtual bool SetSessionID(const FString& InSessionID) override;

	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;

	virtual void RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity) override;
	virtual void RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider) override;
	virtual void RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount) override;
};
