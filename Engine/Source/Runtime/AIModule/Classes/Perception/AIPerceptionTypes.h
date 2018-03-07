// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AITypes.h"
#include "GenericTeamAgentInterface.h"
#include "AIPerceptionTypes.generated.h"

class UAIPerceptionComponent;
class UAISense;

//////////////////////////////////////////////////////////////////////////
struct AIMODULE_API FAISenseCounter : FAIBasicCounter<uint8>
{};
typedef FAINamedID<FAISenseCounter> FAISenseID;

//////////////////////////////////////////////////////////////////////////
struct AIMODULE_API FPerceptionListenerCounter : FAIBasicCounter<uint32>
{};
typedef FAIGenericID<FPerceptionListenerCounter> FPerceptionListenerID;

//////////////////////////////////////////////////////////////////////////

UENUM()
enum class EAISenseNotifyType : uint8
{
	/** Continuous update whenever target is perceived. */
	OnEveryPerception,
	/** From "visible" to "not visible" or vice versa. */
	OnPerceptionChange,
};

struct FPerceptionChannelWhitelist
{
	typedef int32 FFlagsContainer;

	FFlagsContainer AcceptedChannelsMask;

	// by default accept all
	FPerceptionChannelWhitelist() : AcceptedChannelsMask()
	{}

	void Clear()
	{
		AcceptedChannelsMask = 0;
	}

	bool IsEmpty() const
	{
		return (AcceptedChannelsMask == 0);
	}

	FORCEINLINE FPerceptionChannelWhitelist& FilterOutChannel(FAISenseID Channel)
	{
		AcceptedChannelsMask &= ~(1 << Channel);
		return *this;
	}

	FORCEINLINE_DEBUGGABLE FPerceptionChannelWhitelist& AcceptChannel(FAISenseID Channel)
	{
		AcceptedChannelsMask |= (1 << Channel);
		return *this;
	}

	FORCEINLINE bool ShouldRespondToChannel(FAISenseID Channel) const
	{
		return (AcceptedChannelsMask & (1 << Channel)) != 0;
	}

	FORCEINLINE FPerceptionChannelWhitelist& MergeFilterIn(const FPerceptionChannelWhitelist& OtherFilter)
	{
		AcceptedChannelsMask |= OtherFilter.AcceptedChannelsMask;
		return *this;
	}

	FORCEINLINE FFlagsContainer GetAcceptedChannelsMask() const 
	{ 
		return AcceptedChannelsMask;
	}

	struct FConstIterator
	{
	private:
		FFlagsContainer RemainingChannelsToTest;
		const FPerceptionChannelWhitelist& Whitelist;
		int32 CurrentIndex;

	public:
		FConstIterator(const FPerceptionChannelWhitelist& InWhitelist)
			: RemainingChannelsToTest((FFlagsContainer)-1), Whitelist(InWhitelist)
			, CurrentIndex(INDEX_NONE)
		{
			FindNextAcceptedChannel();
		}

		FORCEINLINE void FindNextAcceptedChannel()
		{
			const FFlagsContainer& Flags = Whitelist.GetAcceptedChannelsMask();

			while ((RemainingChannelsToTest & Flags) != 0 && ((1 << ++CurrentIndex) | Flags) == 0)
			{
				RemainingChannelsToTest &= ~(1 << CurrentIndex);
			}
		}

		FORCEINLINE explicit operator bool() const
		{
			return (RemainingChannelsToTest & Whitelist.GetAcceptedChannelsMask()) != 0;
		}

		FORCEINLINE int32 operator*() const
		{
			return CurrentIndex;
		}

		FORCEINLINE void operator++()
		{
			// mark "old" index as already used
			RemainingChannelsToTest &= ~(1 << CurrentIndex);
			FindNextAcceptedChannel();
		}
	};
};

USTRUCT(BlueprintType)
struct AIMODULE_API FAIStimulus
{
	GENERATED_USTRUCT_BODY()

	static const float NeverHappenedAge;

	enum FResult
	{
		SensingSucceeded,
		SensingFailed
	};

protected:
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	float Age;

	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	float ExpirationAge;
public:
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	float Strength;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	FVector StimulusLocation;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	FVector ReceiverLocation;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	FName Tag;

	FAISenseID Type;

protected:
	uint32 bWantsToNotifyOnlyOnValueChange : 1;

	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	uint32 bSuccessfullySensed:1; // currently used only for marking failed sight tests

	/** this means the stimulus was originally created with a "time limit" and this time has passed. 
	 *	Expiration also results in calling MarkNoLongerSensed */
	uint32 bExpired:1;	
	
public:
	
	/** this is the recommended constructor. Use others if you know what you're doing. */
	FAIStimulus(const UAISense& Sense, float StimulusStrength, const FVector& InStimulusLocation, const FVector& InReceiverLocation, FResult Result = SensingSucceeded, FName InStimulusTag = NAME_None);

	// default constructor
	FAIStimulus()
		: Age(NeverHappenedAge), ExpirationAge(NeverHappenedAge), Strength(-1.f), StimulusLocation(FAISystem::InvalidLocation)
		, ReceiverLocation(FAISystem::InvalidLocation), Tag(NAME_None), Type(FAISenseID::InvalidID()), bWantsToNotifyOnlyOnValueChange(false)
		, bSuccessfullySensed(false), bExpired(false)
	{}

	FAIStimulus& SetExpirationAge(float InExpirationAge) { ExpirationAge = InExpirationAge; return *this; }
	FAIStimulus& SetStimulusAge(float StimulusAge) { Age = StimulusAge; return *this; }
	FAIStimulus& SetWantsNotifyOnlyOnValueChange(bool InEnable) { bWantsToNotifyOnlyOnValueChange = InEnable; return *this; }
	
	FORCEINLINE float GetAge() const { return Strength > 0 ? Age : NeverHappenedAge; }
	/** @return false when this stimulus is no longer valid, when it is Expired */
	FORCEINLINE bool AgeStimulus(float ConstPerceptionAgingRate) 
	{ 
		Age += ConstPerceptionAgingRate; 
		return Age < ExpirationAge;
	}
	FORCEINLINE bool WasSuccessfullySensed() const { return bSuccessfullySensed; }
	FORCEINLINE bool IsExpired() const { return bExpired; }
	FORCEINLINE void MarkNoLongerSensed() { bSuccessfullySensed = false; }
	FORCEINLINE void MarkExpired() { bExpired = true; MarkNoLongerSensed(); }
	FORCEINLINE bool IsActive() const { return WasSuccessfullySensed() == true && GetAge() < NeverHappenedAge; }
	FORCEINLINE bool WantsToNotifyOnlyOnPerceptionChange() const { return bWantsToNotifyOnlyOnValueChange; }
	FORCEINLINE bool IsValid() const { return Type != FAISenseID::InvalidID(); }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString GetDebugDescription() const;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
};

USTRUCT(BlueprintType)
struct AIMODULE_API FAISenseAffiliationFilter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sense")
	uint32 bDetectEnemies : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sense")
	uint32 bDetectNeutrals : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sense")
	uint32 bDetectFriendlies : 1;
	
	uint8 GetAsFlags() const { return (bDetectEnemies << ETeamAttitude::Hostile) | (bDetectNeutrals << ETeamAttitude::Neutral) | (bDetectFriendlies << ETeamAttitude::Friendly); }
	FORCEINLINE bool ShouldDetectAll() const { return (bDetectEnemies & bDetectNeutrals & bDetectFriendlies); }

	static FORCEINLINE uint8 DetectAllFlags() { return (1 << ETeamAttitude::Hostile) | (1 << ETeamAttitude::Neutral) | (1 << ETeamAttitude::Friendly); }

	static bool ShouldSenseTeam(FGenericTeamId TeamA, FGenericTeamId TeamB, uint8 AffiliationFlags)
	{
		static const uint8 AllFlags = DetectAllFlags();
		return AffiliationFlags == AllFlags || ((1 << FGenericTeamId::GetAttitude(TeamA, TeamB)) & AffiliationFlags);
	}

	static bool ShouldSenseTeam(const IGenericTeamAgentInterface* TeamAgent, const AActor& TargetActor, uint8 AffiliationFlags)
	{
		static const uint8 AllFlags = DetectAllFlags();
		return AffiliationFlags == AllFlags 
			|| (TeamAgent == nullptr ? (AffiliationFlags & (1 << ETeamAttitude::Neutral)) : ((1 << TeamAgent->GetTeamAttitudeTowards(TargetActor)) & AffiliationFlags));
	}
};

/** Should contain only cached information common to all senses. Sense-specific data needs to be stored by senses themselves */
struct AIMODULE_API FPerceptionListener
{
	TWeakObjectPtr<UAIPerceptionComponent> Listener;

	FPerceptionChannelWhitelist Filter;

	FVector CachedLocation;
	FVector CachedDirection;

	FGenericTeamId TeamIdentifier;

private:
	uint32 bHasStimulusToProcess : 1;

	FPerceptionListenerID ListenerID;

	FPerceptionListener();
public:
	FPerceptionListener(UAIPerceptionComponent& InListener);

	void UpdateListenerProperties(UAIPerceptionComponent& Listener);

	bool operator==(const UAIPerceptionComponent* Other) const { return Listener.Get() == Other; }
	bool operator==(const FPerceptionListener& Other) const { return Listener == Other.Listener; }

	void CacheLocation();

	void RegisterStimulus(AActor* Source, const FAIStimulus& Stimulus);

	FORCEINLINE bool HasAnyNewStimuli() const { return bHasStimulusToProcess; }
	void ProcessStimuli();

	FORCEINLINE bool HasSense(FAISenseID SenseID) const { return Filter.ShouldRespondToChannel(SenseID); }

	// used to remove "dead" listeners
	static const FPerceptionListener NullListener;

	FORCEINLINE FPerceptionListenerID GetListenerID() const { return ListenerID; }

	FName GetBodyActorName() const;
	uint32 GetBodyActorUniqueID() const;

	/** Returns pointer to the actor representing this listener's physical body */
	const AActor* GetBodyActor() const;

	const IGenericTeamAgentInterface* GetTeamAgent() const;

private:
	friend class UAIPerceptionSystem;
	FORCEINLINE void SetListenerID(FPerceptionListenerID InListenerID) { ListenerID = InListenerID; }
	FORCEINLINE void MarkForStimulusProcessing() { bHasStimulusToProcess = true; }
};

struct AIMODULE_API FPerceptionStimuliSource
{
	TWeakObjectPtr<AActor> SourceActor;
	FPerceptionChannelWhitelist RelevantSenses;
};

namespace AIPerception
{
	typedef TMap<FPerceptionListenerID, FPerceptionListener> FListenerMap;
}
