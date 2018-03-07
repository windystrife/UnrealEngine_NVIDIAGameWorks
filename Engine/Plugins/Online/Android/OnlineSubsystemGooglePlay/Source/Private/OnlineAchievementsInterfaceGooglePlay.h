// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineAchievementsInterface.h"
#include "OnlineSubsystemTypes.h"
#include "AndroidRuntimeSettings.h"

THIRD_PARTY_INCLUDES_START
#include <jni.h>
#include "gpg/achievement_manager.h"
THIRD_PARTY_INCLUDES_END

/**
 *	IOnlineAchievements - Interface class for Achievements
 */
class FOnlineAchievementsGooglePlay : public IOnlineAchievements
{
private:

	/** Reference to the main GameCenter subsystem */
	class FOnlineSubsystemGooglePlay* AndroidSubsystem;
	
	/** hide the default constructor, we need a reference to our OSS */
	FOnlineAchievementsGooglePlay() {}

	/** Our own cache of achievement data directly from Google Play */
	gpg::AchievementManager::FetchAllResponse GoogleAchievements;

	/** Looks up the Google achievement id in the mapping and returns the corresponding cached Achievement object */
	gpg::Achievement GetGoogleAchievementFromUnrealId(const FString& UnrealId) const;

	/** Uses the mapping to convert a Google achievement id to an Unreal achievement name */
	static FString GetUnrealIdFromGoogleId(const UAndroidRuntimeSettings* Settings, const FString& GoogleId);

	/** Convert the progress of a Google achievement to a double percentage 0.0 - 100.0 */
	static double GetProgressFromGoogleAchievement(const gpg::Achievement& InAchievement);

	/**
	 * Convenience function to create an Unreal achievement from a Google achievement
	 *
	 * @param Settings the Android runtime settings containing the achievement id mapping
	 * @param GoogleAchievement the Google achievement object to convert
	 */
	static FOnlineAchievement GetUnrealAchievementFromGoogleAchievement(
		const UAndroidRuntimeSettings* Settings,
		const gpg::Achievement& GoogleAchievement);

	/**
	 * Using the WriteObject, fires off achievement progress calls to the Google backend. Non-blocking.
	 * The GoogleAchievements list should be valid before this is called.
	 *
	 * @param PlayerId the id of the player who's making progress
	 * @param bWasSuccessful whether a previous QueryAchievements call was successful
	 * @param WriteObject achievement write object provided by the user
	 * @param Delegate delegate to execute when the write operation is finished
	 */
	void FinishAchievementWrite(
		const FUniqueNetId& PlayerId,
		const bool bWasSuccessful,
		FOnlineAchievementsWriteRef WriteObject,
		FOnAchievementsWrittenDelegate Delegate);
	
PACKAGE_SCOPE:
	/** Clears the cache of Google achievements that was populated by a QueryAchievements() call. */
	void ClearCache();

	/** Called from the query achievements task to fill in the cache. */
	void UpdateCache(const gpg::AchievementManager::FetchAllResponse& Results);

public:

	//~ Begin IOnlineAchievements Interface
	virtual void WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate = FOnAchievementsWrittenDelegate()) override;
	virtual void QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
	virtual void QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override ;
	virtual EOnlineCachedResult::Type GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement) override;
	virtual EOnlineCachedResult::Type GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement>& OutAchievements) override;
	virtual EOnlineCachedResult::Type GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc) override;
#if !UE_BUILD_SHIPPING
	virtual bool ResetAchievements( const FUniqueNetId& PlayerId ) override;
#endif // !UE_BUILD_SHIPPING
	//~ End IOnlineAchievements Interface

	/**
	 * Constructor
	 *
	 * @param InSubsystem - A reference to the owning subsystem
	 */
	FOnlineAchievementsGooglePlay( class FOnlineSubsystemGooglePlay* InSubsystem );
};

typedef TSharedPtr<FOnlineAchievementsGooglePlay, ESPMode::ThreadSafe> FOnlineAchievementsGooglePlayPtr;
