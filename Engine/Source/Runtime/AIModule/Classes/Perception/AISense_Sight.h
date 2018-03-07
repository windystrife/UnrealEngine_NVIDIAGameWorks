// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GenericTeamAgentInterface.h"
#include "Perception/AISense.h"
#include "AISense_Sight.generated.h"

class IAISightTargetInterface;
class UAISense_Sight;
class UAISenseConfig_Sight;

namespace ESightPerceptionEventName
{
	enum Type
	{
		Undefined,
		GainedSight,
		LostSight
	};
}

USTRUCT()
struct AIMODULE_API FAISightEvent
{
	GENERATED_USTRUCT_BODY()

	typedef UAISense_Sight FSenseClass;

	float Age;
	ESightPerceptionEventName::Type EventType;	

	UPROPERTY()
	AActor* SeenActor;

	UPROPERTY()
	AActor* Observer;

	FAISightEvent(){}

	FAISightEvent(AActor* InSeenActor, AActor* InObserver, ESightPerceptionEventName::Type InEventType)
		: Age(0.f), EventType(InEventType), SeenActor(InSeenActor), Observer(InObserver)
	{
	}
};

struct FAISightTarget
{
	typedef uint32 FTargetId;
	static const FTargetId InvalidTargetId;

	TWeakObjectPtr<AActor> Target;
	IAISightTargetInterface* SightTargetInterface;
	FGenericTeamId TeamId;
	FTargetId TargetId;

	FAISightTarget(AActor* InTarget = NULL, FGenericTeamId InTeamId = FGenericTeamId::NoTeam);

	FORCEINLINE FVector GetLocationSimple() const
	{
		return Target.IsValid() ? Target->GetActorLocation() : FVector::ZeroVector;
	}

	FORCEINLINE const AActor* GetTargetActor() const { return Target.Get(); }
};

struct FAISightQuery
{
	FPerceptionListenerID ObserverId;
	FAISightTarget::FTargetId TargetId;

	float Age;
	float Score;
	float Importance;

	FVector LastSeenLocation;

	uint32 bLastResult : 1;

	FAISightQuery(FPerceptionListenerID ListenerId = FPerceptionListenerID::InvalidID(), FAISightTarget::FTargetId Target = FAISightTarget::InvalidTargetId)
		: ObserverId(ListenerId), TargetId(Target), Age(0), Score(0), Importance(0), LastSeenLocation(FAISystem::InvalidLocation), bLastResult(false)
	{
	}

	void RecalcScore()
	{
		Score = Age + Importance;
	}

	void ForgetPreviousResult()
	{
		LastSeenLocation = FAISystem::InvalidLocation;
		bLastResult = false;
	}

	class FSortPredicate
	{
	public:
		FSortPredicate()
		{}

		bool operator()(const FAISightQuery& A, const FAISightQuery& B) const
		{
			return A.Score > B.Score;
		}
	};
};

UCLASS(ClassGroup=AI, config=Game)
class AIMODULE_API UAISense_Sight : public UAISense
{
	GENERATED_UCLASS_BODY()

public:
	struct FDigestedSightProperties
	{
		float PeripheralVisionAngleCos;
		float SightRadiusSq;
		float AutoSuccessRangeSqFromLastSeenLocation;
		float LoseSightRadiusSq;
		uint8 AffiliationFlags;

		FDigestedSightProperties();
		FDigestedSightProperties(const UAISenseConfig_Sight& SenseConfig);
	};	
	
	typedef TMap<FAISightTarget::FTargetId, FAISightTarget> FTargetsContainer;
	FTargetsContainer ObservedTargets;
	TMap<FPerceptionListenerID, FDigestedSightProperties> DigestedProperties;

	TArray<FAISightQuery> SightQueryQueue;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "AI Perception", config)
	int32 MaxTracesPerTick;

	UPROPERTY(EditDefaultsOnly, Category = "AI Perception", config)
	int32 MinQueriesPerTimeSliceCheck;

	UPROPERTY(EditDefaultsOnly, Category = "AI Perception", config)
	double MaxTimeSlicePerTick;

	UPROPERTY(EditDefaultsOnly, Category = "AI Perception", config)
	float HighImportanceQueryDistanceThreshold;

	float HighImportanceDistanceSquare;

	UPROPERTY(EditDefaultsOnly, Category = "AI Perception", config)
	float MaxQueryImportance;

	UPROPERTY(EditDefaultsOnly, Category = "AI Perception", config)
	float SightLimitQueryImportance;

	ECollisionChannel DefaultSightCollisionChannel;

public:

	virtual void PostInitProperties() override;
	
	void RegisterEvent(const FAISightEvent& Event);	

	virtual void RegisterSource(AActor& SourceActors) override;
	virtual void UnregisterSource(AActor& SourceActor) override;
	virtual void CleanseInvalidSources() override;
	
	virtual void OnListenerForgetsActor(const FPerceptionListener& Listener, AActor& ActorToForget) override;
	virtual void OnListenerForgetsAll(const FPerceptionListener& Listener) override;

protected:
	virtual float Update() override;

	virtual bool ShouldAutomaticallySeeTarget(const FDigestedSightProperties& PropDigest, FAISightQuery* SightQuery, FPerceptionListener& Listener, AActor* TargetActor, float& OutStimulusStrength) const;

	void OnNewListenerImpl(const FPerceptionListener& NewListener);
	void OnListenerUpdateImpl(const FPerceptionListener& UpdatedListener);
	void OnListenerRemovedImpl(const FPerceptionListener& UpdatedListener);	
	
	void GenerateQueriesForListener(const FPerceptionListener& Listener, const FDigestedSightProperties& PropertyDigest);

	enum FQueriesOperationPostProcess
	{
		DontSort,
		Sort
	};
	void RemoveAllQueriesByListener(const FPerceptionListener& Listener, FQueriesOperationPostProcess PostProcess);
	void RemoveAllQueriesToTarget(const FAISightTarget::FTargetId& TargetId, FQueriesOperationPostProcess PostProcess);

	/** returns information whether new LoS queries have been added */
	bool RegisterTarget(AActor& TargetActor, FQueriesOperationPostProcess PostProcess);

	FORCEINLINE void SortQueries() { SightQueryQueue.Sort(FAISightQuery::FSortPredicate()); }

	float CalcQueryImportance(const FPerceptionListener& Listener, const FVector& TargetLocation, const float SightRadiusSq) const;
};
