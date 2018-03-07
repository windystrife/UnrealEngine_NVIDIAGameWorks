// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAchievementsInterfaceGooglePlay.h"
#include "OnlineAsyncTaskManagerGooglePlay.h"
#include "Online.h"
#include "Android/AndroidJNI.h"
#include "OnlineAsyncTaskGooglePlayQueryAchievements.h"
#include "OnlineSubsystemGooglePlay.h"
#include "UObject/Class.h"

using namespace gpg;

FOnlineAchievementsGooglePlay::FOnlineAchievementsGooglePlay( FOnlineSubsystemGooglePlay* InSubsystem )
	: AndroidSubsystem(InSubsystem)
{
	// Make sure the Google achievement cache is initialized to an invalid status so we know to query it first
	GoogleAchievements.status = ResponseStatus::ERROR_TIMEOUT;
}

void FOnlineAchievementsGooglePlay::QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	if (AndroidSubsystem->GetGameServices() == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	auto QueryTask = new FOnlineAsyncTaskGooglePlayQueryAchievements(AndroidSubsystem, FUniqueNetIdString(PlayerId), Delegate);
	AndroidSubsystem->QueueAsyncTask(QueryTask);
}


void FOnlineAchievementsGooglePlay::QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	// Just query achievements to get descriptions
	// FIXME: This feels a little redundant, but we can see how platforms evolve, and make a decision then
	QueryAchievements( PlayerId, Delegate );
}

void FOnlineAchievementsGooglePlay::FinishAchievementWrite(
	const FUniqueNetId& PlayerId,
	const bool bWasSuccessful,
	FOnlineAchievementsWriteRef WriteObject,
	FOnAchievementsWrittenDelegate Delegate)
{
	// If the Google achievement cache is invalid, we can't do anything here - need to
	// be able to get achievement type and max steps for incremental achievements.
	if (GoogleAchievements.status != ResponseStatus::VALID && GoogleAchievements.status != ResponseStatus::VALID_BUT_STALE)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	auto DefaultSettings = GetDefault<UAndroidRuntimeSettings>();

	for (FStatPropertyArray::TConstIterator It(WriteObject->Properties); It; ++It)
	{
		// Access the stat and the value.
		const FVariantData& Stat = It.Value();

		// Create an achievement object which should be reported to the server.
		const FString UnrealAchievementId(It.Key().ToString());
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
				UE_LOG(LogOnline, Error, TEXT("FOnlineAchievementsGooglePlay Trying to write an achievement with incompatible format. Not a float or int"));
				break;
			}
		}

		const auto GoogleAchievement = GetGoogleAchievementFromUnrealId(UnrealAchievementId);
		
		if (!GoogleAchievement.Valid())
		{
			continue;
		}

		const FString GoogleId(GoogleAchievement.Id().c_str());

		UE_LOG(LogOnline, Log, TEXT("Writing achievement name: %s, Google id: %s, progress: %.0f"),
			*UnrealAchievementId, *GoogleId, PercentComplete);
		
		switch(GoogleAchievement.Type())
		{
			case gpg::AchievementType::INCREMENTAL:
			{
				float StepFraction = (PercentComplete / 100.0f) * GoogleAchievement.TotalSteps();
				int RoundedSteps =  FMath::RoundToInt(StepFraction);

				if(RoundedSteps > 0)
				{
					UE_LOG(LogOnline, Log, TEXT("  Incremental: setting progress to %d"), RoundedSteps);
					AndroidSubsystem->GetGameServices()->Achievements().SetStepsAtLeast(GoogleAchievement.Id(), RoundedSteps);
				}
				else
				{
					UE_LOG(LogOnline, Log, TEXT("  Incremental: not setting progress to %d"), RoundedSteps);
				}
				break;
			}

			case gpg::AchievementType::STANDARD:
			{
				// Standard achievements only unlock if the progress is at least 100%.
				if (PercentComplete >= 100.0f)
				{
					UE_LOG(LogOnline, Log, TEXT("  Standard: unlocking"));
					AndroidSubsystem->GetGameServices()->Achievements().Unlock(GoogleAchievement.Id());
				}
				break;
			}
		}
	}

	Delegate.ExecuteIfBound(PlayerId, true);
}

void FOnlineAchievementsGooglePlay::WriteAchievements( const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate )
{
	// @todo: verify the PlayerId passed in is the currently signed in player
		
	if (AndroidSubsystem->GetGameServices() == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	// We need valid data from Google first because we use the achievement type to convert Unreal progress
	// (percentage) to Google progress (binary locked/unlocked or integer number of arbitrary steps).
	// Kick off a query if we don't have valid data.
	if (GoogleAchievements.status != ResponseStatus::VALID)
	{
		auto QueryTask = new FOnlineAsyncTaskGooglePlayQueryAchievements(AndroidSubsystem, FUniqueNetIdString(PlayerId),
			FOnQueryAchievementsCompleteDelegate::CreateRaw(
				this,
				&FOnlineAchievementsGooglePlay::FinishAchievementWrite,
				WriteObject,
				Delegate));
		AndroidSubsystem->QueueAsyncTask(QueryTask);
	}
	else
	{
		FinishAchievementWrite(PlayerId, true, WriteObject, Delegate);
	}
}

EOnlineCachedResult::Type FOnlineAchievementsGooglePlay::GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement>& OutAchievements)
{
	// @todo: verify the PlayerId passed in is the currently signed in player

	OutAchievements.Empty();

	if (GoogleAchievements.status != ResponseStatus::VALID)
	{
		return EOnlineCachedResult::NotFound;
	}

	auto DefaultSettings = GetDefault<UAndroidRuntimeSettings>();

	for (auto& CurrentAchievement : GoogleAchievements.data)
	{
		const auto UnrealAchievement = GetUnrealAchievementFromGoogleAchievement(DefaultSettings, CurrentAchievement);
		if (UnrealAchievement.Id.IsEmpty())
		{
			// Empty id means the achievement wasn't found.
			continue;
		}

		OutAchievements.Add(UnrealAchievement);
	}

	return EOnlineCachedResult::Success;
}


EOnlineCachedResult::Type FOnlineAchievementsGooglePlay::GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc)
{
	auto FoundAchievement = GetGoogleAchievementFromUnrealId(AchievementId);
	if (!FoundAchievement.Valid())
	{
		return EOnlineCachedResult::NotFound;
	}

	OutAchievementDesc.Title = FText::FromString(FString(FoundAchievement.Name().c_str()));
	OutAchievementDesc.LockedDesc = FText::FromString(FString(FoundAchievement.Description().c_str()));
	OutAchievementDesc.UnlockedDesc = FText::FromString(FString(FoundAchievement.Description().c_str()));
	OutAchievementDesc.bIsHidden = FoundAchievement.State() == AchievementState::HIDDEN;
	OutAchievementDesc.UnlockTime = FDateTime(0); // @todo: this

	return EOnlineCachedResult::Success;
}

#if !UE_BUILD_SHIPPING
bool FOnlineAchievementsGooglePlay::ResetAchievements( const FUniqueNetId& PlayerId )
{
	UE_LOG(LogOnline, Log, TEXT("Resetting Google Play achievements."));

	extern void AndroidThunkCpp_ResetAchievements();
	AndroidThunkCpp_ResetAchievements();

	return false;
};
#endif // !UE_BUILD_SHIPPING

EOnlineCachedResult::Type FOnlineAchievementsGooglePlay::GetCachedAchievement( const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement )
{
	// @todo: verify the PlayerId passed in is the currently signed in player

	auto FoundAchievement = GetGoogleAchievementFromUnrealId(AchievementId);
	if (!FoundAchievement.Valid())
	{
		return EOnlineCachedResult::NotFound;
	}

	OutAchievement.Id = AchievementId;
	OutAchievement.Progress = GetProgressFromGoogleAchievement(FoundAchievement);

	return EOnlineCachedResult::Success;
}

Achievement FOnlineAchievementsGooglePlay::GetGoogleAchievementFromUnrealId(const FString& UnrealId) const
{
	auto DefaultSettings = GetDefault<UAndroidRuntimeSettings>();

	FString TargetGoogleId;

	for(const auto& Mapping : DefaultSettings->AchievementMap)
	{
		if(Mapping.Name.Equals(UnrealId))
		{
			TargetGoogleId = Mapping.AchievementID;
			break;
		}
	}

	for (auto& CurrentAchievement : GoogleAchievements.data)
	{
		if (!CurrentAchievement.Valid())
		{
			continue;
		}

		const FString CurrentGoogleId = CurrentAchievement.Id().c_str();

		if (CurrentGoogleId.Equals(TargetGoogleId))
		{
			return CurrentAchievement;
		}
	}

	// Not found, return empty achievement, Valid() will return false for this one.
	return Achievement();
}

FString FOnlineAchievementsGooglePlay::GetUnrealIdFromGoogleId(const UAndroidRuntimeSettings* Settings, const FString& GoogleId)
{
	for(const auto& Mapping : Settings->AchievementMap)
	{
		if(Mapping.AchievementID.Equals(GoogleId))
		{
			return Mapping.Name;
		}
	}

	return FString();
}

void FOnlineAchievementsGooglePlay::UpdateCache(const AchievementManager::FetchAllResponse& Results)
{
	GoogleAchievements = Results;
}

void FOnlineAchievementsGooglePlay::ClearCache()
{
	GoogleAchievements.status = ResponseStatus::ERROR_TIMEOUT; // Is there a better error to use here?
	GoogleAchievements.data.clear();
}

double FOnlineAchievementsGooglePlay::GetProgressFromGoogleAchievement(const Achievement& InAchievement)
{
	if (!InAchievement.Valid())
	{
		return 0.0f;
	}

	if (InAchievement.State() == AchievementState::UNLOCKED)
	{
		return 100.0;
	}

	if (InAchievement.Type() == AchievementType::INCREMENTAL)
	{
		double Fraction = (double)InAchievement.CurrentSteps() / (double)InAchievement.TotalSteps();
		return Fraction * 100.0;
	}
	
	return 0.0;
}

FOnlineAchievement FOnlineAchievementsGooglePlay::GetUnrealAchievementFromGoogleAchievement(const UAndroidRuntimeSettings* Settings, const gpg::Achievement& GoogleAchievement)
{
	FOnlineAchievement OutAchievement;

	if (!GoogleAchievement.Valid())
	{
		OutAchievement.Progress = 0.0;
		return OutAchievement;
	}

	const FString GoogleId = GoogleAchievement.Id().c_str();

	OutAchievement.Id = GetUnrealIdFromGoogleId(Settings, GoogleId);
	OutAchievement.Progress = GetProgressFromGoogleAchievement(GoogleAchievement);

	return OutAchievement;
}
