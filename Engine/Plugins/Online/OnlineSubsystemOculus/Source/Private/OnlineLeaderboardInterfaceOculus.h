// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineLeaderboardInterface.h"
#include "OnlineIdentityOculus.h"
#include "OnlineSubsystemOculusPackage.h"

/**
*	IOnlineLeaderboard - Interface class for Leaderboard
*/
class FOnlineLeaderboardOculus : public IOnlineLeaderboards
{
private:
	
	/** Reference to the owning subsystem */
	FOnlineSubsystemOculus& OculusSubsystem;

	bool ReadOculusLeaderboards(bool bOnlyFriends, bool bOnlyLoggedInUser, FOnlineLeaderboardReadRef& ReadObject);
	void OnReadLeaderboardsComplete(ovrMessageHandle Message, bool bIsError, const FOnlineLeaderboardReadRef& ReadObject);

public:

	/**
	* Constructor
	*
	* @param InSubsystem - A reference to the owning subsystem
	*/
	FOnlineLeaderboardOculus(FOnlineSubsystemOculus& InSubsystem);

	/**
	* Default destructor
	*/
	virtual ~FOnlineLeaderboardOculus() = default;

	// Begin IOnlineLeaderboard interface
	virtual bool ReadLeaderboards(const TArray< TSharedRef<const FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundUser(TSharedRef<const FUniqueNetId> Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual void FreeStats(FOnlineLeaderboardRead& ReadObject) override;
	virtual bool WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject) override;
	virtual bool FlushLeaderboards(const FName& SessionName) override;
	virtual bool WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores) override;
	// End IOnlineLeaderboard interface
};
