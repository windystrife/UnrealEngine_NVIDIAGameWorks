// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneFolder.h"
#include "Evaluation/MovieSceneEvaluationCustomVersion.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "SequencerObjectVersion.h"

/* UMovieScene interface
 *****************************************************************************/

UMovieScene::UMovieScene(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SelectionRange(FFloatRange::Empty())
	, PlaybackRange(FFloatRange::Empty())
#if WITH_EDITORONLY_DATA
	, bPlaybackRangeLocked(false)
#endif
	, bForceFixedFrameIntervalPlayback(false)
	, FixedFrameInterval(0.0f)
	, InTime_DEPRECATED(FLT_MAX)
	, OutTime_DEPRECATED(-FLT_MAX)
	, StartTime_DEPRECATED(FLT_MAX)
	, EndTime_DEPRECATED(-FLT_MAX)
{
#if WITH_EDITORONLY_DATA
	EditorData.WorkingRange = EditorData.ViewRange = TRange<float>::Empty();
#endif
}

void UMovieScene::Serialize( FArchive& Ar )
{
	Ar.UsingCustomVersion(FMovieSceneEvaluationCustomVersion::GUID);
	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);

#if WITH_EDITOR

	// Perform optimizations for cooking
	if (Ar.IsCooking())
	{
		// @todo: Optimize master tracks?

		// Optimize object bindings
		OptimizeObjectArray(Spawnables);
		OptimizeObjectArray(Possessables);
	}

#endif // WITH_EDITOR

	Super::Serialize(Ar);
}

#if WITH_EDITOR

template<typename T>
void UMovieScene::OptimizeObjectArray(TArray<T>& ObjectArray)
{
	for (int32 ObjectIndex = ObjectArray.Num() - 1; ObjectIndex >= 0; --ObjectIndex)
	{
		FGuid ObjectGuid = ObjectArray[ObjectIndex].GetGuid();

		// Find the binding relating to this object, and optimize its tracks
		// @todo: ObjectBindings mapped by ID to avoid linear search
		for (int32 BindingIndex = 0; BindingIndex < ObjectBindings.Num(); ++BindingIndex)
		{
			FMovieSceneBinding& Binding = ObjectBindings[BindingIndex];
			if (Binding.GetObjectGuid() != ObjectGuid)
			{
				continue;
			}
			
			bool bShouldRemoveObject = false;

			// Optimize any tracks
			Binding.PerformCookOptimization(bShouldRemoveObject);

			// Remove the object if it's completely redundant
			if (bShouldRemoveObject)
			{
				ObjectBindings.RemoveAtSwap(BindingIndex, 1, false);
				ObjectArray.RemoveAtSwap(ObjectIndex, 1, false);
			}

			// Process next object
			break;
		}
	}
}

// @todo sequencer: Some of these methods should only be used by tools, and should probably move out of MovieScene!
FGuid UMovieScene::AddSpawnable( const FString& Name, UObject& ObjectTemplate )
{
	Modify();

	FMovieSceneSpawnable NewSpawnable( Name, ObjectTemplate );
	Spawnables.Add( NewSpawnable );

	// Add a new binding so that tracks can be added to it
	new (ObjectBindings) FMovieSceneBinding( NewSpawnable.GetGuid(), NewSpawnable.GetName() );

	return NewSpawnable.GetGuid();
}


bool UMovieScene::RemoveSpawnable( const FGuid& Guid )
{
	bool bAnythingRemoved = false;
	if( ensure( Guid.IsValid() ) )
	{
		for( auto SpawnableIter( Spawnables.CreateIterator() ); SpawnableIter; ++SpawnableIter )
		{
			auto& CurSpawnable = *SpawnableIter;
			if( CurSpawnable.GetGuid() == Guid )
			{
				Modify();
				RemoveBinding( Guid );

				Spawnables.RemoveAt( SpawnableIter.GetIndex() );
				
				bAnythingRemoved = true;
				break;
			}
		}
	}

	return bAnythingRemoved;
}

FMovieSceneSpawnable* UMovieScene::FindSpawnable( const TFunctionRef<bool(FMovieSceneSpawnable&)>& InPredicate )
{
	return Spawnables.FindByPredicate(InPredicate);
}

#endif //WITH_EDITOR


FMovieSceneSpawnable& UMovieScene::GetSpawnable(int32 Index)
{
	return Spawnables[Index];
}

int32 UMovieScene::GetSpawnableCount() const
{
	return Spawnables.Num();
}

FMovieSceneSpawnable* UMovieScene::FindSpawnable( const FGuid& Guid )
{
	return Spawnables.FindByPredicate([&](FMovieSceneSpawnable& Spawnable) {
		return Spawnable.GetGuid() == Guid;
	});
}


FGuid UMovieScene::AddPossessable( const FString& Name, UClass* Class )
{
	Modify();

	FMovieScenePossessable NewPossessable( Name, Class );
	Possessables.Add( NewPossessable );

	// Add a new binding so that tracks can be added to it
	new (ObjectBindings) FMovieSceneBinding( NewPossessable.GetGuid(), NewPossessable.GetName() );

	return NewPossessable.GetGuid();
}


bool UMovieScene::RemovePossessable( const FGuid& PossessableGuid )
{
	bool bAnythingRemoved = false;

	for( auto PossesableIter( Possessables.CreateIterator() ); PossesableIter; ++PossesableIter )
	{
		auto& CurPossesable = *PossesableIter;

		if( CurPossesable.GetGuid() == PossessableGuid )
		{	
			Modify();

			// Remove the parent-child link for a parent spawnable/child possessable if necessary
			if (CurPossesable.GetParent().IsValid())
			{
				FMovieSceneSpawnable* ParentSpawnable = FindSpawnable(CurPossesable.GetParent());
				if (ParentSpawnable)
				{
					ParentSpawnable->RemoveChildPossessable(PossessableGuid);
				}
			}

			// Found it!
			Possessables.RemoveAt( PossesableIter.GetIndex() );

			RemoveBinding( PossessableGuid );

			bAnythingRemoved = true;
			break;
		}
	}

	return bAnythingRemoved;
}


bool UMovieScene::ReplacePossessable( const FGuid& OldGuid, const FMovieScenePossessable& InNewPosessable )
{
	bool bAnythingReplaced = false;

	for (auto& Possessable : Possessables)
	{
		if (Possessable.GetGuid() == OldGuid)
		{	
			Modify();

			// Found it!
			if (InNewPosessable.GetPossessedObjectClass() == nullptr)
			{
				// @todo: delete this when
				// bool ReplacePossessable(const FGuid& OldGuid, const FGuid& NewGuid, const FString& Name)
				// is removed
				Possessable.SetGuid(InNewPosessable.GetGuid());
				Possessable.SetName(InNewPosessable.GetName());
			}
			else
			{
				Possessable = InNewPosessable;
			}

			ReplaceBinding( OldGuid, InNewPosessable.GetGuid(), InNewPosessable.GetName() );
			bAnythingReplaced = true;

			break;
		}
	}

	return bAnythingReplaced;
}


FMovieScenePossessable* UMovieScene::FindPossessable( const FGuid& Guid )
{
	for (auto& Possessable : Possessables)
	{
		if (Possessable.GetGuid() == Guid)
		{
			return &Possessable;
		}
	}

	return nullptr;
}

FMovieScenePossessable* UMovieScene::FindPossessable( const TFunctionRef<bool(FMovieScenePossessable&)>& InPredicate )
{
	return Possessables.FindByPredicate(InPredicate);
}

int32 UMovieScene::GetPossessableCount() const
{
	return Possessables.Num();
}


FMovieScenePossessable& UMovieScene::GetPossessable( const int32 Index )
{
	return Possessables[Index];
}


FText UMovieScene::GetObjectDisplayName(const FGuid& ObjectId)
{
#if WITH_EDITORONLY_DATA
	FText* Result = ObjectsToDisplayNames.Find(ObjectId.ToString());

	if (Result && !Result->IsEmpty())
	{
		return *Result;
	}

	FMovieSceneSpawnable* Spawnable = FindSpawnable(ObjectId);

	if (Spawnable != nullptr)
	{
		return FText::FromString(Spawnable->GetName());
	}

	FMovieScenePossessable* Possessable = FindPossessable(ObjectId);

	if (Possessable != nullptr)
	{
		return FText::FromString(Possessable->GetName());
	}
#endif
	return FText::GetEmpty();
}



#if WITH_EDITORONLY_DATA
void UMovieScene::SetObjectDisplayName(const FGuid& ObjectId, const FText& DisplayName)
{
	if (DisplayName.IsEmpty())
	{
		ObjectsToDisplayNames.Remove(ObjectId.ToString());
	}
	else
	{
		ObjectsToDisplayNames.Add(ObjectId.ToString(), DisplayName);
	}
}


TArray<UMovieSceneFolder*>&  UMovieScene::GetRootFolders()
{
	return RootFolders;
}
#endif


void UMovieScene::SetPlaybackRange(float Start, float End, bool bAlwaysMarkDirty)
{
	if (ensure(End >= Start))
	{
		const auto NewRange = TRange<float>(Start, TRangeBound<float>::Inclusive(End));

		if (PlaybackRange == NewRange)
		{
			return;
		}

		if (bAlwaysMarkDirty)
		{
			Modify();
		}

		PlaybackRange = NewRange;

#if WITH_EDITORONLY_DATA
		// Initialize the working and view range with a little bit more space
		const float OutputViewSize = PlaybackRange.GetUpperBoundValue() - PlaybackRange.GetLowerBoundValue();
		const float OutputChange = OutputViewSize * 0.1f;

		TRange<float> ExpandedPlaybackRange = TRange<float>(PlaybackRange.GetLowerBoundValue() - OutputChange, PlaybackRange.GetUpperBoundValue() + OutputChange);

		if (EditorData.WorkingRange.IsEmpty())
		{
			EditorData.WorkingRange = ExpandedPlaybackRange;
		}

		if (EditorData.ViewRange.IsEmpty())
		{
			EditorData.ViewRange = ExpandedPlaybackRange;
		}
#endif
	}
}

void UMovieScene::SetWorkingRange(float Start, float End)
{
#if WITH_EDITORONLY_DATA
	EditorData.WorkingRange = TRange<float>(Start, End);
#endif
}

void UMovieScene::SetViewRange(float Start, float End)
{
#if WITH_EDITORONLY_DATA
	EditorData.ViewRange = TRange<float>(Start, End);
#endif
}

#if WITH_EDITORONLY_DATA
bool UMovieScene::IsPlaybackRangeLocked() const
{
	return bPlaybackRangeLocked;
}

void UMovieScene::SetPlaybackRangeLocked(bool bLocked)
{
	bPlaybackRangeLocked = bLocked;
}
#endif

bool UMovieScene::GetForceFixedFrameIntervalPlayback() const
{
	return bForceFixedFrameIntervalPlayback;
}


void UMovieScene::SetForceFixedFrameIntervalPlayback( bool bInForceFixedFrameIntervalPlayback )
{
	bForceFixedFrameIntervalPlayback = bInForceFixedFrameIntervalPlayback;
}


void UMovieScene::SetFixedFrameInterval( float InFixedFrameInterval )
{
	FixedFrameInterval = InFixedFrameInterval;
}


const float UMovieScene::FixedFrameIntervalEpsilon = .0001f;

float UMovieScene::CalculateFixedFrameTime( float Time, float FixedFrameInterval )
{
	return ( FMath::RoundToInt( Time / FixedFrameInterval ) ) * FixedFrameInterval + FixedFrameIntervalEpsilon;
}


TArray<UMovieSceneSection*> UMovieScene::GetAllSections() const
{
	TArray<UMovieSceneSection*> OutSections;

	// Add all master type sections 
	for( int32 TrackIndex = 0; TrackIndex < MasterTracks.Num(); ++TrackIndex )
	{
		OutSections.Append( MasterTracks[TrackIndex]->GetAllSections() );
	}

	// Add all object binding sections
	for (const auto& Binding : ObjectBindings)
	{
		for (const auto& Track : Binding.GetTracks())
		{
			OutSections.Append(Track->GetAllSections());
		}
	}

	return OutSections;
}


UMovieSceneTrack* UMovieScene::FindTrack(TSubclassOf<UMovieSceneTrack> TrackClass, const FGuid& ObjectGuid, const FName& TrackName) const
{
	check( ObjectGuid.IsValid() );
	
	for (const auto& Binding : ObjectBindings)
	{
		if (Binding.GetObjectGuid() != ObjectGuid) 
		{
			continue;
		}

		for (const auto& Track : Binding.GetTracks())
		{
			if (Track->GetClass() == TrackClass)
			{
				if (TrackName == NAME_None || Track->GetTrackName() == TrackName)
				{
					return Track;
				}
			}
		}
	}
	
	return nullptr;
}


UMovieSceneTrack* UMovieScene::AddTrack( TSubclassOf<UMovieSceneTrack> TrackClass, const FGuid& ObjectGuid )
{
	UMovieSceneTrack* CreatedType = nullptr;

	check( ObjectGuid.IsValid() )

	for (auto& Binding : ObjectBindings)
	{
		if( Binding.GetObjectGuid() == ObjectGuid ) 
		{
			Modify();

			CreatedType = NewObject<UMovieSceneTrack>(this, TrackClass, NAME_None, RF_Transactional);
			check(CreatedType);
			
			Binding.AddTrack( *CreatedType );
		}
	}

	return CreatedType;
}

bool UMovieScene::AddGivenTrack(UMovieSceneTrack* InTrack, const FGuid& ObjectGuid)
{
	check(ObjectGuid.IsValid());

	Modify();
	for (auto& Binding : ObjectBindings)
	{
		if (Binding.GetObjectGuid() == ObjectGuid)
		{
			InTrack->Rename(nullptr, this);
			check(InTrack);
			Binding.AddTrack(*InTrack);
			return true;
		}
	}

	return false;
}

bool UMovieScene::RemoveTrack(UMovieSceneTrack& Track)
{
	Modify();

	bool bAnythingRemoved = false;

	for (auto& Binding : ObjectBindings)
	{
		if (Binding.RemoveTrack(Track))
		{
			bAnythingRemoved = true;

			// The track was removed from the current binding, stop
			// searching now as it cannot exist in any other binding
			break;
		}
	}

	return bAnythingRemoved;
}

bool UMovieScene::FindTrackBinding(const UMovieSceneTrack& InTrack, FGuid& OutGuid) const
{
	for (auto& Binding : ObjectBindings)
	{
		for(auto& Track : Binding.GetTracks())
		{
			if(Track == &InTrack)
			{
				OutGuid = Binding.GetObjectGuid();
				return true;
			}
		}
	}

	return false;
}

UMovieSceneTrack* UMovieScene::FindMasterTrack( TSubclassOf<UMovieSceneTrack> TrackClass ) const
{
	UMovieSceneTrack* FoundTrack = nullptr;

	for (const auto Track : MasterTracks)
	{
		if( Track->GetClass() == TrackClass )
		{
			FoundTrack = Track;
			break;
		}
	}

	return FoundTrack;
}


UMovieSceneTrack* UMovieScene::AddMasterTrack( TSubclassOf<UMovieSceneTrack> TrackClass )
{
	Modify();

	UMovieSceneTrack* CreatedType = NewObject<UMovieSceneTrack>(this, TrackClass, NAME_None, RF_Transactional);
	MasterTracks.Add( CreatedType );
	
	return CreatedType;
}


bool UMovieScene::AddGivenMasterTrack(UMovieSceneTrack* InTrack)
{
	if (!MasterTracks.Contains(InTrack))
	{
		Modify();
		InTrack->Rename(nullptr, this);
		MasterTracks.Add(InTrack);
		return true;
	}
	return false;
}


bool UMovieScene::RemoveMasterTrack(UMovieSceneTrack& Track) 
{
	Modify();

	return (MasterTracks.RemoveSingle(&Track) != 0);
}


bool UMovieScene::IsAMasterTrack(const UMovieSceneTrack& Track) const
{
	for ( const UMovieSceneTrack* MasterTrack : MasterTracks)
	{
		if (&Track == MasterTrack)
		{
			return true;
		}
	}

	return false;
}


UMovieSceneTrack* UMovieScene::AddCameraCutTrack( TSubclassOf<UMovieSceneTrack> TrackClass )
{
	if( !CameraCutTrack )
	{
		Modify();
		CameraCutTrack = NewObject<UMovieSceneTrack>(this, TrackClass, FName("Camera Cuts"), RF_Transactional);
	}

	return CameraCutTrack;
}


UMovieSceneTrack* UMovieScene::GetCameraCutTrack()
{
	return CameraCutTrack;
}


void UMovieScene::RemoveCameraCutTrack()
{
	if( CameraCutTrack )
	{
		Modify();
		CameraCutTrack = nullptr;
	}
}

void UMovieScene::SetCameraCutTrack(UMovieSceneTrack* InTrack)
{
	Modify();
	InTrack->Rename(nullptr, this);
	CameraCutTrack = InTrack;
}


void UMovieScene::UpgradeTimeRanges()
{
	// Legacy upgrade for playback ranges:
	// We used to optionally store a start/end and in/out time for sequences.
	// The only 2 uses were UWidgetAnimations and ULevelSequences.
	// Widget animations used to always calculate their length automatically, from the section boundaries, and always started at 0
	// Level sequences defaulted to having a fixed play range.
	// We now expose the playback range more visibly, but we need to upgrade the old data.

	if (InTime_DEPRECATED != FLT_MAX && OutTime_DEPRECATED != -FLT_MAX)
	{
		// Finite range already defined in old data
		PlaybackRange = TRange<float>(InTime_DEPRECATED, TRangeBound<float>::Inclusive(OutTime_DEPRECATED));
	}
	else if (PlaybackRange.IsEmpty())
	{
		// No range specified, so automatically calculate one by determining the maximum upper bound of the sequence
		// In this instance (UMG), playback always started at 0
		float MaxBound = 0.f;

		for (const auto& Track : MasterTracks)
		{
			auto Range = Track->GetSectionBoundaries();
			if (Range.HasUpperBound())
			{
				MaxBound = FMath::Max(MaxBound, Range.GetUpperBoundValue());
			}
		}

		for (const auto& Binding : ObjectBindings)
		{
			auto Range = Binding.GetTimeRange();
			if (Range.HasUpperBound())
			{
				MaxBound = FMath::Max(MaxBound, Range.GetUpperBoundValue());
			}
		}

		PlaybackRange = TRange<float>(0.f, TRangeBound<float>::Inclusive(MaxBound));
	}
	else if (PlaybackRange.GetUpperBound().IsExclusive())
	{
		// playback ranges are now always inclusive
		PlaybackRange = TRange<float>(PlaybackRange.GetLowerBound(), TRangeBound<float>::Inclusive(PlaybackRange.GetUpperBoundValue()));
	}

	// PlaybackRange must always be defined to a finite range
	if (!PlaybackRange.HasLowerBound() || !PlaybackRange.HasUpperBound() || PlaybackRange.IsDegenerate())
	{
		PlaybackRange = TRange<float>(0.f, 0.f);
	}

#if WITH_EDITORONLY_DATA
	// Legacy upgrade for working range
	if (StartTime_DEPRECATED != FLT_MAX && EndTime_DEPRECATED != -FLT_MAX)
	{
		EditorData.WorkingRange = TRange<float>(StartTime_DEPRECATED, EndTime_DEPRECATED);
	}
	else if (EditorData.WorkingRange.IsEmpty())
	{
		EditorData.WorkingRange = PlaybackRange;
	}

	if (EditorData.ViewRange.IsEmpty())
	{
		EditorData.ViewRange = PlaybackRange;
	}
#endif
}

/* UObject interface
 *****************************************************************************/

void UMovieScene::PostLoad()
{
	// Remove any null tracks
	for( int32 TrackIndex = 0; TrackIndex < MasterTracks.Num(); )
	{
		if (MasterTracks[TrackIndex] == nullptr)
		{
			MasterTracks.RemoveAt(TrackIndex);
		}
		else
		{
			++TrackIndex;
		}
	}

	UpgradeTimeRanges();

	for (FMovieSceneSpawnable& Spawnable : Spawnables)
	{
		if (UObject* Template = Spawnable.GetObjectTemplate())
		{
			// Spawnables are no longer marked archetype
			Template->ClearFlags(RF_ArchetypeObject);
			
			FMovieSceneSpawnable::MarkSpawnableTemplate(*Template);
		}
	}
	Super::PostLoad();
}


void UMovieScene::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

#if WITH_EDITORONLY_DATA
	// compress meta data mappings prior to saving
	for (auto It = ObjectsToDisplayNames.CreateIterator(); It; ++It)
	{
		FGuid ObjectId;

		if (!FGuid::Parse(It.Key(), ObjectId) || ((FindPossessable(ObjectId) == nullptr) && (FindSpawnable(ObjectId) == nullptr)))
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = ObjectsToLabels.CreateIterator(); It; ++It)
	{
		FGuid ObjectId;

		if (!FGuid::Parse(It.Key(), ObjectId) || ((FindPossessable(ObjectId) == nullptr) && (FindSpawnable(ObjectId) == nullptr)))
		{
			It.RemoveCurrent();
		}
	}
#endif
}


/* UMovieScene implementation
 *****************************************************************************/

void UMovieScene::RemoveBinding(const FGuid& Guid)
{
	// update each type
	for (int32 BindingIndex = 0; BindingIndex < ObjectBindings.Num(); ++BindingIndex)
	{
		if (ObjectBindings[BindingIndex].GetObjectGuid() == Guid)
		{
			ObjectBindings.RemoveAt(BindingIndex);
			break;
		}
	}
}


void UMovieScene::ReplaceBinding(const FGuid& OldGuid, const FGuid& NewGuid, const FString& Name)
{
	for (auto& Binding : ObjectBindings)
	{
		if (Binding.GetObjectGuid() == OldGuid)
		{
			Binding.SetObjectGuid(NewGuid);
			Binding.SetName(Name);

			// Changing a binding guid invalidates any tracks contained within the binding
			// Make sure they are written into the transaction buffer by calling modify
			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				Track->Modify();
			}
			break;
		}
	}
}
