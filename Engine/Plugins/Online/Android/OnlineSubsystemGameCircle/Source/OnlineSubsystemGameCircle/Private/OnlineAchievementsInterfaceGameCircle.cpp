// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAchievementsInterfaceGameCircle.h"
#include "Online.h"
#include "OnlineAchievementsInterface.h"
#include "Android/AndroidJNI.h"
#include "AGS/PlayerClientInterface.h"
#include "OnlineSubsystemGameCircle.h"


FOnlineAchievementsGameCircle::FOnlineAchievementsGameCircle( FOnlineSubsystemGameCircle* InSubsystem )
	: AndroidSubsystem(InSubsystem)
	, AmazonAchievementsData(nullptr)
{
}

void FOnlineAchievementsGameCircle::QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	if(AndroidSubsystem->GetCallbackManager() == nullptr || AndroidSubsystem->GetIdentityGameCircle()->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	AmazonGames::AchievementsClientInterface::getAchievements(new FOnlineGetAchievementsCallback(AndroidSubsystem, FUniqueNetIdString(PlayerId), Delegate));
}

void FOnlineAchievementsGameCircle::QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	// FIXME: This feels a little redundant, but we can see how platforms evolve, and make a decision then
	QueryAchievements(PlayerId, Delegate);
}

void FOnlineAchievementsGameCircle::WriteAchievements( const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate )
{		
	if (AndroidSubsystem->GetCallbackManager() == nullptr || AndroidSubsystem->GetIdentityGameCircle()->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}


	for (FStatPropertyArray::TConstIterator It(WriteObject->Properties); It; ++It)
	{
		// Access the stat and the value.
		const FVariantData& Stat = It.Value();

		// Create an achievement object which should be reported to the server.
		const FString AchievementId(It.Key().ToString());
		float PercentComplete = 0.0f;

		// Setup the percentage complete with the value we are writing from the variant type
		switch (Stat.GetType())
		{
		case EOnlineKeyValuePairDataType::Int32:
			{
				int32 Value;
				Stat.GetValue(Value);
				PercentComplete = (float)Value;
				break;
			}
		case EOnlineKeyValuePairDataType::Float:
			{
				float Value;
				Stat.GetValue(Value);
				PercentComplete = Value;
				break;
			}
		default:
			{
				UE_LOG(LogOnline, Error, TEXT("FOnlineAchievementsGameCircle Trying to write an achievement with incompatible format. Not a float or int"));
				break;
			}
		}

		AmazonGames::AchievementsClientInterface::updateProgress(FOnlineSubsystemGameCircle::ConvertFStringToStdString(AchievementId).c_str(), 
																	PercentComplete, new FOnlineUpdateProgressCallback(AndroidSubsystem));
	}

	Delegate.ExecuteIfBound(PlayerId, true);
}

EOnlineCachedResult::Type FOnlineAchievementsGameCircle::GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement>& OutAchievements)
{
	OutAchievements = UnrealAchievements;
	return EOnlineCachedResult::Success;
}


EOnlineCachedResult::Type FOnlineAchievementsGameCircle::GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc)
{
	std::string stdAchievementID = FOnlineSubsystemGameCircle::ConvertFStringToStdString(AchievementId);

	for(int AmazonIndex = 0; AmazonIndex < AmazonAchievementsData->numAchievements; ++AmazonIndex)
	{
		if(stdAchievementID == AmazonAchievementsData->achievements[AmazonIndex].id)
		{
			OutAchievementDesc.Title = FText::FromString( FString(AmazonAchievementsData->achievements[AmazonIndex].title) );
			if(FMath::IsNearlyEqual(AmazonAchievementsData->achievements[AmazonIndex].progress, 100.f))
			{
				OutAchievementDesc.UnlockedDesc = FText::FromString( FString(AmazonAchievementsData->achievements[AmazonIndex].description) );
			}
			else
			{
				OutAchievementDesc.LockedDesc = FText::FromString( FString(AmazonAchievementsData->achievements[AmazonIndex].description) );
			}
			OutAchievementDesc.bIsHidden = AmazonAchievementsData->achievements[AmazonIndex].isHidden;

			return EOnlineCachedResult::Success;
		}
	}

	return EOnlineCachedResult::NotFound;
}

#if !UE_BUILD_SHIPPING
bool FOnlineAchievementsGameCircle::ResetAchievements( const FUniqueNetId& PlayerId )
{
	return false;
};
#endif // !UE_BUILD_SHIPPING

void FOnlineAchievementsGameCircle::SaveGetAchievementsCallbackResponse(const AmazonGames::AchievementsData* responseStruct)
{
	int AchIndex;
	FOnlineAchievement NewAchievement;

	AmazonAchievementsData = responseStruct;

	FPlatformMisc::LowLevelOutputDebugString(TEXT("SaveAchievements_Internal"));
	UnrealAchievements.Empty();
	for(AchIndex = 0; AchIndex < responseStruct->numAchievements; ++AchIndex)
	{
		NewAchievement.Id = responseStruct->achievements[AchIndex].id;
		NewAchievement.Progress = responseStruct->achievements[AchIndex].progress;

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s"), *NewAchievement.ToDebugString());

		UnrealAchievements.Add(NewAchievement);
	}
}

EOnlineCachedResult::Type FOnlineAchievementsGameCircle::GetCachedAchievement( const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement )
{
	for(const FOnlineAchievement& CachedAchievement : UnrealAchievements)
	{
		if(CachedAchievement.Id == AchievementId)
		{
			OutAchievement = CachedAchievement;
			return EOnlineCachedResult::Success;
		}
	}

	return EOnlineCachedResult::NotFound;
}

void FOnlineAchievementsGameCircle::ClearCache()
{
	UnrealAchievements.Empty();
	AmazonAchievementsData = nullptr;
}
