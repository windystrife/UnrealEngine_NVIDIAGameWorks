// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/EndUserSettings.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"

#define LOCTEXT_NAMESPACE "EndUserSettings"

UEndUserSettings::UEndUserSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bSendAnonymousUsageDataToEpic(true)
	, bSendMeanTimeBetweenFailureDataToEpic(false)
	, bAllowUserIdInUsageData(false)
{
}

void UEndUserSettings::GetToogleCategoryAndPropertyNames(FName& OutCategory, FName& OutProperty) const
{
	OutCategory = FName("Privacy");
	OutProperty = FName("bSendAnonymousUsageDataToEpic");
};

FText UEndUserSettings::GetFalseStateLabel() const
{
	return LOCTEXT("FalseStateLabel", "End-users don't send");
};

FText UEndUserSettings::GetFalseStateTooltip() const
{
	return LOCTEXT("FalseStateTooltip", "By default, your end-users don't send anonymous usage data to Epic Games.");
};

FText UEndUserSettings::GetFalseStateDescription() const
{
	return LOCTEXT("FalseStateDescription", "You have defaulted your users' settings to not send anonymous usage data to Epic Games. You can allow users to opt-in by adding this setting to your game UI and calling UEndUserSettings::SetSendAnonymousUsageDataToEpic() with their choice. Please consider defaulting this to true or allowing users to switch it on to help improve Unreal Engine. Epic Games will never sell or trade individual usage data to / with third party organizations. When permitted, we collect information about your users' game sessions, the game they're playing and for how long they play. Their information would be encrypted and sent anonymously, and only when they run your product.");
};

FText UEndUserSettings::GetTrueStateLabel() const
{
	return LOCTEXT("TrueStateLabel", "End-users send to Epic");
};

FText UEndUserSettings::GetTrueStateTooltip() const
{
	return LOCTEXT("TrueStateTooltip", "By default, your end-users send anonymous usage data to Epic Games.");
};

FText UEndUserSettings::GetTrueStateDescription() const
{
	return LOCTEXT("TrueStateDescription", "You have defaulted your users' settings to send anonymous usage data to Epic Games. You can allow users to opt-out by adding this setting to your game UI and calling UEndUserSettings::SetSendAnonymousUsageDataToEpic() with their choice. Thank you for helping to improve Unreal Engine. Epic Games will never sell or trade individual usage data to / with third party organizations. We will collect information about your users' game sessions, the game they're playing and for how long they play. Their information is sent anonymously. Their information is only sent when they run your product, it is encrypted and sent to our servers.");
};

FString UEndUserSettings::GetAdditionalInfoUrl() const
{
	return FString(TEXT("http://epicgames.com/privacynotice"));
};

FText UEndUserSettings::GetAdditionalInfoUrlLabel() const
{
	return LOCTEXT("HyperlinkLabel", "Epic Games Privacy Notice");
};

void UEndUserSettings::SetSendAnonymousUsageDataToEpic(bool bEnable)
{
	if (bSendAnonymousUsageDataToEpic != bEnable)
	{
		bSendAnonymousUsageDataToEpic = bEnable;
		OnSendAnonymousUsageDataToEpicChanged();
	}
}

void UEndUserSettings::OnSendAnonymousUsageDataToEpicChanged()
{
	if (bSendAnonymousUsageDataToEpic)
	{
		// Attempt to initialize analytics and send opt-in event
		if (!FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::Initialize();
			if (FEngineAnalytics::IsAvailable())
			{
				FEngineAnalytics::GetProvider().RecordEvent(FString("Engine.Privacy.EndUserOptIn"));
			}
		}
	}
	else
	{
		// Send opt-out event and shutdown analytics
		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(FString("Engine.Privacy.EndUserOptOut"));
			const bool bIsEngineShutdown = false;
			FEngineAnalytics::Shutdown(bIsEngineShutdown);
		}
	}
}

#undef LOCTEXT_NAMESPACE
