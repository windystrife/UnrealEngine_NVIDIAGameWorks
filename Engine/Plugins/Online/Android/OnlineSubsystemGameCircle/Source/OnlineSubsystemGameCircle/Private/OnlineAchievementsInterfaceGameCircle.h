// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAchievementsInterface.h"

#include "OnlineAGSAchievementsClientCallbacks.h"

/**
 *	IOnlineAchievements - Interface class for Achievements
 */
class FOnlineAchievementsGameCircle : public IOnlineAchievements
{
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
	FOnlineAchievementsGameCircle( class FOnlineSubsystemGameCircle* InSubsystem );

	void SaveGetAchievementsCallbackResponse(const AmazonGames::AchievementsData* responseStruct);

private:

	/** Reference to the main GameCenter subsystem */
	class FOnlineSubsystemGameCircle* AndroidSubsystem;

	TArray< FOnlineAchievement > UnrealAchievements;
	const AmazonGames::AchievementsData * AmazonAchievementsData;

	/** hide the default constructor, we need a reference to our OSS */
	FOnlineAchievementsGameCircle() {}

PACKAGE_SCOPE:
	/** Clears the cache of Google achievements that was populated by a QueryAchievements() call. */
	void ClearCache();
};

typedef TSharedPtr<FOnlineAchievementsGameCircle, ESPMode::ThreadSafe> FOnlineAchievementsGameCirclePtr;
