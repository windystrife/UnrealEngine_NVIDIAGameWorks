// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneLevelVisibilityTemplate.h"
#include "Engine/LevelStreaming.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"


bool GetLevelVisibility(const ULevelStreaming& Level)
{
#if WITH_EDITOR
	if (GIsEditor && !Level.GetWorld()->IsPlayInEditor())
	{
		return Level.bShouldBeVisibleInEditor;
	}
	else
#endif
	{
		return Level.bShouldBeVisible;
	}
}

void SetLevelVisibility(ULevelStreaming& Level, bool bVisible, EFlushLevelStreamingType* FlushStreamingType = nullptr)
{
#if WITH_EDITOR
	if (GIsEditor && !Level.GetWorld()->IsPlayInEditor())
	{
		Level.bShouldBeVisibleInEditor = bVisible;
		Level.GetWorld()->FlushLevelStreaming();

		// Iterate over the level's actors
		ULevel* LoadedLevel = Level.GetLoadedLevel();
		if (LoadedLevel != nullptr)
		{
			TArray<AActor*>& Actors = LoadedLevel->Actors;
			for ( int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex )
			{
				AActor* Actor = Actors[ActorIndex];
				if ( Actor )
				{
					if (Actor->bHiddenEdLevel == bVisible )
					{
						Actor->bHiddenEdLevel = !bVisible;
						if ( bVisible )
						{
							Actor->ReregisterAllComponents();
						}
						else
						{
							Actor->UnregisterAllComponents();
						}
					}
				}
			}
		}
	}
	else
#endif
	{
		Level.bShouldBeVisible = bVisible;

		if (FlushStreamingType && (*FlushStreamingType == EFlushLevelStreamingType::None))
		{
			*FlushStreamingType = EFlushLevelStreamingType::Visibility;
		}

		if (bVisible && !Level.IsLevelLoaded())
		{
			Level.bShouldBeLoaded = true;
			if (FlushStreamingType)
			{
				*FlushStreamingType = EFlushLevelStreamingType::Full;
			}
		}
	}
}

// TODO: This was copied from LevelStreaming.cpp, it should be in a set of shared utilities somewhere.
FString MakeSafeLevelName(const FName& InLevelName, UWorld& World)
{
	// Special case for PIE, the PackageName gets mangled.
	if (!World.StreamingLevelsPrefix.IsEmpty())
	{
		FString PackageName = World.StreamingLevelsPrefix + FPackageName::GetShortName(InLevelName);
		if (!FPackageName::IsShortPackageName(InLevelName))
		{
			PackageName = FPackageName::GetLongPackagePath(InLevelName.ToString()) + TEXT( "/" ) + PackageName;
		}

		return PackageName;
	}

	return InLevelName.ToString();
}

ULevelStreaming* GetStreamingLevel(FString SafeLevelName, UWorld& World)
{
	if (FPackageName::IsShortPackageName(SafeLevelName))
	{
		// Make sure MyMap1 and Map1 names do not resolve to a same streaming level
		SafeLevelName = TEXT("/") + SafeLevelName;
	}

	for (ULevelStreaming* LevelStreaming : World.StreamingLevels)
	{
		if (LevelStreaming && LevelStreaming->GetWorldAssetPackageName().EndsWith(SafeLevelName, ESearchCase::IgnoreCase))
		{
			return LevelStreaming;
		}
	}

	return nullptr;
}

struct FLevelStreamingPreAnimatedToken : IMovieScenePreAnimatedToken
{
	FLevelStreamingPreAnimatedToken(bool bInIsVisible)
		: bVisible(bInIsVisible)
	{
	}

	virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
	{
		ULevelStreaming* LevelStreaming = CastChecked<ULevelStreaming>(&Object);
		SetLevelVisibility(*LevelStreaming, bVisible);
	}

	bool bVisible;
};

struct FLevelStreamingPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		ULevelStreaming* LevelStreaming = CastChecked<ULevelStreaming>(&Object);
		return FLevelStreamingPreAnimatedToken(GetLevelVisibility(*LevelStreaming));
	}
};

struct FLevelStreamingSharedTrackData : IPersistentEvaluationData
{
	bool HasAnythingToDo() const
	{
		return VisibilityMap.Num() != 0;
	}

	void AssignLevelVisibilityOverrides(TArrayView<const FName> LevelNames, ELevelVisibility Visibility, int32 Bias, FMovieSceneEvaluationKey Entity)
	{
		for (FName Name : LevelNames)
		{
			VisibilityMap.FindOrAdd(Name).Add(Entity, Bias, Visibility);
		}
	}
	
	void UnassignLevelVisibilityOverrides(TArrayView<const FName> LevelNames, ELevelVisibility Visibility, int32 Bias, FMovieSceneEvaluationKey Entity)
	{
		for (FName Name : LevelNames)
		{
			FVisibilityData* Data = VisibilityMap.Find(Name);
			if (Data)
			{
				Data->Remove(Entity);
			}
		}
	}

	void ApplyLevelVisibility(IMovieScenePlayer& Player)
	{
		UObject* Context = Player.GetPlaybackContext();
		UWorld* World = Context ? Context->GetWorld() : nullptr;
		if (!World)
		{
			return;
		}

		FLevelStreamingPreAnimatedTokenProducer TokenProducer;

		TArray<FName, TInlineAllocator<8>> LevelsToRestore;

		EFlushLevelStreamingType FlushStreamingType = EFlushLevelStreamingType::None;
		for (auto& Pair : VisibilityMap)
		{
			FName SafeLevelName(*MakeSafeLevelName(Pair.Key, *World));

			ULevelStreaming* Level = GetLevel(SafeLevelName, *World);
			if (!Level)
			{
				continue;
			}

			TOptional<ELevelVisibility> DesiredVisibility = Pair.Value.CalculateVisibility();
			if (!DesiredVisibility.IsSet())
			{
				if (Pair.Value.IsEmpty())
				{
					LevelsToRestore.Add(Pair.Key);
				}

				// Restore the state from before our evaluation
				if (Pair.Value.bPreviousState.IsSet())
				{
					SetLevelVisibility(*Level, Pair.Value.bPreviousState.GetValue(), &FlushStreamingType);
				}
			}
			else
			{
				const bool bShouldBeVisible = DesiredVisibility.GetValue() == ELevelVisibility::Visible;
				if (GetLevelVisibility(*Level) != bShouldBeVisible)
				{
					if (!Pair.Value.bPreviousState.IsSet())
					{
						Pair.Value.bPreviousState = GetLevelVisibility(*Level);
					}

					// Globally save preanimated state
					Player.SavePreAnimatedState(*Level, TMovieSceneAnimTypeID<FLevelStreamingSharedTrackData>(), TokenProducer);

					SetLevelVisibility(*Level, bShouldBeVisible, &FlushStreamingType);
				}
			}
		}

		for (FName Level : LevelsToRestore)
		{
			VisibilityMap.Remove(Level);
		}

		if (FlushStreamingType != EFlushLevelStreamingType::None)
		{
			World->FlushLevelStreaming( FlushStreamingType );
		}
	}

private:

	ULevelStreaming* GetLevel(FName SafeLevelName, UWorld& World)
	{
		if (TWeakObjectPtr<ULevelStreaming>* FoundStreamingLevel = NameToLevelMap.Find(SafeLevelName))
		{
			if (ULevelStreaming* Level = FoundStreamingLevel->Get())
			{
				return Level;
			}

			NameToLevelMap.Remove(SafeLevelName);
		}

		if (SafeLevelName == NAME_None)
		{
			return nullptr;
		}

		ULevelStreaming* Level = GetStreamingLevel(SafeLevelName.ToString(), World);
		if (Level)
		{
			NameToLevelMap.Add(SafeLevelName, Level);
		}

		return Level;
	}
	
	struct FVisibilityData
	{
		TOptional<bool> bPreviousState;

		void Add(FMovieSceneEvaluationKey Entity, int32 Bias, ELevelVisibility Visibility)
		{
			Requests.Add(FVisibilityRequest{Entity, Bias, Visibility });
		}

		void Remove(FMovieSceneEvaluationKey Entity)
		{
			Requests.RemoveAll(
				[=](const FVisibilityRequest& In)
				{
					return In.Entity == Entity;
				}
			);
		}
		
		/** Check whether this visibility data is empty */
		bool IsEmpty() const
		{
			return Requests.Num() == 0;
		}

		/** Returns whether or not this level name should be visible or not */
		TOptional<ELevelVisibility> CalculateVisibility() const
		{
			// Count of things asking for this level to be (in)visible. > 0 signifies visible, < 0 signifies invisible, 0 signifies previous state
			int32 VisibilityRequestCount = 0;

			int32 HighestBias = TNumericLimits<int32>::Lowest();
			for (const FVisibilityRequest& Request : Requests)
			{
				int32 IncAmount = Request.Visibility == ELevelVisibility::Visible ? 1 : -1;
				if (Request.Bias > HighestBias)
				{
					VisibilityRequestCount = IncAmount;
					HighestBias = Request.Bias;
				}
				else if (Request.Bias == HighestBias)
				{
					VisibilityRequestCount += IncAmount;
				}
			}

			if (VisibilityRequestCount == 0)
			{
				return TOptional<ELevelVisibility>();
			}
			else
			{
				return VisibilityRequestCount > 0 ? ELevelVisibility::Visible : ELevelVisibility::Hidden;
			}
		}

	private:
		struct FVisibilityRequest
		{
			/** The entity that made the request */
			FMovieSceneEvaluationKey Entity;
			/** The bias of the entity */
			int32 Bias;
			/** The actual visibility requested */
			ELevelVisibility Visibility;
		};
		TArray<FVisibilityRequest, TInlineAllocator<2>> Requests;
	};
	TMap<FName, FVisibilityData> VisibilityMap;

	TMap<FName, TWeakObjectPtr<ULevelStreaming>> NameToLevelMap;
};

FMovieSceneLevelVisibilitySectionTemplate::FMovieSceneLevelVisibilitySectionTemplate(const UMovieSceneLevelVisibilitySection& Section)
	: Visibility(Section.GetVisibility())
	, LevelNames(Section.GetLevelNames())
{
}

struct FLevelVisibilityExecutionToken : IMovieSceneSharedExecutionToken
{
	FLevelVisibilityExecutionToken()
	{
		// Evaluate at the start of the frame (so that possessables in sub levels work properly)
		Order = -100;
	}

	virtual void Execute(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FLevelStreamingSharedTrackData* TrackData = PersistentData.Find<FLevelStreamingSharedTrackData>(FMovieSceneLevelVisibilitySectionTemplate::GetSharedDataKey());
		if (TrackData)
		{
			TrackData->ApplyLevelVisibility(Player);
		}
	}
};

FSharedPersistentDataKey FMovieSceneLevelVisibilitySectionTemplate::GetSharedDataKey()
{
	static FMovieSceneSharedDataId DataId(FMovieSceneSharedDataId::Allocate());
	return FSharedPersistentDataKey(DataId, FMovieSceneEvaluationOperand());
}

void FMovieSceneLevelVisibilitySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	FMovieSceneSharedDataId SharedKey = GetSharedDataKey().UniqueId;
	if (!ExecutionTokens.FindShared(SharedKey))
	{
		ExecutionTokens.AddShared(SharedKey, FLevelVisibilityExecutionToken());
	}
}

void FMovieSceneLevelVisibilitySectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FLevelStreamingSharedTrackData& TrackData = PersistentData.GetOrAdd<FLevelStreamingSharedTrackData>(GetSharedDataKey());

	const FMovieSceneSubSequenceData* SubData = Player.GetEvaluationTemplate().GetHierarchy().FindSubData(PersistentData.GetSectionKey().SequenceID);

	TrackData.AssignLevelVisibilityOverrides(LevelNames, Visibility, SubData ? SubData->HierarchicalBias : 0, PersistentData.GetSectionKey());
}

void FMovieSceneLevelVisibilitySectionTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FLevelStreamingSharedTrackData* TrackData = PersistentData.Find<FLevelStreamingSharedTrackData>(FMovieSceneLevelVisibilitySectionTemplate::GetSharedDataKey());
	if (TrackData)
	{
		const FMovieSceneSubSequenceData* SubData = Player.GetEvaluationTemplate().GetHierarchy().FindSubData(PersistentData.GetSectionKey().SequenceID);

		TrackData->UnassignLevelVisibilityOverrides(LevelNames, Visibility, SubData ? SubData->HierarchicalBias : 0, PersistentData.GetSectionKey());

		TrackData->ApplyLevelVisibility(Player);
	}
}