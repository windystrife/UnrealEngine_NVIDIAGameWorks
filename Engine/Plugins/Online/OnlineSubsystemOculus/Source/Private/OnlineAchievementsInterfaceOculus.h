// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAchievementsInterface.h"
#include "OnlineIdentityOculus.h"
#include "OnlineSubsystemOculusPackage.h"

/**
* Enum that signifies how an achievement is to be unlocked
*/
enum class EAchievementType {
	Unknown = 0,
	Simple,
	Bitfield,
	Count
};

/**
*	FOnlineAchievementDescOculus - Interface class for accessing the oculus achievement description information
*/
struct FOnlineAchievementDescOculus : FOnlineAchievementDesc
{
	/** The way this achievement is unlocked */
	EAchievementType Type;

	/** The value that needs to be reached for "Count" Type achievements to unlock */
	uint64 Target;

	/** How many fields needs to be set for "Bitfield" Type achievements to unlock */
	uint32 BitfieldLength;

};

/**
*	FOnlineAchievementOculus - Interface class for accessing the oculus achievement information
*/
struct FOnlineAchievementOculus : FOnlineAchievement
{
	FOnlineAchievementOculus(const ovrAchievementProgressHandle& AchievementProgress) :
		bIsUnlocked(ovr_AchievementProgress_GetIsUnlocked(AchievementProgress)),
		Count(ovr_AchievementProgress_GetCount(AchievementProgress)),
		Bitfield(ovr_AchievementProgress_GetBitfield(AchievementProgress))
	{
		FString AchievementName(ovr_AchievementProgress_GetName(AchievementProgress));
		Id = AchievementName;
	}

	FOnlineAchievementOculus(const FOnlineAchievementDescOculus& AchievementDesc) :
		bIsUnlocked(false),
		Count(0)
	{
		Id = AchievementDesc.Title.ToString();
		Progress = 0;
		if (AchievementDesc.Type == EAchievementType::Bitfield)
		{
			Bitfield = TEXT("");
			for (uint32 i = 0; i < AchievementDesc.BitfieldLength; ++i)
			{
				Bitfield.AppendChar('0');
			}
		}
	}

	/** The player's current progress toward a targeted numeric goal */
	uint64 Count;

	/** The player's current progress toward a set of goals that doesn't have to be completed in order */
	FString Bitfield;

	/** Whether or not this achievement was unlocked */
	bool bIsUnlocked;

};

/**
*	IOnlineAchievements - Interface class for achievements
*/
class FOnlineAchievementsOculus : public IOnlineAchievements
{
private:
	
	/** Reference to the owning subsystem */
	FOnlineSubsystemOculus& OculusSubsystem;

	/** Mapping of players to their achievements */
	TMap<FUniqueNetIdOculus, TArray<FOnlineAchievement>> PlayerAchievements;

	/** Cached achievements (not player-specific) */
	TMap<FString, FOnlineAchievementDescOculus> AchievementDescriptions;

	void GetWriteAchievementCountValue(FVariantData VariantData, uint64& OutData) const;
	void GetWriteAchievementBitfieldValue(FVariantData VariantData, FString& OutData, uint32 BitfieldLength) const;
	double CalculatePlayerAchievementProgress(const FOnlineAchievementOculus Achievement);

public:

	/**
	* Constructor
	*
	* @param InSubsystem - A reference to the owning subsystem
	*/
	FOnlineAchievementsOculus(FOnlineSubsystemOculus& InSubsystem);

	/**
	* Default destructor
	*/
	virtual ~FOnlineAchievementsOculus() = default;

	// Begin IOnlineAchievements interface
	virtual void WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate = FOnAchievementsWrittenDelegate()) override;
	virtual void QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
	virtual void QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
	virtual EOnlineCachedResult::Type GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement) override;
	virtual EOnlineCachedResult::Type GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement> & OutAchievements) override;
	virtual EOnlineCachedResult::Type GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc) override;
#if !UE_BUILD_SHIPPING
	virtual bool ResetAchievements(const FUniqueNetId& PlayerId) override;
#endif // !UE_BUILD_SHIPPING
	// End IOnlineAchievements interface
};
