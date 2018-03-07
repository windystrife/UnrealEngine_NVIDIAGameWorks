// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Interpolation.cpp: Code for supporting interpolation of properties in-game.
=============================================================================*/

#include "Interpolation.h"
#include "Misc/MessageDialog.h"
#include "Misc/App.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "HitProxies.h"
#include "GameFramework/Controller.h"
#include "Components/PrimitiveComponent.h"
#include "Components/LightComponent.h"
#include "Components/DecalComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "GameFramework/WorldSettings.h"
#include "Components/BillboardComponent.h"
#include "Particles/Emitter.h"
#include "Animation/Skeleton.h"
#include "Engine/Texture2D.h"
#include "Engine/Light.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/LocalPlayer.h"
#include "Sound/SoundBase.h"
#include "ContentStreaming.h"
#include "TimerManager.h"
#include "Engine/LevelScriptActor.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/MatineeActorCameraAnim.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpTrackInst.h"
#include "Matinee/InterpTrackInstProperty.h"
#include "Matinee/InterpTrack.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackInstMove.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackInstDirector.h"
#include "Matinee/InterpTrackVisibility.h"
#include "Matinee/InterpTrackInstVisibility.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackInstAnimControl.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackInstEvent.h"
#include "Matinee/InterpTrackToggle.h"
#include "Matinee/InterpTrackInstToggle.h"
#include "Matinee/InterpTrackFade.h"
#include "Matinee/InterpTrackInstFade.h"
#include "Matinee/InterpTrackSlomo.h"
#include "Matinee/InterpTrackInstSlomo.h"
#include "Matinee/InterpTrackVectorBase.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackInstSound.h"
#include "Matinee/InterpTrackLinearColorBase.h"
#include "Matinee/InterpTrackFloatProp.h"
#include "Matinee/InterpTrackInstFloatProp.h"
#include "Matinee/InterpTrackVectorProp.h"
#include "Matinee/InterpTrackInstVectorProp.h"
#include "Matinee/InterpTrackBoolProp.h"
#include "Matinee/InterpTrackInstBoolProp.h"
#include "Matinee/InterpTrackColorProp.h"
#include "Matinee/InterpTrackInstColorProp.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpTrackInstLinearColorProp.h"
#include "Matinee/InterpTrackAudioMaster.h"
#include "Matinee/InterpTrackInstAudioMaster.h"
#include "Matinee/InterpTrackColorScale.h"
#include "Matinee/InterpTrackInstColorScale.h"
#include "Matinee/InterpTrackFloatParticleParam.h"
#include "Matinee/InterpTrackInstFloatParticleParam.h"
#include "Matinee/InterpTrackFloatMaterialParam.h"
#include "Matinee/InterpTrackInstFloatMaterialParam.h"
#include "Matinee/InterpTrackVectorMaterialParam.h"
#include "Matinee/InterpTrackInstVectorMaterialParam.h"
#include "Matinee/InterpTrackParticleReplay.h"
#include "Matinee/InterpTrackInstParticleReplay.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpGroupDirector.h"
#include "Matinee/InterpGroupInstDirector.h"
#include "Matinee/InterpGroupCamera.h"
#include "Matinee/InterpGroupInstCamera.h"
#include "Matinee/InterpFilter.h"
#include "Matinee/InterpFilter_Classes.h"
#include "Matinee/InterpFilter_Custom.h"
#include "Materials/MaterialInstanceActor.h"
#include "Matinee/MatineeAnimInterface.h"
#include "Animation/SkeletalMeshActor.h"
#include "AudioDevice.h"
#include "InterpolationHitProxy.h"
#include "AnimationUtils.h"
#include "MatineeUtils.h"
#include "Matinee/InterpTrackFloatAnimBPParam.h"
#include "Matinee/InterpTrackInstFloatAnimBPParam.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Particles/ParticleSystemReplay.h"
#include "GameFramework/GameState.h"

#if WITH_EDITOR
#include "Sound/SoundCue.h"
#include "Editor.h"
#endif

#if PLATFORM_HTML5
#include <emscripten/emscripten.h>
#endif


DEFINE_LOG_CATEGORY(LogMatinee);

// Priority with which to display sounds triggered by Matinee sound tracks.
#define SUBTITLE_PRIORITY_MATINEE	10000

IMPLEMENT_HIT_PROXY(HInterpEdInputInterface,HHitProxy);
IMPLEMENT_HIT_PROXY(HInterpTrackKeypointProxy,HHitProxy);
IMPLEMENT_HIT_PROXY(HInterpTrackSubGroupKeypointProxy,HHitProxy);
IMPLEMENT_HIT_PROXY(HInterpTrackKeyHandleProxy,HHitProxy);

// Matinee related Interfaces

/** Number of seconds to look ahead for camera cuts (for notifying the streaming system). */
float GCameraCutLookAhead = 10.0f;

/** Get Pawn from the given Actor **/
APawn* GetPawn(AActor *Actor)
{
	if (Actor)
	{
		APawn * Pawn = Cast<APawn>(Actor);
		if (!Pawn)
		{
			if ( Cast<AController>(Actor) )
			{
				Pawn = Cast<AController>(Actor)->GetPawn();
			}
		}

		return Pawn;
	}

	return NULL;
}

/*-----------------------------------------------------------------------------
	Macros for making arrays-of-structs type tracks easier
-----------------------------------------------------------------------------*/

#define STRUCTTRACK_GETNUMKEYFRAMES( TrackClass, KeyArray ) \
int32 TrackClass::GetNumKeyframes() const \
{ \
	return KeyArray.Num(); \
}

#define STRUCTTRACK_GETTIMERANGE( TrackClass, KeyArray, TimeVar ) \
void TrackClass::GetTimeRange(float& StartTime, float& EndTime) const \
{ \
	if(KeyArray.Num() == 0) \
	{ \
		StartTime = 0.f; \
		EndTime = 0.f; \
	} \
	else \
	{ \
		StartTime = KeyArray[0].TimeVar; \
		EndTime = KeyArray[ KeyArray.Num()-1 ].TimeVar; \
	} \
}

// The default implementation returns the time of the last keyframe.
#define STRUCTTRACK_GETTRACKENDTIME( TrackClass, KeyArray, TimeVar ) \
float TrackClass::GetTrackEndTime() const \
{ \
	return KeyArray.Num() ? KeyArray[KeyArray.Num() - 1].TimeVar : 0.0f; \
} 

#define STRUCTTRACK_GETKEYFRAMETIME( TrackClass, KeyArray, TimeVar ) \
float TrackClass::GetKeyframeTime(int32 KeyIndex) const \
{ \
	if( KeyIndex < 0 || KeyIndex >= KeyArray.Num() ) \
	{ \
		return 0.f; \
	} \
	return KeyArray[KeyIndex].TimeVar; \
}

#define STRUCTTRACK_GETKEYFRAMEINDEX( TrackClass, KeyArray, TimeVar ) \
int32 TrackClass::GetKeyframeIndex(float KeyTime) const \
{ \
	int32 RetIndex = INDEX_NONE; \
	if( KeyArray.Num() > 0 ) \
	{ \
		float CurTime = KeyArray[0].TimeVar; \
		/* Loop through every keyframe until we find a keyframe with the passed in time. */ \
		/* Stop searching once all the keyframes left to search have larger times than the passed in time. */ \
		for( int32 KeyIndex = 0; KeyIndex < KeyArray.Num() && CurTime <= KeyTime; ++KeyIndex ) \
		{ \
			if( KeyTime == KeyArray[KeyIndex].TimeVar ) \
			{ \
				RetIndex = KeyIndex; \
				break; \
			} \
			CurTime = KeyArray[KeyIndex].TimeVar; \
		} \
	} \
	return RetIndex; \
}

#define STRUCTTRACK_SETKEYFRAMETIME( TrackClass, KeyArray, TimeVar, KeyType ) \
int32 TrackClass::SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder) \
{ \
	if( KeyIndex < 0 || KeyIndex >= KeyArray.Num() ) \
	{ \
		return KeyIndex; \
	} \
	if(bUpdateOrder) \
	{ \
		/* First, remove cut from track */ \
		KeyType MoveKey = KeyArray[KeyIndex]; \
		KeyArray.RemoveAt(KeyIndex); \
		/* Set its time to the new one. */ \
		MoveKey.TimeVar = NewKeyTime; \
		/* Find correct new position and insert. */ \
		int32 i=0; \
		for( i=0; i<KeyArray.Num() && KeyArray[i].TimeVar < NewKeyTime; i++) {}; \
		KeyArray.InsertZeroed(i); \
		KeyArray[i] = MoveKey; \
		return i; \
	} \
	else \
	{ \
		KeyArray[KeyIndex].TimeVar = NewKeyTime; \
		return KeyIndex; \
	} \
}

#define STRUCTTRACK_REMOVEKEYFRAME( TrackClass, KeyArray ) \
void TrackClass::RemoveKeyframe(int32 KeyIndex) \
{ \
	if( KeyIndex < 0 || KeyIndex >= KeyArray.Num() ) \
	{ \
		return; \
	} \
	KeyArray.RemoveAt(KeyIndex); \
}

#define STRUCTTRACK_DUPLICATEKEYFRAME( TrackClass, KeyArray, TimeVar, KeyType ) \
int32 TrackClass::DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack) \
{ \
	if( KeyIndex < 0 || KeyIndex >= KeyArray.Num() ) \
	{ \
		return INDEX_NONE; \
	} \
	/* Make sure the destination track is specified. */ \
	TrackClass* DestTrack = this; \
	if ( ToTrack ) \
	{ \
		DestTrack = CastChecked< TrackClass >( ToTrack ); \
	} \
	KeyType NewKey = KeyArray[KeyIndex]; \
	NewKey.TimeVar = NewKeyTime; \
	/* Find the correct index to insert this key. */ \
	int32 i=0; for( i=0; i<DestTrack->KeyArray.Num() && DestTrack->KeyArray[i].TimeVar < NewKeyTime; i++) {}; \
	DestTrack->KeyArray.InsertZeroed(i); \
	DestTrack->KeyArray[i] = NewKey; \
	return i; \
}

#define STRUCTTRACK_GETCLOSESTSNAPPOSITION( TrackClass, KeyArray, TimeVar ) \
bool TrackClass::GetClosestSnapPosition(float InPosition, TArray<int32> &IgnoreKeys, float& OutPosition) \
{ \
	if(KeyArray.Num() == 0) \
	{ \
		return false; \
	} \
	bool bFoundSnap = false; \
	float ClosestSnap = 0.f; \
	float ClosestDist = BIG_NUMBER; \
	for(int32 i=0; i<KeyArray.Num(); i++) \
	{ \
		if(!IgnoreKeys.Contains(i)) \
		{ \
			float Dist = FMath::Abs( KeyArray[i].TimeVar - InPosition ); \
			if(Dist < ClosestDist) \
			{ \
				ClosestSnap = KeyArray[i].TimeVar; \
				ClosestDist = Dist; \
				bFoundSnap = true; \
			} \
		} \
	} \
	OutPosition = ClosestSnap; \
	return bFoundSnap; \
}

/*-----------------------------------------------------------------------------
	InterpTools
-----------------------------------------------------------------------------*/

namespace InterpTools
{
	/**
	 * Removes any extraneous text that Matinee includes when storing 
	 * the property name, such as the owning struct or component.
	 *
	 * @param	PropertyName	The property name to prune. 
	 */
	FName PruneInterpPropertyName( const FName& PropertyName )
	{
		FString PropertyString = PropertyName.ToString();

		// Check to see if there is a period in the name, which is the case 
		// for structs and components that own interp variables. In these  
		// cases, we want to cut off the preceeding text up and the period.
		int32 PeriodPosition = PropertyString.Find(TEXT("."));

		if(PeriodPosition != INDEX_NONE)
		{
			// We found a period; Only capture the text after the 
			// period, which represents the actual property name.
			PropertyString = PropertyString.Mid( PeriodPosition + 1 );
		}

		return FName(*PropertyString);
	}
}

/*-----------------------------------------------------------------------------
	AMatineeActor
-----------------------------------------------------------------------------*/

uint8 AMatineeActor::IgnoreActorSelectionCount = 0;

void AMatineeActor::PushIgnoreActorSelection()
{
	++IgnoreActorSelectionCount;
}

void AMatineeActor::PopIgnoreActorSelection()
{
	check(IgnoreActorSelection());
	--IgnoreActorSelectionCount;
}

bool AMatineeActor::IgnoreActorSelection()
{
	return IgnoreActorSelectionCount > 0;
}

AMatineeActor::AMatineeActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SceneManagerObject;
			FName ID_Matinee;
			FText NAME_Matinee;
			FConstructorStatics()
				: SceneManagerObject(TEXT("/Engine/EditorResources/SceneManager"))
				, ID_Matinee(TEXT("Matinee"))
				, NAME_Matinee(NSLOCTEXT("SpriteCategory", "Matinee", "Matinee"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.SceneManagerObject.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Matinee;
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Matinee;
		SpriteComponent->SetupAttachment(RootComponent);
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif // WITH_EDITORONLY_DATA

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bPlayOnLevelLoad = false;
#if WITH_EDITORONLY_DATA
	bIsBeingEdited = false;
#endif // WITH_EDITORONLY_DATA
	bAlwaysRelevant = true;
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	NetPriority = 2.7f;
	NetUpdateFrequency = 1.0f;
	InterpPosition = -1.0f;
	PlayRate = 1.0f;
	ClientSidePositionErrorTolerance = 0.1f;
	ReplicationForceIsPlaying = 0;
}

void AMatineeActor::PostLoad()
{
	SetReplicates(!bClientSideOnly);
	Super::PostLoad();
}

FName AMatineeActor::GetFunctionNameForEvent(FName EventName,bool bUseCustomEventName)
{
	FName EventFuncName;
	if( bUseCustomEventName )
	{
		EventFuncName = EventName;
	}
	else
	{
		EventFuncName = *(FString::Printf(TEXT("%s_%s"), *MatineeControllerName.ToString(), *EventName.ToString()));	
	}

	return EventFuncName;
}

void AMatineeActor::NotifyEventTriggered(FName EventName, float EventTime, bool bUseCustomEventName)
{
	ULevel* Level = GetLevel();
	ALevelScriptActor* LevelScriptActor = Level->LevelScriptActor;
	if(LevelScriptActor != NULL)
	{
		FName EventFuncName = GetFunctionNameForEvent(EventName,bUseCustomEventName);
		UFunction* EventFunction = LevelScriptActor->FindFunction(EventFuncName);
		if(EventFunction != NULL)
		{
			if(EventFunction->NumParms == 0)
			{
				LevelScriptActor->ProcessEvent(EventFunction, NULL);
			}
			else
			{
				UE_LOG(LogMatinee, Log, TEXT("NotifyEventTriggered: Function '%s' does not have zero parameters."), *EventFuncName.ToString());
			}
		}
		else
		{
			UE_LOG(LogMatinee, Log, TEXT("NotifyEventTriggered: Unable to find function '%s'"), *EventFuncName.ToString());
		}
	}

#if !UE_BUILD_SHIPPING
	if (EventName == NAME_PerformanceCapture)
	{
		FString PackageName = GetOutermost()->GetName();
		
		FString MapName;
		FString FolderName;
		PackageName.Split(TEXT("/"), &FolderName, &MapName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		GEngine->PerformanceCapture(GetWorld(), MapName, GetName(), EventTime);
	}
#endif	// UE_BUILD_SHIPPING
}

void AMatineeActor::Play()
{
	if( !bIsPlaying || bPaused )
	{
		// Disable the radio filter if we are just beginning to play
		DisableRadioFilterIfNeeded();
	}

	if( GetWorld()->IsGameWorld() && !bIsPlaying && !bPaused )
	{
		// The matinee was not previously playing.  Initialize the group instances now.
		InitInterp();
	}

	// Jump to specific location if desired.
	if(bForceStartPos && !bIsPlaying)
	{
		UpdateInterp(ForceStartPosition, false, true);
	}
	// See if we should rewind to beginning...
	else if(bRewindOnPlay && (!bIsPlaying || bRewindIfAlreadyPlaying))
	{
		if(bNoResetOnRewind)
		{
			//ResetMovementInitialTransforms();
		}

		// 'Jump' interpolation to the start (ie. will not fire events between current position and start).
		UpdateInterp(0.f, false, true);
	}

	if (!bIsPlaying)
	{
		if (OnPlay.IsBound())
		{
			OnPlay.Broadcast();
		}
	}

	bReversePlayback = false;
	bIsPlaying = true;
	bPaused = false;
	SetActorTickEnabled(true);
}

void AMatineeActor::Reverse()
{
	if( GetWorld()->IsGameWorld() && !bIsPlaying && !bPaused )
	{
		// The matinee was not previously playing.  Initialize the group instances now.
		InitInterp();
	}

	if (!bIsPlaying)
	{
		if (OnPlay.IsBound())
		{
			OnPlay.Broadcast();
		}
	}

	bReversePlayback = true;
	bIsPlaying = true;
	bPaused = false;
	SetActorTickEnabled(true);
}

void AMatineeActor::Stop()
{
	// Re-enable the radio filter
	EnableRadioFilter();

	if (bIsPlaying)
	{
		if (OnStop.IsBound())
		{
			OnStop.Broadcast();
		}
	}

	bIsPlaying = false;
	bPaused = false;
	SetActorTickEnabled(false);

	if (GetWorld()->IsGameWorld())
	{
		// We should only terminate the interp in the game.  The editor handles this from inside the matinee editor
		TermInterp();
	}
}

void AMatineeActor::Pause()
{
	if (bIsPlaying)
	{
		if (!bPaused)
		{
			if (OnPause.IsBound())
			{
				OnPause.Broadcast();
			}
		}
		else if (OnPlay.IsBound())
		{
			OnPlay.Broadcast();
		}

		EnableRadioFilter();
		bPaused = !bPaused;
		SetActorTickEnabled(!bPaused);
	}
}

void AMatineeActor::ChangePlaybackDirection()
{
	if (!bIsPlaying)
	{
		if (OnPlay.IsBound())
		{
			OnPlay.Broadcast();
		}
	}

	bReversePlayback = !bReversePlayback;
	bIsPlaying = true;
	bPaused = false;
	SetActorTickEnabled(true);
}

void AMatineeActor::SetLoopingState(bool bNewLooping)
{
	bLooping = bNewLooping;
}

#if WITH_EDITOR
template<class MapValue>
void ReplaceMapKeys(const TMap<UObject*,UObject*>& ReplacementMap, TMap<TWeakObjectPtr<AActor>, MapValue>& MapToReplaceIn)
{
	for(auto Iter = ReplacementMap.CreateConstIterator(); Iter; ++Iter)
	{
		if( AActor* OldActor = Cast<AActor>(Iter.Key()) )
		{
			MapValue PairValue;
			if(MapToReplaceIn.RemoveAndCopyValue(OldActor, PairValue))
			{
				if(AActor* NewActor = Cast<AActor>(Iter.Value()))
				{
					MapToReplaceIn.Add(NewActor, PairValue);
				}
			}
		}
	}
}

void AMatineeActor::OnObjectsReplaced(const TMap<UObject*,UObject*>& ReplacementMap)
{
	ReplaceMapKeys(ReplacementMap, SavedActorTransforms);
	ReplaceMapKeys(ReplacementMap, SavedActorVisibilities);
}
#endif //WITH_EDITOR

void AMatineeActor::EnableGroupByName(FString GroupName, bool bEnable)
{
	UInterpGroupInst* FirstGroupInst = FindFirstGroupInstByName(GroupName);

	if (FirstGroupInst)
	{
		UInterpGroup* Group = FirstGroupInst->Group;
		for ( UInterpTrack* Track : Group->InterpTracks)
		{
			Track->EnableTrack(bEnable, true);
		}
	}
}

void AMatineeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(TimerHandle_CheckPriorityRefresh);
	}

	Super::EndPlay(EndPlayReason);
}

void AMatineeActor::SetPosition(float NewPosition,bool bJump)
{
	// if we aren't currently active, temporarily activate to change the position
	bool bTempActivate = !bIsPlaying;
	if (bTempActivate)
	{
		InitInterp();
	}

	UpdateInterp(NewPosition, false, bJump);

	if (bTempActivate)
	{
		TermInterp();
	}

	UpdateReplicatedData(false);
}

void AMatineeActor::AddPlayerToDirectorTracks(class APlayerController* PC)
{
	// if we aren't initialized (i.e. not currently running) then do nothing
	if (PC != NULL && MatineeData != NULL && GroupInst.Num() > 0 && GetWorld()->IsGameWorld() )
	{
		for (int32 i = 0; i < MatineeData->InterpGroups.Num(); i++)
		{
			UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(MatineeData->InterpGroups[i]);
			if (DirGroup != NULL)
			{
				bool bAlreadyHasGroup = false;
				for (int32 j = 0; j < GroupInst.Num(); j++)
				{
					if (GroupInst[j]->Group == DirGroup && GroupInst[j]->GroupActor == PC)
					{
						bAlreadyHasGroup = true;
						break;
					}
				}
				if (!bAlreadyHasGroup)
				{
					// Make sure this sequence is compatible with the player
					if( IsMatineeCompatibleWithPlayer( PC ) )
					{
						// create a new instance with this player
						UInterpGroupInstDirector* NewGroupInstDir = NewObject<UInterpGroupInstDirector>(this, NAME_None, RF_Transactional);
						GroupInst.Add(NewGroupInstDir);

						// and initialize the instance
						NewGroupInstDir->InitGroupInst(DirGroup, PC);
					}
				}
			}
		}
	}
}

void AMatineeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if ( bIsPlaying && MatineeData != NULL )
	{
		StepInterp(DeltaTime, false);
	}
}


void AMatineeActor::GetAffectedActors( TArray<AActor*>& OutActors, bool bMovementTrackOnly )
{
	for(int32 i=0; i<GroupInst.Num(); i++)
	{
		if(GroupInst[i]->GetGroupActor())
		{
			UInterpGroup* Group = GroupInst[i]->Group;
			TArray<UInterpTrack*> MovementTracks;
			Group->FindTracksByClass(UInterpTrackMove::StaticClass(), MovementTracks);

			// If we either dont just want movement tracks, or we do and we have a movement track, add to array.
			if(!bMovementTrackOnly || MovementTracks.Num() > 0)
			{	
				OutActors.AddUnique( GroupInst[i]->GetGroupActor() );
			}
		}
	}
}

void AMatineeActor::GetControlledActors( TArray<AActor*>& OutActors )
{
	OutActors.Empty();
	for(int32 GroupIdx=0; GroupIdx<GroupActorInfos.Num(); GroupIdx++)
	{
		FInterpGroupActorInfo& Info = GroupActorInfos[GroupIdx];
		for(int32 ActorIdx=0; ActorIdx<Info.Actors.Num(); ActorIdx++)
		{
			AActor* Actor = Info.Actors[ActorIdx];
			if(Actor != NULL)
			{
				OutActors.AddUnique(Actor);
			}
		}
	}
}


void AMatineeActor::UpdateStreamingForCameraCuts(float CurrentTime, bool bPreview)
{
	// Only supports forward-playing non-looping matinees.
	if ( GetWorld()->IsGameWorld() && bIsPlaying && !bReversePlayback && !bLooping )
	{
		for ( int32 CameraCutIndex=0; CameraCutIndex < CameraCuts.Num(); CameraCutIndex++ )
		{
			FCameraCutInfo& CutInfo = CameraCuts[CameraCutIndex];
			float TimeDifference = CutInfo.TimeStamp - CurrentTime;
			if ( TimeDifference > 0.0f && TimeDifference < GCameraCutLookAhead )
			{
				IStreamingManager::Get().AddViewSlaveLocation( CutInfo.Location );
			}
			else if ( TimeDifference >= GCameraCutLookAhead )
			{
				break;
			}
		}
	}
}

static int32 MaxDepthBuckets = 10;

void AMatineeActor::UpdateInterp( float NewPosition, bool bPreview, bool bJump )
{
	if( MatineeData )
	{
		NewPosition = FMath::Clamp(NewPosition, 0.f, MatineeData->InterpLength);

		// Initialize the "buckets" to sort group insts by attachment depth. 
		TArray< TArray<UInterpGroupInst*> > SortedGroupInsts;
		SortedGroupInsts.AddZeroed(MaxDepthBuckets);

		for(int32 GroupIndex=0; GroupIndex<GroupInst.Num(); GroupIndex++)
		{
			UInterpGroupInst* GrInst = GroupInst[GroupIndex];

			check(GrInst->Group);

			// Determine the depth of group inst by the 
			// number of parents in the attachment chain.
			int32 ActorParentCount = 0;

			// A group inst may not have actor. In 
			// that case, the depth will be zero. 
			if( GrInst->GetGroupActor() )
			{
				AActor* CurrentParent = GrInst->GetGroupActor()->GetAttachParentActor();

				// To figure out the update order, just walk up the 
				// attachment tree to calculate the depth of this group. 
				while( CurrentParent != NULL )
				{
					ActorParentCount++;

					CurrentParent = CurrentParent->GetAttachParentActor();
				}
			}

			if( !SortedGroupInsts.IsValidIndex(ActorParentCount) )
			{
				// Increase the maximum bucket size to prevent resizing on next update. 
				MaxDepthBuckets = ActorParentCount + 1;

				// Add enough buckets to make the actor's parent depth valid. 
				const int32 BucketsToAdd = MaxDepthBuckets - SortedGroupInsts.Num();
				SortedGroupInsts.AddZeroed(BucketsToAdd);

				// Hopefully, somebody will notice this alert. If so, increase MaxDepthBuckets to the logged max. 
				UE_LOG(LogMatinee, Log, TEXT("WARNING: Reached maximum group actor depth in AMatineeActor::UpdateInterp()! Increase max to %d."), MaxDepthBuckets );
			}

			// Add the group inst into the corresponding bucket for its depth level.
			SortedGroupInsts[ActorParentCount].Add(GrInst);
		}

		// Update each group in order by the group inst's attachment depth. 
		for( int32 AttachDepthIndex = 0; AttachDepthIndex < SortedGroupInsts.Num(); ++AttachDepthIndex )
		{
			TArray<UInterpGroupInst*>& Groups = SortedGroupInsts[AttachDepthIndex];

			for( int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex )
			{
				Groups[GroupIndex]->Group->UpdateGroup( NewPosition, Groups[GroupIndex], bPreview, bJump );

				const bool bhasBeenTerminated = (GroupInst.Num() == 0);
#if WITH_EDITORONLY_DATA
				if (bhasBeenTerminated && !bIsBeingEdited)
#else
				if (bhasBeenTerminated)
#endif
				{
					UE_LOG(LogMatinee, Log, TEXT("WARNING: A matinee was stopped while updating group '%s'; the next groups will not be updated."), *Groups[GroupIndex]->Group->GetFullGroupName(true));
					InterpPosition = NewPosition;
					return;
				}
			}
		}


		InterpPosition = NewPosition;
	}
}

void AMatineeActor::InitInterp()
{
	// if groupinst still exists, that means it hasn't been properly terminated, so terminate it
	// this happens in client, when persistent level hasn't been unloaded, but restarted by server
	// it's not terminated, but reinitialized
	if (GroupInst.Num() != 0)
	{
		// group did not terminate, and it's trying to re-init, so will terminate here
		TermInterp();
	}

	if( MatineeData )
	{
		// Register myself as the active matinee if one is not active.
		if (GEngine->ActiveMatinee.IsValid() == false)
		{
			GEngine->ActiveMatinee = this;
		}

		TMap<FName, FInterpGroupActorInfo*> InterpGroupToActorInfoMap;
	
		// Build a mapping of group names to actor infos for fast lookup later
		for( int32 InfoIndex = 0; InfoIndex < GroupActorInfos.Num(); ++InfoIndex )
		{
			InterpGroupToActorInfoMap.Add( GroupActorInfos[InfoIndex].ObjectName, &GroupActorInfos[InfoIndex] );
		}

		// Cache whether or not we want to enable extreme content within this sequence
		bShouldShowGore = true;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if( GetWorld() != NULL && GetWorld()->GetWorldSettings() != NULL )
		{
			AGameState const* const GameState = GetWorld()->GetGameState<AGameState>();
			if( GameState != NULL )
			{
				bShouldShowGore = GameState->ShouldShowGore();
			}
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		for(int32 GroupIndex=0; GroupIndex<MatineeData->InterpGroups.Num(); GroupIndex++)
		{
			UInterpGroup* Group = MatineeData->InterpGroups[GroupIndex];

			// If this is a DirectorGroup, we find a player controller and pass it in instead of looking to a variable.
			UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(Group);
			if(DirGroup)
			{
				// Need to do a game specific check here because there are no player controllers in the editor and matinee expects a group instance to be initialized.
				if( GetWorld()->IsGameWorld() )
				{
					// iterate through the controller list
					for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
					{
						APlayerController *PC = Iterator->Get();

						// If it's a player and this sequence is compatible with the player...
						if (IsMatineeCompatibleWithPlayer( PC ) )
						{
							// create a new instance with this player
							UInterpGroupInstDirector* NewGroupInstDir = NewObject<UInterpGroupInstDirector>(this, NAME_None, RF_Transactional);
							GroupInst.Add(NewGroupInstDir);

							// and initialize the instance
							NewGroupInstDir->InitGroupInst(DirGroup, PC);
						}
					}

					// if you don't have player controller, it will create inst track later
				}
				else
				{
					// In the editor always create a director group instance with a NULL group actor since there are no player controllers.
					UInterpGroupInstDirector* NewGroupInstDir = NewObject<UInterpGroupInstDirector>(this, NAME_None, RF_Transactional);
					GroupInst.Add(NewGroupInstDir);

					// and initialize the instance
					NewGroupInstDir->InitGroupInst(DirGroup, NULL);
				}
			}
			else
			{
				// Folder groups don't get variables
				if( !Group->bIsFolder )
				{
					FInterpGroupActorInfo* GroupInfo = InterpGroupToActorInfoMap.FindRef( Group->GetFName() );
				
					if( GroupInfo && GroupInfo->Actors.Num() > 0 )
					{
						for( int32 ActorIndex = 0; ActorIndex < GroupInfo->Actors.Num(); ++ActorIndex )
						{
							AActor* Actor = GroupInfo->Actors[ ActorIndex ];

							UInterpGroupInst* NewGroupInst = NewObject<UInterpGroupInst>(this, NAME_None, RF_Transactional);
							GroupInst.Add(NewGroupInst);

							NewGroupInst->InitGroupInst(Group, Actor);
						}
					}
					else
					{
						// we need to create groupinst when actor does not exist. 
						// Create new InterpGroupInst
						UInterpGroupInst* NewGroupInst = NewObject<UInterpGroupInst>(this, NAME_None, RF_Transactional);
						GroupInst.Add(NewGroupInst);
						// Initialise group instance, saving ref to actor it works on.
						NewGroupInst->InitGroupInst(Group, NULL);
					}
				}
			}
		}

		// set matinee actor when initialize it, otherwise, we'll have random tick order
		for(int32 i=0; i<GroupInst.Num(); i++)
		{
			if(GroupInst[i]->GroupActor)
			{
				GroupInst[i]->GroupActor->AddControllingMatineeActor( *this );
			}
		}

		EnableCinematicMode(true);
	}

	// Scan the matinee data for camera cuts and set up the CameraCut array.
	SetupCameraCuts();

	UpdateReplicatedData(true);
}


void AMatineeActor::TermInterp()
{
	// Destroy each group instance.
	for(int32 i=0; i<GroupInst.Num(); i++)
	{
		if(GroupInst[i]->GroupActor)
		{
			GroupInst[i]->GroupActor->RemoveControllingMatineeActor( *this );
		}

		GroupInst[i]->TermGroupInst(true);

	}
	GroupInst.Empty();

	// Unregister myself as the active matinee if one is not active.
	if (GEngine->ActiveMatinee.Get() == this)
	{
		GEngine->ActiveMatinee.Reset();
	}

	// disable cinematic mode
	EnableCinematicMode(false);
}

void AMatineeActor::UpdateInterpForParentMovementTracks( float Time, UInterpGroupInst* ViewGroupInst )
{
	AActor* Actor = ViewGroupInst->GetGroupActor();
	if( Actor )
	{
		AActor* Parent = Actor->GetAttachParentActor();

		UInterpGroupInst* ParentInst = FindGroupInst(Parent);
		if(ParentInst)
		{
			UInterpTrackInst* ParentTrackInst = nullptr;
			for(UInterpTrackInst* Inst : ParentInst->TrackInst)
			{
				if(Inst->GetGroupActor() == Parent)
				{
					ParentTrackInst = Inst;
					break;
				}
			}

			if(ParentTrackInst && ParentInst->Group)
			{
				TArray<UInterpTrack*> FoundTracks;
				ParentInst->Group->FindTracksByClass(UInterpTrackMove::StaticClass(), FoundTracks);
				if (FoundTracks.Num() > 0)
				{
					//Just use the first one, multiple move tracks wouldnt work well anyway
					UInterpTrackMove* MoveTrack = CastChecked<UInterpTrackMove>(FoundTracks[0]);
					MoveTrack->ConditionalUpdateTrack(Time, ParentTrackInst, true);
				}
			}
		}
	}
}

void AMatineeActor::SetupCameraCuts()
{
	if( MatineeData )
	{
		UInterpGroupDirector* DirGroup = MatineeData->FindDirectorGroup();
		UInterpTrackDirector* DirTrack = DirGroup ? DirGroup->GetDirectorTrack() : NULL;
		if ( DirTrack && DirTrack->CutTrack.Num() > 0 )
		{
			CameraCuts.Reserve( DirTrack->CutTrack.Num() );

			float OldInterpPosition = InterpPosition;

			// Find the starting camera location for each cut.
			for( int32 KeyFrameIndex=0; KeyFrameIndex < DirTrack->CutTrack.Num(); KeyFrameIndex++)
			{
				const FDirectorTrackCut& Cut = DirTrack->CutTrack[KeyFrameIndex];
				int32 GroupIndex = MatineeData->FindGroupByName( Cut.TargetCamGroup );
				UInterpGroupInst* ViewGroupInst = (GroupIndex != INDEX_NONE) ? FindFirstGroupInstByName( Cut.TargetCamGroup.ToString() ) : NULL;
				if ( GroupIndex != INDEX_NONE && ViewGroupInst )
				{
					// Find a valid move track for this cut.
					UInterpGroup* Group = MatineeData->InterpGroups[GroupIndex];
					for(int32 TrackIndex=0; TrackIndex < Group->InterpTracks.Num(); TrackIndex++)
					{
						UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Group->InterpTracks[TrackIndex]);
						if ( MoveTrack && !MoveTrack->IsDisabled() && TrackIndex < ViewGroupInst->TrackInst.Num() )
						{
							FCameraCutInfo CameraCut;
							FRotator CameraRotation;

							UInterpTrackInst* TrackInst = ViewGroupInst->TrackInst[ TrackIndex ];
							UpdateInterpForParentMovementTracks(Cut.Time + .01f, ViewGroupInst);
							bool bSucceeded = MoveTrack->GetLocationAtTime( TrackInst, Cut.Time+0.01f, CameraCut.Location, CameraRotation );
							UpdateInterpForParentMovementTracks(OldInterpPosition, ViewGroupInst);

							// Only add locations that aren't (0,0,0)
							if ( bSucceeded && CameraCut.Location.IsNearlyZero() == false )
							{
								//UE_LOG(LogTemp, Display, TEXT("Cut %d   %s    %f    %f %f %f"), CameraCuts.Num(), *Cut.TargetCamGroup.ToString(), Cut.Time, CameraCut.Location.X, CameraCut.Location.Y, CameraCut.Location.Z);
								CameraCut.TimeStamp = Cut.Time;
								CameraCuts.Add( CameraCut );
								break;
							}
						}
					}
				}
			}
		}
	}
}


bool AMatineeActor::IsMatineeCompatibleWithPlayer( APlayerController* InPC ) const
{
	bool bBindPlayerToMatinee = true;

	// If the 'preferred split screen' value is non-zero, we'll only bind this Matinee to
	// player controllers that are associated with the specified game player index
	if( PreferredSplitScreenNum != 0 )
	{
		bBindPlayerToMatinee = false;
		ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>( InPC->Player );
		if( LocalPlayer != NULL )
		{
			const int32 GamePlayerIndex = GEngine->GetGamePlayers(GetWorld()).Find( LocalPlayer );
			if( ( GamePlayerIndex + 1 ) == PreferredSplitScreenNum )
			{
				bBindPlayerToMatinee = true;
			}
		}
	}

	if (bReplicates == false && InPC->IsLocalController() == false)
	{
		bBindPlayerToMatinee = false;
	}

	return bBindPlayerToMatinee;
}

void AMatineeActor::StepInterp( float DeltaTime, bool bPreview )
{
	if( !bIsPlaying || bPaused || !MatineeData )
		return;
	
	bool bSkipUpdate = false;
	
	if (bClientSideOnly && bSkipUpdateIfNotVisible)
	{
		bSkipUpdate = true;
		
		for( int32 InfoIndex = 0; InfoIndex < GroupActorInfos.Num(); ++InfoIndex )
		{
			const FInterpGroupActorInfo& Info = GroupActorInfos[InfoIndex];
			for (int32 ActorIndex = 0; ActorIndex < Info.Actors.Num() && bSkipUpdate; ++ActorIndex)
			{
				AActor* Actor = Info.Actors[ ActorIndex ];
				if (Actor != NULL && Actor->GetLastRenderTime() > Actor->GetWorld()->TimeSeconds - 1.f)
				{
					bSkipUpdate = false;
				}
			}
		}
	}

	if (!bSkipUpdate)
	{
		float NewPosition;
		bool bLooped = 0;
		bool bShouldStopPlaying = false;

		// Playing forwards
		if(!bReversePlayback)
		{
			NewPosition = InterpPosition + (DeltaTime * PlayRate);

			if(NewPosition > MatineeData->InterpLength)
			{
				// If looping, play to end, jump to start, and set target to somewhere near the beginning.
				if(bLooping && MatineeData->InterpLength > 0.0f )
				{
					UpdateInterp(MatineeData->InterpLength, bPreview);

					if(bNoResetOnRewind)
					{
						//ResetMovementInitialTransforms();
					}

					UpdateInterp(0.f, bPreview, true);

					while(NewPosition > MatineeData->InterpLength)
					{
						NewPosition -= MatineeData->InterpLength;
					}

					bLooped = true;
				}
				// If not looping, snap to end and stop playing.
				else
				{
					NewPosition = MatineeData->InterpLength;
					bShouldStopPlaying = true;
				}
			}
		}
		// Playing backwards.
		else
		{
			NewPosition = InterpPosition - (DeltaTime* PlayRate);

			if(NewPosition < 0.f)
			{
				// If looping, play to start, jump to end, and set target to somewhere near the end.
				if(bLooping)
				{
					UpdateInterp(0.f, bPreview);
					UpdateInterp(MatineeData->InterpLength, bPreview, true);

					while(NewPosition < 0.f)
					{
						NewPosition += MatineeData->InterpLength;
					}

					bLooped = true;
				}
				// If not looping, snap to start and stop playing.
				else
				{
					NewPosition = 0.f;
					bShouldStopPlaying = true;
				}
			}
		}

		UpdateInterp(NewPosition, bPreview);

		// We reached the end of the sequence (or the beginning, if playing backwards), so stop playback
		// now.  Note that we do that *after* calling UpdateInterp so that tracks that test bIsPlaying
		// will complete the full sequence before we stop them.
		if( bShouldStopPlaying )
		{
			Stop();

			// Name should reflect the value UEdGraphSchema_K2::PN_MatineeFinished.  Accessing it directly would cause editor dependencies though
			NotifyEventTriggered( FName(TEXT("Finished")), NewPosition );

			// Events can turn us back on
			if (bIsPlaying)
			{			
				// Client has also stopped, but we are playing again, force replication of a flag to indicate we are playing.
				ReplicationForceIsPlaying++;
				UpdateReplicatedData(true);
			}
		}

		UpdateStreamingForCameraCuts(NewPosition, bPreview);

		// if we looped back to the start, notify the replicated actor so it can refresh any clients
		if (bLooped)
		{
			UpdateReplicatedData(false);
		}
		else
		{
			// otherwise, just update position without notifying it
			// so that clients that join the game during movement will get the correct updated position
			// but nothing will get replicated to other clients that should be simulating the movement
			InterpPosition = NewPosition;
		}
	}


}

void AMatineeActor::DisableRadioFilterIfNeeded()
{
	FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();
	if( AudioDevice )
	{
		AudioDevice->EnableRadioEffect( !bDisableRadioFilter );
	}
}


void AMatineeActor::EnableCinematicMode(bool bEnable)
{
	// iterate through the controller list and set cinematic mode if necessary
	bool bNeedsCinematicMode = bDisableMovementInput || bDisableLookAtInput || bHidePlayer || bHideHud;

	if (bNeedsCinematicMode)
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController *PC = Iterator->Get();
			if (bReplicates || PC->IsLocalController())
			{
				PC->SetCinematicMode(bEnable, bHidePlayer, bHideHud, bDisableMovementInput, bDisableLookAtInput);
			}
		}
	}
}

void AMatineeActor::EnableRadioFilter()
{
	FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();
	if( AudioDevice )
	{
		AudioDevice->EnableRadioEffect( true );
	}
}


UInterpGroupInst* AMatineeActor::FindGroupInst(const AActor* Actor) const
{
	if(!Actor || Actor->IsPendingKill() )
	{
		return NULL;
	}

	for(int32 i=0; i<GroupInst.Num(); i++)
	{
		if( GroupInst[i]->HasActor(Actor) )
		{
			return GroupInst[i];
		}
	}

	return NULL;
}

#if WITH_EDITOR
void AMatineeActor::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty( PropertyChangedEvent );
	if( PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AMatineeActor, MatineeData) )
	{
		// Create new entries
		if( MatineeData )
		{
			// Matinee data is about to change so the group actor infos are no longer valid.
			EnsureActorGroupConsistency();
		}
	}
	ValidateActorGroups();
}

bool AMatineeActor::CanEditChange( const UProperty* Property ) const
{
	bool bIsEditable = Super::CanEditChange( Property );
	if( bIsEditable && Property != NULL )
	{
		if ( Property->GetFName() == TEXT( "MatineeData" ) )
		{
			bIsEditable = !bIsBeingEdited;
		}
	}

	return bIsEditable;
}
#endif	// WITH_EDITOR

#if WITH_EDITOR

void AMatineeActor::ValidateActorGroups()
{
	for( int32 GroupIndex = 0; GroupIndex < GroupActorInfos.Num(); ++GroupIndex )
	{
		TArray<class AActor *> & ActorList = GroupActorInfos[GroupIndex].Actors;
		for (int32 I=0; I<ActorList.Num(); ++I)
		{
			if (IsValidActorToAdd(ActorList[I]) != AMatineeActor::ActorAddOk)
			{
				ActorList[I] = NULL;
			}
		}
	}
}

AMatineeActor::EActorAddWarningType AMatineeActor::IsValidActorToAdd(const AActor* ActorToCheck) const
{
	ULevel * CurrentLevel = GetLevel();
	if ( !ActorToCheck )
	{
		// it's okay to have NULL actor
		return ActorAddOk;
	}
	if (ActorToCheck->GetLevel() != CurrentLevel)
	{
		return ActorAddWarningSameLevel;
	}
	else if (ActorToCheck->IsRootComponentStatic())
	{
		return ActorAddWarningStatic;
	}
	else if (ActorToCheck->IsA(AMatineeActor::StaticClass()))
	{
		return ActorAddWarningGroup;
	}

	return ActorAddOk;
}
#endif	//WITH_EDITOR

#if	WITH_EDITOR
void AMatineeActor::EnsureActorGroupConsistency()
{
	// this gets called first time it initialize
	// we'll need to verify GroupName has been converted to ObjectName
	// we can't do this in PostLoad since we don't know what Actor
	// Ensure consistency between interp data and actor to group mappings
	// this gets called when initialized, or interpData changed or Redo/Undo
	// do not call this all the time, whenever Groups changed, make sure it applies to the GroupActorInfos
	if( MatineeData )
	{
		const TArray<UInterpGroup*>& InterpGroups = MatineeData->InterpGroups;

		// Search from the back so we can remove elements from the back and not invalidate other elements.
		for( int32 InfoIndex =  GroupActorInfos.Num()-1; InfoIndex >=0; --InfoIndex )
		{
			FInterpGroupActorInfo& Info = GroupActorInfos[ InfoIndex ];

			bool bInvalidGroupInfo = true;
			for( int32 GroupIndex = 0; GroupIndex < InterpGroups.Num(); ++GroupIndex )
			{
				UInterpGroup* InterpGroup = InterpGroups[ GroupIndex ];

				// Make sure Group's name matches GroupActorInfos name
				if( InterpGroup->GetFName() == Info.ObjectName )
				{
					bInvalidGroupInfo = false;
				}
			}

			// we do not see this group, delete it
			if( bInvalidGroupInfo )
			{
				UE_LOG(LogMatinee, Warning, TEXT("GROUP DELETE: No group exists for (%s)."), *Info.ObjectName.ToString());
				// The group is no longer found so remove its actor info now.
				GroupActorInfos.RemoveAt( InfoIndex );
			}
		}

		// make sure if we have extra groups to add
		for( int32 GroupIndex = 0; GroupIndex < InterpGroups.Num(); ++GroupIndex )
		{
			UInterpGroup* InterpGroup = InterpGroups[ GroupIndex ];

			bool bFoundInfo = false;
			for( int32 InfoIndex = 0; InfoIndex < GroupActorInfos.Num(); ++InfoIndex )
			{
				FInterpGroupActorInfo& Info = GroupActorInfos[ InfoIndex ];
				
				if( InterpGroup->GetFName() == Info.ObjectName )
				{
					bFoundInfo = true;
				}
			}

			if( !bFoundInfo )
			{
				UE_LOG(LogMatinee, Warning, TEXT("GROUP ADD: New group found for (%s)."), *InterpGroup->GetName());

				FInterpGroupActorInfo NewInfo;
				NewInfo.ObjectName = InterpGroup->GetFName();

				// this is slow, but this isn't supposed to happen very often and only in editor, 
				for (int32 I=0; I<GroupInst.Num(); ++I)
				{
					if (GroupInst[I]->Group == InterpGroup)
					{
						// make sure actors list are same, it's okay if null
						NewInfo.Actors.Add(GroupInst[I]->GroupActor);
					}
				}

				GroupActorInfos.Add( NewInfo );
			}
		}
	}
}

void AMatineeActor::DeleteActorGroupInfo(class UInterpGroup * Group, AActor* ActorToDelete)
{
	// this one just refreshes the old actor to new actor for the group that's set up
	for (int32 InfoIndex = 0; InfoIndex<GroupActorInfos.Num(); ++InfoIndex)
	{
		// if same name
		FInterpGroupActorInfo & GroupInfo = GroupActorInfos[InfoIndex];
		if (GroupInfo.ObjectName == Group->GetFName())
		{
			// find the actor you're looking for. It can be NULL
			for (int32 ActorID = 0; ActorID<GroupInfo.Actors.Num(); ++ActorID)
			{
				if (GroupInfo.Actors[ActorID] == ActorToDelete)
				{
					if (ActorToDelete)
					{
						ActorToDelete->RemoveControllingMatineeActor(*this);
					}

					GroupInfo.Actors.RemoveAt(ActorID);
					return;
				}
			}
		}
	}
}

void AMatineeActor::DeleteGroupinfo(class UInterpGroup * GroupToDelete)
{
	// Remove the associated actors from this matinee actors GroupActorInfos array as well
	for( int32 InfoIndex =  GroupActorInfos.Num()-1; InfoIndex >=0; --InfoIndex )
	{
		FInterpGroupActorInfo& Info = GroupActorInfos[ InfoIndex ];

		if(GroupToDelete->GetFName() == Info.ObjectName)
		{
			for (int32 ActorIndex = 0; ActorIndex<Info.Actors.Num(); ++ActorIndex)
			{
				if ( Info.Actors[ActorIndex] )
				{
					// clear Matinee Actor
					Info.Actors[ActorIndex]->RemoveControllingMatineeActor(*this);
				}
			}

			GroupActorInfos.RemoveAt( InfoIndex );
			break;
		}
	}
}

void AMatineeActor::ReplaceActorGroupInfo(UInterpGroup * Group, AActor* OldActor, AActor* NewActor)
{
	// this one just refreshes the old actor to new actor for the group that's set up
	for (int32 InfoIndex = 0; InfoIndex<GroupActorInfos.Num(); ++InfoIndex)
	{
		// if same name
		FInterpGroupActorInfo & GroupInfo = GroupActorInfos[InfoIndex];
		if (GroupInfo.ObjectName == Group->GetFName())
		{
			if (GroupInfo.Actors.Num() > 0)
			{
				// find the actor you're looking for. It can be NULL
				for (int32 ActorID = 0; ActorID<GroupInfo.Actors.Num(); ++ActorID)
				{
					if (GroupInfo.Actors[ActorID] == OldActor)
					{
						if (OldActor)
						{
							OldActor->RemoveControllingMatineeActor(*this);
						}

						GroupInfo.Actors[ActorID] = NewActor;

						if (NewActor)
						{
							NewActor->AddControllingMatineeActor(*this);
						}
						return;
					}
				}
			}
		}
	}
}

void AMatineeActor::SaveActorVisibility( AActor* Actor )
{
	check( GIsEditor );

	if( Actor != NULL )
	{
		if ( !Actor->IsPendingKill() )
		{
			const uint8* SavedVisibility = SavedActorVisibilities.Find( Actor );
			if ( !SavedVisibility )
			{
				// Save both bHidden and bHiddenEdTemporary to make it work properly in the editor
				uint8 bSaveHidden = (Actor->bHidden ? 1 : 0) | (Actor->IsTemporarilyHiddenInEditor() ? 2 : 0);
				SavedActorVisibilities.Add( Actor, bSaveHidden );
			}
		}
	}
}

void AMatineeActor::ConditionallySaveActorState( UInterpGroupInst* InGroupInst, AActor* Actor )
{
#if WITH_EDITORONLY_DATA
	bool bShouldCaptureTransforms = false;
	bool bShouldCaptureVisibility = false;

	// Iterate over all of this group's tracks
	for( int32 TrackIdx = 0; TrackIdx < InGroupInst->Group->InterpTracks.Num(); ++TrackIdx )
	{
		const UInterpTrack* CurTrack = InGroupInst->Group->InterpTracks[ TrackIdx ];

		if ( CurTrack->IsDisabled() )
		{
			continue;
		}

		// Is this is a 'movement' track?  If so, then we'll consider it worthy of our test
		if( CurTrack->IsA( UInterpTrackMove::StaticClass() ) )
		{
			bShouldCaptureTransforms = true;
		}

		// Is this an 'anim control' track?  If so, we'll need to capture the object's transforms along
		// with all of it's attached objects.  As the object animates, any attached objects will wander
		// so we'll need to make sure to use this to restore their transform later.
		if( CurTrack->IsA( UInterpTrackAnimControl::StaticClass() ) )
		{
			bShouldCaptureTransforms = true;
		}

		// Is this a 'visibility' track?  If so, we'll save the actor's original bHidden state
		if( CurTrack->IsA( UInterpTrackVisibility::StaticClass() ) )
		{
			bShouldCaptureVisibility = true;
		}
	}

	if( bShouldCaptureTransforms )
	{
		SaveActorTransforms( Actor );
	}

	if( bShouldCaptureVisibility )
	{
		// Save visibility state
		SaveActorVisibility( Actor );
	}
#endif // WITH_EDITORONLY_DATA
}

void AMatineeActor::SaveActorTransforms( AActor* Actor )
{
	check( GIsEditor );
#if WITH_EDITORONLY_DATA
	if( Actor != NULL )
	{
		// Save transforms for the parent actor and all of its children
		const FSavedTransform* SavedTransform = SavedActorTransforms.Find( Actor );
		if ( !SavedTransform && Actor->GetRootComponent() )
		{
			FSavedTransform NewSavedTransform;
			NewSavedTransform.Translation = Actor->GetRootComponent()->RelativeLocation;
			NewSavedTransform.Rotation = Actor->GetRootComponent()->RelativeRotation;

			SavedActorTransforms.Add( Actor, NewSavedTransform );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void AMatineeActor::RestoreActorTransforms()
{
	check( GIsEditor );

#if WITH_EDITORONLY_DATA
	for( TMap<TWeakObjectPtr<AActor>, FSavedTransform>::TIterator It(SavedActorTransforms) ; It ; ++It )
	{
		TWeakObjectPtr<AActor> SavedActor = It.Key();
		if ( SavedActor.IsValid() )
		{
			FSavedTransform& SavedTransform = It.Value();

			// only update actor position/rotation if the track changed its position/rotation
			if( SavedActor->GetRootComponent() && (SavedActor->GetRootComponent()->RelativeLocation != SavedTransform.Translation || SavedActor->GetRootComponent()->RelativeRotation != SavedTransform.Rotation) )
			{
				SavedActor->GetRootComponent()->SetRelativeLocationAndRotation(SavedTransform.Translation, SavedTransform.Rotation);
			}
		}
	}
	SavedActorTransforms.Empty();
#endif
}	

void AMatineeActor::RestoreActorVisibilities()
{
	check( GIsEditor );

#if WITH_EDITORONLY_DATA
	for( TMap<TWeakObjectPtr<AActor>, uint8>::TIterator It(SavedActorVisibilities) ; It ; ++It )
	{
		TWeakObjectPtr<AActor> SavedActor = It.Key();
		if ( SavedActor.IsValid() )
		{
			// Restore bHidden and bHiddenEdTemporary flags
			bool bSavedHidden = ( It.Value() & 1 ) ? true : false;
			bool bSavedHiddenEditor = ( It.Value() & 2 ) ? true : false;

			// only update actor if something has actually changed
			if( SavedActor->bHidden != bSavedHidden )
			{
				SavedActor->SetActorHiddenInGame( bSavedHidden );
			}
			if( SavedActor->IsTemporarilyHiddenInEditor() != bSavedHiddenEditor )
			{
				SavedActor->SetIsTemporarilyHiddenInEditor( bSavedHiddenEditor );
			}
		}
	}
	SavedActorVisibilities.Empty();
#endif // WITH_EDITORONLY_DATA
}

void AMatineeActor::RecaptureActorState()
{
	check( GIsEditor );

#if WITH_EDITORONLY_DATA
	// We now need to remove from the saved actor transformation state any actors
	// that belonged to the removed group instances, along with actors rooted to
	// those group actors.  However, another group could be affecting an actor which
	// is an ancestor of the removed actor(S).  So, we store the current scrub position,
	// restore all actor transforms (including the ones assigned to the groups that were
	// removed), then save off the transforms for actors referenced (directly or indirectly)
	// by group instances, then restore the scrub position.

	const float SavedScrubPosition = InterpPosition;

	RestoreActorVisibilities();
	RestoreActorTransforms();
	for(int32 i=0; i<GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = GroupInst[i];
		AActor* InstGroupActor = GrInst->GetGroupActor();
		if( InstGroupActor )
		{
			ConditionallySaveActorState( GrInst, InstGroupActor );
		}
	}
	UpdateInterp( SavedScrubPosition, true );
#endif // WITH_EDITORONLY_DATA
}

void AMatineeActor::InitGroupActorForGroup(class UInterpGroup* InGroup, class AActor* InGroupActor)
{
	bool bFoundGroup = false;
	for( int32 GroupIndex = 0; GroupIndex < GroupActorInfos.Num(); ++GroupIndex )
	{
		FInterpGroupActorInfo& Info = GroupActorInfos[ GroupIndex ];
		if( Info.ObjectName == InGroup->GetFName() )
		{
			bFoundGroup = true;
			Info.Actors.AddUnique( InGroupActor );

			if (InGroupActor)
			{
				InGroupActor->AddControllingMatineeActor(*this);
			}
		}
	}

	if( !bFoundGroup )
	{
		FInterpGroupActorInfo NewInfo;
		NewInfo.ObjectName = InGroup->GetFName();
		NewInfo.Actors.Add( InGroupActor );

		GroupActorInfos.Add( NewInfo );

		if (InGroupActor)
		{
			InGroupActor->AddControllingMatineeActor(*this);
		}
	}

	PostEditChange();
}

bool AMatineeActor::GetReferencedContentObjects( TArray<UObject*>& Objects ) const
{
	Super::GetReferencedContentObjects(Objects);

	if (MatineeData)
	{
		Objects.Add(MatineeData);
	}
	return true;
}

#endif

UInterpGroupInst* AMatineeActor::FindFirstGroupInst(class UInterpGroup* InGroup)
{
	if(!InGroup)
	{
		return NULL;
	}

	for(int32 i=0; i<GroupInst.Num(); i++)
	{
		if( GroupInst[i]->Group == InGroup )
		{
			return GroupInst[i];
		}
	}

	return NULL;
}

UInterpGroupInst* AMatineeActor::FindFirstGroupInstByName(const FString& InGroupName)
{
	for(int32 i=0; i<GroupInst.Num(); i++)
	{
		if( GroupInst[i]->Group->GroupName.ToString() == InGroupName )
		{
			return GroupInst[i];
		}
	}

	return NULL;
}


AActor* AMatineeActor::FindViewedActor()
{
	UInterpGroupDirector* DirGroup = MatineeData->FindDirectorGroup();
	if(DirGroup)
	{
		UInterpTrackDirector* DirTrack = DirGroup->GetDirectorTrack();
		if(DirTrack)
		{
			float CutTime, CutTransitionTime;
			FName ViewGroupName = DirTrack->GetViewedGroupName(InterpPosition, CutTime, CutTransitionTime);
			UInterpGroupInst* ViewGroupInst = FindFirstGroupInstByName(ViewGroupName.ToString());
			if(ViewGroupInst)
			{
				return ViewGroupInst->GetGroupActor();
			}
		}
	}

	return NULL;
}

void AMatineeActor::UpdateReplicatedData( bool bIsBeginningPlay )
{
	ForceNetUpdate();

	if (bIsPlaying || bIsBeginningPlay)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_CheckPriorityRefresh, this, &AMatineeActor::CheckPriorityRefresh, 1.0f, true);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_CheckPriorityRefresh);
	}
}

void AMatineeActor::BeginPlay()
{
	Super::BeginPlay();

	if( bPlayOnLevelLoad )
	{
		Play();
	}
}

void AMatineeActor::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);
	
	if (MatineeData)
	{
		for (int32 GroupIndex = 0; GroupIndex < MatineeData->InterpGroups.Num(); GroupIndex++)
		{
			UInterpGroup* Group = MatineeData->InterpGroups[GroupIndex];
			for (int32 TrackIndex = 0; TrackIndex < Group->InterpTracks.Num(); TrackIndex++)
			{
				Group->InterpTracks[TrackIndex]->ApplyWorldOffset(InOffset, bWorldShift);
			}
		}
	}
}

void AMatineeActor::CheckPriorityRefresh()
{
	//local Controller C;
	// check if it has a director group - if so, it's controlling the camera, so it's important
	for (int32 i = 0; i < GroupInst.Num(); i++)
	{
		if (Cast<UInterpGroupInstDirector>(GroupInst[i]) != NULL)
		{
			ForceNetUpdate();
			return;
		}
	}
}


/*-----------------------------------------------------------------------------
  AMatineeActorCameraAnim
-----------------------------------------------------------------------------*/

AMatineeActorCameraAnim::AMatineeActorCameraAnim(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bEditable = false;
	bListedInSceneOutliner = false;
#endif
}

/*-----------------------------------------------------------------------------
  UInterpData
-----------------------------------------------------------------------------*/
UInterpData::UInterpData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InterpLength = 5.0f;
	EdSectionStart = 0.0f;
	EdSectionEnd = 1.0f;
	PathBuildTime = 0.0f;
	bShouldBakeAndPrune = false;
}

#if WITH_EDITORONLY_DATA
void UInterpData::CreateDefaultFilters()
{
	UInterpFilter* FilterAll = NewObject<UInterpFilter>(this, TEXT("FilterAll"), RF_Transient | RF_TextExportTransient);
	FilterAll->Caption = TEXT("All");
	DefaultFilters.Add(FilterAll);

	UInterpFilter_Classes* FilterCameras = NewObject<UInterpFilter_Classes>(this, TEXT("FilterCameras"), RF_Transient | RF_TextExportTransient);
	FilterCameras->Caption = TEXT("Cameras");
	FilterCameras->ClassToFilterBy = ACameraActor::StaticClass();
	DefaultFilters.Add(FilterCameras);

	UInterpFilter_Classes* FilterSkeletalMeshes = NewObject<UInterpFilter_Classes>(this, TEXT("FilterSkeletalMeshes"), RF_Transient | RF_TextExportTransient);
	FilterSkeletalMeshes->Caption = TEXT("Skeletal Meshes");
	FilterSkeletalMeshes->ClassToFilterBy = ASkeletalMeshActor::StaticClass();
	DefaultFilters.Add(FilterSkeletalMeshes);

	UInterpFilter_Classes* FilterLighting = NewObject<UInterpFilter_Classes>(this, TEXT("FilterLighting"), RF_Transient | RF_TextExportTransient);
	FilterLighting->Caption = TEXT("Lights");
	FilterLighting->ClassToFilterBy = ALight::StaticClass();
	DefaultFilters.Add(FilterLighting);

	UInterpFilter_Classes* FilterEmitters = NewObject<UInterpFilter_Classes>(this, TEXT("FilterEmitters"), RF_Transient | RF_TextExportTransient);
	FilterEmitters->Caption = TEXT("Particles");
	FilterEmitters->ClassToFilterBy = AEmitter::StaticClass();
	DefaultFilters.Add(FilterEmitters);

	UInterpFilter_Classes* FilterSounds = NewObject<UInterpFilter_Classes>(this, TEXT("FilterSounds"), RF_Transient | RF_TextExportTransient);
	FilterSounds->Caption = TEXT("Sounds");
	FilterSounds->TrackClasses.Add(UInterpTrackSound::StaticClass());
	DefaultFilters.Add(FilterSounds);

	UInterpFilter_Classes* FilterEvents = NewObject<UInterpFilter_Classes>(this, TEXT("FilterEvents"), RF_Transient | RF_TextExportTransient);
	FilterEvents->Caption = TEXT("Events");
	FilterEvents->TrackClasses.Add(UInterpTrackEvent::StaticClass());
	DefaultFilters.Add(FilterEvents);
}
#endif

void UInterpData::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
#if WITH_EDITORONLY_DATA
		CreateDefaultFilters();
#endif
	}
}

void UInterpData::PostLoad(void)
{
	Super::PostLoad();

	// Ensure the cached director group is emptied out
	CachedDirectorGroup = NULL;

#if WITH_EDITORONLY_DATA
	if (GIsEditor && DefaultFilters.Num() == 0)
	{
		CreateDefaultFilters();
	}
#endif
	// If in the game, cache off the director group intentionally to avoid
	// frequent searches for it
	if ( IsRunningGame() )
	{
		for( int32 i = 0; i < InterpGroups.Num(); ++i )
		{
			UInterpGroupDirector* TestDirGroup = Cast<UInterpGroupDirector>( InterpGroups[i] );
			if( TestDirGroup )
			{
				check( !CachedDirectorGroup ); // Should only have 1 DirectorGroup at most!
				CachedDirectorGroup = TestDirGroup;
			}
		}
	}
}

int32 UInterpData::FindGroupByName(FName InGroupName)
{
	if(InGroupName != NAME_None)
	{
		for(int32 i=0; i<InterpGroups.Num(); i++)
		{
			const FName& GroupName = InterpGroups[i]->GroupName;
			if( GroupName == InGroupName )
			{
				return i;
			}
		}
	}

	return INDEX_NONE;
}

int32 UInterpData::FindGroupByName(const FString& InGroupName)
{
	for(int32 i=0; i<InterpGroups.Num(); i++)
	{
		const FName& GroupName = InterpGroups[i]->GroupName;
		if( GroupName.ToString() == InGroupName )
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void UInterpData::FindTracksByClass(UClass* TrackClass, TArray<UInterpTrack*>& OutputTracks)
{
	for(int32 i=0; i<InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = InterpGroups[i];
		Group->FindTracksByClass(TrackClass, OutputTracks);
	}
}

UInterpGroupDirector* UInterpData::FindDirectorGroup()
{
	UInterpGroupDirector* DirGroup = NULL;

	// If not in game, recheck all the interp groups to ensure there's either zero or one
	// director group and that it hasn't changed
	if ( GIsEditor  )
	{
		for(int32 i=0; i<InterpGroups.Num(); i++)
		{
			UInterpGroupDirector* TestDirGroup = Cast<UInterpGroupDirector>( InterpGroups[i] );
			if(TestDirGroup)
			{
				check(!DirGroup); // Should only have 1 DirectorGroup at most!
				DirGroup = TestDirGroup;
			}
		}
	}

	// If in game, just use the cached director group, as it cannot have changed
	else
	{
		DirGroup = CachedDirectorGroup;
	}

	return DirGroup;
}

bool UInterpData::IsEventName(const FName& InEventName) const
{
	return AllEventNames.Contains( InEventName );
}

void UInterpData::GetAllEventNames(TArray<FName>& OutEventNames) const
{
	OutEventNames = AllEventNames;
}

void UInterpData::UpdateEventNames()
{
	AllEventNames.Empty();

	TArray<UInterpTrack*> Tracks;
	FindTracksByClass(UInterpTrackEvent::StaticClass(), Tracks);

	for(int32 i=0; i<Tracks.Num(); i++)
	{
		UInterpTrackEvent* EventTrack = CastChecked<UInterpTrackEvent>( Tracks[i] );
		for(int32 j=0; j<EventTrack->EventTrack.Num(); j++)
		{
			AllEventNames.AddUnique( EventTrack->EventTrack[j].EventName );
		}
	}
}

/*-----------------------------------------------------------------------------
 UInterpGroup
-----------------------------------------------------------------------------*/
UInterpGroup::UInterpGroup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName NAME_UInterpGroup;
		FConstructorStatics()
			: NAME_UInterpGroup(TEXT("UInterpGroup"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	GroupName = ConstructorStatics.NAME_UInterpGroup;
	GroupColor = FColor(100, 80, 200, 255);

	bVisible = true;
	SetSelected( false );
}

void UInterpGroup::PostLoad()
{
	Super::PostLoad();

	// Remove any NULLs in the InterpTracks array.
	int32 TrackIndex = 0;
	while(TrackIndex < InterpTracks.Num())
	{
		if(InterpTracks[TrackIndex])
		{
			TrackIndex++;
		}
		else
		{
			InterpTracks.RemoveAt(TrackIndex);
		}
	}
}

void UInterpGroup::UpdateGroup(float NewPosition, UInterpGroupInst* GrInst, bool bPreview, bool bJump)
{
	checkf( InterpTracks.Num() == GrInst->TrackInst.Num(), TEXT("UpdateGroup track mismatch! Outer = %s"), *GetNameSafe(GetOuter()) );

	// Update animation state of Actor.
#if 0
	// if in editor and preview and anim control exists, let them update weight before update track
	if ( GIsEditor && bPreview && HasAnimControlTrack() )
	{
		UpdateAnimWeights(NewPosition, GrInst, bPreview, bJump);
	}
#endif

	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		// If the track instances have been removed from the group instance, this means that a previous track update has terminated the sequence.
		// The group instance itself will still be valid, but unreferenced.
		const bool bHasBeenTerminated = (GrInst->TrackInst.Num() == 0);

		if (bHasBeenTerminated)
		{
			break;
		}

		UInterpTrack* Track = InterpTracks[i];
		UInterpTrackInst* TrInst = GrInst->TrackInst[i];

		//Tracks that are disabled or are presently recording should NOT be updated
		if ( Track->IsDisabled() || Track->bIsRecording)
		{
			continue;
		}

		if(bPreview)
		{
			Track->ConditionalPreviewUpdateTrack(NewPosition, TrInst);
		}
		else
		{
			Track->ConditionalUpdateTrack(NewPosition, TrInst, bJump);
		}
	}

#if 0
	// Update animation state of Actor.
	UpdateAnimWeights(NewPosition, GrInst, bPreview, bJump);
#endif // disable this, it doesn't w
}

bool UInterpGroup::HasSelectedTracks() const
{
	for(auto TrackIt = InterpTracks.CreateConstIterator(); TrackIt; ++TrackIt)
	{
		UInterpTrack* const Track = *TrackIt;
		if(Track->IsSelected())
		{
			return true;
		}
	}

	return false;
}

/** Utility function for adding a weight entry to a slot with the given name. Creates a new entry in the array if there is not already one present. */
static void AddSlotInfo(TArray<FAnimSlotInfo>& SlotInfos, FName SlotName, float InChannelWeight)
{
	// Look for an existing entry with this name.
	for(int32 i=0; i<SlotInfos.Num(); i++)
	{
		// If we find one, add weight to array and we are done.
		if(SlotInfos[i].SlotName == SlotName)
		{
			SlotInfos[i].ChannelWeights.Add(InChannelWeight);
			return;
		}
	}

	// If we didn't find one, add a new entry to the array now.
	const int32 NewIndex = SlotInfos.AddZeroed();
	SlotInfos[NewIndex].SlotName = SlotName;
	SlotInfos[NewIndex].ChannelWeights.Add(InChannelWeight);
}

bool UInterpGroup::HasAnimControlTrack() const
{
	bool bHasAnimTrack = false;
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		if( InterpTracks[i]->bIsAnimControlTrack )
		{
			bHasAnimTrack = true;
		}
	}

	return bHasAnimTrack;
}


bool UInterpGroup::HasMoveTrack() const
{
	bool bHasMoveTrack = false;
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		if( InterpTracks[i]->IsA(UInterpTrackMove::StaticClass()) )
		{
			bHasMoveTrack = true;
			break;
		}
	}

	return bHasMoveTrack;
}

/**
 * We keep this around here as otherwise we are constantly allocating/deallocating the FAnimSlotInfos that Matinee is using
 * NOTE:  We probably need to clear this out every N calls
 **/
namespace
{
	TArray<FAnimSlotInfo> UpdateAnimWeightsSlotInfos;
}

void UInterpGroup::UpdateAnimWeights(float NewPosition, class UInterpGroupInst* GrInst, bool bPreview, bool bJump)
{
	// Get the Actor this group is working on.
	AActor* Actor = GrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	// Find Anim Interface. If not return. When initialize, it will print error
	IMatineeAnimInterface * IMAI = Cast<IMatineeAnimInterface>(Actor);
	if (!IMAI)
	{
		return;
	}
	
	float TotalSlotNodeAnimWeight = 0.f;
	bool UsingSlotNode = false;
	FName SlotNodeNameUsed;

	// Now iterate over tracks looking for AnimControl ones.
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrack* Track = InterpTracks[i];
		check(Track);

		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
		if( AnimTrack && !AnimTrack->IsDisabled() )
		{
			// Add entry for this track to the SlotInfos array.
			const float TrackWeight = AnimTrack->GetWeightForTime(NewPosition);
			// if it's using slot node, then add weight
			if ( AnimTrack->SlotName != NAME_None )
			{
				TotalSlotNodeAnimWeight += TrackWeight;
				UsingSlotNode = true;
				SlotNodeNameUsed = AnimTrack->SlotName;
			}

			AddSlotInfo(UpdateAnimWeightsSlotInfos, AnimTrack->SlotName, TrackWeight);
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// no weight is set and using slot node
	// sometimes effect artist put slot node name for non animtree
	// so I need to filter that out
	if ( (Cast<APawn>(Actor)) && UsingSlotNode && TotalSlotNodeAnimWeight <= ZERO_ANIMWEIGHT_THRESH )
	{
		UE_LOG(LogAnimation, Log, TEXT("SlotName (%s) is set, but no weight is applied. Please add a key to curve editor and set weight."), *SlotNodeNameUsed.ToString());
	}
#endif
	// Finally, pass the array to the Actor. Does different things depending on whether we are in Matinee or not.
	if(bPreview)
	{
		IMAI->PreviewSetAnimWeights(UpdateAnimWeightsSlotInfos);
	}
	else
	{
		IMAI->SetAnimWeights(UpdateAnimWeightsSlotInfos);
	}

	UpdateAnimWeightsSlotInfos.Reset();
}

void UInterpGroup::EnsureUniqueName()
{
	UInterpData* IData = CastChecked<UInterpData>( GetOuter() );

	FName NameBase = GroupName;
	int32 Suffix = 0;

	// Test all other groups apart from this one to see if name is already in use
	bool bNameInUse = false;
	for(int32 i=0; i<IData->InterpGroups.Num(); i++)
	{
		if( (IData->InterpGroups[i] != this) && (IData->InterpGroups[i]->GroupName == GroupName) )
		{
			bNameInUse = true;
		}
	}

	// If so - keep appending numbers until we find a name that isn't!
	while( bNameInUse )
	{
		FString GroupNameString = FString::Printf(TEXT("%s%d"), *NameBase.ToString(), Suffix);
		GroupName = FName( *GroupNameString );
		Suffix++;

		bNameInUse = false;
		for(int32 i=0; i<IData->InterpGroups.Num(); i++)
		{
			if( (IData->InterpGroups[i] != this) && (IData->InterpGroups[i]->GroupName == GroupName) )
			{
				bNameInUse = true;
			}
		}
	}
}


void UInterpGroup::FindTracksByClass(UClass* TrackClass, TArray<UInterpTrack*>& OutputTracks)
{
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrack* Track = InterpTracks[i];
		if( Track->IsA(TrackClass) )
		{
			OutputTracks.Add(Track);
		}
	}
}

int32 UInterpGroup::GetAnimTracksUsingSlot(FName InSlotName)
{
	int32 NumTracks = 0;
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>( InterpTracks[i] );
		if(AnimTrack && AnimTrack->SlotName == InSlotName)
		{
			NumTracks++;
		}
	}
	return NumTracks;
}

AActor* UInterpGroup::SelectGroupActor( UInterpGroupInst* GrInst, bool bDeselectActors )
{
	AMatineeActor::PushIgnoreActorSelection();

#if WITH_EDITOR
	// Deselect all, if specified
	if ( bDeselectActors )
	{
		GEditor->SelectNone( true, true );
	}
#endif // WITH_EDITOR

	check( GrInst );
	check( GrInst->TrackInst.Num() == InterpTracks.Num() );

	AActor* Actor = GrInst->GetGroupActor();
#if WITH_EDITOR
	// Select the actor, if it isn't already
	if( Actor && !Actor->IsSelected() )
	{
		GEditor->SelectActor( Actor, true, true );
	}
#endif // WITH_EDITOR

	AMatineeActor::PopIgnoreActorSelection();

	return Actor;
}

AActor* UInterpGroup::DeselectGroupActor( UInterpGroupInst* GrInst )
{
	check( GrInst );
	check( GrInst->TrackInst.Num() == InterpTracks.Num() );

	AActor* Actor = GrInst->GroupActor;
#if WITH_EDITOR
	// Deselect the actor, if it's selected
	if( Actor && Actor->IsSelected() )
	{
		GEditor->SelectActor( Actor, false, true );
	}
#endif // WITH_EDITOR

	return Actor;
}

/*-----------------------------------------------------------------------------
	UInterpFilter
-----------------------------------------------------------------------------*/
UInterpFilter::UInterpFilter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UInterpFilter::FilterData(class AMatineeActor* InMatineeActor)
{
	// Mark our custom filtered groups as visible
	for( int32 GroupIdx = 0; GroupIdx < InMatineeActor->MatineeData->InterpGroups.Num(); GroupIdx++)
	{
		UInterpGroup* CurGroup = InMatineeActor->MatineeData->InterpGroups[ GroupIdx ];
		CurGroup->bVisible = true;

		for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
		{
			UInterpTrack* CurTrack = CurGroup->InterpTracks[ CurTrackIndex ];
			CurTrack->bVisible = true;
		}
	}
}

/*-----------------------------------------------------------------------------
	UInterpFilter_Classes
-----------------------------------------------------------------------------*/
UInterpFilter_Classes::UInterpFilter_Classes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpFilter_Classes::FilterData(class AMatineeActor* InMatineeActor)
{
#if WITH_EDITORONLY_DATA
	for(int32 GroupIdx=0; GroupIdx<InMatineeActor->MatineeData->InterpGroups.Num(); GroupIdx++)
	{
		UInterpGroup* Group = InMatineeActor->MatineeData->InterpGroups[GroupIdx];
		UInterpGroupInst* GroupInst = InMatineeActor->FindFirstGroupInst(Group);

		bool bIncludeThisGroup = true;

		// Folder groups may not have a group instance
		if( GroupInst != NULL )
		{
			// We avoid filtering out director groups (unless all of the group's tracks are filtered out below)
			if( !Group->IsA( UInterpGroupDirector::StaticClass() ) )
			{
				// If we were set to filter specific classes, then do that! (Otherwise, the group will always
				// be included)
				if( ClassToFilterBy != NULL )
				{
					AActor* Actor = GroupInst->GetGroupActor();
					if( Actor != NULL )
					{
						if( !Actor->IsA( ClassToFilterBy ) )
						{
							bIncludeThisGroup = false;
						}
					}
					else
					{
						// No actor bound but we have an actor filter set, so don't include it
						bIncludeThisGroup = false;
					}
				}
			}

			// If we were set to only include the group if it contains specific types of
			// tracks, then do that now
			if( TrackClasses.Num() > 0 )
			{
				bool bHasAppropriateTrack = false;

				for( int32 CurTrackIndex = 0; CurTrackIndex < Group->InterpTracks.Num(); ++CurTrackIndex )
				{
					UInterpTrack* CurTrack = Group->InterpTracks[ CurTrackIndex ];
					if( CurTrack != NULL )
					{
						for( int32 TrackClassIndex = 0; TrackClassIndex < TrackClasses.Num(); ++TrackClassIndex )
						{
							if( CurTrack->IsA( TrackClasses[ TrackClassIndex ] ) )
							{
								// We found a track that matches the filter!
								bHasAppropriateTrack = true;
								break;
							}
						}
					}
				}

				if( !bHasAppropriateTrack )
				{
					// Group doesn't contain any tracks that matches the desired filter
					bIncludeThisGroup = false;
				}
			}
		}
		else
		{
			// No group inst, so don't include it unless it's a folder
			bIncludeThisGroup = Group->bIsFolder;
		}

		if( bIncludeThisGroup )
		{
			// Mark the group as visible!
			Group->bVisible = true;

			for( int32 CurTrackIndex = 0; CurTrackIndex < Group->InterpTracks.Num(); ++CurTrackIndex )
			{
				UInterpTrack* CurTrack = Group->InterpTracks[ CurTrackIndex ];
				if( CurTrack != NULL )
				{
					// If we need to, go through and constrain which track types are visible using our
					// list of filtered track classes
					if( TrackClasses.Num() > 0 )
					{
						for( int32 TrackClassIndex = 0; TrackClassIndex < TrackClasses.Num(); ++TrackClassIndex )
						{
							if( CurTrack->IsA( TrackClasses[ TrackClassIndex ] ) )
							{
								// We found a track that matches the filter!
								CurTrack->bVisible = true;
							}
						}
					}
					else
					{
						// No track filter set, so make sure they're all visible
						CurTrack->bVisible = true;
					}
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/*-----------------------------------------------------------------------------
	UInterpFilter_Custom
-----------------------------------------------------------------------------*/
UInterpFilter_Custom::UInterpFilter_Custom(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpFilter_Custom::FilterData(AMatineeActor* InMatineeActor)
{
#if WITH_EDITORONLY_DATA
	// Mark our custom filtered groups as visible
	for(int32 GroupIdx=0; GroupIdx<GroupsToInclude.Num(); GroupIdx++)
	{
		UInterpGroup* CurGroup = GroupsToInclude[ GroupIdx ];
		CurGroup->bVisible = true;

		for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
		{
			UInterpTrack* CurTrack = CurGroup->InterpTracks[ CurTrackIndex ];
			CurTrack->bVisible = true;
		}
	}
#endif // WITH_EDITORONLY_DATA
}


/*-----------------------------------------------------------------------------
 UInterpGroupInst
-----------------------------------------------------------------------------*/
UInterpGroupInst::UInterpGroupInst(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


AActor* UInterpGroupInst::GetGroupActor() const
{
	if(!GroupActor || GroupActor->IsPendingKill())
	{
		return NULL;
	}
	else 
	{
		return GroupActor;
	}
}

void UInterpGroupInst::SetGroupActor(AActor* Actor)
{
	GroupActor = Actor;
}

void UInterpGroupInst::SaveGroupActorState()
{
	check(Group);
	for(int32 i=0; i<TrackInst.Num(); i++)
	{
		TrackInst[i]->SaveActorState( Group->InterpTracks[i] );
	}
}

void UInterpGroupInst::RestoreGroupActorState()
{
	check(Group);
	for(int32 i=0; i<TrackInst.Num(); i++)
	{
		TrackInst[i]->RestoreActorState( Group->InterpTracks[i] );
	}
}


void UInterpGroupInst::InitGroupInst(UInterpGroup* InGroup, AActor* InGroupActor)
{
	check(InGroup);

	// If this group has already been initialized, terminate it before reinitializing it
	// This can happen in networked games with placed pawns referenced by an InterpGroupAI
	if( TrackInst.Num() )
	{
		TermGroupInst(true);
	}

	Group = InGroup;
	GroupActor = InGroupActor;

	for(int32 i=0; i<InGroup->InterpTracks.Num(); i++)
	{
		// Construct Track instance object
		UInterpTrackInst* TrInst = NewObject<UInterpTrackInst>(this, InGroup->InterpTracks[i]->TrackInstClass, NAME_None, RF_Transactional);
		TrackInst.Add(TrInst);

		TrInst->InitTrackInst( InGroup->InterpTracks[i] );
	}

	// If we have an anim control track, do startup for that.
	bool bHasAnimTrack = Group->HasAnimControlTrack();
	if (bHasAnimTrack && GroupActor != NULL && !GroupActor->IsPendingKill())
	{
		IMatineeAnimInterface* IMAI = Cast<IMatineeAnimInterface>(GroupActor);
		if (IMAI)
		{
			// If in the editor and we haven't started playing, this should be Matinee! Bit yuck...
			if(GIsEditor && !InGroupActor->GetWorld()->HasBegunPlay())
			{
				// Then set the ones specified by this Group.
				IMAI->PreviewBeginAnimControl( Group );
			}
			else if (bHasAnimTrack)
			{
				// If in game - call script function that notifies us to that.
				IMAI->BeginAnimControl( Group );
			}
		}
		else
		{
			// this is when initialized. Print error if the interface is not found
			UE_LOG(LogMatinee, Warning, TEXT("InterpGroup : MatineeAnimInterface is missing for (%s)"), *GroupActor->GetName());		
		}
	}
}


void UInterpGroupInst::TermGroupInst(bool bDeleteTrackInst)
{
	for(int32 i=0; i<TrackInst.Num(); i++)
	{
		// Do any track cleanup
		UInterpTrack* Track = Group->InterpTracks[i];
		TrackInst[i]->TermTrackInst( Track );
	}
	TrackInst.Empty();

	// If we have an anim control track, do startup for that.
	bool bHasAnimTrack = Group->HasAnimControlTrack();
	if (GroupActor != NULL && !GroupActor->IsPendingKill())
	{
		IMatineeAnimInterface * IMAI = Cast<IMatineeAnimInterface>(GroupActor);
		if (IMAI)
		{
			// If in the editor and we haven't started playing, this should be Matinee!
			// We always call PreviewFinishAnimControl, even if we don't have an AnimTrack now, because we may have done at some point during editing in Matinee.
			if (GIsEditor && !GroupActor->GetWorld()->HasBegunPlay())
			{
				// Restore the AnimSets that was set on this actor when we entered Matinee.
				IMAI->PreviewFinishAnimControl(Group);		
			}
			else if (bHasAnimTrack)
			{
				// Only call FinishAnimControl in the game if we have an anim track.
				// If in game - call script function to say we've finish with the anim control.
				IMAI->FinishAnimControl(Group);
			}
		}
	}
}


/*-----------------------------------------------------------------------------
 UInterpGroupDirector
-----------------------------------------------------------------------------*/
UInterpGroupDirector::UInterpGroupDirector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName NAME_DirGroup;
		FConstructorStatics()
			: NAME_DirGroup(TEXT("DirGroup"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	GroupName = ConstructorStatics.NAME_DirGroup;
}

AActor* UInterpGroupDirector::SelectGroupActor( UInterpGroupInst* GrInst, bool bDeselectActors )
{
	AActor* Actor = Super::SelectGroupActor( GrInst, bDeselectActors );

#if WITH_EDITOR
	// Special case handling as a fallback if no other actor superceeds
	if ( !Actor )
	{
		UInterpTrackDirector* DirTrack = GetDirectorTrack();
		if ( DirTrack )
		{
			Actor = DirTrack->GetPreviewCamera();

			// Select the actor, if it isn't already
			if( Actor && !Actor->IsSelected() )
			{
				AMatineeActor::PushIgnoreActorSelection();
				GEditor->SelectActor( Actor, true, true );
				AMatineeActor::PopIgnoreActorSelection();
			}
		}
	}
#endif // WITH_EDITOR

	return Actor;
}

AActor* UInterpGroupDirector::DeselectGroupActor( UInterpGroupInst* GrInst )
{
	AActor* Actor = Super::DeselectGroupActor( GrInst );

#if WITH_EDITOR
	// Special case handling as a fallback if no other actor superceeds
	if ( !Actor )
	{
		UInterpTrackDirector* DirTrack = GetDirectorTrack();
		if ( DirTrack )
		{
			Actor = DirTrack->GetPreviewCamera();

			// Deselect the actor, if it's selected
			if( Actor && Actor->IsSelected() )
			{
				GEditor->SelectActor( Actor, false, true );
			}
		}
	}
#endif // WITH_EDITOR

	return Actor;
}

UInterpTrackDirector* UInterpGroupDirector::GetDirectorTrack()
{
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackDirector* DirTrack = Cast<UInterpTrackDirector>( InterpTracks[i] );
		if( DirTrack && !DirTrack->IsDisabled() )
		{
			return DirTrack;
		}
	}

	return NULL;
}


UInterpTrackFade* UInterpGroupDirector::GetFadeTrack()
{
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackFade* FadeTrack = Cast<UInterpTrackFade>( InterpTracks[i] );
		if( FadeTrack && !FadeTrack->IsDisabled() )
		{
			return FadeTrack;
		}
	}

	return NULL;
}


UInterpTrackSlomo* UInterpGroupDirector::GetSlomoTrack()
{
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackSlomo* SlomoTrack = Cast<UInterpTrackSlomo>( InterpTracks[i] );
		if( SlomoTrack && !SlomoTrack->IsDisabled() )
		{
			return SlomoTrack;
		}
	}

	return NULL;
}

UInterpTrackColorScale* UInterpGroupDirector::GetColorScaleTrack()
{
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackColorScale* ColorTrack = Cast<UInterpTrackColorScale>( InterpTracks[i] );
		if( ColorTrack && !ColorTrack->IsDisabled() )
		{
			return ColorTrack;
		}
	}

	return NULL;
}


UInterpTrackAudioMaster* UInterpGroupDirector::GetAudioMasterTrack()
{
	for(int32 i=0; i<InterpTracks.Num(); i++)
	{
		UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( InterpTracks[i] );
		if( AudioMasterTrack && !AudioMasterTrack->IsDisabled() )
		{
			return AudioMasterTrack;
		}
	}

	return NULL;
}

/*-----------------------------------------------------------------------------
 FInterpEdSelKey
-----------------------------------------------------------------------------*/

/** 
 * Recursive function used by GetOwningTrack();  to search through all subtracks
 */
UInterpTrack* FInterpEdSelKey::GetOwningTrack( UInterpTrack* pTrack )
{
	if ( pTrack )
	{
		// Loop through all the sub tracks trying to find the one that owns us
		for( int32 iSubTrack = 0; iSubTrack < pTrack->SubTracks.Num(); iSubTrack++ )
		{
			UInterpTrack* pSubTrack = pTrack->SubTracks[ iSubTrack ];
			if ( pSubTrack )
			{
				UInterpTrack* pOwner = GetOwningTrack( pSubTrack );
				if ( pOwner )
				{
					return pOwner;
				}
				else if ( Track == pSubTrack )
				{
					return pTrack;
				}
			}
		}
	}
	return NULL;
}

/** 
 * Returns the parent track of this key.  If this track isn't a subtrack, Track is returned (it owns itself)
 */
UInterpTrack* FInterpEdSelKey::GetOwningTrack()
{
	if ( Group )
	{
		// Loop through all the interp tracks trying to find the one that owns us
		for( int32 iInterpTrack = 0; iInterpTrack < Group->InterpTracks.Num(); iInterpTrack++ )
		{	
			UInterpTrack* pOwner = GetOwningTrack( Group->InterpTracks[ iInterpTrack ] );
			if ( pOwner )
			{
				return pOwner;
			}
		}
	}
	return Track;
}

/** 
 * Returns the sub group name of the parent track of this key. If this track isn't a subtrack, nothing is returned
 */
FString FInterpEdSelKey::GetOwningTrackSubGroupName( int32* piSubTrack )
{
#if WITH_EDITORONLY_DATA
	// Get the owning track
	const UInterpTrack* pOwningTrack = GetOwningTrack();
	if ( pOwningTrack )
	{
		// Loop through all the sub tracks trying to find our index
		for( int32 iSubTrack = 0; iSubTrack < pOwningTrack->SubTracks.Num(); iSubTrack++ )
		{
			const UInterpTrack* pSubTrack = pOwningTrack->SubTracks[ iSubTrack ];
			if ( pSubTrack && pSubTrack == Track )
			{
				// Loop through all the sub track groups trying to find a reference to our index
				for( int32 iSubTrackGroup = 0; iSubTrackGroup < pOwningTrack->SubTrackGroups.Num(); iSubTrackGroup++ )
				{
					const FSubTrackGroup& rSubTrackGroup = pOwningTrack->SubTrackGroups[ iSubTrackGroup ];

					// Loop through all the track indices trying to find a reference to our index
					for( int32 iTrackIndex = 0; iTrackIndex < rSubTrackGroup.TrackIndices.Num(); iTrackIndex++ )
					{
						const int32& rTrackIndex = rSubTrackGroup.TrackIndices[ iTrackIndex ];
						if ( iSubTrack == rTrackIndex )
						{
							// Send this back if requested
							if ( piSubTrack )
							{
								*piSubTrack = iSubTrack;
							}
							return rSubTrackGroup.GroupName;
						}
					}
				}
				break;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
	return FString();
}

/*-----------------------------------------------------------------------------
 UInterpTrack
-----------------------------------------------------------------------------*/
UInterpTrack::UInterpTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackInstClass = UInterpTrackInst::StaticClass();
	ActiveCondition = ETAC_Always;
	TrackTitle = TEXT("Track");
	bVisible = true;
	SetSelected( false );
	bIsRecording = false;
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Float.MAT_Groups_Float"), NULL, LOAD_None, NULL ));
	bIsCollapsed = false;
#endif // WITH_EDITORONLY_DATA
}


void UInterpTrack::ConditionalPreviewUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst)
{
	// Is the track enabled?
	bool bIsTrackEnabled = !bDisableTrack;
	UInterpGroupInst* GrInst = Cast<UInterpGroupInst>( TrInst->GetOuter() );
	if( GrInst != NULL )
	{
		AMatineeActor* MatineeActor = Cast<AMatineeActor>( GrInst->GetOuter() );
		if( MatineeActor != NULL )
		{
			if( (ActiveCondition == ETAC_GoreEnabled && !MatineeActor->bShouldShowGore) ||
				(ActiveCondition == ETAC_GoreDisabled && MatineeActor->bShouldShowGore) )
			{
				bIsTrackEnabled = false;
			}
		}
	}

	if(bIsTrackEnabled)
	{
		PreviewUpdateTrack( NewPosition, TrInst );
	}
	else
	{
		TrInst->RestoreActorState(this);
	}
}


void UInterpTrack::ConditionalUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst, bool bJump)
{
	// Is the track enabled?
	bool bIsTrackEnabled = !bDisableTrack;
	UInterpGroupInst* GrInst = Cast<UInterpGroupInst>( TrInst->GetOuter() );
	if( GrInst != NULL )
	{
		AMatineeActor* MatineeActor = Cast<AMatineeActor>( GrInst->GetOuter() );
		if( MatineeActor != NULL )
		{
			if( (ActiveCondition == ETAC_GoreEnabled && !MatineeActor->bShouldShowGore) ||
				(ActiveCondition == ETAC_GoreDisabled && MatineeActor->bShouldShowGore) )
			{
				bIsTrackEnabled = false;
			}
		}
	}

	if( bIsTrackEnabled )
	{
		UpdateTrack(NewPosition, TrInst, bJump);
	}
	else
	{
		TrInst->RestoreActorState(this);
	}
}



const FString	UInterpTrack::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackHelper") );
}

const FString	UInterpTrack::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.InterpTrackHelper") );
}

UInterpGroup* UInterpTrack::GetOwningGroup()
{
	UObject* Outer = NULL;
	for( Outer = GetOuter(); Outer && !Outer->IsA( UInterpGroup::StaticClass() ); Outer = Outer->GetOuter() );
	return CastChecked<UInterpGroup>(Outer);
}

void UInterpTrack::EnableTrack( bool bInEnable, bool bPropagateToSubTracks )
{
	bDisableTrack = !bInEnable;

	if( bPropagateToSubTracks )
	{
		for( int32 SubTrackIndex = 0; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
		{
			SubTracks[ SubTrackIndex ]->EnableTrack( bInEnable, bPropagateToSubTracks );
		}
	}
}

/*-----------------------------------------------------------------------------
 UInterpTrackInst
-----------------------------------------------------------------------------*/
UInterpTrackInst::UInterpTrackInst(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


AActor* UInterpTrackInst::GetGroupActor() const
{
	if (GetOuter())
	{
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>(GetOuter());
		return GrInst->GetGroupActor();
	}
	return nullptr;
}


UWorld* UInterpTrackInst::GetWorld() const
{
	// find an actor
	AActor const* Actor = GetGroupActor();
	if (Actor == nullptr)
	{
		// search the outer chain for an actor
		Actor = GetTypedOuter<AActor>();
	}

	return Actor ? Actor->GetWorld() : nullptr;
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstProperty
-----------------------------------------------------------------------------*/

UInterpTrackInstProperty::UInterpTrackInstProperty(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UInterpTrackInstProperty::SetupPropertyUpdateCallback(AActor* InActor, const FName& TrackPropertyName)
{
	// Try to find a custom callback to use when updating the property.  This callback will be called instead of UpdateComponents.
	void* PropertyScope = NULL;
	UObject* PropertyOuterObject = FMatineeUtils::FindObjectAndPropOffset(/*out*/ PropertyScope, /*out*/ InterpProperty, InActor, TrackPropertyName);
	if ((InterpProperty != NULL) && (PropertyOuterObject != NULL))
	{
		PropertyOuterObjectInst = PropertyOuterObject;
	}
}


void UInterpTrackInstProperty::CallPropertyUpdateCallback()
{
	// call post interp change if we have valid outer
	if(PropertyOuterObjectInst != NULL)
	{
		PropertyOuterObjectInst->PostInterpChange(InterpProperty);
	}
}

void UInterpTrackInstProperty::TermTrackInst(UInterpTrack* Track)
{
	// Clear references
	InterpProperty=NULL;
	PropertyOuterObjectInst=NULL;

	Super::TermTrackInst(Track);
}

/*-----------------------------------------------------------------------------
 UInterpTrackMoveAxis
-----------------------------------------------------------------------------*/
UInterpTrackMoveAxis::UInterpTrackMoveAxis(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurveTension = 0.0f;
	bSubTrackOnly = true;
	TrackTitle = TEXT("Move Axis Track");
}

int32 UInterpTrackMoveAxis::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	// We must be outered to a move track
	UInterpTrackMove* TrackParent = CastChecked<UInterpTrackMove>( GetOuter() );

	// Let the parent add keyframes to us based on its settings.
	int32 NewKeyIndex = TrackParent->AddChildKeyframe( this, Time, TrInst, InitInterpMode );

	return NewKeyIndex;
}

void UInterpTrackMoveAxis::UpdateKeyframe(int32 KeyIndex, class UInterpTrackInst* TrInst)
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	UInterpTrackMove* TrackParent = CastChecked<UInterpTrackMove>( GetOuter() );

	// Let our parent decide how to update us
	TrackParent->UpdateChildKeyframe( this, KeyIndex, TrInst );
}

int32 UInterpTrackMoveAxis::SetKeyframeTime( int32 KeyIndex, float NewKeyTime, bool bUpdateOrder )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
		return KeyIndex;

	int32 NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = FloatTrack.MovePoint(KeyIndex, NewKeyTime);
		int32 NewLookupKeyIndex = LookupTrack.MovePoint(KeyIndex, NewKeyTime);
		check( NewKeyIndex == NewLookupKeyIndex );
	}
	else
	{
		FloatTrack.Points[KeyIndex].InVal = NewKeyTime;
		LookupTrack.Points[KeyIndex].Time = NewKeyTime;
	}

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackMoveAxis::RemoveKeyframe( int32 KeyIndex )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	Super::RemoveKeyframe( KeyIndex );
	LookupTrack.Points.RemoveAt( KeyIndex );
}

int32 UInterpTrackMoveAxis::DuplicateKeyframe( int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );

	// Make sure the destination track is specified.
	UInterpTrackMoveAxis* DestTrack = this;
	if ( ToTrack )
	{
		DestTrack = CastChecked< UInterpTrackMoveAxis >( ToTrack );
	}

	int32 NewIndex = Super::DuplicateKeyframe( KeyIndex, NewKeyTime, DestTrack );
	FName& OldName = DestTrack->LookupTrack.Points[KeyIndex].GroupName;
	int32 NewLookupKeyIndex = DestTrack->LookupTrack.AddPoint( NewKeyTime, OldName );
	
	check( NewIndex == NewLookupKeyIndex );

	return NewIndex;
}

FName UInterpTrackMoveAxis::GetLookupKeyGroupName( int32 KeyIndex )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	check( KeyIndex < LookupTrack.Points.Num() );

	return LookupTrack.Points[KeyIndex].GroupName;
}

void UInterpTrackMoveAxis::SetLookupKeyGroupName( int32 KeyIndex, const FName& NewGroupName )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	check( KeyIndex < LookupTrack.Points.Num() );

	LookupTrack.Points[KeyIndex].GroupName = NewGroupName;
}

void UInterpTrackMoveAxis::ClearLookupKeyGroupName( int32 KeyIndex )
{
	SetLookupKeyGroupName( KeyIndex, NAME_None );
}

void UInterpTrackMoveAxis::GetKeyframeValue( UInterpTrackInst* TrInst, int32 KeyIndex, float& OutTime, float &OutValue, float* OutArriveTangent, float* OutLeaveTangent )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );

	bool bUseTrackKeyframe = true;

	// If there is a valid group name in the lookup track at thsi index, use the lookup track to get transform information
	const FName& GroupName = LookupTrack.Points[KeyIndex].GroupName;

	if( GroupName != NAME_None && TrInst )
	{
		// Lookup position from the lookup track.
		AActor* Actor = TrInst->GetGroupActor();
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
		AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
		UInterpGroupInst* LookupGroupInst = MatineeActor->FindFirstGroupInstByName(GroupName.ToString());

		if(Actor && LookupGroupInst && LookupGroupInst->GetGroupActor())
		{
			AActor* LookupActor = LookupGroupInst->GetGroupActor();

			// Slight hack here so that if we are trying to look at a Player variable, it looks at their Pawn.
			APlayerController* PC = Cast<APlayerController>(LookupActor);
			if(PC && PC->GetPawn())
			{
				LookupActor = PC->GetPawn();
			}

			// Find position
			if( MoveAxis == AXIS_TranslationX || MoveAxis == AXIS_TranslationY || MoveAxis == AXIS_TranslationZ )
			{
				FVector ActorLoc = LookupActor->GetActorLocation();
				OutValue = ActorLoc[MoveAxis];
			}
			else
			{
				OutValue = LookupActor->GetActorRotation().Euler()[MoveAxis - 3];
			}

			OutTime = LookupTrack.Points[ KeyIndex ].Time;
			// Find arrive and leave tangents.
			if(OutLeaveTangent != NULL || OutArriveTangent != NULL)
			{
				if(KeyIndex==0 || KeyIndex==(LookupTrack.Points.Num()-1))	// if we are an endpoint, set tangents to 0.
				{
					if(OutArriveTangent!=NULL)
					{
						FMemory::Memset( OutArriveTangent, 0, sizeof(float) );
					}

					if(OutLeaveTangent != NULL)
					{
						FMemory::Memset( OutLeaveTangent, 0, sizeof(float) );
					}
				}
				else
				{
					float PrevPos, NextPos;
					float PrevTime, NextTime;
					float AutoTangent;

					// Get previous and next positions for the tangents.
					GetKeyframeValue(TrInst, KeyIndex-1, PrevTime, PrevPos, NULL, NULL);
					GetKeyframeValue(TrInst, KeyIndex+1, NextTime, NextPos, NULL, NULL);

					// @todo: Should this setting be exposed in some way to the Lookup track?
					const bool bWantClamping = false;

					ComputeCurveTangent(
						PrevTime, PrevPos,
						OutTime, OutValue,
						NextTime, NextPos,
						CurveTension,				// Tension
						bWantClamping,
						AutoTangent );					// Out


					if(OutArriveTangent!=NULL)
					{
						*OutArriveTangent = AutoTangent;
					}

					if(OutLeaveTangent != NULL)
					{
						*OutLeaveTangent = AutoTangent;
					}
				}
			}

			bUseTrackKeyframe = false;
		}
	}

	if( bUseTrackKeyframe )
	{
		OutTime = FloatTrack.Points[KeyIndex].InVal;
		OutValue = FloatTrack.Points[KeyIndex].OutVal;

		if(OutArriveTangent != NULL)
		{
			*OutArriveTangent = FloatTrack.Points[KeyIndex].ArriveTangent;
		}

		if(OutLeaveTangent != NULL)
		{
			*OutLeaveTangent = FloatTrack.Points[KeyIndex].LeaveTangent;
		}
	}
}

float UInterpTrackMoveAxis::EvalValueAtTime( UInterpTrackInst* TrInst, float Time )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );

	const int32 NumPoints = FloatTrack.Points.Num();
	float KeyTime; // unused
	float OutValue;
	if( NumPoints == 0  )
	{
		return 0.0f;
	}
	else if( NumPoints < 2 || ( Time <= FloatTrack.Points[0].InVal ) )
	{
		GetKeyframeValue( TrInst, 0, KeyTime, OutValue, NULL, NULL );
		return OutValue;
	}
	else if( Time >= FloatTrack.Points[NumPoints-1].InVal )
	{
		GetKeyframeValue( TrInst, NumPoints - 1, KeyTime, OutValue, NULL, NULL );
		return OutValue;
	}
	else
	{
		for( int32 i=1; i<NumPoints; i++ )
		{	
			if( Time < FloatTrack.Points[i].InVal )
			{
				const float Diff = FloatTrack.Points[i].InVal - FloatTrack.Points[i-1].InVal;

				if( Diff > 0.f && FloatTrack.Points[i-1].InterpMode != CIM_Constant )
				{
					const float Alpha = (Time - FloatTrack.Points[i-1].InVal) / Diff;

					if( FloatTrack.Points[i-1].InterpMode == CIM_Linear )	// Linear interpolation
					{
						float PrevPos, CurrentPos;
						GetKeyframeValue( TrInst, i-1, KeyTime, PrevPos, NULL, NULL);
						GetKeyframeValue( TrInst, i, KeyTime, CurrentPos, NULL, NULL);

						OutValue =  FMath::Lerp( PrevPos, CurrentPos, Alpha );
						return OutValue;
					}
					else	//Cubic Interpolation
					{
						// Get keyframe positions and tangents.
						float CurrentPos, CurrentArriveTangent, PrevPos, PrevLeaveTangent;
						GetKeyframeValue( TrInst, i-1, KeyTime, PrevPos, NULL, &PrevLeaveTangent);
						GetKeyframeValue( TrInst, i, KeyTime, CurrentPos, &CurrentArriveTangent, NULL);

						OutValue = FMath::CubicInterp( PrevPos, PrevLeaveTangent * Diff, CurrentPos, CurrentArriveTangent * Diff, Alpha );
						return OutValue;
					}
				}
				else	// Constant Interpolation
				{
					GetKeyframeValue( TrInst, i-1, KeyTime, OutValue, NULL, NULL);
					return OutValue;
				}
			}
		}
	}

	// Shouldnt really reach here
	GetKeyframeValue( TrInst, NumPoints-1, KeyTime, OutValue, NULL, NULL);
	return OutValue;
}

/** 
 * Reduce Keys within Tolerance
 *
 * @param bIntervalStart	start of the key to reduce
 * @param bIntervalEnd		end of the key to reduce
 * @param Tolerance			tolerance
 */
void UInterpTrackMoveAxis::ReduceKeys( float IntervalStart, float IntervalEnd, float Tolerance )
{
	FInterpCurve<MatineeKeyReduction::SFLOAT>& OldCurve = (FInterpCurve<MatineeKeyReduction::SFLOAT>&) FloatTrack;

	// Create all the control points. They are six-dimensional, since
	// the Euler rotation key and the position key times must match.
	MatineeKeyReduction::MCurve<MatineeKeyReduction::SFLOAT, 1> Curve;
	Curve.RelativeTolerance = Tolerance / 100.0f;
	Curve.IntervalStart = IntervalStart - 0.0005f;  // 0.5ms pad to allow for floating-point precision.
	Curve.IntervalEnd = IntervalEnd + 0.0005f;  // 0.5ms pad to allow for floating-point precision.

	Curve.CreateControlPoints(OldCurve, 0);
	if (Curve.HasControlPoints())
	{
		Curve.FillControlPoints(OldCurve, 1, 0);

		// Reduce the curve.
		Curve.Reduce();

		// Copy the reduced keys over to the new curve.
		Curve.CopyCurvePoints(OldCurve.Points, 1, 0);
	}

	// Refer the look-up track to nothing.
	LookupTrack.Points.Empty();
	FName DefaultName(NAME_None);
	uint32 PointCount = FloatTrack.Points.Num();
	for (uint32 Index = 0; Index < PointCount; ++Index)
	{
		LookupTrack.AddPoint(FloatTrack.Points[Index].InVal, DefaultName );
	}
}

/*----------------------------------------------------------------------------
 UInterpTrackMove
-----------------------------------------------------------------------------*/
UInterpTrackMove::UInterpTrackMove(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackInstClass = UInterpTrackInstMove::StaticClass();
	bOnePerGroup = true;
	TrackTitle = TEXT("Movement");
	LinCurveTension = 0.0f;
	AngCurveTension = 0.0f;
	RotMode = IMR_Keyframed;
	bShowTranslationOnCurveEd = true;
	bShowRotationOnCurveEd = false;
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Move.MAT_Groups_Move"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITORONLY_DATA
	int32 NewArrayIndex0 = SupportedSubTracks.Add(FSupportedSubTrackInfo());
	SupportedSubTracks[NewArrayIndex0].SupportedClass = UInterpTrackMoveAxis::StaticClass();
	SupportedSubTracks[NewArrayIndex0].SubTrackName = TEXT("X");
	SupportedSubTracks[NewArrayIndex0].GroupIndex = 0;

	int32 NewArrayIndex1 = SupportedSubTracks.Add(FSupportedSubTrackInfo());
	SupportedSubTracks[NewArrayIndex1].SupportedClass = UInterpTrackMoveAxis::StaticClass();
	SupportedSubTracks[NewArrayIndex1].SubTrackName = TEXT("Y");
	SupportedSubTracks[NewArrayIndex1].GroupIndex = 0;

	int32 NewArrayIndex2 = SupportedSubTracks.Add(FSupportedSubTrackInfo());
	SupportedSubTracks[NewArrayIndex2].SupportedClass = UInterpTrackMoveAxis::StaticClass();
	SupportedSubTracks[NewArrayIndex2].SubTrackName = TEXT("Z");
	SupportedSubTracks[NewArrayIndex2].GroupIndex = 0;

	int32 NewArrayIndex3 = SupportedSubTracks.Add(FSupportedSubTrackInfo());
	SupportedSubTracks[NewArrayIndex3].SupportedClass = UInterpTrackMoveAxis::StaticClass();
	SupportedSubTracks[NewArrayIndex3].SubTrackName = TEXT("X");
	SupportedSubTracks[NewArrayIndex3].GroupIndex = 1 ;

	int32 NewArrayIndex4 = SupportedSubTracks.Add(FSupportedSubTrackInfo());
	SupportedSubTracks[NewArrayIndex4].SupportedClass = UInterpTrackMoveAxis::StaticClass();
	SupportedSubTracks[NewArrayIndex4].SubTrackName = TEXT("Y");
	SupportedSubTracks[NewArrayIndex4].GroupIndex = 1 ;

	int32 NewArrayIndex5 = SupportedSubTracks.Add(FSupportedSubTrackInfo());
	SupportedSubTracks[NewArrayIndex5].SupportedClass = UInterpTrackMoveAxis::StaticClass();
	SupportedSubTracks[NewArrayIndex5].SubTrackName = TEXT("Z");
	SupportedSubTracks[NewArrayIndex5].GroupIndex = 1 ;
#endif // WITH_EDITORONLY_DATA
}

void UInterpTrackMove::SetTrackToSensibleDefault()
{
}

void UInterpTrackMove::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	for (int32 PointIdx = 0; PointIdx < PosTrack.Points.Num(); ++PointIdx)
	{
		PosTrack.Points[PointIdx].OutVal+= InOffset;
	}

	if (SubTracks.Num() != 0)
	{
		UInterpTrackMoveAxis* PosXTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks[AXIS_TranslationX] );
		for (int32 PointIdx = 0; PointIdx < PosXTrack->FloatTrack.Points.Num(); ++PointIdx)
		{
			PosXTrack->FloatTrack.Points[PointIdx].OutVal+= InOffset.X;
		}
		
		UInterpTrackMoveAxis* PosYTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks[AXIS_TranslationY] );
		for (int32 PointIdx = 0; PointIdx < PosYTrack->FloatTrack.Points.Num(); ++PointIdx)
		{
			PosYTrack->FloatTrack.Points[PointIdx].OutVal+= InOffset.Y;
		}
		
		UInterpTrackMoveAxis* PosZTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks[AXIS_TranslationZ] );
		for (int32 PointIdx = 0; PointIdx < PosZTrack->FloatTrack.Points.Num(); ++PointIdx)
		{
			PosZTrack->FloatTrack.Points[PointIdx].OutVal+= InOffset.Z;
		}
	}
}

int32 UInterpTrackMove::GetNumKeyframes() const
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	return PosTrack.Points.Num();
}

void UInterpTrackMove::GetTimeRange(float& StartTime, float& EndTime) const
{
	// If there are no subtracks, this is an unsplit movemnt track.  Get timerange information directly from this track.
	if( SubTracks.Num() == 0 )
	{
		check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

		if(PosTrack.Points.Num() == 0)
		{
			StartTime = 0.f;
			EndTime = 0.f;
		}
		else
		{
			// PosTrack and EulerTrack should have the same number of keys at the same times.
			check( (PosTrack.Points[0].InVal - EulerTrack.Points[0].InVal) < KINDA_SMALL_NUMBER );
			check( (PosTrack.Points[PosTrack.Points.Num()-1].InVal - EulerTrack.Points[EulerTrack.Points.Num()-1].InVal) < KINDA_SMALL_NUMBER );

			StartTime = PosTrack.Points[0].InVal;
			EndTime = PosTrack.Points[ PosTrack.Points.Num()-1 ].InVal;
		}
	}
	else
	{
		// There are subtracks in this track. Find the min and max time by looking at all our subtracks.
		float SubStartTime = 0.0f, SubEndTime = 0.0f;
		SubTracks[ 0 ]->GetTimeRange( StartTime, EndTime );
		for( int32 SubTrackIndex = 1; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
		{
			SubTracks[SubTrackIndex]->GetTimeRange( SubStartTime, SubEndTime );
			StartTime = FMath::Min( SubStartTime, StartTime );
			EndTime = FMath::Max( SubEndTime, EndTime );
		}
	}
}


float UInterpTrackMove::GetTrackEndTime() const
{
	float EndTime = 0.0f;

	if( PosTrack.Points.Num() )
	{
		check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
		EndTime = PosTrack.Points[PosTrack.Points.Num() - 1].InVal;
	}

	return EndTime;
}

float UInterpTrackMove::GetKeyframeTime(int32 KeyIndex) const
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
		return 0.f;

	check( (PosTrack.Points[KeyIndex].InVal - EulerTrack.Points[KeyIndex].InVal) < KINDA_SMALL_NUMBER );

	return PosTrack.Points[KeyIndex].InVal;
}

int32 UInterpTrackMove::GetKeyframeIndex( float KeyTime ) const
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	int32 RetIndex = INDEX_NONE;
	if( PosTrack.Points.Num() > 0 )
	{
		float CurTime = PosTrack.Points[0].InVal;
		// Loop through every keyframe until we find a keyframe with the passed in time.
		// Stop searching once all the keyframes left to search have larger times than the passed in time.
		for( int32 KeyIndex = 0; KeyIndex < PosTrack.Points.Num() && CurTime <= KeyTime; ++KeyIndex )
		{
			if( KeyTime == PosTrack.Points[KeyIndex].InVal )
			{
				check( (PosTrack.Points[KeyIndex].InVal - EulerTrack.Points[KeyIndex].InVal) < KINDA_SMALL_NUMBER );
				RetIndex = KeyIndex;
				break;
			}
			CurTime = PosTrack.Points[KeyIndex].InVal;	
		}
	}
	return RetIndex;
}

int32 UInterpTrackMove::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	// If there are no subtracks, this track is not split, add a keyframe directly to this track.
	if( SubTracks.Num() == 0 )
	{
		check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

		AActor* Actor = TrInst->GetGroupActor();
		if(!Actor)
		{
			return INDEX_NONE;
		}

		int32 NewKeyIndex = PosTrack.AddPoint( Time, FVector::ZeroVector);
		PosTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

		int32 NewRotKeyIndex = EulerTrack.AddPoint( Time, FVector::ZeroVector );
		EulerTrack.Points[NewRotKeyIndex].InterpMode = InitInterpMode;

		FName DefaultName(NAME_None);
		int32 NewLookupKeyIndex = LookupTrack.AddPoint(Time, DefaultName);

		check((NewKeyIndex == NewRotKeyIndex) && (NewKeyIndex == NewLookupKeyIndex));

		UpdateKeyframe(NewKeyIndex, TrInst);

		PosTrack.AutoSetTangents(LinCurveTension);
		EulerTrack.AutoSetTangents(AngCurveTension);

		return NewKeyIndex;
	}
	else
	{
		// This track has subtracks, add keyframe to each child.  
		AActor* Actor = TrInst->GetGroupActor();
		int32 NewKeyIndex = INDEX_NONE;
		if( Actor )
		{
			for( int32 SubTrackIndex = 0; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
			{
				int32 ReturnIndex = AddChildKeyframe( SubTracks[SubTrackIndex], Time, TrInst, InitInterpMode );
				check( ReturnIndex != INDEX_NONE );

				// Since each child track may add a keyframe at a different index, compute the min index where a keyframe was added.  
				// If a keyframe was added at index 0, we need  to update our initial transform.  The calling function checks for that.
				if( NewKeyIndex > ReturnIndex || NewKeyIndex == INDEX_NONE )
				{
					NewKeyIndex = ReturnIndex;
				}
			}
		}

		return NewKeyIndex;
	}
}

bool UInterpTrackMove::CanAddKeyframe( UInterpTrackInst* TrInst )
{
	return TrInst->GetGroupActor() != NULL;
}

void UInterpTrackMove::UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= EulerTrack.Points.Num() )
	{
		return;
	}

	AActor* Actor = TrInst->GetGroupActor();
	if(Actor == NULL || Actor->GetRootComponent() == NULL)
	{
		return;
	}


	// Don't want to record keyframes if track disabled.
	if(bDisableMovement)
	{
		return;
	}

	UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>(TrInst);


	/* @todo UE4 do we still need this code? 
	if ( APawn * Pawn = Actor->GetAPawn() )
	{
		if ( Pawn->CapsuleComponent )
		{
			NewPos.Z -= Pawn->CapsuleComponent->GetScaledCapsuleHalfHeight();
		}
	}
	*/

	FVector RelativeSpaceEuler = Actor->GetRootComponent()->RelativeRotation.Euler();

	PosTrack.Points[KeyIndex].OutVal = Actor->GetRootComponent()->RelativeLocation;
	
	// peek at an adjacent key frame to attempt to keep rotation continuous
	if( EulerTrack.Points.Num() > 1 )
	{
		int32 AdjacentKeyIndex = ( KeyIndex > 0 ) ? ( KeyIndex - 1 ) : ( KeyIndex + 1 );
		const FVector AdjacentEuler = EulerTrack.Points[ AdjacentKeyIndex ].OutVal;

		// Try to minimize differences in curves
		const FVector EulerDiff = RelativeSpaceEuler - AdjacentEuler;
		if( EulerDiff.X > 180.0f )
		{
			RelativeSpaceEuler.X -= 360.0f;
		}
		else if( EulerDiff.X < -180.0f )
		{
			RelativeSpaceEuler.X += 360.0f;
		}
		if( EulerDiff.Y > 180.0f )
		{
			RelativeSpaceEuler.Y -= 360.0f;
		}
		else if( EulerDiff.Y < -180.0f )
		{
			RelativeSpaceEuler.Y += 360.0f;
		}
		if( EulerDiff.Z > 180.0f )
		{
			RelativeSpaceEuler.Z -= 360.0f;
		}
		else if( EulerDiff.Z < -180.0f )
		{
			RelativeSpaceEuler.Z += 360.0f;
		}
	}

	EulerTrack.Points[KeyIndex].OutVal =  RelativeSpaceEuler;

	// Update the tangent vectors for the changed point, and its neighbours.
	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);
}

int32 UInterpTrackMove::AddChildKeyframe( UInterpTrack* ChildTrack, float Time, UInterpTrackInst* ChildTrackInst, EInterpCurveMode InitInterpMode )
{
	int32 NewKeyIndex = INDEX_NONE;
	UInterpTrackMoveAxis* ChildMoveTrack = CastChecked<UInterpTrackMoveAxis>( ChildTrack );
	AActor* Actor = ChildTrackInst->GetGroupActor();
	if( Actor )
	{
		// Add a new key to our track.
		NewKeyIndex = ChildMoveTrack->FloatTrack.AddPoint( Time, 0.0f );
		ChildMoveTrack->FloatTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

		FName DefaultName(NAME_None);
		int32 NewLookupKeyIndex = ChildMoveTrack->LookupTrack.AddPoint(Time, DefaultName);
	
		check( NewKeyIndex == NewLookupKeyIndex );

		UpdateChildKeyframe( ChildTrack, NewKeyIndex, ChildTrackInst );
	}

	return NewKeyIndex;
}

bool UInterpTrackMove::CanAddChildKeyframe( UInterpTrackInst* ChildTrackInst )
{
	return ChildTrackInst->GetGroupActor() != NULL;
}

void UInterpTrackMove::UpdateChildKeyframe( UInterpTrack* ChildTrack, int32 KeyIndex, UInterpTrackInst* TrackInst )
{
	check(ChildTrack);

	UInterpTrackMoveAxis* ChildMoveTrack = CastChecked<UInterpTrackMoveAxis>( ChildTrack );
	const uint8 MoveAxis = ChildMoveTrack->MoveAxis;

	FInterpCurveFloat& FloatTrack = ChildMoveTrack->FloatTrack;
	if( KeyIndex < 0 || KeyIndex >= ChildMoveTrack->FloatTrack.Points.Num() )
	{
		return;
	}

	UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>( TrackInst );
	AActor* Actor = MoveTrackInst->GetGroupActor();
	if( Actor == NULL || Actor->GetRootComponent() == NULL )
	{
		return;
	}

	if( bDisableMovement )
	{
		return;
	}

	// New position of the actor
	FVector NewPos = Actor->GetRootComponent()->RelativeLocation;
	// New rotation of the actor
	FVector NewRot =  Actor->GetRootComponent()->RelativeRotation.Euler();

	// Now determine what value should be updated in the float track.
	switch( MoveAxis )
	{
	case AXIS_TranslationX:
		FloatTrack.Points[KeyIndex].OutVal = NewPos.X;
		break;
	case AXIS_TranslationY:
		FloatTrack.Points[KeyIndex].OutVal = NewPos.Y;
		break;
	case AXIS_TranslationZ:
		FloatTrack.Points[KeyIndex].OutVal = NewPos.Z;
		break;
	case AXIS_RotationX:
		FloatTrack.Points[KeyIndex].OutVal = NewRot.X;
		break;
	case AXIS_RotationY:
		FloatTrack.Points[KeyIndex].OutVal = NewRot.Y;
		break;
	case AXIS_RotationZ:
		FloatTrack.Points[KeyIndex].OutVal = NewRot.Z;
		break;
	default:
		checkf( false, TEXT("Invalid Move axis") );
	}

	// Update the tangent vectors for the changed point, and its neighbors.
	FloatTrack.AutoSetTangents( ChildMoveTrack->CurveTension );
}

int32 UInterpTrackMove::SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
		return KeyIndex;

	int32 NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = PosTrack.MovePoint(KeyIndex, NewKeyTime);
		int32 NewEulerKeyIndex = EulerTrack.MovePoint(KeyIndex, NewKeyTime);
		int32 NewLookupKeyIndex = LookupTrack.MovePoint(KeyIndex, NewKeyTime);
		check( (NewKeyIndex == NewEulerKeyIndex) && (NewKeyIndex == NewLookupKeyIndex) );
	}
	else
	{
		PosTrack.Points[KeyIndex].InVal = NewKeyTime;
		EulerTrack.Points[KeyIndex].InVal = NewKeyTime;
		LookupTrack.Points[KeyIndex].Time = NewKeyTime;
	}

	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);

	return NewKeyIndex;
}

void UInterpTrackMove::RemoveKeyframe(int32 KeyIndex)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
		return;

	PosTrack.Points.RemoveAt(KeyIndex);
	EulerTrack.Points.RemoveAt(KeyIndex);
	LookupTrack.Points.RemoveAt(KeyIndex);

	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);
}

int32 UInterpTrackMove::DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
	{
		return INDEX_NONE;
	}

	// Make sure the destination track is specified.
	UInterpTrackMove* DestTrack = this;
	if ( ToTrack )
	{
		DestTrack = CastChecked< UInterpTrackMove >( ToTrack );
	}

	FInterpCurvePoint<FVector> PosPoint = PosTrack.Points[KeyIndex];
	int32 NewPosIndex = DestTrack->PosTrack.AddPoint(NewKeyTime, FVector::ZeroVector);
	DestTrack->PosTrack.Points[NewPosIndex] = PosPoint; // Copy properties from source key.
	DestTrack->PosTrack.Points[NewPosIndex].InVal = NewKeyTime;

	FInterpCurvePoint<FVector> EulerPoint = EulerTrack.Points[KeyIndex];
	int32 NewEulerIndex = DestTrack->EulerTrack.AddPoint(NewKeyTime, FVector::ZeroVector);
	DestTrack->EulerTrack.Points[NewEulerIndex] = EulerPoint;
	DestTrack->EulerTrack.Points[NewEulerIndex].InVal = NewKeyTime;

	FName OldName = LookupTrack.Points[KeyIndex].GroupName;
	int32 NewLookupKeyIndex = DestTrack->LookupTrack.AddPoint(NewKeyTime, OldName);

	check((NewPosIndex == NewEulerIndex) && (NewPosIndex == NewLookupKeyIndex));

	DestTrack->PosTrack.AutoSetTangents(LinCurveTension);
	DestTrack->EulerTrack.AutoSetTangents(AngCurveTension);

	return NewPosIndex;
}

bool UInterpTrackMove::GetClosestSnapPosition(float InPosition, TArray<int32> &IgnoreKeys, float& OutPosition)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

	if(PosTrack.Points.Num() == 0)
	{
		return false;
	}

	bool bFoundSnap = false;
	float ClosestSnap = 0.f;
	float ClosestDist = BIG_NUMBER;
	for(int32 i=0; i<PosTrack.Points.Num(); i++)
	{
		if(!IgnoreKeys.Contains(i))
		{
			float Dist = FMath::Abs( PosTrack.Points[i].InVal - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = PosTrack.Points[i].InVal;
				ClosestDist = Dist;
				bFoundSnap = true;
			}
		}
	}

	OutPosition = ClosestSnap;
	return bFoundSnap;
}


void UInterpTrackMove::ConditionalPreviewUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst)
{
	// Is the track enabled?
	bool bIsTrackEnabled = !IsDisabled();
	UInterpGroupInst* GrInst = Cast<UInterpGroupInst>( TrInst->GetOuter() );
	if( GrInst != NULL )
	{
		AMatineeActor* MatineeActor = Cast<AMatineeActor>( GrInst->GetOuter() );
		if( MatineeActor != NULL )
		{
			if( (ActiveCondition == ETAC_GoreEnabled && !MatineeActor->bShouldShowGore) ||
				(ActiveCondition == ETAC_GoreDisabled && MatineeActor->bShouldShowGore) )
			{
				bIsTrackEnabled = false;
			}
		}
	}

	float CurTime = NewPosition;

	if( !bIsTrackEnabled )
	{
		CurTime = 0.0f;
	}

	PreviewUpdateTrack(CurTime, TrInst);
}

void UInterpTrackMove::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	// Don't try and update a mover if its simulating physics
	if(Actor == NULL || Actor->GetRootComponent() == NULL || Actor->GetRootComponent()->IsSimulatingPhysics())
	{
		return;
	}

	// save previous location to calculate velocity
	FVector PrevLocation = Actor->GetRootComponent()->GetComponentLocation();

	if(bDisableMovement)
	{
		NewPosition = 0.0f;
	}

	// Do nothing if no data on this track.
	if( SubTracks.Num() == 0 && EulerTrack.Points.Num() == 0)
	{
		return;
	}

	FVector RelativeSpacePos;
	FRotator RelativeSpaceRot;
	GetKeyTransformAtTime( TrInst, NewPosition, RelativeSpacePos, RelativeSpaceRot );

	// If ignoring rotation, just set translation
	if(RotMode == IMR_Ignore)
	{
		Actor->GetRootComponent()->SetRelativeLocation(RelativeSpacePos);
	}
	// If using 'look at' rotation, compute that and apply in world space
	else if(RotMode == IMR_LookAtGroup)
	{		
		Actor->GetRootComponent()->SetRelativeLocation(RelativeSpacePos);
		FRotator WorldLookAtRot = GetLookAtRotation(TrInst);
		Actor->GetRootComponent()->SetWorldRotation(WorldLookAtRot);
	}
	// Setting relative rotation and translation from track
	else
	{
		Actor->GetRootComponent()->SetRelativeLocationAndRotation(RelativeSpacePos, RelativeSpaceRot);
	}

	FVector NewLocation = Actor->GetRootComponent()->GetComponentLocation();
	// if FApp::GetDeltaTime() == 0.f, I'd think that's paused, then we won't need to update Velocity
	if ( FApp::GetDeltaTime() > 0.f ) 
	{
		// we're not using PreviPosition to NewPosition because MatineeActor itself can have different playrate
		// so we can't guarantee that's the time it took to get there. 
		// this should approximately safe in replication as well
		FVector ComponentVelocity = (NewLocation-PrevLocation)/FApp::GetDeltaTime();
		Actor->GetRootComponent()->ComponentVelocity = ComponentVelocity;
	}
}



void UInterpTrackMove::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );

	bool bJump = !(MatineeActor->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}

#if WITH_EDITOR
void UInterpTrackMove::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UInterpTrackMove::PostEditImport()
{
	Super::PostEditImport();

	// Make sure that our array sizes match up.  If they don't, it is due to default struct keys not being exported. (Only happens for keys at Time=0).
	// @todo: This is a hack and can be removed once the struct properties are exported to text correctly for arrays of structs.
	if(PosTrack.Points.Num() > LookupTrack.Points.Num())	// Lookup track elements weren't imported.
	{
		int32 Count = PosTrack.Points.Num()-LookupTrack.Points.Num();
		FName DefaultName(NAME_None);
		for(int32 PointIdx=0; PointIdx<Count; PointIdx++)
		{
			LookupTrack.AddPoint(PosTrack.Points[PointIdx].InVal, DefaultName);
		}

		for(int32 PointIdx=Count; PointIdx<PosTrack.Points.Num(); PointIdx++)
		{
			LookupTrack.Points[PointIdx].Time = PosTrack.Points[PointIdx].InVal;
		}
	}
	else if(PosTrack.Points.Num()==EulerTrack.Points.Num() && PosTrack.Points.Num() < LookupTrack.Points.Num())	// Pos/euler track elements weren't imported.
	{
		int32 Count = LookupTrack.Points.Num()-PosTrack.Points.Num();

		for(int32 PointIdx=0; PointIdx<Count; PointIdx++)
		{
			PosTrack.AddPoint( LookupTrack.Points[PointIdx].Time, FVector::ZeroVector );
			EulerTrack.AddPoint( LookupTrack.Points[PointIdx].Time, FVector::ZeroVector );
		}

		for(int32 PointIdx=Count; PointIdx<LookupTrack.Points.Num(); PointIdx++)
		{
			PosTrack.Points[PointIdx].InVal = LookupTrack.Points[PointIdx].Time;
			EulerTrack.Points[PointIdx].InVal = LookupTrack.Points[PointIdx].Time;
		}

		PosTrack.AutoSetTangents(LinCurveTension);
		EulerTrack.AutoSetTangents(AngCurveTension);
	}

	check((PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()));
}
#endif


FName UInterpTrackMove::GetLookupKeyGroupName(int32 KeyIndex)
{
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	check( KeyIndex < LookupTrack.Points.Num() );

	return LookupTrack.Points[KeyIndex].GroupName;
}


void UInterpTrackMove::SetLookupKeyGroupName(int32 KeyIndex, const FName &NewGroupName)
{
#if PLATFORM_HTML5
	if( KeyIndex >= LookupTrack.Points.Num() )
	{	// trying to hunt this down...
		// AFAICT, this is only happending on HTML5 -- this might be an enscripten/LLVM corruption issue
		EM_ASM_({
			console.log("ERROR: SetLookupKeyGroupName: index[" + $0 + "] num[" + $1 + "]");
			stackTrace();
		}, KeyIndex, LookupTrack.Points.Num());
		return;
	}
#endif
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	check( KeyIndex < LookupTrack.Points.Num() );

	LookupTrack.Points[KeyIndex].GroupName = NewGroupName;
}

void UInterpTrackMove::ClearLookupKeyGroupName(int32 KeyIndex)
{
	FName DefaultName(NAME_None);
	SetLookupKeyGroupName(KeyIndex, DefaultName);
}


void UInterpTrackMove::GetKeyframePosition(UInterpTrackInst* TrInst, int32 KeyIndex, float& OutTime, FVector &OutPos, FVector *OutArriveTangent, FVector *OutLeaveTangent)
{
	bool bUsePosTrack = true;

	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	check( KeyIndex < LookupTrack.Points.Num() );

	// See if this key is trying to get its position from another group.
	FName GroupName = LookupTrack.Points[KeyIndex].GroupName;
	if(GroupName != NAME_None && TrInst)
	{
		// Lookup position from the lookup track.
		AActor* Actor = TrInst->GetGroupActor();
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
		AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
		UInterpGroupInst* LookupGroupInst = MatineeActor->FindFirstGroupInstByName(GroupName.ToString());

		if(Actor && LookupGroupInst && LookupGroupInst->GetGroupActor())
		{
			AActor* LookupActor = LookupGroupInst->GetGroupActor();

			// Slight hack here so that if we are trying to look at a Player variable, it looks at their Pawn.
			APlayerController* PC = Cast<APlayerController>(LookupActor);
			if(PC && PC->GetPawn())
			{
				LookupActor = PC->GetPawn();
			}

			// Find position
			OutPos = LookupActor->GetActorLocation();
			OutTime = LookupTrack.Points[ KeyIndex ].Time;

			// Find arrive and leave tangents.
			if(OutLeaveTangent != NULL || OutArriveTangent != NULL)
			{
				if(KeyIndex==0 || KeyIndex==(LookupTrack.Points.Num()-1))	// if we are an endpoint, set tangents to 0.
				{
					if(OutArriveTangent!=NULL)
					{
						FMemory::Memset( OutArriveTangent, 0, sizeof(FVector) );
					}

					if(OutLeaveTangent != NULL)
					{
						FMemory::Memset( OutLeaveTangent, 0, sizeof(FVector) );
					}
				}
				else
				{
					FVector PrevPos, NextPos;
					float PrevTime, NextTime;
					FVector AutoTangent;

					// Get previous and next positions for the tangents.
					GetKeyframePosition(TrInst, KeyIndex-1, PrevTime, PrevPos, NULL, NULL);
					GetKeyframePosition(TrInst, KeyIndex+1, NextTime, NextPos, NULL, NULL);


					// @todo: Should this setting be exposed in some way to the Lookup track?
					const bool bWantClamping = false;

					ComputeCurveTangent(
						PrevTime, PrevPos,
						OutTime, OutPos,
						NextTime, NextPos,
						LinCurveTension,				// Tension
						bWantClamping,
						AutoTangent );					// Out


					if(OutArriveTangent!=NULL)
					{
						*OutArriveTangent = AutoTangent;
					}

					if(OutLeaveTangent != NULL)
					{
						*OutLeaveTangent = AutoTangent;
					}
				}
			}

			bUsePosTrack = false;
		}
	}

	// We couldn't lookup a position from another group, so use the value stored in the pos track.
	if(bUsePosTrack)
	{
		OutTime = PosTrack.Points[KeyIndex].InVal;
		OutPos = PosTrack.Points[KeyIndex].OutVal;

		if(OutArriveTangent != NULL)
		{
			*OutArriveTangent = PosTrack.Points[KeyIndex].ArriveTangent;
		}

		if(OutLeaveTangent != NULL)
		{
			*OutLeaveTangent = PosTrack.Points[KeyIndex].LeaveTangent;
		}
	}
}



void UInterpTrackMove::GetKeyframeRotation(UInterpTrackInst* TrInst, int32 KeyIndex, float& OutTime, FVector &OutRot, FVector *OutArriveTangent, FVector *OutLeaveTangent)
{
	bool bUseRotTrack = true;

	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );
	check( KeyIndex < LookupTrack.Points.Num() );

	// See if this key is trying to get its rotation from another group.
	FName GroupName = LookupTrack.Points[KeyIndex].GroupName;
	if(GroupName != NAME_None && TrInst)
	{
		// Lookup rotation from the lookup track.
		AActor* Actor = TrInst->GetGroupActor();
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
		AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
		UInterpGroupInst* LookupGroupInst = MatineeActor->FindFirstGroupInstByName(GroupName.ToString());

		if(Actor && LookupGroupInst && LookupGroupInst->GetGroupActor())
		{
			AActor* LookupActor = LookupGroupInst->GetGroupActor();

			// Slight hack here so that if we are trying to look at a Player variable, it looks at their Pawn.
			APlayerController* PC = Cast<APlayerController>(LookupActor);
			if(PC && PC->GetPawn())
			{
				LookupActor = PC->GetPawn();
			}

			// Find rotation
			OutRot = LookupActor->GetActorRotation().Euler();
			OutTime = LookupTrack.Points[ KeyIndex ].Time;

			// Find arrive and leave tangents.
			if(OutLeaveTangent != NULL || OutArriveTangent != NULL)
			{
				if(KeyIndex==0 || KeyIndex==(LookupTrack.Points.Num()-1))	// if we are an endpoint, set tangents to 0.
				{
					if(OutArriveTangent!=NULL)
					{
						FMemory::Memset( OutArriveTangent, 0, sizeof(FVector) );
					}

					if(OutLeaveTangent != NULL)
					{
						FMemory::Memset( OutLeaveTangent, 0, sizeof(FVector) );
					}
				}
				else
				{
					FVector PrevRot, NextRot;
					float PrevTime, NextTime;
					FVector AutoTangent;

					// Get previous and next positions for the tangents.
					GetKeyframeRotation(TrInst, KeyIndex-1, PrevTime, PrevRot, NULL, NULL);
					GetKeyframeRotation(TrInst, KeyIndex+1, NextTime, NextRot, NULL, NULL);

					// @todo: Should this setting be exposed in some way to the Lookup track?
					const bool bWantClamping = false;

					ComputeCurveTangent(
						PrevTime, PrevRot,
						OutTime, OutRot,
						NextTime, NextRot,
						LinCurveTension,				// Tension
						bWantClamping,
						AutoTangent );					// Out


					if(OutArriveTangent!=NULL)
					{
						*OutArriveTangent = AutoTangent;
					}

					if(OutLeaveTangent != NULL)
					{
						*OutLeaveTangent = AutoTangent;
					}
				}
			}

			bUseRotTrack = false;
		}
	}

	// We couldn't lookup a position from another group, so use the value stored in the pos track.
	if(bUseRotTrack)
	{
		OutTime = EulerTrack.Points[KeyIndex].InVal;
		OutRot = EulerTrack.Points[KeyIndex].OutVal;

		if(OutArriveTangent != NULL)
		{
			*OutArriveTangent = EulerTrack.Points[KeyIndex].ArriveTangent;
		}

		if(OutLeaveTangent != NULL)
		{
			*OutLeaveTangent = EulerTrack.Points[KeyIndex].LeaveTangent;
		}
	}
}



FVector UInterpTrackMove::EvalPositionAtTime(UInterpTrackInst* TrInst, float Time)
{
	// If there are no subtracks, get the position directly from this track.
	if( SubTracks.Num() == 0 )
	{
		FVector OutPos;
		float KeyTime;	// Not used here
		const int32 NumPoints = PosTrack.Points.Num();

		// If no point in curve, return the Default value we passed in.
		if( NumPoints == 0 )
		{
			return FVector::ZeroVector;
		}

		// If only one point, or before the first point in the curve, return the first points value.
		if( NumPoints < 2 || (Time <= PosTrack.Points[0].InVal) )
		{
			GetKeyframePosition(TrInst, 0, KeyTime, OutPos, NULL, NULL);
			return OutPos;
		}

		// If beyond the last point in the curve, return its value.
		if( Time >= PosTrack.Points[NumPoints-1].InVal )
		{
			GetKeyframePosition(TrInst, NumPoints - 1, KeyTime, OutPos, NULL, NULL);
			return OutPos;
		}

		// Somewhere with curve range - linear search to find value.
		for( int32 i=1; i<NumPoints; i++ )
		{	
			if( Time < PosTrack.Points[i].InVal )
			{
				const float Diff = PosTrack.Points[i].InVal - PosTrack.Points[i-1].InVal;

				if( Diff > 0.f && PosTrack.Points[i-1].InterpMode != CIM_Constant )
				{
					const float Alpha = (Time - PosTrack.Points[i-1].InVal) / Diff;

					if( PosTrack.Points[i-1].InterpMode == CIM_Linear )	// Linear interpolation
					{
						FVector PrevPos, CurrentPos;
						GetKeyframePosition(TrInst, i-1, KeyTime, PrevPos, NULL, NULL);
						GetKeyframePosition(TrInst, i, KeyTime, CurrentPos, NULL, NULL);

						OutPos =  FMath::Lerp( PrevPos, CurrentPos, Alpha );
						return OutPos;
					}
					else	//Cubic Interpolation
					{
						// Get keyframe positions and tangents.
						FVector CurrentPos, CurrentArriveTangent, PrevPos, PrevLeaveTangent;
						GetKeyframePosition(TrInst, i-1, KeyTime, PrevPos, NULL, &PrevLeaveTangent);
						GetKeyframePosition(TrInst, i, KeyTime, CurrentPos, &CurrentArriveTangent, NULL);

						OutPos = FMath::CubicInterp( PrevPos, PrevLeaveTangent * Diff, CurrentPos, CurrentArriveTangent * Diff, Alpha );
						return OutPos;
					}
				}
				else	// Constant Interpolation
				{
					GetKeyframePosition(TrInst, i-1, KeyTime, OutPos, NULL, NULL);
					return OutPos;
				}
			}
		}

		// Shouldn't really reach here.
		GetKeyframePosition(TrInst, NumPoints-1, KeyTime, OutPos, NULL, NULL);
		return OutPos;
	}
	else
	{
		// This track has subtracks, get position information from subtracks
		FVector OutPos;

		UInterpTrackMoveAxis* PosXTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks[AXIS_TranslationX] );
		UInterpTrackMoveAxis* PosYTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks[AXIS_TranslationY] );
		UInterpTrackMoveAxis* PosZTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks[AXIS_TranslationZ] );

		OutPos.X = PosXTrack->EvalValueAtTime( TrInst, Time );
		OutPos.Y = PosYTrack->EvalValueAtTime( TrInst, Time );
		OutPos.Z = PosZTrack->EvalValueAtTime( TrInst, Time );

		return OutPos;
	}
}



FVector UInterpTrackMove::EvalRotationAtTime(UInterpTrackInst* TrInst, float Time)
{
	// IF the track has no subtracks, get rotation information directly from this track
	if( SubTracks.Num() == 0 )
	{
		FVector OutRot;
		float KeyTime;	// Not used here
		const int32 NumPoints = EulerTrack.Points.Num();

		// If no point in curve, return the Default value we passed in.
		if( NumPoints == 0 )
		{
			return FVector::ZeroVector;
		}

		// If only one point, or before the first point in the curve, return the first points value.
		if( NumPoints < 2 || (Time <= EulerTrack.Points[0].InVal) )
		{
			GetKeyframeRotation(TrInst, 0, KeyTime, OutRot, NULL, NULL);
			return OutRot;
		}

		// If beyond the last point in the curve, return its value.
		if( Time >= EulerTrack.Points[NumPoints-1].InVal )
		{
			GetKeyframeRotation(TrInst, NumPoints - 1, KeyTime, OutRot, NULL, NULL);
			return OutRot;
		}

		// Somewhere with curve range - linear search to find value.
		for( int32 i=1; i<NumPoints; i++ )
		{	
			if( Time < EulerTrack.Points[i].InVal )
			{
				const float Diff = EulerTrack.Points[i].InVal - EulerTrack.Points[i-1].InVal;

				if( Diff > 0.f && EulerTrack.Points[i-1].InterpMode != CIM_Constant )
				{
					const float Alpha = (Time - EulerTrack.Points[i-1].InVal) / Diff;

					if( EulerTrack.Points[i-1].InterpMode == CIM_Linear )	// Linear interpolation
					{
						FVector PrevRot, CurrentRot;
						GetKeyframeRotation(TrInst, i-1, KeyTime, PrevRot, NULL, NULL);
						GetKeyframeRotation(TrInst, i, KeyTime, CurrentRot, NULL, NULL);

						OutRot =  FMath::Lerp( PrevRot, CurrentRot, Alpha );
						return OutRot;
					}
					else	//Cubic Interpolation
					{
						// Get keyframe rotations and tangents.
						FVector CurrentRot, CurrentArriveTangent, PrevRot, PrevLeaveTangent;
						GetKeyframeRotation(TrInst, i-1, KeyTime, PrevRot, NULL, &PrevLeaveTangent);
						GetKeyframeRotation(TrInst, i, KeyTime, CurrentRot, &CurrentArriveTangent, NULL);

						OutRot = FMath::CubicInterp( PrevRot, PrevLeaveTangent * Diff, CurrentRot, CurrentArriveTangent * Diff, Alpha );
						return OutRot;
					}
				}
				else	// Constant Interpolation
				{
					GetKeyframeRotation(TrInst, i-1, KeyTime, OutRot, NULL, NULL);
					return OutRot;
				}
			}
		}

		// Shouldn't really reach here.
		GetKeyframeRotation(TrInst, NumPoints-1, KeyTime, OutRot, NULL, NULL);
		return OutRot;
	}
	else
	{
		// Track subtracks, find the rotation tracks and get the new rotation from them.
		FVector OutRot;

		UInterpTrackMoveAxis* RotXTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks[AXIS_RotationX] );
		UInterpTrackMoveAxis* RotYTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks[AXIS_RotationY] );
		UInterpTrackMoveAxis* RotZTrack = CastChecked<UInterpTrackMoveAxis>( SubTracks[AXIS_RotationZ] );

		OutRot.X = RotXTrack->EvalValueAtTime( TrInst, Time );
		OutRot.Y = RotYTrack->EvalValueAtTime( TrInst, Time );
		OutRot.Z = RotZTrack->EvalValueAtTime( TrInst, Time );

		return OutRot;
	}
}


void UInterpTrackMove::GetKeyTransformAtTime(UInterpTrackInst* TrInst, float Time, FVector& OutPos, FRotator& OutRot)
{
	// If the tracks has no subtracks, get new transform directly from this track
	if( SubTracks.Num() == 0 )
	{
		FQuat KeyQuat;
		float KeyTime;	// Not used here
		if(bUseQuatInterpolation)
		{
			int32 NumPoints = EulerTrack.Points.Num();

			// If no point in curve, return the Default value we passed in.
			if( NumPoints == 0 )
			{
				KeyQuat = FQuat::Identity;
			}
			// If only one point, or before the first point in the curve, return the first points value.
			else if( NumPoints < 2 || (Time <= EulerTrack.Points[0].InVal) )
			{
				FVector OutEulerRot;
				GetKeyframeRotation(TrInst, 0, KeyTime, OutEulerRot, NULL, NULL);
				KeyQuat = FQuat::MakeFromEuler(OutEulerRot);
			}
			// If beyond the last point in the curve, return its value.
			else if( Time >= EulerTrack.Points[NumPoints-1].InVal )
			{
				FVector OutEulerRot;
				GetKeyframeRotation(TrInst, NumPoints-1, KeyTime, OutEulerRot, NULL, NULL);
				KeyQuat = FQuat::MakeFromEuler(OutEulerRot);
			}
			// Somewhere with curve range - linear search to find value.
			else
			{			
				bool bFoundPos = false;
				for( int32 KeyIdx=1; KeyIdx<NumPoints && !bFoundPos; KeyIdx++ )
				{	
					if( Time < EulerTrack.Points[KeyIdx].InVal )
					{
						float Delta = EulerTrack.Points[KeyIdx].InVal - EulerTrack.Points[KeyIdx-1].InVal;
						float Alpha = FMath::Clamp( (Time - EulerTrack.Points[KeyIdx-1].InVal) / Delta, 0.f, 1.f );
						FVector CurrentRot, PrevRot;

						GetKeyframeRotation(TrInst, KeyIdx-1, KeyTime, PrevRot, NULL, NULL);
						GetKeyframeRotation(TrInst, KeyIdx, KeyTime, CurrentRot, NULL, NULL);

						FQuat Key1Quat = FQuat::MakeFromEuler(PrevRot);
						FQuat Key2Quat = FQuat::MakeFromEuler(CurrentRot);

						KeyQuat = FQuat::Slerp( Key1Quat, Key2Quat, Alpha );

						bFoundPos = true;
					}
				}
			}

			OutRot = FRotator(KeyQuat);
		}
		else
		{
			OutRot = FRotator::MakeFromEuler( EvalRotationAtTime(TrInst, Time) );
		}

		// Evaluate position
		OutPos = EvalPositionAtTime(TrInst, Time);
	}
	else
	{
		// Evaluate rotation from subtracks
		OutRot = FRotator::MakeFromEuler( EvalRotationAtTime(TrInst, Time) );

		// Evaluate position from subtracks
		OutPos = EvalPositionAtTime(TrInst, Time);
	}
}

float GetDistanceFromAxis(EAxisList::Type WeightAxis, const FVector& Eval, const FVector& Base)
{
	switch (WeightAxis)
	{
	case EAxisList::X:
		return fabs(Eval.X-Base.X);
	case EAxisList::Y:
		return fabs(Eval.Y-Base.Y);
	case EAxisList::Z:
		return fabs(Eval.Z-Base.Z);
	case EAxisList::XY:
		return FMath::Sqrt((Eval.X-Base.X)*(Eval.X-Base.X)+(Eval.Y-Base.Y)*(Eval.Y-Base.Y));
	case EAxisList::XZ:
		return FMath::Sqrt((Eval.X-Base.X)*(Eval.X-Base.X)+(Eval.Z-Base.Z)*(Eval.Z-Base.Z));
	case EAxisList::YZ:
		return FMath::Sqrt((Eval.Y-Base.Y)*(Eval.Y-Base.Y)+(Eval.Z-Base.Z)*(Eval.Z-Base.Z));
	case EAxisList::XYZ:
		return (Eval-Base).Size();
	default:
		return 0.f;
	}
}

float UInterpTrackMove::FindBestMatchingTimefromPosition(UInterpTrackInst* TrInst, const FVector& Pos, int32 StartKeyIndex, EAxisList::Type WeightAxis )
{
	// If the tracks has no subtracks, get new transform directly from this track
	check( (PosTrack.Points.Num() == EulerTrack.Points.Num()) && (PosTrack.Points.Num() == LookupTrack.Points.Num()) );

	float OutTime = -1.f, MaxError = BIG_NUMBER, CurrentError, CurrentTime;
	FVector CurrentPosition;

	// we're looking for key1, and key 2 that has this position between
	float KeyIndex1Time = 0, KeyIndex2Time = 0;
	FVector KeyIndex1Position, KeyIndex2Position;
	// need to interpolate, find the 2 keys this position is between
	int32 KeyIndex1=-1, KeyIndex2=-1;

	// find first key - closest
	for (int32 KeyIndex=StartKeyIndex; KeyIndex < PosTrack.Points.Num(); ++KeyIndex)
	{
		GetKeyframePosition(TrInst, KeyIndex, CurrentTime, CurrentPosition, NULL, NULL);

		CurrentError = GetDistanceFromAxis(WeightAxis, CurrentPosition, Pos);

		if (CurrentError < MaxError)
		{
			OutTime = CurrentTime;
			MaxError = CurrentError;
			KeyIndex1Time = CurrentTime;
			KeyIndex1=KeyIndex;
			KeyIndex1Position = CurrentPosition;
		}
		// if current error is getting bigger than maxerror
		// that means, it's going away from it. 
		else if (CurrentError > MaxError)
		{
			break;
		}
	}

	// if Error is less than 10, or we didn't find, we don't care - that should be it
	if (MaxError < 10 || KeyIndex1==-1)
	{
		return OutTime;
	}

	// otherwise, find the second key
	// it should be either KeyIndex1-1 or KeyIndex+1
	if (KeyIndex1-1 > 0)
	{
		GetKeyframePosition(TrInst, KeyIndex1-1, CurrentTime, CurrentPosition, NULL, NULL);
		KeyIndex2Time = CurrentTime;
		KeyIndex2Position = CurrentPosition;
		// save first key error
		float KeyIndex1Error = GetDistanceFromAxis(WeightAxis, CurrentPosition, Pos);

		// try to find later key
		if (KeyIndex1+1 < PosTrack.Points.Num())
		{
			GetKeyframePosition(TrInst, KeyIndex1+1, CurrentTime, CurrentPosition, NULL, NULL);			
			float KeyIndex2Error = GetDistanceFromAxis(WeightAxis, CurrentPosition, Pos);

			// if first key is lower, then use first key as second key
			if (KeyIndex1Error < KeyIndex2Error)
			{
				KeyIndex2=KeyIndex1-1;
			}
			else
			{
				// if not, it's later key that's closer, use that as second key
				KeyIndex2=KeyIndex1+1;
				KeyIndex2Time = CurrentTime;
				KeyIndex2Position = CurrentPosition;
			}
		}
		else
		{
			KeyIndex2=KeyIndex1-1;
		}
	}
	else if (KeyIndex1+1 < PosTrack.Points.Num())
	{
		GetKeyframePosition(TrInst, KeyIndex1+1, CurrentTime, CurrentPosition, NULL, NULL);			
		KeyIndex2=KeyIndex1+1;
		KeyIndex2Time = CurrentTime;
		KeyIndex2Position = CurrentPosition;
	}

	// found second key
	if (KeyIndex2 != -1)
	{
		float Alpha = GetDistanceFromAxis(WeightAxis, KeyIndex1Position, Pos)/GetDistanceFromAxis(WeightAxis, KeyIndex2Position, KeyIndex1Position);
		OutTime = FMath::Lerp(KeyIndex1Time, KeyIndex2Time, Alpha);
#if 0
		// debug code if you'd like to compare the position 
		CurrentPosition = EvalPositionAtTime(TrInst, OutTime);
		UE_LOG(LogMatinee, Log, TEXT("Current Error is %f"), (CurrentPosition-Pos).Size());
#endif
	}

	return OutTime;
}


void UInterpTrackMove::ComputeWorldSpaceKeyTransform( UInterpTrackInstMove* MoveTrackInst,
													  const FVector& RelativeSpacePos,
													  const FRotator& RelativeSpaceRot,
													  FVector& OutPos,
													  FRotator& OutRot )
{
	// Find the reference frame the key is considered in.
	FTransform RelativeToWorld = GetMoveRefFrame( MoveTrackInst );

	// Use rotation part to form transformation matrix.
	FTransform ActorToRelative( RelativeSpaceRot, RelativeSpacePos );

	// Compute the rotation amount in world space
	FTransform ActorToWorld = ActorToRelative * RelativeToWorld;

	// Position
	OutPos = ActorToWorld.GetLocation();

	// Rotation
	OutRot = ActorToWorld.Rotator();
}


FRotator UInterpTrackMove::GetLookAtRotation(UInterpTrackInst* TrInst)
{
	FRotator LookAtRot(0,0,0);
	if(LookAtGroupName != NAME_None)
	{
		AActor* Actor = TrInst->GetGroupActor();

		if (TrInst->GetOuter())
		{
			UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
			AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
			UInterpGroupInst* LookAtGroupInst = MatineeActor->FindFirstGroupInstByName(LookAtGroupName.ToString());

			if(Actor && LookAtGroupInst && LookAtGroupInst->GetGroupActor())
			{
				AActor* LookAtActor = LookAtGroupInst->GetGroupActor();

				// Slight hack here so that if we are trying to look at a Player variable, it looks at their Pawn.
				APlayerController* PC = Cast<APlayerController>(LookAtActor);
				if(PC && PC->GetPawn())
				{
					LookAtActor = PC->GetPawn();
				}

				// Find Rotator that points at LookAtActor
				FVector LookDir = (LookAtActor->GetActorLocation() - Actor->GetActorLocation()).GetSafeNormal();
				LookAtRot = LookDir.Rotation();
			}
		}
	}

	return LookAtRot;
}


bool UInterpTrackMove::GetLocationAtTime(UInterpTrackInst* TrInst, float Time, FVector& OutPos, FRotator& OutRot)
{
	UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>(TrInst);

	check( (SubTracks.Num() > 0) || ((EulerTrack.Points.Num() == PosTrack.Points.Num()) && (EulerTrack.Points.Num() == LookupTrack.Points.Num())));

	// Do nothing if no data on this track.
	if( SubTracks.Num() == 0 && EulerTrack.Points.Num() == 0)
	{
		// would be nice to return error code, so that
		// if no point exists, 
		return false;
	}

	// Find the transform for the given time.
	FVector RelativeSpacePos;
	FRotator RelativeSpaceRot;
	GetKeyTransformAtTime( TrInst, Time, RelativeSpacePos, RelativeSpaceRot );

	// Compute world space key transform
	ComputeWorldSpaceKeyTransform( MoveTrackInst, RelativeSpacePos, RelativeSpaceRot, OutPos, OutRot );

	// if ignore mode, do not apply rotation
	if(RotMode == IMR_Ignore)
	{
		AActor* Actor = TrInst->GetGroupActor();
		if (Actor)
		{
			OutRot = Actor->GetActorRotation();
		}
	}
	// Replace rotation if using a special rotation mode.
	else if(RotMode == IMR_LookAtGroup)
	{		
		OutRot = GetLookAtRotation(TrInst);
	}

	return true;
}


FTransform UInterpTrackMove::GetMoveRefFrame(UInterpTrackInstMove* MoveTrackInst)
{
	AActor* Actor = MoveTrackInst->GetGroupActor();
	FTransform BaseTM = FTransform::Identity;

	if(Actor && Actor->GetRootComponent() && Actor->GetRootComponent()->GetAttachParent())
	{
		BaseTM = Actor->GetRootComponent()->GetAttachParent()->GetSocketTransform(Actor->GetRootComponent()->GetAttachSocketName());
	}

	return BaseTM;
}

/*-----------------------------------------------------------------------------
 UInterpTrackInstMove
-----------------------------------------------------------------------------*/
UInterpTrackInstMove::UInterpTrackInstMove(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstMove::InitTrackInst(UInterpTrack* Track)
{
}

void UInterpTrackMove::CreateSubTracks( bool bCopy )
{
#if WITH_EDITORONLY_DATA
	// Make a group for containing all translation subtracks
	FSubTrackGroup TranslateGroup;
	TranslateGroup.GroupName = TEXT("Translation");
	TranslateGroup.bIsCollapsed = false;
	TranslateGroup.bIsSelected = false;

	// Make a group for containing all rotation subtracks
	FSubTrackGroup RotateGroup;
	RotateGroup.GroupName = TEXT("Rotation");
	RotateGroup.bIsCollapsed = false;
	RotateGroup.bIsSelected = false;

	// Add the new subtracks
	SubTrackGroups.Add( TranslateGroup );
	SubTrackGroups.Add( RotateGroup );

	// For each supported subtrack, add a new track based on the supported subtrack parameters.
	for( int32 SubClassIndex = 0; SubClassIndex < SupportedSubTracks.Num(); ++SubClassIndex )
	{
		FSupportedSubTrackInfo& SubTrackInfo = SupportedSubTracks[ SubClassIndex ];
		check( SubTrackInfo.SupportedClass );

		UInterpTrack* TrackDef = SubTrackInfo.SupportedClass->GetDefaultObject<UInterpTrack>();
		check( TrackDef && TrackDef->bSubTrackOnly );

		UInterpTrack* NewSubTrack = NULL;
		NewSubTrack = NewObject<UInterpTrack>(this, SubTrackInfo.SupportedClass, NAME_None, RF_Transactional);
		check( NewSubTrack );

		int32 NewTrackIndex = SubTracks.Add( NewSubTrack );

		if( !bCopy )
		{
			NewSubTrack->SetTrackToSensibleDefault();
		}

		UInterpTrackMoveAxis* MoveSubTrack = Cast<UInterpTrackMoveAxis>( NewSubTrack );
		MoveSubTrack->TrackTitle = SubTrackInfo.SubTrackName;
		MoveSubTrack->MoveAxis = EInterpMoveAxis(SubClassIndex);

		NewSubTrack->Modify();

		// Add the index to this track into the correct subtrack group.
		if( SubTrackInfo.GroupIndex != INDEX_NONE )
		{
			SubTrackGroups[ SubTrackInfo.GroupIndex ].TrackIndices.Add( SubClassIndex );
		}
	}
#endif // WITH_EDITORONLY_DATA
}


void UInterpTrackMove::SplitTranslationAndRotation()
{
#if WITH_EDITORONLY_DATA
	check( SubTrackGroups.Num() == 0 && SubTracks.Num() == 0 );
	
	// First create the new subtracks
	CreateSubTracks( false );

	UInterpTrackMoveAxis* MoveAxies[6];
	for( int32 SubTrackIndex = 0; SubTrackIndex < 6; ++SubTrackIndex )
	{
		MoveAxies[ SubTrackIndex ] = Cast<UInterpTrackMoveAxis>( SubTracks[ SubTrackIndex ] );
	}

	// Populate the translation tracks with data.
	for( int32 KeyIndex = 0; KeyIndex < PosTrack.Points.Num(); ++KeyIndex )
	{
		// For each keyframe in the orginal position track, add one keyframe to each translation track at the same location and with the same options.
		float Time = PosTrack.Points[ KeyIndex ].InVal;
		const FVector& Pos = PosTrack.Points[ KeyIndex ].OutVal;
		MoveAxies[AXIS_TranslationX]->FloatTrack.AddPoint( Time, Pos.X );
		MoveAxies[AXIS_TranslationY]->FloatTrack.AddPoint( Time, Pos.Y );
		MoveAxies[AXIS_TranslationZ]->FloatTrack.AddPoint( Time, Pos.Z );
		MoveAxies[AXIS_TranslationX]->FloatTrack.Points[ KeyIndex ].InterpMode = PosTrack.Points[ KeyIndex ].InterpMode;
		MoveAxies[AXIS_TranslationY]->FloatTrack.Points[ KeyIndex ].InterpMode = PosTrack.Points[ KeyIndex ].InterpMode;
		MoveAxies[AXIS_TranslationZ]->FloatTrack.Points[ KeyIndex ].InterpMode = PosTrack.Points[ KeyIndex ].InterpMode;
		MoveAxies[AXIS_TranslationX]->FloatTrack.Points[ KeyIndex ].ArriveTangent = PosTrack.Points[ KeyIndex ].ArriveTangent[ AXIS_TranslationX ];
		MoveAxies[AXIS_TranslationY]->FloatTrack.Points[ KeyIndex ].ArriveTangent = PosTrack.Points[ KeyIndex ].ArriveTangent[ AXIS_TranslationY ];
		MoveAxies[AXIS_TranslationZ]->FloatTrack.Points[ KeyIndex ].ArriveTangent = PosTrack.Points[ KeyIndex ].ArriveTangent[ AXIS_TranslationZ ];
		MoveAxies[AXIS_TranslationX]->FloatTrack.Points[ KeyIndex ].LeaveTangent = PosTrack.Points[ KeyIndex ].LeaveTangent[ AXIS_TranslationX ];
		MoveAxies[AXIS_TranslationY]->FloatTrack.Points[ KeyIndex ].LeaveTangent = PosTrack.Points[ KeyIndex ].LeaveTangent[ AXIS_TranslationY ];
		MoveAxies[AXIS_TranslationZ]->FloatTrack.Points[ KeyIndex ].LeaveTangent = PosTrack.Points[ KeyIndex ].LeaveTangent[ AXIS_TranslationZ ];

		// Copy lookup track info.
		MoveAxies[AXIS_TranslationX]->LookupTrack.Points.AddUninitialized();
		MoveAxies[AXIS_TranslationX]->LookupTrack.Points[ KeyIndex ] = LookupTrack.Points[ KeyIndex ];
		MoveAxies[AXIS_TranslationY]->LookupTrack.Points.AddUninitialized();
		MoveAxies[AXIS_TranslationY]->LookupTrack.Points[ KeyIndex ] = LookupTrack.Points[ KeyIndex ];
		MoveAxies[AXIS_TranslationZ]->LookupTrack.Points.AddUninitialized();
		MoveAxies[AXIS_TranslationZ]->LookupTrack.Points[ KeyIndex ] = LookupTrack.Points[ KeyIndex ];

	}

	// Populate the rotation tracks with data.
	for( int32 KeyIndex = 0; KeyIndex < EulerTrack.Points.Num(); ++KeyIndex )
	{
		// For each keyframe in the orginal rotation track, add one keyframe to each rotation track at the same location and with the same options.
		float Time = EulerTrack.Points[ KeyIndex ].InVal;
		const FVector& Rot = EulerTrack.Points[ KeyIndex ].OutVal;
		MoveAxies[AXIS_RotationX]->FloatTrack.AddPoint( Time, Rot.X );
		MoveAxies[AXIS_RotationY]->FloatTrack.AddPoint( Time, Rot.Y );
		MoveAxies[AXIS_RotationZ]->FloatTrack.AddPoint( Time, Rot.Z );
		MoveAxies[AXIS_RotationX]->FloatTrack.Points[ KeyIndex ].InterpMode = EulerTrack.Points[ KeyIndex ].InterpMode;
		MoveAxies[AXIS_RotationY]->FloatTrack.Points[ KeyIndex ].InterpMode = EulerTrack.Points[ KeyIndex ].InterpMode;
		MoveAxies[AXIS_RotationZ]->FloatTrack.Points[ KeyIndex ].InterpMode = EulerTrack.Points[ KeyIndex ].InterpMode;
		MoveAxies[AXIS_RotationX]->FloatTrack.Points[ KeyIndex ].ArriveTangent = EulerTrack.Points[ KeyIndex ].ArriveTangent[AXIS_RotationX-3];
		MoveAxies[AXIS_RotationY]->FloatTrack.Points[ KeyIndex ].ArriveTangent = EulerTrack.Points[ KeyIndex ].ArriveTangent[AXIS_RotationY-3];
		MoveAxies[AXIS_RotationZ]->FloatTrack.Points[ KeyIndex ].ArriveTangent = EulerTrack.Points[ KeyIndex ].ArriveTangent[AXIS_RotationZ-3];
		MoveAxies[AXIS_RotationX]->FloatTrack.Points[ KeyIndex ].LeaveTangent = EulerTrack.Points[ KeyIndex ].LeaveTangent[AXIS_RotationX-3];
		MoveAxies[AXIS_RotationY]->FloatTrack.Points[ KeyIndex ].LeaveTangent = EulerTrack.Points[ KeyIndex ].LeaveTangent[AXIS_RotationY-3];
		MoveAxies[AXIS_RotationZ]->FloatTrack.Points[ KeyIndex ].LeaveTangent = EulerTrack.Points[ KeyIndex ].LeaveTangent[AXIS_RotationZ-3];

		MoveAxies[AXIS_RotationX]->LookupTrack.Points.AddUninitialized();
		MoveAxies[AXIS_RotationX]->LookupTrack.Points[ KeyIndex ] = LookupTrack.Points[ KeyIndex ];
		MoveAxies[AXIS_RotationY]->LookupTrack.Points.AddUninitialized();
		MoveAxies[AXIS_RotationY]->LookupTrack.Points[ KeyIndex ] = LookupTrack.Points[ KeyIndex ];
		MoveAxies[AXIS_RotationZ]->LookupTrack.Points.AddUninitialized();
		MoveAxies[AXIS_RotationZ]->LookupTrack.Points[ KeyIndex ] = LookupTrack.Points[ KeyIndex ];
	}

	// Clear out old data.
	LookupTrack.Points.Empty();
	PosTrack.Points.Empty();
	EulerTrack.Points.Empty();
#endif // WITH_EDITORONLY_DATA
}


void UInterpTrackMove::ReduceKeys( float IntervalStart, float IntervalEnd, float Tolerance )
{
	if( SubTracks.Num() == 0 )
	{
		// Create all the control points. They are six-dimensional, since
		// the Euler rotation key and the position key times must match.
		MatineeKeyReduction::MCurve<FTwoVectors, 6> Curve;
		Curve.RelativeTolerance = Tolerance / 100.0f;
		Curve.IntervalStart = IntervalStart - 0.0005f; // 0.5ms pad to allow for floating-point precision.
		Curve.IntervalEnd = IntervalEnd + 0.0005f;  // 0.5ms pad to allow for floating-point precision.

		Curve.CreateControlPoints(PosTrack, 0);
		Curve.CreateControlPoints(EulerTrack, 3);
		if (Curve.HasControlPoints())
		{
			Curve.FillControlPoints(PosTrack, 3, 0);
			Curve.FillControlPoints(EulerTrack, 3, 3);

			// Reduce the 6D curve.
			Curve.Reduce();

			// Copy the reduced keys over to the new curve.
			Curve.CopyCurvePoints(PosTrack.Points, 3, 0);
			Curve.CopyCurvePoints(EulerTrack.Points, 3, 3);
		}

		// Refer the look-up track to nothing.
		LookupTrack.Points.Empty();
		FName Nothing(NAME_None);
		uint32 PointCount = PosTrack.Points.Num();
		for (uint32 Index = 0; Index < PointCount; ++Index)
		{
			LookupTrack.AddPoint(PosTrack.Points[Index].InVal, Nothing);
		}
	}
	else
	{
		// Reduce keys for all subtracks.
		for( int32 SubTrackIndex = 0; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
		{
			SubTracks[ SubTrackIndex ]->Modify();
			SubTracks[ SubTrackIndex ]->ReduceKeys(IntervalStart, IntervalEnd, Tolerance);
		}
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackFloatBase
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackFloatBase, FloatTrack.Points)
STRUCTTRACK_GETTIMERANGE(UInterpTrackFloatBase, FloatTrack.Points, InVal)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackFloatBase, FloatTrack.Points, InVal)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackFloatBase, FloatTrack.Points, InVal)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackFloatBase, FloatTrack.Points, InVal)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackFloatBase, FloatTrack.Points, InVal)

UInterpTrackFloatBase::UInterpTrackFloatBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackTitle = TEXT("Generic Float Track");
	CurveTension = 0.0f;
}

int32 UInterpTrackFloatBase::SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder)
{
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
		return KeyIndex;

	int32 NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = FloatTrack.MovePoint(KeyIndex, NewKeyTime);
	}
	else
	{
		FloatTrack.Points[KeyIndex].InVal = NewKeyTime;
	}

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackFloatBase::RemoveKeyframe(int32 KeyIndex)
{
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
		return;

	FloatTrack.Points.RemoveAt(KeyIndex);

	FloatTrack.AutoSetTangents(CurveTension);
}


int32 UInterpTrackFloatBase::DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack)
{
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
	{
		return INDEX_NONE;
	}

	// Make sure the destination track is specified.
	UInterpTrackFloatBase* DestTrack = this;
	if ( ToTrack )
	{
		DestTrack = CastChecked< UInterpTrackFloatBase >( ToTrack );
	}

	FInterpCurvePoint<float> FloatPoint = FloatTrack.Points[KeyIndex];
	int32 NewKeyIndex = DestTrack->FloatTrack.AddPoint(NewKeyTime, 0.f);
	DestTrack->FloatTrack.Points[NewKeyIndex] = FloatPoint; // Copy properties from source key.
	DestTrack->FloatTrack.Points[NewKeyIndex].InVal = NewKeyTime;

	DestTrack->FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

#if WITH_EDITOR
void UInterpTrackFloatBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FloatTrack.AutoSetTangents(CurveTension);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

/*-----------------------------------------------------------------------------
	UInterpTrackToggle
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackToggle, ToggleTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackToggle, ToggleTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackToggle, ToggleTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackToggle, ToggleTrack, Time)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackToggle, ToggleTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackToggle, ToggleTrack, Time, FToggleTrackKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackToggle, ToggleTrack)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackToggle, ToggleTrack, Time, FToggleTrackKey)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackToggle, ToggleTrack, Time)

UInterpTrackToggle::UInterpTrackToggle(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TrackInstClass = UInterpTrackInstToggle::StaticClass();
	TrackTitle = TEXT("Toggle");
	bActivateSystemEachUpdate = false;
	bActivateWithJustAttachedFlag = true;
	bFireEventsWhenForwards = true;
	bFireEventsWhenBackwards = true;
	bFireEventsWhenJumpingForwards = true;

#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MAT_Groups_Toggle.MAT_Groups_Toggle"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackToggle::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstToggle* ToggleInst = CastChecked<UInterpTrackInstToggle>(TrInst);

	int32 i = 0;
	for (i = 0; i < ToggleTrack.Num() && ToggleTrack[i].Time < Time; i++);
	ToggleTrack.InsertUninitialized(i);
	ToggleTrack[i].Time = Time;
	ToggleTrack[i].ToggleAction = ToggleInst->Action;

	return i;
}

void UInterpTrackToggle::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if (Actor == NULL)
	{
		return;
	}

	UInterpTrackInstToggle* ToggleInst = CastChecked<UInterpTrackInstToggle>(TrInst);
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( ToggleInst->GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
	UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
	UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );



	AEmitter* EmitterActor = Cast<AEmitter>(Actor);
	if( EmitterActor && bActivateSystemEachUpdate )
	{
		// @todo: Deprecate this legacy particle track behavior!  It doesn't support playing skipped events,
		//        and it doesn't support network synchronization!
		if ((NewPosition > ToggleInst->LastUpdatePosition)  && !bJump)
		{
			for (int32 KeyIndex = ToggleTrack.Num() - 1; KeyIndex >= 0; KeyIndex--)
			{
				FToggleTrackKey& ToggleKey = ToggleTrack[KeyIndex];
				if( ToggleKey.Time < NewPosition )
				{
					// We have found the key to the left of the position
					if( ToggleKey.ToggleAction == ETTA_On )
					{
						EmitterActor->GetParticleSystemComponent()->ActivateSystem( bActivateWithJustAttachedFlag );
					}
					else if( ToggleKey.ToggleAction == ETTA_Trigger )
					{
						if( ToggleKey.Time >= ToggleInst->LastUpdatePosition )
						{
							EmitterActor->GetParticleSystemComponent()->SetActive( true, bActivateWithJustAttachedFlag );
						}
					}
					else
					{
						EmitterActor->GetParticleSystemComponent()->DeactivateSystem();
					}
					break;
				}
			}
		}
	}
	else
	{
		// This is the normal pathway for toggle tracks.  It supports firing toggle events
		// even when jummping forward in time (skipping a cutscene.)

		// NOTE: We don't fire events when jumping forwards in Matinee preview since that would
		//       fire off particles while scrubbing, which we currently don't want.
		const bool bShouldActuallyFireEventsWhenJumpingForwards =
			bFireEventsWhenJumpingForwards && !( GIsEditor && !Actor->GetWorld()->HasBegunPlay() );

		// @todo: Make this configurable?
		const bool bInvertBoolLogicWhenPlayingBackwards = true;

		// Only allow triggers to play when jumping when scrubbing in editor's Matinee preview.  We
		// never want to allow this in game, since this could cause many particles to fire off
		// when a cinematic is skipped (as we "jump" to the end point)
		const bool bPlayTriggersWhenJumping = GIsEditor && !Actor->GetWorld()->HasBegunPlay();


		// We'll consider playing events in reverse if we're either actively playing in reverse or if
		// we're in a paused state but forcing an update to an older position (scrubbing backwards in editor.)
		bool bIsPlayingBackwards =
			( MatineeActor->bIsPlaying && MatineeActor->bReversePlayback ) ||
			( bJump && !MatineeActor->bIsPlaying && NewPosition < ToggleInst->LastUpdatePosition );


		// Find the interval between last update and this to check events with.
		bool bFireEvents = true;


		if( bJump )
		{
			// If we are playing forwards, and the flag is set, fire events even if we are 'jumping'.
			if( bShouldActuallyFireEventsWhenJumpingForwards && !bIsPlayingBackwards )
			{
				bFireEvents = true;
			}
			else
			{
				bFireEvents = false;
			}
		}

		// If playing sequence forwards.
		float MinTime, MaxTime;
		if( !bIsPlayingBackwards )
		{
			MinTime = ToggleInst->LastUpdatePosition;
			MaxTime = NewPosition;

			// Slight hack here.. if playing forwards and reaching the end of the sequence, force it over a little to ensure we fire events actually on the end of the sequence.
			if( MaxTime == IData->InterpLength )
			{
				MaxTime += (float)KINDA_SMALL_NUMBER;
			}

			if( !bFireEventsWhenForwards )
			{
				bFireEvents = false;
			}
		}
		// If playing sequence backwards.
		else
		{
			MinTime = NewPosition;
			MaxTime = ToggleInst->LastUpdatePosition;

			// Same small hack as above for backwards case.
			if( MinTime == 0.0f )
			{
				MinTime -= (float)KINDA_SMALL_NUMBER;
			}

			if( !bFireEventsWhenBackwards )
			{
				bFireEvents = false;
			}
		}


		// If we should be firing events for this track...
		if( bFireEvents )
		{
			// See which events fall into traversed region.
			int32 KeyIndexToPlay = INDEX_NONE;
			for(int32 CurKeyIndex = 0; CurKeyIndex < ToggleTrack.Num(); ++CurKeyIndex )
			{
				FToggleTrackKey& ToggleKey = ToggleTrack[ CurKeyIndex ];

				float EventTime = ToggleKey.Time;

				// Need to be slightly careful here and make behavior for firing events symmetric when playing forwards of backwards.
				bool bFireThisEvent = false;
				if( !bIsPlayingBackwards )
				{
					if( EventTime >= MinTime && EventTime < MaxTime )
					{
						bFireThisEvent = true;
					}
				}
				else
				{
					if( EventTime > MinTime && EventTime <= MaxTime )
					{
						bFireThisEvent = true;
					}
				}

				if( bFireThisEvent )
				{
					// Check for "fire and forget" events that must always be played
					if (ToggleKey.ToggleAction == ETTA_Trigger && EmitterActor )
					{
						// Don't play triggers when jumping forward unless we're configured to do that
						if( bPlayTriggersWhenJumping || !bJump )
						{
							// Use ActivateSystem as multiple triggers should fire it multiple times.
							EmitterActor->GetParticleSystemComponent()->ActivateSystem(bActivateWithJustAttachedFlag);
							// don't set bCurrentlyActive (assume it's a one shot effect which the client will perform through its own matinee simulation)
						}
					}
					else
					{
						// The idea here is that there's no point in playing multiple bool-style events in a
						// single frame, so we skip over events to find the most relevant.
						if( KeyIndexToPlay == INDEX_NONE ||
							( !bIsPlayingBackwards && CurKeyIndex > KeyIndexToPlay ) ||
							( bIsPlayingBackwards && CurKeyIndex < KeyIndexToPlay ) )
						{
							// Found the key we want to play!  
							KeyIndexToPlay = CurKeyIndex;
						}
					}
				}
			}

			if( KeyIndexToPlay != INDEX_NONE )
			{
				FToggleTrackKey& ToggleKey = ToggleTrack[ KeyIndexToPlay ];

				ALight* LightActor = Cast<ALight>(Actor);

				if( EmitterActor )
				{
					// Trigger keys should have been handled earlier!
					check( ToggleKey.ToggleAction != ETTA_Trigger );

					bool bShouldActivate = (ToggleKey.ToggleAction == ETTA_On);
					if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
					{
						// Playing in reverse, so invert bool logic
						bShouldActivate = !bShouldActivate;
					}

					EmitterActor->GetParticleSystemComponent()->SetActive( bShouldActivate, bActivateWithJustAttachedFlag );
					EmitterActor->bCurrentlyActive = bShouldActivate;
					if (!MatineeActor->bClientSideOnly)
					{
						EmitterActor->ForceNetRelevant();
					}
				}
				else if( LightActor != NULL )
				{
					// We'll only allow *toggleable* lights to be toggled like this!  Static lights are ignored.
					if( LightActor->IsToggleable() )
					{
						bool bShouldActivate = (ToggleKey.ToggleAction == ETTA_On);
						if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
						{
							// Playing in reverse, so invert bool logic
							bShouldActivate = !bShouldActivate;
						}

						LightActor->GetLightComponent()->SetVisibility( bShouldActivate );
					}
				}
				else
				{
					// Find the function to call on the actor
					FName FunctionName = TEXT("OnInterpToggle");
					UFunction* ToggleFunction = Actor->FindFunction( FunctionName );
					// Make sure we call the right function. It should have one param.
					if( ToggleFunction && ToggleFunction->NumParms == 1 )
					{		
						int32 ShouldActivate = (ToggleKey.ToggleAction == ETTA_On || ToggleKey.ToggleAction == ETTA_Trigger);
						if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
						{
							// Playing in reverse, so invert bool logic
							ShouldActivate = !ShouldActivate;
						}

						// Call the function
						Actor->ProcessEvent( ToggleFunction, &ShouldActivate );	
					}
				}
			}
		}
	}

	ToggleInst->LastUpdatePosition = NewPosition;
}


void UInterpTrackToggle::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );

	// Dont play sounds unless we are preview playback (ie not scrubbing).
	bool bJump = !(MatineeActor->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}

const FString UInterpTrackToggle::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackToggleHelper") );
}

const FString UInterpTrackToggle::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackToggleHelper") );
}

/*-----------------------------------------------------------------------------
	UInterpTrackVectorBase
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackVectorBase, VectorTrack.Points)
STRUCTTRACK_GETTIMERANGE(UInterpTrackVectorBase, VectorTrack.Points, InVal)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackVectorBase, VectorTrack.Points, InVal)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackVectorBase, VectorTrack.Points, InVal)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackVectorBase, VectorTrack.Points, InVal)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackVectorBase, VectorTrack.Points, InVal)

UInterpTrackVectorBase::UInterpTrackVectorBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackTitle = TEXT("Generic Vector Track");
	CurveTension = 0.0f;
}

int32 UInterpTrackVectorBase::SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder)
{
	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
		return KeyIndex;

	int32 NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = VectorTrack.MovePoint(KeyIndex, NewKeyTime);
	}
	else
	{
		VectorTrack.Points[KeyIndex].InVal = NewKeyTime;
	}

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackVectorBase::RemoveKeyframe(int32 KeyIndex)
{
	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
		return;

	VectorTrack.Points.RemoveAt(KeyIndex);

	VectorTrack.AutoSetTangents(CurveTension);
}


int32 UInterpTrackVectorBase::DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack)
{
	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
	{
		return INDEX_NONE;
	}

	// Make sure the destination track is specified.
	UInterpTrackVectorBase* DestTrack = this;
	if ( ToTrack )
	{
		DestTrack = CastChecked< UInterpTrackVectorBase >( ToTrack );
	}

	FInterpCurvePoint<FVector> VectorPoint = VectorTrack.Points[KeyIndex];
	int32 NewKeyIndex = DestTrack->VectorTrack.AddPoint(NewKeyTime, FVector::ZeroVector);
	DestTrack->VectorTrack.Points[NewKeyIndex] = VectorPoint; // Copy properties from source key.
	DestTrack->VectorTrack.Points[NewKeyIndex].InVal = NewKeyTime;

	DestTrack->VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

#if WITH_EDITOR
void UInterpTrackVectorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	VectorTrack.AutoSetTangents(CurveTension);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR


/*-----------------------------------------------------------------------------
	UInterpTrackLinearColorBase
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackLinearColorBase, LinearColorTrack.Points)
STRUCTTRACK_GETTIMERANGE(UInterpTrackLinearColorBase, LinearColorTrack.Points, InVal)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackLinearColorBase, LinearColorTrack.Points, InVal)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackLinearColorBase, LinearColorTrack.Points, InVal)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackLinearColorBase, LinearColorTrack.Points, InVal)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackLinearColorBase, LinearColorTrack.Points, InVal)

UInterpTrackLinearColorBase::UInterpTrackLinearColorBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackTitle = TEXT("Generic LinearColor Track");
	CurveTension = 0.0f;
}

int32 UInterpTrackLinearColorBase::SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder)
{
	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
		return KeyIndex;

	int32 NewKeyIndex = KeyIndex;
	if(bUpdateOrder)
	{
		NewKeyIndex = LinearColorTrack.MovePoint(KeyIndex, NewKeyTime);
	}
	else
	{
		LinearColorTrack.Points[KeyIndex].InVal = NewKeyTime;
	}

	LinearColorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackLinearColorBase::RemoveKeyframe(int32 KeyIndex)
{
	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
		return;

	LinearColorTrack.Points.RemoveAt(KeyIndex);

	LinearColorTrack.AutoSetTangents(CurveTension);
}


int32 UInterpTrackLinearColorBase::DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack)
{
	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
	{
		return INDEX_NONE;
	}

	// Make sure the destination track is specified.
	UInterpTrackLinearColorBase* DestTrack = this;
	if ( ToTrack )
	{
		DestTrack = CastChecked< UInterpTrackLinearColorBase >( ToTrack );
	}

	FInterpCurvePoint<FLinearColor> VectorPoint = LinearColorTrack.Points[KeyIndex];
	int32 NewKeyIndex = DestTrack->LinearColorTrack.AddPoint(NewKeyTime, FLinearColor(0.f, 0.f, 0.f, 0.f));
	DestTrack->LinearColorTrack.Points[NewKeyIndex] = VectorPoint; // Copy properties from source key.
	DestTrack->LinearColorTrack.Points[NewKeyIndex].InVal = NewKeyTime;

	DestTrack->LinearColorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}


#if WITH_EDITOR
void UInterpTrackLinearColorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	LinearColorTrack.AutoSetTangents(CurveTension);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR


/*-----------------------------------------------------------------------------
	UInterpTrackFloatProp
-----------------------------------------------------------------------------*/
UInterpTrackFloatProp::UInterpTrackFloatProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackInstClass = UInterpTrackInstFloatProp::StaticClass();
	TrackTitle = TEXT("Float Property");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Float.MAT_Groups_Float"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackFloatProp::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstFloatProp* PropInst = CastChecked<UInterpTrackInstFloatProp>(TrInst);
	if( !PropInst->FloatProp )
		return INDEX_NONE;

	int32 NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

	UpdateKeyframe(NewKeyIndex, TrInst);

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

bool UInterpTrackFloatProp::CanAddKeyframe( UInterpTrackInst* TrInst )
{
	UInterpTrackInstFloatProp* PropInst = CastChecked<UInterpTrackInstFloatProp>(TrInst);
	return PropInst->FloatProp != NULL;
}

void UInterpTrackFloatProp::UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst)
{
	UInterpTrackInstFloatProp* PropInst = CastChecked<UInterpTrackInstFloatProp>(TrInst);
	if( !PropInst->FloatProp )
		return;

	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
		return;

	FloatTrack.Points[KeyIndex].OutVal = *PropInst->FloatProp;

	FloatTrack.AutoSetTangents(CurveTension);
}


void UInterpTrackFloatProp::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, false);
}

void UInterpTrackFloatProp::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstFloatProp* PropInst = CastChecked<UInterpTrackInstFloatProp>(TrInst);
	if(!PropInst->FloatProp)
		return;

	*PropInst->FloatProp = FloatTrack.Eval( NewPosition, *PropInst->FloatProp );

	// If we have a custom callback for this property, call that
	PropInst->CallPropertyUpdateCallback();
}

const FString	UInterpTrackFloatProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackFloatPropHelper") );
}

const FString	UInterpTrackFloatProp::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackFloatPropHelper") );
}


void UInterpTrackFloatProp::ReduceKeys( float IntervalStart, float IntervalEnd, float Tolerance )
{
	FInterpCurve<MatineeKeyReduction::SFLOAT>& OldCurve = (FInterpCurve<MatineeKeyReduction::SFLOAT>&) FloatTrack;

	// Create all the control points. They are six-dimensional, since
	// the Euler rotation key and the position key times must match.
	MatineeKeyReduction::MCurve<MatineeKeyReduction::SFLOAT, 1> Curve;
	Curve.RelativeTolerance = Tolerance / 100.0f;
	Curve.IntervalStart = IntervalStart - 0.0005f;  // 0.5ms pad to allow for floating-point precision.
	Curve.IntervalEnd = IntervalEnd + 0.0005f;  // 0.5ms pad to allow for floating-point precision.

	Curve.CreateControlPoints(OldCurve, 0);
	if (Curve.HasControlPoints())
	{
		Curve.FillControlPoints(OldCurve, 1, 0);

		// Reduce the curve.
		Curve.Reduce();

		// Copy the reduced keys over to the new curve.
		Curve.CopyCurvePoints(OldCurve.Points, 1, 0);
	}
}
/*-----------------------------------------------------------------------------
  UInterpTrackInstFloatProp
-----------------------------------------------------------------------------*/
UInterpTrackInstFloatProp::UInterpTrackInstFloatProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstFloatProp::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!FloatProp)
		return;

	// Remember current value of property for when we quite Matinee
	ResetFloat = *FloatProp;
}

void UInterpTrackInstFloatProp::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!FloatProp)
		return;

	// Restore original value of property
	*FloatProp = ResetFloat;

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ReregisterAllComponents(); // @todo UE4 insist on a property update callback, and use here?
}

void UInterpTrackInstFloatProp::InitTrackInst(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	// Store a pointer to the float data for the property we will be interpolating.
	UInterpTrackFloatProp* TrackProp = Cast<UInterpTrackFloatProp>(Track);
	FloatProp = FMatineeUtils::GetInterpFloatPropertyRef(Actor, TrackProp->PropertyName);

	SetupPropertyUpdateCallback(Actor, TrackProp->PropertyName);
}

/*-----------------------------------------------------------------------------
	UInterpTrackVectorProp
-----------------------------------------------------------------------------*/
UInterpTrackVectorProp::UInterpTrackVectorProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackInstClass = UInterpTrackInstVectorProp::StaticClass();
	TrackTitle = TEXT("Vector Property");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Vector.MAT_Groups_Vector"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackVectorProp::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstVectorProp* PropInst = CastChecked<UInterpTrackInstVectorProp>(TrInst);
	if( !PropInst->VectorProp )
		return INDEX_NONE;

	int32 NewKeyIndex = VectorTrack.AddPoint( Time, FVector::ZeroVector);
	VectorTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

	UpdateKeyframe(NewKeyIndex, TrInst);

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

bool UInterpTrackVectorProp::CanAddKeyframe( UInterpTrackInst* TrInst )
{
	UInterpTrackInstVectorProp* PropInst = CastChecked<UInterpTrackInstVectorProp>(TrInst);
	return PropInst->VectorProp != NULL;
}

void UInterpTrackVectorProp::UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst)
{
	UInterpTrackInstVectorProp* PropInst = CastChecked<UInterpTrackInstVectorProp>(TrInst);
	if( !PropInst->VectorProp )
		return;

	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
		return;

	VectorTrack.Points[KeyIndex].OutVal = *PropInst->VectorProp;

	VectorTrack.AutoSetTangents(CurveTension);
}


void UInterpTrackVectorProp::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, false);
}

void UInterpTrackVectorProp::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstVectorProp* PropInst = CastChecked<UInterpTrackInstVectorProp>(TrInst);
	if(!PropInst->VectorProp)
		return;

	FVector NewVectorValue = VectorTrack.Eval( NewPosition, *PropInst->VectorProp );
	*PropInst->VectorProp = NewVectorValue;

	// If we have a custom callback for this property, call that
	PropInst->CallPropertyUpdateCallback();
}

const FString	UInterpTrackVectorProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackVectorPropHelper") );
}

const FString	UInterpTrackVectorProp::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackVectorPropHelper") );
}


/*-----------------------------------------------------------------------------
	UInterpTrackInstVectorProp
-----------------------------------------------------------------------------*/
UInterpTrackInstVectorProp::UInterpTrackInstVectorProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstVectorProp::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!VectorProp)
		return;

	// Remember current value of property for when we quite Matinee
	ResetVector = *VectorProp;
}

void UInterpTrackInstVectorProp::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!VectorProp)
		return;

	// Restore original value of property
	*VectorProp = ResetVector;

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ReregisterAllComponents(); // @todo UE4 insist on a property update callback, and use here?
}

void UInterpTrackInstVectorProp::InitTrackInst(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackVectorProp* TrackProp = Cast<UInterpTrackVectorProp>(Track);
	VectorProp = FMatineeUtils::GetInterpVectorPropertyRef(Actor, TrackProp->PropertyName);

	SetupPropertyUpdateCallback(Actor, TrackProp->PropertyName);
}

/*-----------------------------------------------------------------------------
	UInterpTrackBoolProp
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackBoolProp, BoolTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackBoolProp, BoolTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackBoolProp, BoolTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackBoolProp, BoolTrack, Time)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackBoolProp, BoolTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackBoolProp, BoolTrack, Time, FBoolTrackKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackBoolProp, BoolTrack)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackBoolProp, BoolTrack, Time, FBoolTrackKey)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackBoolProp, BoolTrack, Time)

UInterpTrackBoolProp::UInterpTrackBoolProp(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TrackInstClass = UInterpTrackInstBoolProp::StaticClass();
	TrackTitle = TEXT("Bool Property");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Float.MAT_Groups_Float"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackBoolProp::AddKeyframe( float Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode )
{
	UInterpTrackInstBoolProp* BoolPropInst = CastChecked<UInterpTrackInstBoolProp>(TrackInst);

	if( !BoolPropInst->BoolPropertyAddress || !BoolPropInst->BoolProperty )
		return INDEX_NONE;

	FBoolTrackKey BoolKey;
	BoolKey.Time = Time;
	BoolKey.Value = BoolPropInst->BoolProperty->GetPropertyValue( BoolPropInst->BoolPropertyAddress );

	int32 NewKeyIndex = BoolTrack.Add( BoolKey );
	UpdateKeyframe( NewKeyIndex, TrackInst );

	return NewKeyIndex;
}

bool UInterpTrackBoolProp::CanAddKeyframe( UInterpTrackInst* TrackInst )
{
	UInterpTrackInstBoolProp* BoolPropInst = CastChecked<UInterpTrackInstBoolProp>(TrackInst);
	return BoolPropInst->BoolPropertyAddress != NULL && BoolPropInst->BoolProperty != NULL;
}

void UInterpTrackBoolProp::UpdateKeyframe( int32 KeyIndex, UInterpTrackInst* TrackInst )
{
	UInterpTrackInstBoolProp* PropInst = CastChecked<UInterpTrackInstBoolProp>(TrackInst);

	// We must have a valid pointer to the boolean to modify
	if( !PropInst->BoolPropertyAddress || !PropInst->BoolProperty )
		return;

	// Must have a valid key index.
	if( !BoolTrack.IsValidIndex(KeyIndex) )
		return;

	BoolTrack[KeyIndex].Value = PropInst->BoolProperty->GetPropertyValue( PropInst->BoolPropertyAddress );
}

void UInterpTrackBoolProp::PreviewUpdateTrack( float NewPosition, UInterpTrackInst* TrackInst )
{
	UpdateTrack( NewPosition, TrackInst, false );
}

void UInterpTrackBoolProp::UpdateTrack(float NewPosition, UInterpTrackInst* TrackInst, bool bJump)
{
	AActor* Actor = TrackInst->GetGroupActor();

	// If we don't have a group actors, then we can't modify the boolean stored on the actor.
	if( !Actor )
	{
		return;
	}

	UInterpTrackInstBoolProp* PropInst = CastChecked<UInterpTrackInstBoolProp>(TrackInst);

	// We must have a valid pointer to the boolean to modify
	if( !PropInst->BoolPropertyAddress || !PropInst->BoolProperty )
		return;

	bool NewBoolValue = false;
	const int32 NumOfKeys = BoolTrack.Num();

	// If we have zero keys, use the property's original value. 
	if( NumOfKeys == 0 )
	{
		NewBoolValue = PropInst->BoolProperty->GetPropertyValue( PropInst->BoolPropertyAddress );
	}
	// If we only have one key or the position is before
	// the first key, use the value of the first key.
	else if( NumOfKeys == 1 || NewPosition <= BoolTrack[0].Time )
	{
		NewBoolValue = BoolTrack[0].Value;
	}
	// If the position is past the last key, use the value of the last key.
	else if( NewPosition >= BoolTrack[NumOfKeys - 1].Time )
	{
		NewBoolValue = BoolTrack[NumOfKeys - 1].Value;
	}
	// Else, search through all the keys, looking for the 
	// keys that encompass the new timeline position.
	else
	{
		// Start iterating from the second key because we already 
		// determined if the new position is less than the first key.
		for( int32 KeyIndex = 1; KeyIndex < NumOfKeys; KeyIndex++ )
		{
			if( NewPosition < BoolTrack[KeyIndex].Time )
			{
				// We found the key that comes after the new position, 
				// use the value of the proceeding key. 
				NewBoolValue = BoolTrack[KeyIndex - 1].Value;
				break;
			}
		}
	}

	PropInst->BoolProperty->SetPropertyValue( PropInst->BoolPropertyAddress, NewBoolValue );

	// If we have a custom callback for this property, call that
	PropInst->CallPropertyUpdateCallback();
}

const FString UInterpTrackBoolProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackBoolPropHelper") );
}

const FString UInterpTrackBoolProp::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackBoolPropHelper") );
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstBoolProp
-----------------------------------------------------------------------------*/
UInterpTrackInstBoolProp::UInterpTrackInstBoolProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstBoolProp::InitTrackInst( UInterpTrack* Track )
{
	AActor* Actor = GetGroupActor();

	if( !Actor )
	{
		return;
	}

	// Store a pointer to the bitfield data for the property we will be interpolating.
	UInterpTrackBoolProp* TrackProp = CastChecked<UInterpTrackBoolProp>(Track);	
	BoolPropertyAddress = FMatineeUtils::GetInterpBoolPropertyRef(Actor, TrackProp->PropertyName, BoolProperty);

	SetupPropertyUpdateCallback(Actor, TrackProp->PropertyName);
}

void UInterpTrackInstBoolProp::SaveActorState( UInterpTrack* Track )
{
	if( !GetGroupActor() || !BoolPropertyAddress || !BoolProperty )
	{
		return;
	}

	// Remember current value of property for when we quit Matinee
	ResetBool = BoolProperty->GetPropertyValue(BoolPropertyAddress);
}

void UInterpTrackInstBoolProp::RestoreActorState( UInterpTrack* Track )
{
	AActor* Actor = GetGroupActor();

	if( !Actor || !BoolPropertyAddress || !BoolProperty )
	{
		return;
	}

	// Restore original value of property
	BoolProperty->SetPropertyValue(BoolPropertyAddress, ResetBool);

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ReregisterAllComponents(); // @todo UE4 insist on a property update callback, and use here?
}

/*-----------------------------------------------------------------------------
	UInterpTrackColorProp
-----------------------------------------------------------------------------*/
UInterpTrackColorProp::UInterpTrackColorProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	TrackInstClass = UInterpTrackInstColorProp::StaticClass();
	TrackTitle = TEXT("Color Property");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_ColorTrack.MAT_ColorTrack"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackColorProp::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstColorProp* PropInst = CastChecked<UInterpTrackInstColorProp>(TrInst);
	if( !PropInst->ColorProp )
		return INDEX_NONE;

	int32 NewKeyIndex = VectorTrack.AddPoint( Time, FVector::ZeroVector);
	VectorTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

	UpdateKeyframe(NewKeyIndex, TrInst);

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

bool UInterpTrackColorProp::CanAddKeyframe( UInterpTrackInst* TrackInst )
{
	UInterpTrackInstColorProp* PropInst = CastChecked<UInterpTrackInstColorProp>(TrackInst);
	return PropInst->ColorProp != NULL;
}

void UInterpTrackColorProp::UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst)
{
	UInterpTrackInstColorProp* PropInst = CastChecked<UInterpTrackInstColorProp>(TrInst);
	if( !PropInst->ColorProp )
	{
		return;
	}

	if( KeyIndex < 0 || KeyIndex >= VectorTrack.Points.Num() )
	{
		return;
	}

	FColor ColorValue = *PropInst->ColorProp;
	FLinearColor LinearValue(ColorValue);
	VectorTrack.Points[KeyIndex].OutVal = FVector(LinearValue.R, LinearValue.G, LinearValue.B);

	VectorTrack.AutoSetTangents(CurveTension);
}


void UInterpTrackColorProp::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, false);
}

void UInterpTrackColorProp::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstColorProp* PropInst = CastChecked<UInterpTrackInstColorProp>(TrInst);
	if(!PropInst->ColorProp)
	{
		return;
	}

	FColor DefaultColor = *PropInst->ColorProp;
	FLinearColor DefaultLinearColor = DefaultColor;
	FVector DefaultColorAsVector(DefaultLinearColor.R, DefaultLinearColor.G, DefaultLinearColor.B);
	FVector NewVectorValue = VectorTrack.Eval( NewPosition, DefaultColorAsVector );
	FColor NewColorValue = FLinearColor(NewVectorValue.X, NewVectorValue.Y, NewVectorValue.Z).ToFColor(true);
	*PropInst->ColorProp = NewColorValue;

	// If we have a custom callback for this property, call that
	PropInst->CallPropertyUpdateCallback();
}

const FString	UInterpTrackColorProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackColorPropHelper") );
}

const FString	UInterpTrackColorProp::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackColorPropHelper") );
}


/*-----------------------------------------------------------------------------
UInterpTrackInstColorProp
-----------------------------------------------------------------------------*/
UInterpTrackInstColorProp::UInterpTrackInstColorProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** Called before Interp editing to put object back to its original state. */
void UInterpTrackInstColorProp::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!ColorProp)
		return;

	// Remember current value of property for when we quite Matinee
	ResetColor = *ColorProp;
}

/** Restore the saved state of this Actor. */
void UInterpTrackInstColorProp::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!ColorProp)
		return;

	// Restore original value of property
	*ColorProp = ResetColor;

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ReregisterAllComponents(); // @todo UE4 insist on a property update callback, and use here?
}

/** Initialize this Track instance. Called in-game before doing any interpolation. */
void UInterpTrackInstColorProp::InitTrackInst(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackColorProp* TrackProp = Cast<UInterpTrackColorProp>(Track);
	ColorProp = FMatineeUtils::GetInterpColorPropertyRef(Actor, TrackProp->PropertyName);

	SetupPropertyUpdateCallback(Actor, TrackProp->PropertyName);
}



/*-----------------------------------------------------------------------------
UInterpTrackLinearColorProp
-----------------------------------------------------------------------------*/
UInterpTrackLinearColorProp::UInterpTrackLinearColorProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackInstClass = UInterpTrackInstLinearColorProp::StaticClass();
	TrackTitle = TEXT("LinearColor Property");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_ColorTrack.MAT_ColorTrack"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackLinearColorProp::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstLinearColorProp* PropInst = CastChecked<UInterpTrackInstLinearColorProp>(TrInst);
	if( !PropInst->ColorProp )
		return INDEX_NONE;

	int32 NewKeyIndex = LinearColorTrack.AddPoint( Time, FLinearColor(0.f, 0.f, 0.f, 1.f));
	LinearColorTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

	UpdateKeyframe(NewKeyIndex, TrInst);

	LinearColorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

bool UInterpTrackLinearColorProp::CanAddKeyframe( UInterpTrackInst* TrInst )
{
	UInterpTrackInstLinearColorProp* PropInst = CastChecked<UInterpTrackInstLinearColorProp>(TrInst);
	return PropInst->ColorProp != NULL;
}

void UInterpTrackLinearColorProp::UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst)
{
	UInterpTrackInstLinearColorProp* PropInst = CastChecked<UInterpTrackInstLinearColorProp>(TrInst);
	if( !PropInst->ColorProp )
	{
		return;
	}

	if( KeyIndex < 0 || KeyIndex >= LinearColorTrack.Points.Num() )
	{
		return;
	}

	FLinearColor ColorValue = *PropInst->ColorProp;
	LinearColorTrack.Points[KeyIndex].OutVal = ColorValue;

	LinearColorTrack.AutoSetTangents(CurveTension);
}


void UInterpTrackLinearColorProp::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, false);
}

void UInterpTrackLinearColorProp::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstLinearColorProp* PropInst = CastChecked<UInterpTrackInstLinearColorProp>(TrInst);
	if(!PropInst->ColorProp)
	{
		return;
	}

	*PropInst->ColorProp =  LinearColorTrack.Eval( NewPosition, *PropInst->ColorProp);

	// If we have a custom callback for this property, call that
	PropInst->CallPropertyUpdateCallback();
}

const FString	UInterpTrackLinearColorProp::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackLinearColorPropHelper") );
}

const FString	UInterpTrackLinearColorProp::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackLinearColorPropHelper") );
}

/*-----------------------------------------------------------------------------
UInterpTrackInstLinearColorProp
-----------------------------------------------------------------------------*/
UInterpTrackInstLinearColorProp::UInterpTrackInstLinearColorProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstLinearColorProp::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!ColorProp)
		return;

	// Remember current value of property for when we quite Matinee
	 ResetColor = *ColorProp;
}

void UInterpTrackInstLinearColorProp::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	if(!ColorProp)
		return;

	// Restore original value of property
	*ColorProp = ResetColor;

	// We update components, so things like draw scale take effect.
	// Don't force update all components unless we're in the editor.
	Actor->ReregisterAllComponents(); // @todo UE4 insist on a property update callback, and use here?
}

void UInterpTrackInstLinearColorProp::InitTrackInst(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackLinearColorProp* TrackProp = Cast<UInterpTrackLinearColorProp>(Track);
	ColorProp = FMatineeUtils::GetInterpLinearColorPropertyRef(Actor, TrackProp->PropertyName);

	SetupPropertyUpdateCallback(Actor, TrackProp->PropertyName);
}


/*-----------------------------------------------------------------------------
	UInterpTrackEvent
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackEvent, EventTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackEvent, EventTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackEvent, EventTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackEvent, EventTrack, Time)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackEvent, EventTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackEvent, EventTrack, Time, FEventTrackKey)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackEvent, EventTrack, Time, FEventTrackKey)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackEvent, EventTrack, Time)

UInterpTrackEvent::UInterpTrackEvent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TrackInstClass = UInterpTrackInstEvent::StaticClass();
	TrackTitle = TEXT("Event");
	bFireEventsWhenForwards = true;
	bFireEventsWhenBackwards = true;
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Event.MAT_Groups_Event"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackEvent::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	FEventTrackKey NewEventKey;
	NewEventKey.EventName = NAME_None;
	NewEventKey.Time = Time;

	// Find the correct index to insert this key.
	int32 i=0; for( i=0; i<EventTrack.Num() && EventTrack[i].Time < Time; i++);
	EventTrack.InsertUninitialized(i);
	EventTrack[i] = NewEventKey;

	// We don't update the AllEventNames array here, because the name has not yet been set
	// see UInterpTrackEventHelper::PostCreateKeyframe instead

	return i;
}

void UInterpTrackEvent::RemoveKeyframe(int32 KeyIndex)
{
	if( KeyIndex < 0 || KeyIndex >= EventTrack.Num() )
	{
		return;
	}
	EventTrack.RemoveAt(KeyIndex);

	UInterpGroup* Group = Cast<UInterpGroup>( GetOuter() );
	if ( Group )
	{
		UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );
		IData->Modify();
		IData->UpdateEventNames();
	}
}

void UInterpTrackEvent::PreviewUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );

	bool bJump = !( MatineeActor->bIsPlaying );
	UpdateTrack(NewPosition, TrInst, bJump);
}

void UInterpTrackEvent::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	UInterpTrackInstEvent* EventInst = CastChecked<UInterpTrackInstEvent>(TrInst);
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( EventInst->GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
	UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
	UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );

	// We'll consider playing events in reverse if we're either actively playing in reverse or if
	// we're in a paused state but forcing an update to an older position (scrubbing backwards in editor.)
	bool bIsPlayingBackwards =
		( MatineeActor->bIsPlaying && MatineeActor->bReversePlayback ) ||
		( bJump && !MatineeActor->bIsPlaying && NewPosition < EventInst->LastUpdatePosition );

	// Find the interval between last update and this to check events with.
	bool bFireEvents = true;

	if(bJump)
	{
		// If we are playing forwards, and the flag is set, fire events even if we are 'jumping'.
		if(bFireEventsWhenJumpingForwards && !bIsPlayingBackwards)
		{
			bFireEvents = true;
		}
		else
		{
			bFireEvents = false;
		}
	}

	// If playing sequence forwards.
	float MinTime, MaxTime;
	if(!bIsPlayingBackwards)
	{
		MinTime = EventInst->LastUpdatePosition;
		MaxTime = NewPosition;

		// Slight hack here.. if playing forwards and reaching the end of the sequence, force it over a little to ensure we fire events actually on the end of the sequence.
		if(MaxTime == IData->InterpLength)
		{
			MaxTime += (float)KINDA_SMALL_NUMBER;
		}

		if(!bFireEventsWhenForwards)
		{
			bFireEvents = false;
		}
	}
	// If playing sequence backwards.
	else
	{
		MinTime = NewPosition;
		MaxTime = EventInst->LastUpdatePosition;

		// Same small hack as above for backwards case.
		if(MinTime == 0.f)
		{
			MinTime -= (float)KINDA_SMALL_NUMBER;
		}

		if(!bFireEventsWhenBackwards)
		{
			bFireEvents = false;
		}
	}

	// If we should be firing events for this track...
	if(bFireEvents)
	{
		// See which events fall into traversed region.
		for(int32 i=0; i<EventTrack.Num(); i++)
		{
			float EventTime = EventTrack[i].Time;

			// Need to be slightly careful here and make behavior for firing events symmetric when playing forwards of backwards.
			bool bFireThisEvent = false;
			if(!bIsPlayingBackwards)
			{
				if( EventTime >= MinTime && EventTime < MaxTime )
				{
					bFireThisEvent = true;
				}
			}
			else
			{
				if( EventTime > MinTime && EventTime <= MaxTime )
				{
					bFireThisEvent = true;
				}
			}

			if( bFireThisEvent )
			{
				MatineeActor->NotifyEventTriggered(EventTrack[i].EventName, EventTime, bUseCustomEventName);
			}
		}
	}

	// Update LastUpdatePosition.
	EventInst->LastUpdatePosition = NewPosition;
}

const FString	UInterpTrackEvent::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackEventHelper") );
}

const FString UInterpTrackEvent::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackEventHelper") );
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstEvent
-----------------------------------------------------------------------------*/
UInterpTrackInstEvent::UInterpTrackInstEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstEvent::InitTrackInst(UInterpTrack* Track)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );

	LastUpdatePosition = MatineeActor->InterpPosition;
}


/*-----------------------------------------------------------------------------
	UInterpTrackDirector
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackDirector, CutTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackDirector, CutTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackDirector, CutTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackDirector, CutTrack, Time)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackDirector, CutTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackDirector, CutTrack, Time, FDirectorTrackCut)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackDirector, CutTrack)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackDirector, CutTrack, Time, FDirectorTrackCut)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackDirector, CutTrack, Time)

UInterpTrackDirector::UInterpTrackDirector(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

	bOnePerGroup = true;
	bDirGroupOnly = true;
	TrackInstClass = UInterpTrackInstDirector::StaticClass();
	TrackTitle = TEXT("Director");
	bSimulateCameraCutsOnClients = true;
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Director.MAT_Groups_Director"), NULL, LOAD_None, NULL ));
	PreviewCamera = NULL;
#endif // WITH_EDITORONLY_DATA
}

void UInterpTrackDirector::PostLoad()
{
	Super::PostLoad();

	//if shot names have not been assigned, do it now
	int32 i=0;
	for (i=0; i < GetNumKeyframes(); i++)
	{
		int32 ShotNum = CutTrack[i].ShotNumber;
		if (ShotNum == 0)
		{
			ShotNum = GenerateCameraShotNumber(i);
			CutTrack[i].ShotNumber = GenerateCameraShotNumber(i);
		} 
	}
}

int32 UInterpTrackDirector::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	FDirectorTrackCut NewCut;
	NewCut.TargetCamGroup = NAME_None;
	NewCut.TransitionTime = 0.f;
	NewCut.Time = Time;

	// Find the correct index to insert this cut.
	int32 i=0; for( i=0; i<CutTrack.Num() && CutTrack[i].Time < Time; i++);
	CutTrack.InsertUninitialized(i);
	CutTrack[i] = NewCut;
	
	//Generate a shot name
	int32 ShotNum = GenerateCameraShotNumber(i);
	CutTrack[i].ShotNumber = ShotNum;

	return i;
}

void UInterpTrackDirector::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrackInst)
{
#if WITH_EDITOR
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrackInst->GetOuter() );
	UInterpGroupDirector* DirGroup = CastChecked<UInterpGroupDirector>( GrInst->Group );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );

	// Update the camera actor as the track is scrubbed
	const bool TrackOrGroupSelected = ( IsSelected() | DirGroup->IsSelected() );
	if ( UpdatePreviewCamera( MatineeActor, TrackOrGroupSelected ) )
	{
		// Refresh the selected group actor (deselect previous actors, otherwise we'll have multiple cameras selected)
		const bool bDeselectActors = true;
		DirGroup->SelectGroupActor( GrInst, bDeselectActors );
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
bool UInterpTrackDirector::UpdatePreviewCamera(AMatineeActor* MatineeActor, bool bInIsSelected)
{
	check( MatineeActor );
#if WITH_EDITORONLY_DATA
	// If selected, get the viewed actor in the matinee
	AActor* Actor = bInIsSelected ? MatineeActor->FindViewedActor() : NULL;
	if ( PreviewCamera != Actor )
	{
		// Try casting it to a camera actor
		PreviewCamera = Cast< ACameraActor >( Actor );
		return ( PreviewCamera != NULL );	// true if new camera is selected
	}
#endif // WITH_EDITORONLY_DATA
	return false;
}
#endif // WITH_EDITOR

void UInterpTrackDirector::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	UInterpTrackInstDirector* DirInst = CastChecked<UInterpTrackInstDirector>(TrInst);
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );

	// Actor for a Director group should be a PlayerController.
	APlayerController* PC = Cast<APlayerController>(GrInst->GetGroupActor());
	if( PC )
	{
		AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
		// server is authoritative on viewtarget changes
		if (PC->Role == ROLE_Authority || MatineeActor->bClientSideOnly || bSimulateCameraCutsOnClients)
		{
			float CutTime, CutTransitionTime;
			FName ViewGroupName = GetViewedGroupName(NewPosition, CutTime, CutTransitionTime);
			// if our group name was specified, make sure we use ourselves instead of any other instances with that name (there might be multiple in the multiplayer case)
			UInterpGroupInst* ViewGroupInst = (ViewGroupName == GrInst->Group->GroupName) ? GrInst : MatineeActor->FindFirstGroupInstByName(ViewGroupName.ToString());
			
			AActor* ViewTarget = PC->GetViewTarget();
			if( ViewGroupInst && ViewGroupInst->GetGroupActor() && (ViewGroupInst->GetGroupActor() != PC) )
			{
				// If our desired view target is different from our current one...
				if( ViewTarget != ViewGroupInst->GroupActor )
				{
					// If we don't have a backed up ViewTarget, back up this one.
					if( !DirInst->OldViewTarget )
					{
						// If the actor's current view target is a director track camera, then we want to store 
						// the director track's 'old view target' in case the current Matinee sequence finishes
						// before our's does.
						UInterpTrackInstDirector* PreviousDirInst = PC->GetControllingDirector();
						if( PreviousDirInst != NULL && PreviousDirInst->OldViewTarget != NULL )
						{
							// Store the underlying director track's old view target so we can restore this later
							DirInst->OldViewTarget = PreviousDirInst->OldViewTarget;
						}
						else
						{
							DirInst->OldViewTarget = ViewTarget;
						}
					}

					PC->SetControllingDirector(DirInst, bSimulateCameraCutsOnClients);
	
					PC->NotifyDirectorControl(true, MatineeActor); 

					//UE_LOG(LogMatinee, Log, TEXT("UInterpTrackDirector::UpdateTrack SetViewTarget ViewGroupInst->GroupActor Time:%f Name: %s"), PC->GetWorld()->GetTimeSeconds(), *ViewGroupInst->GroupActor->GetFName());
					// Change view to desired view target.
					FViewTargetTransitionParams TransitionParams;
					TransitionParams.BlendTime = CutTransitionTime;

					// a bit ugly here, but we don't want this particular SetViewTarget to bash OldViewTarget
					AActor* const BackupViewTarget = DirInst->OldViewTarget;
					PC->SetViewTarget( ViewGroupInst->GroupActor, TransitionParams);
					
					if ( PC->PlayerCameraManager )
					{
						PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
					}
					
					DirInst->OldViewTarget = BackupViewTarget;
				}
			}
			// If assigning to nothing or the PlayerController, restore any backed up viewtarget.
			else if (DirInst->OldViewTarget != NULL)
			{
				//UE_LOG(LogMatinee, Log, TEXT("UInterpTrackDirector::UpdateTrack SetViewTarget DirInst->OldViewTarget Time:%f Name: %s"), PC->GetWorld()->GetTimeSeconds(), *DirInst->OldViewTarget->GetFName());
				if (!DirInst->OldViewTarget->IsPendingKill())
				{
					FViewTargetTransitionParams TransitionParams;
					TransitionParams.BlendTime = CutTransitionTime;
					PC->SetViewTarget( DirInst->OldViewTarget, TransitionParams );
				}

				PC->NotifyDirectorControl(false, MatineeActor); 
				PC->SetControllingDirector(NULL, false);
				
				DirInst->OldViewTarget = NULL;
			}
		}
	}
}

int32 UInterpTrackDirector::GetNearestKeyframeIndex( float KeyTime ) const
{
	int32 PrevKeyIndex = INDEX_NONE; // Index of key before current time.
	if(CutTrack.Num() > 0 && CutTrack[0].Time < KeyTime)
	{
		for( int32 i=0; i < CutTrack.Num() && CutTrack[i].Time <= KeyTime; i++)
		{
			PrevKeyIndex = i;
		}
	}

	return PrevKeyIndex;
}

FName UInterpTrackDirector::GetViewedGroupName(float CurrentTime, float& CutTime, float& CutTransitionTime)
{
	int32 KeyIndex = GetNearestKeyframeIndex(CurrentTime);
	// If no index found - we are before first frame (or no frames present), so use the director group name.
	if(KeyIndex == INDEX_NONE)
	{
		CutTime = 0.f;
		CutTransitionTime = 0.f;

		UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
		return Group->GroupName;
	}
	else
	{
		CutTime = CutTrack[KeyIndex].Time;
		CutTransitionTime = CutTrack[KeyIndex].TransitionTime;

		return CutTrack[KeyIndex].TargetCamGroup;
	}
}


const FString UInterpTrackDirector::GetViewedCameraShotName(float CurrentTime) const
{
	FString ShotName = TEXT("");
   
	int32 KeyIndex = GetNearestKeyframeIndex(CurrentTime);
	if (KeyIndex != INDEX_NONE)
	{
		ShotName = GetFormattedCameraShotName(KeyIndex);
	}
	return ShotName;
}


const FString	UInterpTrackDirector::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackDirectorHelper") );
}

const FString	UInterpTrackDirector::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackDirectorHelper") );
}

const int32 UInterpTrackDirector::GenerateCameraShotNumber(int32 KeyIndex) const
{
	//NOTE: this could give us an off by 1 error depending on when it is called.
	//The current implementation assumes the shot is already inserted int CutTrackArray
	
	const int32 Interval = 10;
	int32 ShotNum = Interval;
	int32 LastKeyIndex = GetNumKeyframes() - 1;
	
	int32 PrevShotNum = 0;
	//get the preceding shot number if any
	if (KeyIndex > 0)
	{
		PrevShotNum = CutTrack[KeyIndex - 1].ShotNumber; 
	}
   
	if (KeyIndex < LastKeyIndex)
	{
		//we're inserting before something before the first frame
		int32 NextShotNum = CutTrack[KeyIndex + 1].ShotNumber;
		if (NextShotNum == 0)
		{
			NextShotNum = PrevShotNum + (Interval*2);
		}
		
		if (NextShotNum > PrevShotNum)
		{
			//find a midpoint if we're in order

			//try to stick to the nearest interval if possible
			int32 NearestInterval = PrevShotNum - (PrevShotNum % Interval) + Interval;
			if (NearestInterval > PrevShotNum && NearestInterval < NextShotNum)
			{
				ShotNum = NearestInterval;
			}
			//else find the exact mid point
			else
			{
				ShotNum = ((NextShotNum - PrevShotNum) / 2) + PrevShotNum;
			}
		}
		else
		{
			//Just use the previous shot number + 1 with we're out of order
			ShotNum = PrevShotNum + 1;
		}
	}
	else
	{   
		//we're adding to the end of the track
		ShotNum = PrevShotNum + Interval;
	}

	return ShotNum;
}

const FString UInterpTrackDirector::GetFormattedCameraShotName(int32 KeyIndex) const
{
	int32 ShotNum = CutTrack[KeyIndex].ShotNumber;
	FString NameString = TEXT("Shot_");
	FString NumString = FString::Printf(TEXT("%d"),ShotNum);
	int32 LEN = NumString.Len();
	for (int i=0; i<(4 - LEN); i++)
	{
		NameString += TEXT("0");
	} 
	NameString += NumString;
	return NameString;
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstDirector
-----------------------------------------------------------------------------*/
UInterpTrackInstDirector::UInterpTrackInstDirector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstDirector::TermTrackInst(UInterpTrack* Track)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>(GetOuter());
	APlayerController* PC = Cast<APlayerController>(GrInst->GetGroupActor());
	if (PC != NULL)
	{
		AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
		if (OldViewTarget != NULL && !OldViewTarget->IsPendingKill())
		{
			// if we haven't already, restore original view target.
			AActor* ViewTarget = PC->GetViewTarget();
			if (ViewTarget != OldViewTarget)
			{
				PC->SetViewTarget(OldViewTarget);
			}
		}
		// this may be a duplicate call if it was already called in UpdateTrack(), but that's better than not at all and leaving code thinking we stayed in matinee forever
		PC->NotifyDirectorControl(false, MatineeActor);
		PC->SetControllingDirector(NULL, false);
	}
	
	OldViewTarget = NULL;

	Super::TermTrackInst(Track);
}


/*-----------------------------------------------------------------------------
	UInterpTrackFade
-----------------------------------------------------------------------------*/
UInterpTrackFade::UInterpTrackFade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FadeColor = FLinearColor::Black;
	
	bOnePerGroup = true;
	bDirGroupOnly = true;
	TrackInstClass = UInterpTrackInstFade::StaticClass();
	TrackTitle = TEXT("Fade");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Fade.MAT_Groups_Fade"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackFade::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	int32 NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackFade::UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst)
{
	// Do nothing here - fading is all set up through curve editor.
}

void UInterpTrackFade::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	// Do nothing - in the editor Matinee itself handles updating the editor viewports.
}

void UInterpTrackFade::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	// when doing a skip in game, don't update fading - we only want it applied when actually running
	if (!bJump || !FApp::IsGame())
	{
		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );

		// Actor for a Director group should be a PlayerController.
		APlayerController* PC = Cast<APlayerController>(GrInst->GetGroupActor());
		if(PC && PC->PlayerCameraManager && !PC->PlayerCameraManager->IsPendingKill())
		{
			PC->PlayerCameraManager->SetManualCameraFade(GetFadeAmountAtTime(NewPosition), FadeColor, bFadeAudio);
		}
	}
}

float UInterpTrackFade::GetFadeAmountAtTime(float Time)
{
	float Fade = FloatTrack.Eval(Time, 0.f);
	Fade = FMath::Clamp(Fade, 0.f, 1.f);
	return Fade;
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstFade
-----------------------------------------------------------------------------*/
UInterpTrackInstFade::UInterpTrackInstFade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstFade::TermTrackInst(UInterpTrack* Track)
{
	UInterpTrackFade *FadeTrack = Cast<UInterpTrackFade>(Track);
	if (FadeTrack == NULL || !FadeTrack->bPersistFade)
	{
		UInterpGroupInst* const GrInst = CastChecked<UInterpGroupInst>(GetOuter());
		APlayerController* const PC = Cast<APlayerController>(GrInst->GroupActor);
		if(PC && PC->PlayerCameraManager && !PC->PlayerCameraManager->IsPendingKill())
		{
			PC->PlayerCameraManager->StopCameraFade();

			// if the player is remote, ensure they got it
			// this handles cases where the LDs stream out this level immediately afterwards,
			// which can mean the client never gets the matinee replication if it was temporarily unresponsive
			if (!PC->IsLocalPlayerController())
			{
				PC->ClientSetCameraFade(false);
			}
		}
	}

	Super::TermTrackInst(Track);
}

/*-----------------------------------------------------------------------------
	UInterpTrackSlomo
-----------------------------------------------------------------------------*/
UInterpTrackSlomo::UInterpTrackSlomo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bOnePerGroup = true;
	bDirGroupOnly = true;
	TrackInstClass = UInterpTrackInstSlomo::StaticClass();
	TrackTitle = TEXT("Slomo");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Slomo.MAT_Groups_Slomo"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}


int32 UInterpTrackSlomo::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	int32 NewKeyIndex = FloatTrack.AddPoint( Time, 1.0f );
	FloatTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackSlomo::UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst)
{
	// Do nothing here - slomo is all set up through curve editor.
}

void UInterpTrackSlomo::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	// Do nothing - in the editor Matinee itself handles updating the editor viewports.
}

void UInterpTrackSlomo::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	check( TrInst );
	// do nothing if we're the client, as the server will replicate TimeDilation
	if (CastChecked<UInterpTrackInstSlomo>(TrInst)->ShouldBeApplied())
	{
		AWorldSettings* WorldSettings = TrInst->GetWorld()->GetWorldSettings();
		WorldSettings->MatineeTimeDilation = GetSlomoFactorAtTime(NewPosition);
		WorldSettings->ForceNetUpdate();
	}
}

float UInterpTrackSlomo::GetSlomoFactorAtTime(float Time)
{
	float Slomo = FloatTrack.Eval(Time, 0.f);
	Slomo = FMath::Max(Slomo, KINDA_SMALL_NUMBER);
	return Slomo;
}

void UInterpTrackSlomo::SetTrackToSensibleDefault()
{
	FloatTrack.Points.Empty();
	FloatTrack.AddPoint(0.f, 1.f);
}	

/*-----------------------------------------------------------------------------
	UInterpTrackInstSlomo
-----------------------------------------------------------------------------*/
UInterpTrackInstSlomo::UInterpTrackInstSlomo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstSlomo::SaveActorState(UInterpTrack* Track)
{
	OldTimeDilation = GetWorld()->GetWorldSettings()->MatineeTimeDilation;
}

void UInterpTrackInstSlomo::RestoreActorState(UInterpTrack* Track)
{
	GetWorld()->GetWorldSettings()->MatineeTimeDilation = OldTimeDilation;
}

bool UInterpTrackInstSlomo::ShouldBeApplied()
{
	if (GIsEditor)
	{
		return true;
	}
	else if (GetWorld()->GetNetMode() == NM_Client)
	{
		return false;
	}
	else
	{
		// if GroupActor is NULL, then this is the instance created on a dedicated server when no players were around
		// otherwise, check that GroupActor is the first player
		AActor* GroupActor = GetGroupActor();
		return (GroupActor == NULL || (GEngine != NULL && GEngine->GetFirstGamePlayer(GetWorld()) && GEngine->GetFirstGamePlayer(GetWorld())->PlayerController == GroupActor));
	}
}

void UInterpTrackInstSlomo::InitTrackInst(UInterpTrack* Track)
{
	// do nothing if we're the client, as the server will replicate TimeDilation
	if (ShouldBeApplied())
	{
		OldTimeDilation = GetWorld()->GetWorldSettings()->MatineeTimeDilation;
	}
}

void UInterpTrackInstSlomo::TermTrackInst(UInterpTrack* Track)
{
	// do nothing if we're the client, as the server will replicate TimeDilation
	if (ShouldBeApplied())
	{
		AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();
		if(OldTimeDilation <= 0.f)
		{
			UE_LOG(LogMatinee, Warning, TEXT("WARNING! OldTimeDilation was not initialized in %s!  Setting to 1.0f"),*GetPathName());
			OldTimeDilation = 1.0f;
		}
		WorldSettings->MatineeTimeDilation = OldTimeDilation;
		WorldSettings->ForceNetUpdate();
	}

	Super::TermTrackInst(Track);
}

/*-----------------------------------------------------------------------------
	UInterpTrackAnimControl
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackAnimControl, AnimSeqs)
STRUCTTRACK_GETTIMERANGE(UInterpTrackAnimControl, AnimSeqs, StartTime)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackAnimControl, AnimSeqs, StartTime)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackAnimControl, AnimSeqs, StartTime)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackAnimControl, AnimSeqs, StartTime, FAnimControlTrackKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackAnimControl, AnimSeqs)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackAnimControl, AnimSeqs, StartTime, FAnimControlTrackKey)

UInterpTrackAnimControl::UInterpTrackAnimControl(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	TrackInstClass = UInterpTrackInstAnimControl::StaticClass();
	TrackTitle = TEXT("Anim");
	bIsAnimControlTrack = true;
	SlotName = FAnimSlotGroup::DefaultSlotName;
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Anim.MAT_Groups_Anim"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

void UInterpTrackAnimControl::PostLoad()
{
	Super::PostLoad();

	// Fix any anims with zero play rate.
	for(int32 i=0; i<AnimSeqs.Num(); i++)
	{
		if(AnimSeqs[i].AnimPlayRate < 0.001f)
		{
			AnimSeqs[i].AnimPlayRate = 1.f;
		}
	}
}

int32 UInterpTrackAnimControl::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	FAnimControlTrackKey NewSeq;
	NewSeq.AnimSeq = NULL;
	NewSeq.bLooping = false;
	NewSeq.AnimStartOffset = 0.f;
	NewSeq.AnimEndOffset = 0.f;
	NewSeq.AnimPlayRate = 1.f;
	NewSeq.StartTime = Time;
	NewSeq.bReverse = false;

	// Find the correct index to insert this cut.
	int32 i=0; for( i=0; i<AnimSeqs.Num() && AnimSeqs[i].StartTime < Time; i++);
	AnimSeqs.InsertUninitialized(i);
	AnimSeqs[i] = NewSeq;

	// FIXME: set weight to be 1 for AI group by default (curve editor value to be 1)

	return i;
}

bool UInterpTrackAnimControl::GetClosestSnapPosition(float InPosition, TArray<int32> &IgnoreKeys, float& OutPosition)
{
	if(AnimSeqs.Num() == 0)
	{
		return false;
	}

	bool bFoundSnap = false;
	float ClosestSnap = 0.f;
	float ClosestDist = BIG_NUMBER;
	for(int32 i=0; i<AnimSeqs.Num(); i++)
	{
		if(!IgnoreKeys.Contains(i))
		{
			float SeqStartTime = AnimSeqs[i].StartTime;
			float SeqEndTime = SeqStartTime;

			float SeqLength = 0.f;
			UAnimSequence* Seq = AnimSeqs[i].AnimSeq;
			if(Seq)
			{
				SeqLength = FMath::Max((Seq->SequenceLength - (AnimSeqs[i].AnimStartOffset + AnimSeqs[i].AnimEndOffset)) / AnimSeqs[i].AnimPlayRate, 0.01f);
				SeqEndTime += SeqLength;
			}

			// If there is a sequence following this one - we stop drawing this block where the next one begins.
			if((i < AnimSeqs.Num()-1) && !IgnoreKeys.Contains(i+1))
			{
				SeqEndTime = FMath::Min( AnimSeqs[i+1].StartTime, SeqEndTime );
			}

			float Dist = FMath::Abs( SeqStartTime - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = SeqStartTime;
				ClosestDist = Dist;
				bFoundSnap = true;
			}

			Dist = FMath::Abs( SeqEndTime - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = SeqEndTime;
				ClosestDist = Dist;
				bFoundSnap = true;
			}
		}
	}

	OutPosition = ClosestSnap;
	return bFoundSnap;
}

FColor UInterpTrackAnimControl::GetKeyframeColor(int32 KeyIndex) const
{
	return FColor(0,0,0);
}

float UInterpTrackAnimControl::GetTrackEndTime() const
{
	float EndTime = 0.0f;

	if( AnimSeqs.Num() )
	{
		// Since the keys are sorted in chronological order, choose the 
		// last anim key on the track to find the track end time.
		const FAnimControlTrackKey& AnimKey = AnimSeqs[ AnimSeqs.Num()-1 ];

		// The end time should be no less than the 
		// timeline position of the anim key.
		EndTime = AnimKey.StartTime;

		// If there is a valid anim sequence, add the total time of the 
		// anim, accounting for factors, such as: offsets and play rate. 
		const UAnimSequence* AnimSequence = AnimKey.AnimSeq;
		if( AnimSequence )
		{
			// When calculating the end time, we do not consider the AnimStartOffset since we 
			// are not calculating the length of the anim key. We just want the time where it ends.
			EndTime += FMath::Max( (AnimSequence->SequenceLength - AnimKey.AnimEndOffset) / AnimKey.AnimPlayRate, 0.01f );
		}

	}

	return EndTime;
}


bool UInterpTrackAnimControl::GetAnimForTime(float InTime, UAnimSequence** OutAnimSequencePtr, float& OutPosition, bool& bOutLooping)
{
	bool bResetTime = false;

	if(AnimSeqs.Num() == 0)
	{
		(*OutAnimSequencePtr) = NULL;
		OutPosition = 0.f;
	}
	else
	{
		if(InTime < AnimSeqs[0].StartTime)
		{
			(*OutAnimSequencePtr) = AnimSeqs[0].AnimSeq;
			OutPosition = AnimSeqs[0].AnimStartOffset;
			// Reverse position if the key is set to be reversed.
			if(AnimSeqs[0].bReverse)
			{
				UAnimSequence *Seq = AnimSeqs[0].AnimSeq;

				if(Seq)
				{
					OutPosition = ConditionallyReversePosition(AnimSeqs[0], Seq, OutPosition);
				}

				bOutLooping = AnimSeqs[0].bLooping;
			}

			// animation didn't start yet
			bResetTime = true; 
		}
		else
		{
			int32 i=0; for( i=0; i<AnimSeqs.Num()-1 && AnimSeqs[i+1].StartTime <= InTime; i++);

			(*OutAnimSequencePtr) = AnimSeqs[i].AnimSeq;
			OutPosition = ((InTime - AnimSeqs[i].StartTime) * AnimSeqs[i].AnimPlayRate);

			UAnimSequence *Seq = AnimSeqs[i].AnimSeq;
			if(Seq)
			{
				float SeqLength = FMath::Max(Seq->SequenceLength - (AnimSeqs[i].AnimStartOffset + AnimSeqs[i].AnimEndOffset), 0.01f);

				if(AnimSeqs[i].bLooping)
				{
					OutPosition = FMath::Fmod(OutPosition, SeqLength);
					OutPosition += AnimSeqs[i].AnimStartOffset;
				}
				else
				{
					OutPosition = FMath::Clamp(OutPosition + AnimSeqs[i].AnimStartOffset, 0.f, (Seq->SequenceLength - AnimSeqs[i].AnimEndOffset) + (float)KINDA_SMALL_NUMBER);
				}

				// Reverse position if the key is set to be reversed.
				if(AnimSeqs[i].bReverse)
				{
					OutPosition = ConditionallyReversePosition(AnimSeqs[i], Seq, OutPosition);
					bResetTime = (OutPosition == (Seq->SequenceLength - AnimSeqs[i].AnimEndOffset));
				}
				else
				{
					bResetTime = (OutPosition == AnimSeqs[i].AnimStartOffset);
				}

				bOutLooping = AnimSeqs[i].bLooping;
			}
		}
	}

	return bResetTime;
}

float UInterpTrackAnimControl::GetWeightForTime(float InTime)
{
	return FloatTrack.Eval(InTime, 0.f);
}



float UInterpTrackAnimControl::ConditionallyReversePosition(FAnimControlTrackKey &SeqKey, UAnimSequence* Seq, float InPosition)
{
	float Result = InPosition;

	// Reverse position if the key is set to be reversed.
	if(SeqKey.bReverse)
	{
		if(Seq == NULL)
		{
			Seq = SeqKey.AnimSeq;
		}

		// Reverse the clip.
		if(Seq)
		{
			const float RealLength = Seq->SequenceLength - (SeqKey.AnimStartOffset+SeqKey.AnimEndOffset);
			Result = (RealLength - (InPosition-SeqKey.AnimStartOffset))+SeqKey.AnimStartOffset;	// Mirror the cropped clip.
		}
	}

	return Result;
}

void UInterpTrackAnimControl::PreviewUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	UInterpTrackInstAnimControl* AnimInst = CastChecked<UInterpTrackInstAnimControl>(TrInst);

	// Calculate this channels index within the named slot.
	int32 ChannelIndex = CalcChannelIndex();

	UAnimSequence* NewAnimSeq = NULL;

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>(TrInst->GetOuter());
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>(GrInst->GetOuter());
	bool bJump = !( MatineeActor->bIsPlaying );
	float NewAnimPosition, TimeElapsed = (bJump || NewPosition < AnimInst->LastUpdatePosition)? 0.f : NewPosition - AnimInst->LastUpdatePosition;
	bool bNewLooping;
	bool bResetTime = GetAnimForTime(NewPosition, &NewAnimSeq, NewAnimPosition, bNewLooping);

	if( NewAnimSeq != NULL )
	{
		// if we're going backward or if not @ the first frame of the animation
		bool bFireNotifier = !bSkipAnimNotifiers && (!bResetTime) ;
		IMatineeAnimInterface * IMAI = Cast<IMatineeAnimInterface>(Actor);
		if (IMAI)
		{
			IMAI->PreviewSetAnimPosition(SlotName, ChannelIndex, NewAnimSeq, NewAnimPosition, bNewLooping, bFireNotifier, TimeElapsed);
		}
		AnimInst->LastUpdatePosition = NewPosition;
	}
}

void UInterpTrackAnimControl::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if(!Actor)
	{
		return;
	}

	//UE_LOG(LogMatinee, Log, TEXT("UInterpTrackAnimControl::UpdateTrack, NewPosition: %3.2f"), NewPosition);

	UInterpTrackInstAnimControl* AnimInst = CastChecked<UInterpTrackInstAnimControl>(TrInst);
	IMatineeAnimInterface * IMAI = Cast<IMatineeAnimInterface>(Actor);
	if (!IMAI)
	{
		// Actor does not support Matinee Anim Interface
		return;
	}
	// Calculate this channels index within the named slot.
	int32 ChannelIndex = CalcChannelIndex();

	// Don't do complicated stuff for notifies if playing backwards, or not moving at all.
	if(AnimSeqs.Num() == 0 || NewPosition <= AnimInst->LastUpdatePosition || bJump)
	{
		UAnimSequence* NewAnimSequence = NULL;
		float NewAnimPosition = 0.0f;
		bool bNewLooping = false;
		GetAnimForTime(NewPosition, &NewAnimSequence, NewAnimPosition, bNewLooping);

		if( NewAnimSequence != NULL )
		{
			IMAI->SetAnimPosition(SlotName, ChannelIndex, NewAnimSequence, NewAnimPosition, false, bNewLooping);
		}
	}
	// Playing forwards - need to do painful notify stuff.
	else
	{
		// Find which anim we are starting in. -1 Means before first anim.
		int32 StartSeqIndex = -1; 
		for( StartSeqIndex = -1; StartSeqIndex<AnimSeqs.Num()-1 && AnimSeqs[StartSeqIndex+1].StartTime <= AnimInst->LastUpdatePosition; StartSeqIndex++);

		// Find which anim we are ending in. -1 Means before first anim.
		int32 EndSeqIndex = -1; 
		for( EndSeqIndex = -1; EndSeqIndex<AnimSeqs.Num()-1 && AnimSeqs[EndSeqIndex+1].StartTime <= NewPosition; EndSeqIndex++);

		// Now start walking from the first block.
		int32 CurrentSeqIndex = StartSeqIndex;
		while(CurrentSeqIndex <= EndSeqIndex)
		{
			// If we are before the first anim - do nothing but set ourselves to the beginning of the first anim.
			if(CurrentSeqIndex == -1)
			{
				FAnimControlTrackKey &SeqKey = AnimSeqs[0];
				float Position = SeqKey.AnimStartOffset;

				// Reverse position if the key is set to be reversed.
				if( SeqKey.bReverse )
				{
					Position = ConditionallyReversePosition(SeqKey, NULL, Position);
				}

				if( SeqKey.AnimSeq != NULL )
				{
					IMAI->SetAnimPosition(SlotName, ChannelIndex, SeqKey.AnimSeq, Position, false, SeqKey.bLooping);
				}
			}
			// If we are within an anim.
			else
			{
				// Find the name and starting time
				FAnimControlTrackKey &AnimSeq = AnimSeqs[CurrentSeqIndex];
				UAnimSequence* CurrentAnimSequence = AnimSeq.AnimSeq;
				float CurrentSeqStart = AnimSeq.StartTime;
				float CurrentStartOffset = AnimSeq.AnimStartOffset;
				float CurrentEndOffset = AnimSeq.AnimEndOffset;
				float CurrentRate = AnimSeq.AnimPlayRate;

				// Find the time we are currently at.
				// If this is the first start anim - its the 'current' position of the Matinee.
				float FromTime = (CurrentSeqIndex == StartSeqIndex) ? AnimInst->LastUpdatePosition : CurrentSeqStart;

				// Find the time we want to move to.
				// If this is the last anim - its the 'new' position of the Matinee. Otherwise, its the start of the next anim.
				// Safe to address AnimSeqs at CurrentSeqIndex+1 in the second case, as it must be <EndSeqIndex and EndSeqIndex<AnimSeqs.Num().
				float ToTime = (CurrentSeqIndex == EndSeqIndex) ? NewPosition : AnimSeqs[CurrentSeqIndex+1].StartTime; 
				
				// If looping, we need to play through the sequence multiple times, to ensure notifies are execute correctly.
				if( AnimSeq.bLooping )
				{
					UAnimSequence* Seq = CurrentAnimSequence;
					if(Seq)
					{
						// Find position we should not play beyond in this sequence.
						float SeqEnd = (Seq->SequenceLength - CurrentEndOffset);

						// Find time this sequence will take to play
						float SeqLength = FMath::Max(Seq->SequenceLength - (CurrentStartOffset + CurrentEndOffset), 0.01f);

						// Find the number of loops we make. 
						// @todo: This will need to be updated if we decide to support notifies in reverse.
						if(AnimSeq.bReverse == false)
						{
							int32 FromLoopNum = FMath::FloorToInt( (((FromTime - CurrentSeqStart) * CurrentRate) + CurrentStartOffset)/SeqLength );
							int32 ToLoopNum = FMath::FloorToInt( (((ToTime - CurrentSeqStart) * CurrentRate) + CurrentStartOffset)/SeqLength );
							int32 NumLoopsToJump = ToLoopNum - FromLoopNum;

							for(int32 i=0; i<NumLoopsToJump; i++)
							{
								IMAI->SetAnimPosition(SlotName, ChannelIndex, Seq, SeqEnd + KINDA_SMALL_NUMBER, true, true);
								IMAI->SetAnimPosition(SlotName, ChannelIndex, Seq, CurrentStartOffset, false, true);
							}
						}

						float AnimPos = FMath::Fmod((ToTime - CurrentSeqStart) * CurrentRate, SeqLength) + CurrentStartOffset;

						// Reverse position if the key is set to be reversed.
						if( AnimSeq.bReverse )
						{
							AnimPos = ConditionallyReversePosition(AnimSeq, Seq, AnimPos);
						}

						if( Seq != NULL )
						{
							IMAI->SetAnimPosition(SlotName, ChannelIndex, Seq, AnimPos, !bSkipAnimNotifiers, true);
						}
					}
				}
				// No looping or reversed - its easy - wind to desired time.
				else
				{
					float AnimPos = ((ToTime - CurrentSeqStart) * CurrentRate) + CurrentStartOffset;

					UAnimSequence* Seq = CurrentAnimSequence;
					if( Seq )
					{
						float SeqEnd = (Seq->SequenceLength - CurrentEndOffset);
						AnimPos = FMath::Clamp( AnimPos, 0.f, SeqEnd + (float)KINDA_SMALL_NUMBER );
					}

					// Conditionally reverse the position.
					AnimPos = ConditionallyReversePosition(AnimSeq, Seq, AnimPos);

					if( Seq )
					{
						// if Current Animation Position == StartOffset, that means we clear all PreviousTime and new Time, 
						// jump there - bFireNotifier == false will clear PreviousTime and CurrentTime to match
						IMAI->SetAnimPosition(SlotName, ChannelIndex, Seq, AnimPos, (AnimPos != CurrentStartOffset)?!bSkipAnimNotifiers:false , false);
					}
				}

				// If we are not yet at target anim, set position at start of next anim.
				if( CurrentSeqIndex < EndSeqIndex )
				{
					FAnimControlTrackKey &SeqKey = AnimSeqs[CurrentSeqIndex+1];
					float Position = SeqKey.AnimStartOffset;

					// Conditionally reverse the position.
					if( SeqKey.bReverse )
					{
						Position = ConditionallyReversePosition(SeqKey, NULL, Position);
					}

					if( SeqKey.AnimSeq != NULL )
					{
						IMAI->SetAnimPosition(SlotName, ChannelIndex, SeqKey.AnimSeq, Position, false, SeqKey.bLooping);
					}
				}
			}

			// Move on the CurrentSeqIndex counter.
			CurrentSeqIndex++;
		}
	}

	// Now remember the location we are at this frame, to use as the 'From' time next frame.
	AnimInst->LastUpdatePosition = NewPosition;
}


int32 UInterpTrackAnimControl::SplitKeyAtPosition(float InPosition)
{
	// Check we are over a valid animation
	int32 SplitSeqIndex = -1; 
	for( SplitSeqIndex = -1; SplitSeqIndex < AnimSeqs.Num()-1 && AnimSeqs[SplitSeqIndex+1].StartTime <= InPosition; SplitSeqIndex++ );
	if(SplitSeqIndex == -1)
	{
		return INDEX_NONE;
	}

	// Check the sequence is valid.
	FAnimControlTrackKey& SplitKey = AnimSeqs[SplitSeqIndex];
	UAnimSequence* Seq = SplitKey.AnimSeq;
	if(!Seq)
	{
		return INDEX_NONE;
	}

	// Check we are over an actual chunk of sequence.
	float SplitAnimPos = ((InPosition - SplitKey.StartTime) * SplitKey.AnimPlayRate) + SplitKey.AnimStartOffset;
	if(SplitAnimPos <= SplitKey.AnimStartOffset || SplitAnimPos >= (Seq->SequenceLength - SplitKey.AnimEndOffset))
	{
		return INDEX_NONE;
	}

	// Create new Key.
	FAnimControlTrackKey NewKey;
	NewKey.AnimPlayRate = SplitKey.AnimPlayRate;
	NewKey.AnimSeq = SplitKey.AnimSeq;
	NewKey.StartTime = InPosition;
	NewKey.bLooping = SplitKey.bLooping;
	NewKey.AnimStartOffset = SplitAnimPos; // Start position in the new animation wants to be the place we are currently at.
	NewKey.AnimEndOffset = SplitKey.AnimEndOffset; // End place is the same as the one we are splitting.

	SplitKey.AnimEndOffset = Seq->SequenceLength - SplitAnimPos; // New end position is where we are.
	SplitKey.bLooping = false; // Disable looping for section before the cut.

	// Add new key to track.
	AnimSeqs.InsertZeroed(SplitSeqIndex+1);
	AnimSeqs[SplitSeqIndex+1] = NewKey;

	return SplitSeqIndex+1;
}


int32 UInterpTrackAnimControl::CropKeyAtPosition(float InPosition, bool bCutAreaBeforePosition)
{
	// Check we are over a valid animation
	int32 SplitSeqIndex = -1; 
	for( SplitSeqIndex = -1; SplitSeqIndex < AnimSeqs.Num()-1 && AnimSeqs[SplitSeqIndex+1].StartTime <= InPosition; SplitSeqIndex++ );
	if(SplitSeqIndex == -1)
	{
		return INDEX_NONE;
	}

	// Check the sequence is valid.
	FAnimControlTrackKey& SplitKey = AnimSeqs[SplitSeqIndex];
	UAnimSequence* Seq = SplitKey.AnimSeq;
	if(!Seq)
	{
		return INDEX_NONE;
	}

	// Check we are over an actual chunk of sequence.
	float SplitAnimPos = ((InPosition - SplitKey.StartTime) * SplitKey.AnimPlayRate) + SplitKey.AnimStartOffset;
	if(SplitAnimPos <= SplitKey.AnimStartOffset || SplitAnimPos >= (Seq->SequenceLength - SplitKey.AnimEndOffset))
	{
		return INDEX_NONE;
	}

	// Crop either left or right depending on which way the user wants to crop.
	if(bCutAreaBeforePosition)
	{
		SplitKey.StartTime = InPosition;
		SplitKey.AnimStartOffset = SplitAnimPos; // New end position is where we are.
	}
	else
	{
		SplitKey.AnimEndOffset = Seq->SequenceLength - SplitAnimPos; // New end position is where we are.
	}

	return SplitSeqIndex;
}

int32 UInterpTrackAnimControl::CalcChannelIndex()
{
	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());

	// Count number of tracks with same slot name before we reach this one
	int32 ChannelIndex = 0;
	for(int32 i=0; i<Group->InterpTracks.Num(); i++)
	{
		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Group->InterpTracks[i]);

		// If we have reached this track, return current ChannelIndex
		if(AnimTrack == this)
		{
			return ChannelIndex;
		}

		// If not this track, but has same slot name, increment ChannelIndex
		CA_SUPPRESS(6011);
		if(AnimTrack && !AnimTrack->IsDisabled() && AnimTrack->SlotName == SlotName)
		{
			ChannelIndex++;
		}
	}

	check(false && "AnimControl Track Not Found In It's Group!"); // Should not reach here!
	return 0;
}

const FString UInterpTrackAnimControl::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackAnimControlHelper") );
}

const FString UInterpTrackAnimControl::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackAnimControlHelper") );
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstAnimControl
-----------------------------------------------------------------------------*/
UInterpTrackInstAnimControl::UInterpTrackInstAnimControl(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstAnimControl::InitTrackInst(UInterpTrack* Track)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );

	LastUpdatePosition = MatineeActor->InterpPosition;

#if WITH_EDITORONLY_DATA
	AActor* GrActor = GetGroupActor();
	if (GrActor)
	{
		InitPosition = GrActor->GetActorLocation();
		InitRotation = GrActor->GetActorRotation();
	}
#endif
}

/*-----------------------------------------------------------------------------
	UInterpTrackSound
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackSound, Sounds)
STRUCTTRACK_GETTIMERANGE(UInterpTrackSound, Sounds, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackSound, Sounds, Time)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackSound, Sounds, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackSound, Sounds, Time, FSoundTrackKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackSound, Sounds)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackSound, Sounds, Time, FSoundTrackKey)

UInterpTrackSound::UInterpTrackSound(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

	TrackInstClass = UInterpTrackInstSound::StaticClass();
	TrackTitle = TEXT("Sound");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Sound.MAT_Groups_Sound"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA

	bAttach = true;
}

void UInterpTrackSound::SetTrackToSensibleDefault()
{
	VectorTrack.Points.Empty();

	const float DefaultSoundKeyVolume = 1.0f;
	const float DefaultSoundKeyPitch = 1.0f;

	VectorTrack.AddPoint( 0.0f, FVector( DefaultSoundKeyVolume, DefaultSoundKeyPitch, 1.f ) );
}

int32 UInterpTrackSound::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	FSoundTrackKey NewSound;
	NewSound.Sound = NULL;
	NewSound.Time = Time;
	NewSound.Volume = 1.0f;
	NewSound.Pitch = 1.0f;

	// Find the correct index to insert this cut.
	int32 i=0; for( i=0; i<Sounds.Num() && Sounds[i].Time < Time; i++);
	Sounds.InsertUninitialized(i);
	Sounds[i] = NewSound;

	return i;
}

bool UInterpTrackSound::GetClosestSnapPosition(float InPosition, TArray<int32> &IgnoreKeys, float& OutPosition)
{
	if(Sounds.Num() == 0)
	{
		return false;
	}

	bool bFoundSnap = false;
	float ClosestSnap = 0.f;
	float ClosestDist = BIG_NUMBER;
	for(int32 i=0; i<Sounds.Num(); i++)
	{
		if(!IgnoreKeys.Contains(i))
		{
			float SoundStartTime = Sounds[i].Time;
			float SoundEndTime = SoundStartTime;

			// Make block as long as the SoundCue is.
			USoundBase* Sound = Sounds[i].Sound;
			if(Sound)
			{
				SoundEndTime += Sound->GetDuration();
			}

			// Truncate sound cue at next sound in the track.
			if((i < Sounds.Num()-1) && !IgnoreKeys.Contains(i+1))
			{
				SoundEndTime = FMath::Min( Sounds[i+1].Time, SoundEndTime );
			}

			float Dist = FMath::Abs( SoundStartTime - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = SoundStartTime;
				ClosestDist = Dist;
				bFoundSnap = true;
			}

			Dist = FMath::Abs( SoundEndTime - InPosition );
			if(Dist < ClosestDist)
			{
				ClosestSnap = SoundEndTime;
				ClosestDist = Dist;
				bFoundSnap = true;
			}
		}
	}

	OutPosition = ClosestSnap;
	return bFoundSnap;
}

float UInterpTrackSound::GetTrackEndTime() const
{
	float EndTime = 0.0f; 

	if( Sounds.Num() )
	{
		const FSoundTrackKey& SoundKey = Sounds[ Sounds.Num()-1 ];
		EndTime = SoundKey.Time + SoundKey.Sound->Duration;
	}

	return EndTime;
}

void UInterpTrackSound::PostLoad()
{
	Super::PostLoad();
	if (VectorTrack.Points.Num() <= 0)
	{
		SetTrackToSensibleDefault();
	}
}

FSoundTrackKey& UInterpTrackSound::GetSoundTrackKeyAtPosition(float InPosition)
{
	int32 SoundIndex; 
	if (bPlayOnReverse)
	{
		for (SoundIndex = Sounds.Num(); SoundIndex > 0 && Sounds[SoundIndex - 1].Time > InPosition; SoundIndex--);
		if (SoundIndex == Sounds.Num())
		{
			SoundIndex = Sounds.Num() - 1;
		}
	}
	else
	{
		for (SoundIndex = -1; SoundIndex<Sounds.Num()-1 && Sounds[SoundIndex+1].Time < InPosition; SoundIndex++);
		if (SoundIndex == -1)
		{
			SoundIndex = 0;
		}
	}
	return Sounds[SoundIndex];
}

void UInterpTrackSound::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	if (Sounds.Num() <= 0)
	{
		//UE_LOG(LogMatinee, Warning,TEXT("No sounds for sound track %s"),*GetName());
		return;
	}

	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());
	UInterpTrackInstSound* SoundInst = CastChecked<UInterpTrackInstSound>(TrInst);
	AActor* Actor = TrInst->GetGroupActor();


	// If this is a director group and the associated actor is a player controller, we need to make sure that it's
	// the local client's player controller and not some other client's player.  In the case where we're a host
	// with other connected players, we don't want the audio to be played for each of the players -- once is fine!
	bool bIsOkayToPlaySound = true;
	if( Group->IsA( UInterpGroupDirector::StaticClass() ) && Actor != NULL )
	{
		APlayerController* PC = Cast< APlayerController >( Actor );
		if( PC != NULL )
		{
			if (!PC->IsLocalPlayerController() || (GEngine->GetGamePlayers( PC->GetWorld() ).Find(Cast<ULocalPlayer>(PC->Player)) > 0))
			{
				// The director track is trying to play audio for a non-local client's player, or a player beyond the first
				// of a splitscreen matinee that plays for all players.  This is probably not
				// what was intended, so we don't allow it!  This will be played only by the local player's
				// audio track instance.
				bIsOkayToPlaySound = false;
			}
		}
	}


	// Only play sounds if we are playing Matinee forwards, we're not hopping around in time, and we're allowed to
	// play the sound.
	FVector VolumePitchValue( 1.0f, 1.0f, 1.0f );
	if ((bPlayOnReverse ? (NewPosition < SoundInst->LastUpdatePosition) : (NewPosition > SoundInst->LastUpdatePosition)) && !bJump && bIsOkayToPlaySound)
	{
		// Find which sound we are starting in. -1 Means before first sound.
		int32 StartSoundIndex = -1; 
		// Find which sound we are ending in. -1 Means before first sound.
		int32 EndSoundIndex = -1; 

		if (bPlayOnReverse)
		{
			for (StartSoundIndex = Sounds.Num(); StartSoundIndex > 0 && Sounds[StartSoundIndex - 1].Time > SoundInst->LastUpdatePosition; StartSoundIndex--);
			for (EndSoundIndex = Sounds.Num(); EndSoundIndex > 0 && Sounds[EndSoundIndex - 1].Time > NewPosition; EndSoundIndex--);
		}
		else
		{
			for (StartSoundIndex = -1; StartSoundIndex<Sounds.Num()-1 && Sounds[StartSoundIndex+1].Time < SoundInst->LastUpdatePosition; StartSoundIndex++);
			for (EndSoundIndex = -1; EndSoundIndex<Sounds.Num()-1 && Sounds[EndSoundIndex+1].Time < NewPosition; EndSoundIndex++);
		}

		//////
		FSoundTrackKey& SoundTrackKey = GetSoundTrackKeyAtPosition(NewPosition);
		VolumePitchValue *= FVector( SoundTrackKey.Volume, SoundTrackKey.Pitch, 1.0f );
		if (VectorTrack.Points.Num() > 0)
		{
			VolumePitchValue *= VectorTrack.Eval(NewPosition, VolumePitchValue);
		}

		// Check if we're in the audio range, and if we need to start playing the audio,
		// either because it has never been played, or isn't currently playing.
		// We only do this when we've jumped position.
		bool bIsInRangeAndNeedsStart = !bPlaying && SoundTrackKey.Sound != nullptr && NewPosition >= SoundTrackKey.Time && NewPosition <= ( SoundTrackKey.Time + SoundTrackKey.Sound->Duration );
		if ( bIsInRangeAndNeedsStart )
		{
			bIsInRangeAndNeedsStart = SoundInst->PlayAudioComp == NULL || !SoundInst->PlayAudioComp->IsPlaying();
		}

		// If we have moved into a new sound, we should start playing it now, or if we don't have an audio
		// component we must be starting mid playback, so go ahead and create one.  Or if it's not currently playing, but should be
		// lets start it.
		if ( StartSoundIndex != EndSoundIndex || bIsInRangeAndNeedsStart )
		{
			bPlaying = true;

			USoundBase* NewSound = SoundTrackKey.Sound;

			APawn* Speaker = NULL;
			if (bTreatAsDialogue && Actor)
			{
				Speaker = Cast<APawn>(Actor);
				if (Speaker == NULL)
				{
					// if we have a controller, see if it's controlling a speaker
					Speaker =  Cast<AController>(Actor) ? Cast<AController>(Actor)->GetPawn() : NULL;
				}
			}
			
			if (Speaker)
			{
				UGameplayStatics::PlaySoundAtLocation( Actor, NewSound, Speaker->GetActorLocation() );
			}
			else if (!bTreatAsDialogue || !Actor) // Don't play at all if we had a dialogue actor but they are not available/dead now
			{
				float StartTime = NewPosition - SoundTrackKey.Time;
				if (StartTime <= FApp::GetDeltaTime())
				{
					// If the start time is within the past frames delta time, start from the beginning
					// TODO: Consider dealing with play rate, time dilation, clamping
					StartTime = 0.f;
				}

				// If we have a sound playing already (ie. an AudioComponent exists) stop it now.
				if(SoundInst->PlayAudioComp)
				{
					SoundInst->PlayAudioComp->Stop();

					if (Actor)
					{
						if (bAttach && Actor->GetRootComponent())
						{
							SoundInst->PlayAudioComp->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
						}
						else
						{
							SoundInst->PlayAudioComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
							SoundInst->PlayAudioComp->SetWorldLocation(Actor->GetActorLocation());
						}
					}

					SoundInst->PlayAudioComp->SetSound(NewSound);
					SoundInst->PlayAudioComp->SetVolumeMultiplier(VolumePitchValue.X);
					SoundInst->PlayAudioComp->SetPitchMultiplier(VolumePitchValue.Y);
					SoundInst->PlayAudioComp->SubtitlePriority = bSuppressSubtitles ? 0.f : SUBTITLE_PRIORITY_MATINEE;
					SoundInst->PlayAudioComp->Play(StartTime);
				}
				else
				{
					// If there is no AudioComponent - create one now.
					FAudioDevice::FCreateComponentParams Params(SoundInst->GetWorld(), Actor);
					SoundInst->PlayAudioComp = FAudioDevice::CreateComponent(NewSound, Params);
					if(SoundInst->PlayAudioComp)
					{
						// If we have no actor to attach sound to - its location is meaningless, so we turn off spatialization.
						// Also if we are playing on a director group, disable spatialization.
						if(!Actor || Group->IsA(UInterpGroupDirector::StaticClass()))
						{
							SoundInst->PlayAudioComp->bAllowSpatialization = false;
						}
						else
						{
							if (bAttach && Actor->GetRootComponent())
							{
								SoundInst->PlayAudioComp->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
							}
							else
							{
								SoundInst->PlayAudioComp->SetWorldLocation(Actor->GetActorLocation());
							}
						}

						// Start the sound playing.
						SoundInst->PlayAudioComp->SetVolumeMultiplier(VolumePitchValue.X);
						SoundInst->PlayAudioComp->SetPitchMultiplier(VolumePitchValue.Y);
						SoundInst->PlayAudioComp->SubtitlePriority = bSuppressSubtitles ? 0.f : SUBTITLE_PRIORITY_MATINEE;
						SoundInst->PlayAudioComp->Play(StartTime);
					}
				}
			}
		}
	}
	// If Matinee is not being played forward, we're hopping around in time, or we're not allowed to
	// play the sound, then stop any already playing sounds
	else if( SoundInst->PlayAudioComp && SoundInst->PlayAudioComp->IsPlaying() )
	{
		SoundInst->PlayAudioComp->Stop();
		bPlaying = false;
	}


	// Apply master volume and pitch scale
	{
		UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );
		UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
		if( DirGroup != NULL )
		{
			UInterpTrackAudioMaster* AudioMasterTrack = DirGroup->GetAudioMasterTrack();
			if( AudioMasterTrack != NULL )
			{
				VolumePitchValue.X *= AudioMasterTrack->GetVolumeScaleForTime( NewPosition );
				VolumePitchValue.Y *= AudioMasterTrack->GetPitchScaleForTime( NewPosition );
			}
		}
	}


	// Update the sound if its playing
	if (SoundInst->PlayAudioComp)
	{
		SoundInst->PlayAudioComp->SetVolumeMultiplier(VolumePitchValue.X);
		SoundInst->PlayAudioComp->SetPitchMultiplier(VolumePitchValue.Y);
	}

	// Finally update the current position as the last one.
	SoundInst->LastUpdatePosition = NewPosition;
}

void UInterpTrackSound::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	if (Sounds.Num() <= 0)
	{
		//UE_LOG(LogMatinee, Warning,TEXT("No sounds for sound track %s"),*GetName());
		return;
	}

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>(TrInst->GetOuter());
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
	UInterpTrackInstSound* SoundInst = CastChecked<UInterpTrackInstSound>( TrInst );
	UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
	UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );

	// If the new position for the track is past the end of the interp length, then the sound
	// should stop, unless the user has specified to continue playing the sound past matinee's end
	if ( NewPosition >= IData->InterpLength && !bContinueSoundOnMatineeEnd && SoundInst->PlayAudioComp && SoundInst->PlayAudioComp->IsPlaying() )
	{
		SoundInst->PlayAudioComp->Stop();
		bPlaying = false;
	}

	// If the new position for the track is before the last interp position, then the playback must have looped,
	// so force playback to restart from the new position
	const bool bJustLooped = NewPosition < MatineeActor->InterpPosition && MatineeActor->bIsPlaying;
	if (bJustLooped)
	{
		if (SoundInst->PlayAudioComp)
		{
			SoundInst->PlayAudioComp->Stop();
		}
		bPlaying = false;
		const float Epsilon = 0.1f;
		SoundInst->LastUpdatePosition = NewPosition - Epsilon;
	}

	// Dont play sounds unless we are preview playback (ie not scrubbing).
	bool bJump = !( MatineeActor->bIsPlaying );
	UpdateTrack(NewPosition, TrInst, bJump);

#if WITH_EDITOR
	bool bTimeChangedDrastically = !FMath::IsNearlyEqual(NewPosition, MatineeActor->InterpPosition);
	if ( bTimeChangedDrastically && MatineeActor->bIsScrubbing )
	{
		FSoundTrackKey& SoundTrackKey = GetSoundTrackKeyAtPosition(NewPosition);
		const bool bIsInRange = (SoundTrackKey.Sound)
				? (NewPosition >= SoundTrackKey.Time && NewPosition <= (SoundTrackKey.Time + SoundTrackKey.Sound->Duration))
				: false;

		USoundCue* TempPlaybackAudioCue = NewObject<USoundCue>();
		UAudioComponent* Component = FAudioDevice::CreateComponent(TempPlaybackAudioCue);

		if ( bIsInRange && Component )
		{
			float PitchMultiplier = 1.0f / 1.0f;// AudioSection->GetAudioDilationFactor();
			Component->bAllowSpatialization = false;// !AudioTrack->IsAMasterTrack();
			Component->SetSound(SoundTrackKey.Sound);
			Component->SetVolumeMultiplier(1.f);
			Component->SetPitchMultiplier(PitchMultiplier);
			Component->bIsUISound = true;
			Component->Play(NewPosition - SoundTrackKey.Time);

			const float ScrubDuration = 0.1f;
			Component->FadeOut(ScrubDuration, 1.0f);
		}
	}
#endif
}

const FString UInterpTrackSound::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackSoundHelper") );
}

const FString UInterpTrackSound::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackSoundHelper") );
}

void UInterpTrackSound::PreviewStopPlayback(class UInterpTrackInst* TrInst)
{
	UInterpTrackInstSound* SoundTrInst = CastChecked<UInterpTrackInstSound>(TrInst);
	if(SoundTrInst->PlayAudioComp)
	{
		SoundTrInst->PlayAudioComp->Stop();
	}
	bPlaying = false;
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstSound
-----------------------------------------------------------------------------*/
UInterpTrackInstSound::UInterpTrackInstSound(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstSound::InitTrackInst(UInterpTrack* Track)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
	LastUpdatePosition = MatineeActor->InterpPosition;
}

void UInterpTrackInstSound::TermTrackInst(UInterpTrack* Track)
{
	UInterpTrackSound* SoundTrack = CastChecked<UInterpTrackSound>(Track);

	// If we still have an audio component - deal with it.
	if(PlayAudioComp)
	{
		// If we are currently playing, and want to keep the sound playing, 
		// just flag it as 'auto destroy', and it will destroy itself when it finishes.
		if(PlayAudioComp->bIsActive && SoundTrack->bContinueSoundOnMatineeEnd)
		{
			PlayAudioComp->bAutoDestroy = true;
		}
		else
		{
			PlayAudioComp->Stop();
			PlayAudioComp->UnregisterComponent();
		}
		PlayAudioComp = NULL;
	}

	Super::TermTrackInst(Track);
}

/*-----------------------------------------------------------------------------
	UInterpTrackFloatParticleParam
-----------------------------------------------------------------------------*/
UInterpTrackFloatParticleParam::UInterpTrackFloatParticleParam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
	TrackInstClass = UInterpTrackInstFloatParticleParam::StaticClass();
	TrackTitle = TEXT("Float Particle Param");
}

int32 UInterpTrackFloatParticleParam::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	int32 NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackFloatParticleParam::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, false);
}

void UInterpTrackFloatParticleParam::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	AEmitter* Emitter = Cast<AEmitter>(Actor);
	if(!Emitter)
	{
		return;
	}

	float NewFloatValue = FloatTrack.Eval(NewPosition, 0.f);
	Emitter->GetParticleSystemComponent()->SetFloatParameter( ParamName, NewFloatValue );
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstFloatParticleParam
-----------------------------------------------------------------------------*/
UInterpTrackInstFloatParticleParam::UInterpTrackInstFloatParticleParam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstFloatParticleParam::SaveActorState(UInterpTrack* Track)
{
	UInterpTrackFloatParticleParam* ParamTrack = CastChecked<UInterpTrackFloatParticleParam>(Track);
	AActor* Actor = GetGroupActor();
	AEmitter* Emitter = Cast<AEmitter>(Actor);
	if(!Emitter)
	{
		return;
	}

	bool bFoundParam = Emitter->GetParticleSystemComponent()->GetFloatParameter( ParamTrack->ParamName, ResetFloat );
	if(!bFoundParam)
	{
		ResetFloat = 0.f;
	}
}

void UInterpTrackInstFloatParticleParam::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackFloatParticleParam* ParamTrack = CastChecked<UInterpTrackFloatParticleParam>(Track);
	AActor* Actor = GetGroupActor();
	AEmitter* Emitter = Cast<AEmitter>(Actor);
	if(!Emitter)
	{
		return;
	}

	Emitter->GetParticleSystemComponent()->SetFloatParameter( ParamTrack->ParamName, ResetFloat );
}

/*------------------------------------------------------------------------------
	Material paramter tracks: shared functionality.
------------------------------------------------------------------------------*/

/**
 * Adds material refs for a component.
 * @param OutMaterialRefs - The material refs for the track instance.
 * @param Material - The material to override.
 * @param Component - The component.
 */
template <typename ComponentType>
static void AddMaterialRefsForComponent(
	TArray<FPrimitiveMaterialRef>& OutMaterialRefs,
	UMaterialInterface* Material,
	ComponentType* Component )
{
	int32 ElementCount = Component->GetNumMaterials();
	for (int32 ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
	{
		UMaterialInterface* ElementMaterial = Component->GetMaterial(ElementIndex);
		if (ElementMaterial && ( ElementMaterial == Material || (ElementMaterial->IsA(UMaterialInstanceDynamic::StaticClass()) && ((UMaterialInstanceDynamic*)ElementMaterial)->Parent == Material) ) )
		{
			new(OutMaterialRefs) FPrimitiveMaterialRef(Component, ElementIndex);
		}
	}
}

/**
 * Adds material refs required for a material track to affect an actor.
 * @param OutMaterialRefs - The material refs for the track instance.
 * @param Materials - The list of materials to override.
 * @param Actor - The actor.
 */
static void AddMaterialRefsForActor(
	TArray<FPrimitiveMaterialRef>& OutMaterialRefs,
	const TArray<UMaterialInterface*>& Materials,
	AActor* Actor )
{
	if (Actor && !Actor->IsRootComponentStatic())
	{
		TInlineComponentArray<USceneComponent*> Components;
		Actor->GetComponents(Components);

		for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); MaterialIndex++)
		{
			UMaterialInterface* Material = Materials[MaterialIndex];
			for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
			{
				USceneComponent* Component = Components[ComponentIndex];
				if (Component->IsRegistered())
				{
					if (Component->IsA(UPrimitiveComponent::StaticClass()))
					{
						UPrimitiveComponent* Primitive = (UPrimitiveComponent*)Component;
						AddMaterialRefsForComponent<UPrimitiveComponent>(OutMaterialRefs, Material, Primitive);
					}
					else if (Component->IsA(UDecalComponent::StaticClass()))
					{
						UDecalComponent* Decal = (UDecalComponent*)Component;
						AddMaterialRefsForComponent<UDecalComponent>(OutMaterialRefs, Material, Decal);
					}
				}
			}
		}
	}
}

/**
 * Fills out material ref information required for a material track instance.
 * @param OutMaterialRefs - The material refs for the track instance.
 * @param Materials - The list of materials to override.
 * @param TrackInst - The track instance.
 */
static void GetMaterialRefsForTrackInst(
	TArray<FPrimitiveMaterialRef>& OutMaterialRefs,
	const TArray<UMaterialInterface*>& Materials,
	UInterpTrackInst* TrackInst )
{
	check(TrackInst);

	AActor* Actor = TrackInst->GetGroupActor();
	if (Actor && !Actor->IsPendingKill())
	{
		if (Actor->IsA(AMaterialInstanceActor::StaticClass()))
		{
			AMaterialInstanceActor* MIActor = CastChecked<AMaterialInstanceActor>(Actor);
			for (int32 ActorIndex = 0; ActorIndex < MIActor->TargetActors.Num(); ++ActorIndex)
			{
				AActor* TargetActor = MIActor->TargetActors[ActorIndex];
				AddMaterialRefsForActor(OutMaterialRefs, Materials, TargetActor);
			}
		}
		else
		{
			AddMaterialRefsForActor(OutMaterialRefs, Materials, Actor);
		}
	}
}

/**
 * Retrieves the material currently used by the primitive.
 */
static UMaterialInterface* GetMaterialForRef(const FPrimitiveMaterialRef& MaterialRef)
{
	if (MaterialRef.Primitive)
	{
		return MaterialRef.Primitive->GetMaterial(MaterialRef.ElementIndex);
	}
	else if (MaterialRef.Decal)
	{
		return MaterialRef.Decal->GetMaterial(MaterialRef.ElementIndex);
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

/**
 * Sets the material used by the primitive.
 */
static void SetMaterialForRef(const FPrimitiveMaterialRef& MaterialRef, UMaterialInterface* OverrideMaterial)
{
	if (MaterialRef.Primitive)
	{
		MaterialRef.Primitive->SetMaterial(MaterialRef.ElementIndex, OverrideMaterial);
	}
	else if (MaterialRef.Decal)
	{
		MaterialRef.Decal->SetMaterial(MaterialRef.ElementIndex, OverrideMaterial);
	}
}

/**
 * Overrides materials for a list of material refs.
 * @param OutMaterialInstances - Material instances created to override parameters.
 * @param MaterialRefs - List of material refs.
 * @param NewMaterialOuter - The outer for newly created material instances.
 */
static void OverrideMaterials(
	TArray<UMaterialInstanceDynamic*>& OutMaterialInstances,
	const TArray<FPrimitiveMaterialRef>& MaterialRefs,
	UObject* NewMaterialOuter
	)
{
	OutMaterialInstances.Reset();
	OutMaterialInstances.AddZeroed(MaterialRefs.Num());
	for (int32 PrimitiveIndex = 0; PrimitiveIndex < MaterialRefs.Num(); PrimitiveIndex++)
	{
		const FPrimitiveMaterialRef& MaterialRef = MaterialRefs[PrimitiveIndex];
		if (MaterialRef.Primitive != NULL || MaterialRef.Decal != NULL)
		{
			UMaterialInterface* Material = GetMaterialForRef(MaterialRef);
			if (Material && Material->IsA(UMaterialInstanceDynamic::StaticClass()))
			{
				OutMaterialInstances[PrimitiveIndex] = CastChecked<UMaterialInstanceDynamic>(Material);
			}
			else if (Material != NULL)
			{
				UMaterialInstanceDynamic* OverrideMaterial = UMaterialInstanceDynamic::Create(
					Material, NewMaterialOuter
					);
				SetMaterialForRef(MaterialRef, OverrideMaterial);
				OutMaterialInstances[PrimitiveIndex] = OverrideMaterial;
			}
		}
	}
}

/**
 * Restores materials overridden by a track instance.
 * @param MaterialRefs - List of material refs.
 * @param MaterialInstances - List of material instances that have been applied.
 */
static void RestoreMaterials(
	const TArray<FPrimitiveMaterialRef>& MaterialRefs,
	const TArray<UMaterialInstanceDynamic*>& MaterialInstances
	)
{
	check(MaterialRefs.Num() == MaterialInstances.Num());

	for (int32 PrimitiveIndex = 0; PrimitiveIndex < MaterialRefs.Num(); PrimitiveIndex++)
	{
		const FPrimitiveMaterialRef& MaterialRef = MaterialRefs[PrimitiveIndex];
		UMaterialInstanceDynamic* MaterialInstance = MaterialInstances[PrimitiveIndex];
		if (MaterialInstance)
		{
			SetMaterialForRef(MaterialRef, MaterialInstance->Parent);
		}
	}
}

/** helper for PreEditChange() of the material tracks, since there's no material track base class */
static void PreEditChangeMaterialParamTrack()
{
	if (GIsEditor && !FApp::IsGame())
	{
#if WITH_EDITORONLY_DATA
		// we need to reinitialize all material parameter tracks in the Matinee being edited so that changes are applied immediately
		// and so that Materials array modifications properly add/remove any instanced MaterialInstances from affected meshes
		// we can't reinit just the edited track because all active tracks that modify the same base Material share the instanced MIC
		for (TObjectIterator<UInterpTrackInstFloatMaterialParam> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.Contains(*It))
			{
				AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
				if (MatineeActor != NULL && MatineeActor->bIsBeingEdited && MatineeActor->GroupInst.Contains(GrInst))
				{
					It->RestoreActorState(It->InstancedTrack);
					It->TermTrackInst(It->InstancedTrack);
				}
			}
		}
		for (TObjectIterator<UInterpTrackInstVectorMaterialParam> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.Contains(*It))
			{
				AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
				if (MatineeActor != NULL && MatineeActor->bIsBeingEdited && MatineeActor->GroupInst.Contains(GrInst))
				{
					It->RestoreActorState(It->InstancedTrack);
					It->TermTrackInst(It->InstancedTrack);
				}
			}
		}
#endif // WITH_EDITORONLY_DATA
	}
}

/** helper for PostEditChange() of the material tracks, since there's no material track base class */
static void PostEditChangeMaterialParamTrack()
{
#if WITH_EDITORONLY_DATA
	if (GIsEditor && !FApp::IsGame())
	{
		// we need to reinitialize all material parameter tracks so that changes are applied immediately
		// and so that Materials array modifications properly add/remove any instanced MaterialInstances from affected meshes
		// we can't reinit just the edited track because all active tracks that modify the same base Material share the instanced MIC
		for (TObjectIterator<UInterpTrackInstFloatMaterialParam> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.Contains(*It))
			{
				AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
				if (MatineeActor != NULL && MatineeActor->bIsBeingEdited && MatineeActor->GroupInst.Contains(GrInst))
				{
					It->InitTrackInst(It->InstancedTrack);
					It->SaveActorState(It->InstancedTrack);
					It->InstancedTrack->PreviewUpdateTrack(MatineeActor->InterpPosition, *It);
				}
			}
		}
		for (TObjectIterator<UInterpTrackInstVectorMaterialParam> It; It; ++It)
		{
			UInterpGroupInst* GrInst = Cast<UInterpGroupInst>(It->GetOuter());
			if (GrInst != NULL && GrInst->TrackInst.Contains(*It))
			{
				AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
				if (MatineeActor != NULL && MatineeActor->bIsBeingEdited && MatineeActor->GroupInst.Contains(GrInst))
				{
					It->InitTrackInst(It->InstancedTrack);
					It->SaveActorState(It->InstancedTrack);
					It->InstancedTrack->PreviewUpdateTrack(MatineeActor->InterpPosition, *It);
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/*-----------------------------------------------------------------------------
	Float material parameter track.
-----------------------------------------------------------------------------*/
UInterpTrackFloatMaterialParam::UInterpTrackFloatMaterialParam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TrackInstClass = UInterpTrackInstFloatMaterialParam::StaticClass();
	TrackTitle = TEXT("Float UMaterial Param");
}

#if WITH_EDITOR
void UInterpTrackFloatMaterialParam::PreEditChange(UProperty* PropertyThatWillChange)
{
	PreEditChangeMaterialParamTrack();
	Super::PreEditChange(PropertyThatWillChange);
}

void UInterpTrackFloatMaterialParam::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	PostEditChangeMaterialParamTrack();
}
#endif // WITH_EDITOR

int32 UInterpTrackFloatMaterialParam::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	int32 NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;
	if ( NewKeyIndex > 0 )
	{
		if ( NewKeyIndex < FloatTrack.Points.Num() - 1 )
		{
			const float Duration = FloatTrack.Points[NewKeyIndex+1].InVal - FloatTrack.Points[NewKeyIndex-1].InVal;
			const float Remaining = FloatTrack.Points[NewKeyIndex+1].InVal - FloatTrack.Points[NewKeyIndex].InVal;
			const float DurationPct = ( Duration > 0.f ? ((Duration - Remaining) / Duration) : 0.f );
			if( FloatTrack.Points[NewKeyIndex-1].InterpMode == CIM_Linear || FloatTrack.Points[NewKeyIndex-1].InterpMode == CIM_Constant )	// Linear or Constant interpolation
			{
				FloatTrack.Points[NewKeyIndex].OutVal = FMath::Lerp<float>(FloatTrack.Points[NewKeyIndex-1].OutVal, FloatTrack.Points[NewKeyIndex+1].OutVal, DurationPct);
			}
			else	// Cubic Interpolation
			{
				FloatTrack.Points[NewKeyIndex].OutVal = FMath::CubicInterp<float>(FloatTrack.Points[NewKeyIndex-1].OutVal, FloatTrack.Points[NewKeyIndex-1].LeaveTangent * Duration, FloatTrack.Points[NewKeyIndex+1].OutVal, FloatTrack.Points[NewKeyIndex+1].ArriveTangent * Duration, DurationPct);
			}
		}
		else	// Same position as previous point
		{
			FloatTrack.Points[NewKeyIndex].OutVal = FloatTrack.Points[NewKeyIndex-1].OutVal;
		}
	}
	else if ( NewKeyIndex < FloatTrack.Points.Num() - 1 )	// Same position as next point
	{
		FloatTrack.Points[NewKeyIndex].OutVal = FloatTrack.Points[NewKeyIndex+1].OutVal;
	}
	FloatTrack.AutoSetTangents(CurveTension);
	return NewKeyIndex;
}


void UInterpTrackFloatMaterialParam::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, false);
}


void UInterpTrackFloatMaterialParam::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	UInterpTrackInstFloatMaterialParam* ParamTrackInst = Cast<UInterpTrackInstFloatMaterialParam>(TrInst);
	if (ParamTrackInst != NULL)
	{
		float NewFloatValue = FloatTrack.Eval(NewPosition, 0.f);

		for (int32 MaterialIndex = 0; MaterialIndex < ParamTrackInst->MaterialInstances.Num(); MaterialIndex++)
		{
			UMaterialInstanceDynamic* MaterialInstance = ParamTrackInst->MaterialInstances[MaterialIndex];
			if (MaterialInstance)
			{
				MaterialInstance->SetScalarParameterValue(ParamName, NewFloatValue);
			}
		}
	}
}

UInterpTrackInstFloatMaterialParam::UInterpTrackInstFloatMaterialParam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstFloatMaterialParam::InitTrackInst(UInterpTrack* Track)
{
	UInterpTrackFloatMaterialParam* ParamTrack = Cast<UInterpTrackFloatMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		if (GIsEditor && !FApp::IsGame())
		{
			// remember track so we can be reinitialized if the track's material info changes
			InstancedTrack = ParamTrack;
		}

		GetMaterialRefsForTrackInst(PrimitiveMaterialRefs, ParamTrack->TargetMaterials, this);
		OverrideMaterials(MaterialInstances, PrimitiveMaterialRefs, this);
		check(MaterialInstances.Num() == PrimitiveMaterialRefs.Num());
	}
}

void UInterpTrackInstFloatMaterialParam::TermTrackInst(UInterpTrack* Track)
{
	// in the editor, we want to revert Actors to their original state
	// in game, leave the MIC around as Matinee changes persist when it is stopped
	if (GIsEditor && !FApp::IsGame())
	{
		RestoreMaterials(PrimitiveMaterialRefs, MaterialInstances);
	}
	MaterialInstances.Empty();
	ResetFloats.Empty();
	PrimitiveMaterialRefs.Empty();

	Super::TermTrackInst(Track);
}

void UInterpTrackInstFloatMaterialParam::SaveActorState(UInterpTrack* Track)
{
	UInterpTrackFloatMaterialParam* ParamTrack = Cast<UInterpTrackFloatMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		ResetFloats.Reset();
		ResetFloats.AddUninitialized(MaterialInstances.Num());
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialInstances.Num(); MaterialIndex++)
		{
			UMaterialInstanceDynamic* MaterialInstance = MaterialInstances[MaterialIndex];
			float OriginalValue = 0.0f;
			if (MaterialInstance)
			{
				MaterialInstance->GetScalarParameterValue(ParamTrack->ParamName, OriginalValue);
			}
			ResetFloats[MaterialIndex] = OriginalValue;
		}
	}
}

void UInterpTrackInstFloatMaterialParam::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackFloatMaterialParam* ParamTrack = Cast<UInterpTrackFloatMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		check(ResetFloats.Num() == MaterialInstances.Num());

		for (int32 MaterialIndex = 0; MaterialIndex < MaterialInstances.Num(); MaterialIndex++)
		{
			UMaterialInstanceDynamic* MaterialInstance = MaterialInstances[MaterialIndex];
			if (MaterialInstance)
			{
				MaterialInstance->SetScalarParameterValue(ParamTrack->ParamName, ResetFloats[MaterialIndex]);
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Vector material parameter track.
-----------------------------------------------------------------------------*/
UInterpTrackVectorMaterialParam::UInterpTrackVectorMaterialParam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	TrackInstClass = UInterpTrackInstVectorMaterialParam::StaticClass();
	TrackTitle = TEXT("Vector UMaterial Param");
}

#if WITH_EDITOR
void UInterpTrackVectorMaterialParam::PreEditChange(UProperty* PropertyThatWillChange)
{
	PreEditChangeMaterialParamTrack();
	Super::PreEditChange(PropertyThatWillChange);
}

void UInterpTrackVectorMaterialParam::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	PostEditChangeMaterialParamTrack();
}
#endif // WITH_EDITOR

int32 UInterpTrackVectorMaterialParam::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	int32 NewKeyIndex = VectorTrack.AddPoint( Time, FVector::ZeroVector );
	VectorTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;
	VectorTrack.AutoSetTangents(CurveTension);
	return NewKeyIndex;
}

void UInterpTrackVectorMaterialParam::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UpdateTrack(NewPosition, TrInst, false);
}

void UInterpTrackVectorMaterialParam::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	UInterpTrackInstVectorMaterialParam* ParamTrackInst = Cast<UInterpTrackInstVectorMaterialParam>(TrInst);
	if (ParamTrackInst != NULL)
	{
		FVector NewValue = VectorTrack.Eval(NewPosition, FVector::ZeroVector);
		FLinearColor NewLinearColor(NewValue.X, NewValue.Y, NewValue.Z);

		for (int32 MaterialIndex = 0; MaterialIndex < ParamTrackInst->MaterialInstances.Num(); MaterialIndex++)
		{
			UMaterialInstanceDynamic* MaterialInstance = ParamTrackInst->MaterialInstances[MaterialIndex];
			if (MaterialInstance)
			{
				MaterialInstance->SetVectorParameterValue(ParamName, NewLinearColor);
			}
		}
	}
}

UInterpTrackInstVectorMaterialParam::UInterpTrackInstVectorMaterialParam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstVectorMaterialParam::InitTrackInst(UInterpTrack* Track)
{
	UInterpTrackVectorMaterialParam* ParamTrack = Cast<UInterpTrackVectorMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		if (GIsEditor && !FApp::IsGame())
		{
			// remember track so we can be reinitialized if the track's material info changes
			InstancedTrack = ParamTrack;
		}

		GetMaterialRefsForTrackInst(PrimitiveMaterialRefs, ParamTrack->TargetMaterials, this);
		OverrideMaterials(MaterialInstances, PrimitiveMaterialRefs, this);
		check(MaterialInstances.Num() == PrimitiveMaterialRefs.Num());
	}
}

void UInterpTrackInstVectorMaterialParam::TermTrackInst(UInterpTrack* Track)
{
	// in the editor, we want to revert Actors to their original state
	// in game, leave the MIC around as Matinee changes persist when it is stopped
	if (GIsEditor && !FApp::IsGame())
	{
		RestoreMaterials(PrimitiveMaterialRefs, MaterialInstances);
	}
	MaterialInstances.Empty();
	ResetVectors.Empty();
	PrimitiveMaterialRefs.Empty();

	Super::TermTrackInst(Track);
}

void UInterpTrackInstVectorMaterialParam::SaveActorState(UInterpTrack* Track)
{
	UInterpTrackVectorMaterialParam* ParamTrack = Cast<UInterpTrackVectorMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		ResetVectors.Reset();
		ResetVectors.AddUninitialized(MaterialInstances.Num());
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialInstances.Num(); MaterialIndex++)
		{
			UMaterialInstanceDynamic* MaterialInstance = MaterialInstances[MaterialIndex];
			FLinearColor OriginalColor = FLinearColor::Black;
			if (MaterialInstance)
			{
				MaterialInstance->GetVectorParameterValue(ParamTrack->ParamName, OriginalColor);
			}
			ResetVectors[MaterialIndex] = FVector(OriginalColor.R, OriginalColor.G, OriginalColor.B);
		}
	}
}

void UInterpTrackInstVectorMaterialParam::RestoreActorState(UInterpTrack* Track)
{
	UInterpTrackVectorMaterialParam* ParamTrack = Cast<UInterpTrackVectorMaterialParam>(Track);
	if (ParamTrack != NULL)
	{
		check(ResetVectors.Num() == MaterialInstances.Num());

		for (int32 MaterialIndex = 0; MaterialIndex < MaterialInstances.Num(); MaterialIndex++)
		{
			UMaterialInstanceDynamic* MaterialInstance = MaterialInstances[MaterialIndex];
			if (MaterialInstance)
			{
				MaterialInstance->SetVectorParameterValue(ParamTrack->ParamName, ResetVectors[MaterialIndex]);
			}
		}
	}
}

/*------------------------------------------------------------------------------
	Material instance actor, used to control materials on multiple actors from
	a single track in Matinee.
------------------------------------------------------------------------------*/


/**
 * Construct a list of static actor names.
 * @param OutString - The string containing the list of actor's names.
 * @param Actors - The list of actors to check.
 */
static void GetListOfStaticActors(FString& OutString, const TArray<AActor*>& Actors)
{
	for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
	{
		AActor* Actor = Actors[ActorIndex];
		if (Actor && Actor->IsRootComponentStatic())
		{
			OutString += FString::Printf(TEXT("\n%s"), *Actor->GetFullName());
		}
	}
}

AMaterialInstanceActor::AMaterialInstanceActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet() && (SpriteComponent != NULL))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> MaterialInstanceSpriteObject;
			FName ID_Materials;
			FText NAME_Materials;
			FConstructorStatics()
				: MaterialInstanceSpriteObject(TEXT("/Engine/EditorResources/MatInstActSprite"))
				, ID_Materials(TEXT("Materials"))
				, NAME_Materials(NSLOCTEXT("SpriteCategory", "Materials", "Materials"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.MaterialInstanceSpriteObject.Get();
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Materials;
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Materials;
		SpriteComponent->SetupAttachment(SceneComponent);
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif // WITH_EDITORONLY_DATA
}

void AMaterialInstanceActor::PostLoad()
{
	Super::PostLoad();

	// Warn the user if any static actors exist in the list.
	FString StaticActors;
	GetListOfStaticActors(StaticActors, TargetActors);
	if (StaticActors.Len() > 0)
	{
		UE_LOG(LogMatinee, Log, TEXT("Static actors may not be referenced by a material instance actor:%s"), *StaticActors);
	}
}

#if WITH_EDITOR
void AMaterialInstanceActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Warn the user if any static actors exist in the list.
	FString StaticActors;
	GetListOfStaticActors(StaticActors, TargetActors);
	if (StaticActors.Len() > 0)
	{
		const FText WarningMsg = FText::Format( NSLOCTEXT("Engine", "MaterialInstanceActor_NonStaticActorRef", "Static actors may not be referenced by a material instance actor:{0}"), FText::FromString( StaticActors ) );
		UE_LOG(LogMatinee, Log, TEXT("%s"), *WarningMsg.ToString());
		FMessageDialog::Open( EAppMsgType::Ok, WarningMsg);
	}
}
#endif // WITH_EDITOR
/*-----------------------------------------------------------------------------
	UInterpTrackInstToggle
-----------------------------------------------------------------------------*/
UInterpTrackInstToggle::UInterpTrackInstToggle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstToggle::InitTrackInst(UInterpTrack* Track)
{
}



void UInterpTrackInstToggle::SaveActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();

	AEmitter* EmitterActor = Cast<AEmitter>(Actor);
	ALight* LightActor = Cast<ALight>(Actor);

	bSavedActiveState = false;

	if( EmitterActor )
	{
		bSavedActiveState = EmitterActor->bCurrentlyActive;
	}
	else if( LightActor != NULL )
	{
		bSavedActiveState = LightActor->GetLightComponent()->bVisible;
	}
}



void UInterpTrackInstToggle::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();

	AEmitter* EmitterActor = Cast<AEmitter>(Actor);
	ALight* LightActor = Cast<ALight>(Actor);

	if( EmitterActor )
	{
		// Use SetActive to only activate a non-active system...
		EmitterActor->GetParticleSystemComponent()->SetActive(bSavedActiveState);
		EmitterActor->bCurrentlyActive = bSavedActiveState;
		EmitterActor->ForceNetRelevant();
	}
	else if( LightActor != NULL )
	{
		// We'll only allow *toggleable* lights to be toggled like this!  Static lights are ignored.
		if( LightActor->IsToggleable() )
		{
			LightActor->GetLightComponent()->SetVisibility( bSavedActiveState );
		}
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackColorScale
-----------------------------------------------------------------------------*/
UInterpTrackColorScale::UInterpTrackColorScale(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	bOnePerGroup = true;
	bDirGroupOnly = true;
	TrackInstClass = UInterpTrackInstColorScale::StaticClass();
	TrackTitle = TEXT("Color Scale");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_Fade.MAT_Groups_Fade"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackColorScale::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	int32 NewKeyIndex = VectorTrack.AddPoint( Time, FVector(1.f,1.f,1.f) );
	VectorTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;

	VectorTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void UInterpTrackColorScale::UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst)
{
	
}

void UInterpTrackColorScale::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	
}

void UInterpTrackColorScale::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );

	// Actor for a Director group should be a PlayerController.
	APlayerController* PC = Cast<APlayerController>(GrInst->GetGroupActor());
	if(PC && PC->PlayerCameraManager && !PC->PlayerCameraManager->IsPendingKill())
	{
		PC->PlayerCameraManager->bEnableColorScaling = true;
		PC->PlayerCameraManager->ColorScale = GetColorScaleAtTime(NewPosition);

		// Disable the camera's "color scale interpolation" features since that would blow away our changes
		// when the camera's UpdateCamera function is called.  For the moment, we'll be the authority over color scale!
		PC->PlayerCameraManager->bEnableColorScaleInterp = false;
	}
}

FVector UInterpTrackColorScale::GetColorScaleAtTime(float Time)
{
	FVector ColorScale = VectorTrack.Eval(Time, FVector(1.f,1.f,1.f));
	return ColorScale;
}


void UInterpTrackColorScale::SetTrackToSensibleDefault()
{
	VectorTrack.Points.Empty();
	VectorTrack.AddPoint(0.f, FVector(1.f,1.f,1.f));
}	

/*-----------------------------------------------------------------------------
	UInterpTrackInstColorScale
-----------------------------------------------------------------------------*/
UInterpTrackInstColorScale::UInterpTrackInstColorScale(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstColorScale::TermTrackInst(UInterpTrack* Track)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( GetOuter() );
	APlayerController* PC = Cast<APlayerController>(GrInst->GroupActor);
	if(PC && PC->PlayerCameraManager && !PC->PlayerCameraManager->IsPendingKill())
	{
		PC->PlayerCameraManager->bEnableColorScaling = false;
		PC->PlayerCameraManager->ColorScale = FVector(1.f,1.f,1.f);
	}

	Super::TermTrackInst(Track);
}

/*-----------------------------------------------------------------------------
	UInterpTrackAudioMaster
-----------------------------------------------------------------------------*/
UInterpTrackAudioMaster::UInterpTrackAudioMaster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bOnePerGroup = true;
	bDirGroupOnly = true;
	TrackInstClass = UInterpTrackInstAudioMaster::StaticClass();
	TrackTitle = TEXT("Audio Master");
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MatineeGroups/MAT_Groups_AudioMaster.MAT_Groups_AudioMaster"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}


int32 UInterpTrackAudioMaster::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	const float DefaultVolume = 1.0f;
	const float DefaultPitch = 1.0f;

	int32 NewKeyIndex = VectorTrack.AddPoint( Time, FVector( DefaultVolume, DefaultPitch, 0.0f ) );
	VectorTrack.Points[ NewKeyIndex ].InterpMode = InitInterpMode;

	VectorTrack.AutoSetTangents( CurveTension );

	return NewKeyIndex;
}


void UInterpTrackAudioMaster::UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst)
{
}


void UInterpTrackAudioMaster::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
}


void UInterpTrackAudioMaster::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
}


void UInterpTrackAudioMaster::SetTrackToSensibleDefault()
{
	const float DefaultVolume = 1.0f;
	const float DefaultPitch = 1.0f;

	VectorTrack.Points.Empty();
	VectorTrack.AddPoint( 0.0f, FVector( DefaultVolume, DefaultPitch, 0.0f ) );
}	


float UInterpTrackAudioMaster::GetVolumeScaleForTime( float Time ) const
{
	const float DefaultVolume = 1.0f;
	const float DefaultPitch = 1.0f;
	FVector DefaultVolumePitch( DefaultVolume, DefaultPitch, 0.0f );

	return VectorTrack.Eval( Time, DefaultVolumePitch ).X;		// X = Volume
}


float UInterpTrackAudioMaster::GetPitchScaleForTime( float Time ) const
{
	const float DefaultVolume = 1.0f;
	const float DefaultPitch = 1.0f;
	FVector DefaultVolumePitch( DefaultVolume, DefaultPitch, 0.0f );

	return VectorTrack.Eval( Time, DefaultVolumePitch ).Y;		// Y = Pitch
}


/*-----------------------------------------------------------------------------
	UInterpTrackInstAudioMaster
-----------------------------------------------------------------------------*/
UInterpTrackInstAudioMaster::UInterpTrackInstAudioMaster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstAudioMaster::InitTrackInst(UInterpTrack* Track)
{
}

/*-----------------------------------------------------------------------------
	UInterpTrackVisibility
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackVisibility, VisibilityTrack)
STRUCTTRACK_GETTIMERANGE(UInterpTrackVisibility, VisibilityTrack, Time)
STRUCTTRACK_GETTRACKENDTIME(UInterpTrackVisibility, VisibilityTrack, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackVisibility, VisibilityTrack, Time)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackVisibility, VisibilityTrack, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackVisibility, VisibilityTrack, Time, FVisibilityTrackKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackVisibility, VisibilityTrack)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackVisibility, VisibilityTrack, Time, FVisibilityTrackKey)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackVisibility, VisibilityTrack, Time)

UInterpTrackVisibility::UInterpTrackVisibility(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

	TrackInstClass = UInterpTrackInstVisibility::StaticClass();
	TrackTitle = TEXT("Visibility");
	bFireEventsWhenForwards = true;
	bFireEventsWhenBackwards = true;
	bFireEventsWhenJumpingForwards = true;
#if WITH_EDITORONLY_DATA
	TrackIcon = Cast<UTexture2D>(StaticLoadObject( UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/MAT_Groups_Visibility.MAT_Groups_Visibility"), NULL, LOAD_None, NULL ));
#endif // WITH_EDITORONLY_DATA
}

int32 UInterpTrackVisibility::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	UInterpTrackInstVisibility* VisibilityInst = CastChecked<UInterpTrackInstVisibility>(TrInst);

	int32 i = 0;
	for (i = 0; i < VisibilityTrack.Num() && VisibilityTrack[i].Time < Time; i++);
	VisibilityTrack.InsertUninitialized(i);
	VisibilityTrack[i].Time = Time;
	VisibilityTrack[i].Action = VisibilityInst->Action;

	return i;
}

void UInterpTrackVisibility::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if (Actor == NULL)
	{
		return;
	}

	UInterpTrackInstVisibility* VisibilityInst = CastChecked<UInterpTrackInstVisibility>(TrInst);
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );
	UInterpGroup* Group = CastChecked<UInterpGroup>( GetOuter() );
	UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );


	// NOTE: We don't fire events when jumping forwards in Matinee preview since that would
	//       fire off particles while scrubbing, which we currently don't want.
	const bool bShouldActuallyFireEventsWhenJumpingForwards = bFireEventsWhenJumpingForwards;

	// @todo: Make this configurable?
	const bool bInvertBoolLogicWhenPlayingBackwards = true;


	// We'll consider playing events in reverse if we're either actively playing in reverse or if
	// we're in a paused state but forcing an update to an older position (scrubbing backwards in editor.)
	bool bIsPlayingBackwards =
		( MatineeActor->bIsPlaying && MatineeActor->bReversePlayback ) ||
		( bJump && !MatineeActor->bIsPlaying && NewPosition < VisibilityInst->LastUpdatePosition );


	// Find the interval between last update and this to check events with.
	bool bFireEvents = true;


	if( bJump )
	{
		// If we are playing forwards, and the flag is set, fire events even if we are 'jumping'.
		if (bShouldActuallyFireEventsWhenJumpingForwards)
		{
			bFireEvents = true;
		}
		else
		{
			bFireEvents = false;
		}
	}

	// If playing sequence forwards.
	float MinTime, MaxTime;
	if( !bIsPlayingBackwards )
	{
		MinTime = VisibilityInst->LastUpdatePosition;
		MaxTime = NewPosition;

		// Slight hack here.. if playing forwards and reaching the end of the sequence, force it over a little to ensure we fire events actually on the end of the sequence.
		if( MaxTime == IData->InterpLength )
		{
			MaxTime += (float)KINDA_SMALL_NUMBER;
		}

		if( !bFireEventsWhenForwards )
		{
			bFireEvents = false;
		}
	}
	// If playing sequence backwards.
	else
	{
		MinTime = NewPosition;
		MaxTime = VisibilityInst->LastUpdatePosition;

		// Same small hack as above for backwards case.
		if( MinTime == 0.0f )
		{
			MinTime -= (float)KINDA_SMALL_NUMBER;
		}

		if( !bFireEventsWhenBackwards )
		{
			bFireEvents = false;
		}
	}


	// If we should be firing events for this track...
	if( bFireEvents )
	{
		// See which events fall into traversed region.
		for(int32 CurKeyIndex = 0; CurKeyIndex < VisibilityTrack.Num(); ++CurKeyIndex )
		{
			// Iterate backwards if we're playing in reverse so that toggles are applied in the correct order
			int32 ActualKeyIndex = CurKeyIndex;
			if( bIsPlayingBackwards )
			{
				ActualKeyIndex = ( VisibilityTrack.Num() - 1 ) - CurKeyIndex;
			}

			FVisibilityTrackKey& VisibilityKey = VisibilityTrack[ ActualKeyIndex ];

			float EventTime = VisibilityKey.Time;

			// Need to be slightly careful here and make behavior for firing events symmetric when playing forwards of backwards.
			bool bFireThisEvent = false;
			if( !bIsPlayingBackwards )
			{
				if( EventTime >= MinTime && EventTime <= MaxTime )
				{
					bFireThisEvent = true;
				}
			}
			else
			{
				if( EventTime > MinTime && EventTime <= MaxTime )
				{
					bFireThisEvent = true;
				}
			}

			if( bFireThisEvent )
			{
				// NOTE: Because of how Toggle keys work, we need to run every event in the range, not
				//       just the last event.

				if( Actor != NULL )
				{
					// Make sure the key's condition is satisfied
					if( !( VisibilityKey.ActiveCondition == EVTC_GoreEnabled && !MatineeActor->bShouldShowGore ) &&
						!( VisibilityKey.ActiveCondition == EVTC_GoreDisabled && MatineeActor->bShouldShowGore ) )
					{
						if( VisibilityKey.Action == EVTA_Show )
						{
							bool bShouldHide = false;
							if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
							{
								// Playing in reverse, so invert bool logic (Show -> Hide)
								bShouldHide = true;
							}

							// Show the actor
							HideActor( Actor, bShouldHide );
						}
						else if( VisibilityKey.Action == EVTA_Hide )
						{
							bool bShouldHide = true;
							if( bInvertBoolLogicWhenPlayingBackwards && bIsPlayingBackwards )
							{
								// Playing in reverse, so invert bool logic (Hide -> Show)
								bShouldHide = false;
							}

							// Hide the actor
							HideActor( Actor, bShouldHide );
						}
						else if( VisibilityKey.Action == EVTA_Toggle )
						{
							// Toggle the actor's visibility
							HideActor( Actor, !Actor->bHidden );
						}
						if (!MatineeActor->bClientSideOnly && VisibilityKey.ActiveCondition == EVTC_Always)
						{
							Actor->ForceNetRelevant();
						}
					}
				}
			}	
		}
	}

	VisibilityInst->LastUpdatePosition = NewPosition;
}

void UInterpTrackVisibility::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );

	// Dont play sounds unless we are preview playback (ie not scrubbing).
	bool bJump = !(MatineeActor->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}

const FString UInterpTrackVisibility::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackVisibilityHelper") );
}

const FString UInterpTrackVisibility::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackVisibilityHelper") );
}

void UInterpTrackVisibility::HideActor( AActor* Actor, bool bHidden )
{
	Actor->SetActorHiddenInGame( bHidden );

#if WITH_EDITOR
	if ( GIsEditor && !Actor->GetWorld()->IsPlayInEditor() )
	{
		// In editor HiddenGame flag is not respected so set bHiddenEdTemporary too.
		// It will be restored just like HiddenGame flag when Matinee is closed.
		if( Actor->IsTemporarilyHiddenInEditor() != bHidden )
		{
			Actor->SetIsTemporarilyHiddenInEditor( bHidden );
		}
	}
#endif // WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UInterpTrackInstVisibility
-----------------------------------------------------------------------------*/
UInterpTrackInstVisibility::UInterpTrackInstVisibility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstVisibility::InitTrackInst(UInterpTrack* Track)
{

}





/*-----------------------------------------------------------------------------
	UInterpTrackParticleReplay
-----------------------------------------------------------------------------*/

STRUCTTRACK_GETNUMKEYFRAMES(UInterpTrackParticleReplay, TrackKeys)
STRUCTTRACK_GETTIMERANGE(UInterpTrackParticleReplay, TrackKeys, Time)
STRUCTTRACK_GETKEYFRAMETIME(UInterpTrackParticleReplay, TrackKeys, Time)
STRUCTTRACK_GETKEYFRAMEINDEX(UInterpTrackParticleReplay, TrackKeys, Time)
STRUCTTRACK_SETKEYFRAMETIME(UInterpTrackParticleReplay, TrackKeys, Time, FParticleReplayTrackKey)
STRUCTTRACK_REMOVEKEYFRAME(UInterpTrackParticleReplay, TrackKeys)
STRUCTTRACK_DUPLICATEKEYFRAME(UInterpTrackParticleReplay, TrackKeys, Time, FParticleReplayTrackKey)
STRUCTTRACK_GETCLOSESTSNAPPOSITION(UInterpTrackParticleReplay, TrackKeys, Time)

UInterpTrackParticleReplay::UInterpTrackParticleReplay(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

	TrackInstClass = UInterpTrackInstParticleReplay::StaticClass();
	TrackTitle = TEXT("Particle Replay");
}


int32 UInterpTrackParticleReplay::AddKeyframe( float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode )
{
	UInterpTrackInstParticleReplay* ParticleReplayInst = CastChecked<UInterpTrackInstParticleReplay>(TrInst);

	// Figure out which key we should insert before by testing key time values
	int32 InsertBeforeIndex = 0;
	while( InsertBeforeIndex < TrackKeys.Num() && TrackKeys[InsertBeforeIndex].Time < Time )
	{
		++InsertBeforeIndex;
	}

	// Create new key frame
	FParticleReplayTrackKey NewKey;
	NewKey.Time = Time;
	NewKey.ClipIDNumber = 1;	// Default clip ID number
	NewKey.Duration = 1.0f;		// Default duration

	// Insert the new key
	TrackKeys.Insert( NewKey, InsertBeforeIndex );

	return InsertBeforeIndex;
}



void UInterpTrackParticleReplay::UpdateTrack( float NewPosition, UInterpTrackInst* TrInst, bool bJump )
{
	AActor* Actor = TrInst->GetGroupActor();
	if (Actor == NULL)
	{
		return;
	}

	UInterpTrackInstParticleReplay* ParticleReplayInst = CastChecked<UInterpTrackInstParticleReplay>(TrInst);

	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );

	// Particle replay tracks are expecting to be dealing with emitter actors
	AEmitter* EmitterActor = Cast< AEmitter >( Actor );
	if( EmitterActor != NULL && EmitterActor->GetParticleSystemComponent() )
	{
		if( ( NewPosition > ParticleReplayInst->LastUpdatePosition ) && !bJump )
		{
			for (int32 KeyIndex = 0; KeyIndex < TrackKeys.Num(); KeyIndex++)
			{
				FParticleReplayTrackKey& ParticleReplayKey = TrackKeys[KeyIndex];

				// Check to see if we hit this key's start time
				if( ( ParticleReplayKey.Time < NewPosition ) && ( ParticleReplayKey.Time >= ParticleReplayInst->LastUpdatePosition ) )
				{
#if WITH_EDITORONLY_DATA
					if( bIsCapturingReplay )
					{
						// Do we already have data for this clip?
						UParticleSystemReplay* ExistingClipReplay =
							EmitterActor->GetParticleSystemComponent()->FindReplayClipForIDNumber( ParticleReplayKey.ClipIDNumber );
						if( ExistingClipReplay != NULL )
						{
							// Clear the existing clip's frame data.  We're re-recording the clip now!
							ExistingClipReplay->Frames.Empty();
						}

						// Start capturing!
						EmitterActor->GetParticleSystemComponent()->ReplayState = PRS_Capturing;
						EmitterActor->GetParticleSystemComponent()->ReplayClipIDNumber = ParticleReplayKey.ClipIDNumber;
						EmitterActor->GetParticleSystemComponent()->ReplayFrameIndex = 0;

						// Make sure we're alive and kicking
						EmitterActor->GetParticleSystemComponent()->SetActive( true );
					}
					else
#endif // WITH_EDITORONLY_DATA
					{
						// Start playback!
						EmitterActor->GetParticleSystemComponent()->ReplayState = PRS_Replaying;
						EmitterActor->GetParticleSystemComponent()->ReplayClipIDNumber = ParticleReplayKey.ClipIDNumber;
						EmitterActor->GetParticleSystemComponent()->ReplayFrameIndex = 0;

						// Make sure we're alive and kicking
						EmitterActor->GetParticleSystemComponent()->SetActive( true );
					}
				}

				// Check to see if we hit this key's end time
				const float KeyEndTime = ParticleReplayKey.Time + ParticleReplayKey.Duration;
				if( ( KeyEndTime < NewPosition ) && ( KeyEndTime >= ParticleReplayInst->LastUpdatePosition ) )
				{
#if WITH_EDITORONLY_DATA
					if( !bIsCapturingReplay )
#endif // WITH_EDITORONLY_DATA
					{
						// Done playing back replay sequence, so turn off the particle system
						EmitterActor->GetParticleSystemComponent()->SetActive( false );

						// Stop playback/capture!  We'll still keep the particle system in 'replay mode' while
						// the replay track is bound so that the system doesn't start simulating/rendering
						EmitterActor->GetParticleSystemComponent()->ReplayState = PRS_Replaying;
						EmitterActor->GetParticleSystemComponent()->ReplayClipIDNumber = INDEX_NONE;
						EmitterActor->GetParticleSystemComponent()->ReplayFrameIndex = INDEX_NONE;
					}
				}
			}
		}


#if WITH_EDITORONLY_DATA
		// Are we 'jumping in time'? (scrubbing)
		if( bJump )
		{
			if( bIsCapturingReplay )
			{
				// Scrubbing while capturing will stop the capture
				EmitterActor->GetParticleSystemComponent()->ReplayState = PRS_Disabled;
			}
			else
			{
				// Scrubbing while replaying with render the specific frame of the particle system

				// Find the time that the last replay was started
				bool bHaveReplayStartKey = false;
				FParticleReplayTrackKey CurrentReplayStartKey;
				for( int32 KeyIndex = TrackKeys.Num() - 1; KeyIndex >= 0; --KeyIndex )
				{
					FParticleReplayTrackKey& ParticleReplayKey = TrackKeys[KeyIndex];

					// Check to see if we hit this key's start time
					if( ParticleReplayKey.Time < NewPosition )
					{
						CurrentReplayStartKey = ParticleReplayKey;
						bHaveReplayStartKey = true;
						break;
					}
				}

				bool bIsReplayingSingleFrame = false;
				if( bHaveReplayStartKey )
				{
					const float TimeWithinReplay = NewPosition - CurrentReplayStartKey.Time;
					const int32 ReplayFrameIndex = FMath::TruncToInt( TimeWithinReplay / FMath::Max( ( float )KINDA_SMALL_NUMBER, FixedTimeStep ) );


					// Check to see if we have a clip
					UParticleSystemReplay* ParticleSystemReplay =
						EmitterActor->GetParticleSystemComponent()->FindReplayClipForIDNumber( CurrentReplayStartKey.ClipIDNumber );
					if( ParticleSystemReplay != NULL )
					{
						if( ReplayFrameIndex < ParticleSystemReplay->Frames.Num() )
						{
							// Playback specific frame!
							bIsReplayingSingleFrame = true;

							// Make sure replay mode is turned on
							EmitterActor->GetParticleSystemComponent()->ReplayState = PRS_Replaying;
							EmitterActor->GetParticleSystemComponent()->ReplayClipIDNumber = CurrentReplayStartKey.ClipIDNumber;
							EmitterActor->GetParticleSystemComponent()->ReplayFrameIndex = ReplayFrameIndex;

							// Make sure we're alive and kicking
							EmitterActor->GetParticleSystemComponent()->SetActive( true );
						}
					}
				}

				if( !bIsReplayingSingleFrame )
				{
					// Stop playback!  We'll still keep the particle system in 'replay mode' while
					// the replay track is bound so that the system doesn't start simulating/rendering
					EmitterActor->GetParticleSystemComponent()->ReplayState = PRS_Replaying;
					EmitterActor->GetParticleSystemComponent()->ReplayClipIDNumber = INDEX_NONE;
					EmitterActor->GetParticleSystemComponent()->ReplayFrameIndex = INDEX_NONE;

					// We're not currently capturing and we're not in the middle of a replay frame,
					// so turn off the particle system
					EmitterActor->GetParticleSystemComponent()->SetActive( false );
				}
			}
		}
		else
#endif // WITH_EDITORONLY_DATA
		{
			// Okay, we're not scrubbing, but are we replaying a particle system?
			if( EmitterActor->GetParticleSystemComponent()->ReplayState == PRS_Replaying )
			{
				// Advance to next frame (or reverse to the previous frame)
				if( MatineeActor->bReversePlayback )
				{
					--EmitterActor->GetParticleSystemComponent()->ReplayFrameIndex;
				}
				else
				{
					++EmitterActor->GetParticleSystemComponent()->ReplayFrameIndex;
				}
			}
		}
	}


	ParticleReplayInst->LastUpdatePosition = NewPosition;
}


void UInterpTrackParticleReplay::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( GrInst->GetOuter() );

	bool bJump = !(MatineeActor->bIsPlaying);
	UpdateTrack(NewPosition, TrInst, bJump);
}


float UInterpTrackParticleReplay::GetTrackEndTime() const
{
	float EndTime = 0.0f;

	if( TrackKeys.Num() )
	{
		const FParticleReplayTrackKey& ParticleReplayKey = TrackKeys[ TrackKeys.Num()-1 ];
		EndTime = ParticleReplayKey.Time + ParticleReplayKey.Duration;
	}

	return EndTime;
}

/** 
 *	Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
 *
 *	@return		String name of the helper class.
 */
const FString UInterpTrackParticleReplay::GetEdHelperClassName() const
{
	return FString( TEXT("UnrealEd.InterpTrackParticleReplayHelper") );
}

const FString UInterpTrackParticleReplay::GetSlateHelperClassName() const
{
	return FString( TEXT("Matinee.MatineeTrackParticleReplayHelper") );
}


/*-----------------------------------------------------------------------------
	UInterpTrackInstParticleReplay
-----------------------------------------------------------------------------*/
UInterpTrackInstParticleReplay::UInterpTrackInstParticleReplay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInterpTrackInstParticleReplay::InitTrackInst(UInterpTrack* Track)
{

}


void UInterpTrackInstParticleReplay::RestoreActorState(UInterpTrack* Track)
{
	AActor* Actor = GetGroupActor();
	if( Actor != NULL )
	{
		// Particle replay tracks are expecting to be dealing with emitter actors
		AEmitter* EmitterActor = Cast< AEmitter >( Actor );
		if( EmitterActor != NULL && EmitterActor->GetParticleSystemComponent() )
		{
			// Make sure we don't leave the particle system in 'capture mode'
			
			// Stop playback/capture!  We'll still keep the particle system in 'replay mode' while
			// the replay track is bound so that the system doesn't start simulating/rendering
			EmitterActor->GetParticleSystemComponent()->ReplayState = PRS_Disabled;
			EmitterActor->GetParticleSystemComponent()->ReplayClipIDNumber = 0;
			EmitterActor->GetParticleSystemComponent()->ReplayFrameIndex = 0;
		}
	}
}


UInterpGroupCamera::UInterpGroupCamera(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	Target.Location = FVector(140.0f, 0.0f, -40.0f);
#endif // WITH_EDITORONLY_DATA

	CompressTolerance = 5.0f;
}

UInterpGroupInstCamera::UInterpGroupInstCamera(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UInterpGroupInstDirector::UInterpGroupInstDirector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITORONLY_DATA
/** Returns SpriteComponent subobject **/
UBillboardComponent* AMatineeActor::GetSpriteComponent() const { return SpriteComponent; }
/** Returns SpriteComponent subobject **/
UBillboardComponent* AMaterialInstanceActor::GetSpriteComponent() const { return SpriteComponent; }
#endif


/*-----------------------------------------------------------------------------
	Float anim BP parameter track.
-----------------------------------------------------------------------------*/
UInterpTrackFloatAnimBPParam::UInterpTrackFloatAnimBPParam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bRefreshParamter(false)
{
	TrackInstClass = UInterpTrackInstFloatAnimBPParam::StaticClass();
	TrackTitle = TEXT("Float AnimBP Param");
}

void UInterpTrackFloatAnimBPParam::Serialize(FArchive& Ar)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS	
	if (Ar.IsSaving() && Ar.UE4Ver() < VER_UE4_NO_ANIM_BP_CLASS_IN_GAMEPLAY_CODE)
	{
		if ((nullptr != AnimBlueprintClass) && (nullptr == AnimClass))
		{
			AnimClass = AnimBlueprintClass;
		}
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_NO_ANIM_BP_CLASS_IN_GAMEPLAY_CODE)
	{
		if ((nullptr != AnimBlueprintClass) && (nullptr == AnimClass))
		{
			AnimClass = AnimBlueprintClass;
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#if WITH_EDITOR

void UInterpTrackFloatAnimBPParam::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	FName PropertyName = PropertyThatChanged != NULL ? PropertyThatChanged->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UInterpTrackFloatAnimBPParam, ParamName) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UInterpTrackFloatAnimBPParam, AnimClass))
	{
		bRefreshParamter = true;
	}
}
#endif // WITH_EDITOR

int32 UInterpTrackFloatAnimBPParam::AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	int32 NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points[NewKeyIndex].InterpMode = InitInterpMode;
	if ( NewKeyIndex > 0 )
	{
		if ( NewKeyIndex < FloatTrack.Points.Num() - 1 )
		{
			const float Duration = FloatTrack.Points[NewKeyIndex+1].InVal - FloatTrack.Points[NewKeyIndex-1].InVal;
			const float Remaining = FloatTrack.Points[NewKeyIndex+1].InVal - FloatTrack.Points[NewKeyIndex].InVal;
			const float DurationPct = ( Duration > 0.f ? ((Duration - Remaining) / Duration) : 0.f );
			if( FloatTrack.Points[NewKeyIndex-1].InterpMode == CIM_Linear || FloatTrack.Points[NewKeyIndex-1].InterpMode == CIM_Constant )	// Linear or Constant interpolation
			{
				FloatTrack.Points[NewKeyIndex].OutVal = FMath::Lerp<float>(FloatTrack.Points[NewKeyIndex-1].OutVal, FloatTrack.Points[NewKeyIndex+1].OutVal, DurationPct);
			}
			else	// Cubic Interpolation
			{
				FloatTrack.Points[NewKeyIndex].OutVal = FMath::CubicInterp<float>(FloatTrack.Points[NewKeyIndex-1].OutVal, FloatTrack.Points[NewKeyIndex-1].LeaveTangent * Duration, FloatTrack.Points[NewKeyIndex+1].OutVal, FloatTrack.Points[NewKeyIndex+1].ArriveTangent * Duration, DurationPct);
			}
		}
		else	// Same position as previous point
		{
			FloatTrack.Points[NewKeyIndex].OutVal = FloatTrack.Points[NewKeyIndex-1].OutVal;
		}
	}
	else if ( NewKeyIndex < FloatTrack.Points.Num() - 1 )	// Same position as next point
	{
		FloatTrack.Points[NewKeyIndex].OutVal = FloatTrack.Points[NewKeyIndex+1].OutVal;
	}
	FloatTrack.AutoSetTangents(CurveTension);
	return NewKeyIndex;
}


void UInterpTrackFloatAnimBPParam::PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst)
{
	if (bRefreshParamter)
	{
		UInterpTrackInstFloatAnimBPParam* ParamTrackInst = Cast<UInterpTrackInstFloatAnimBPParam>(TrInst);
		if(ParamTrackInst != NULL)
		{
			ParamTrackInst->RefreshParameter(this);
		}

		bRefreshParamter = false;
	}

	UpdateTrack(NewPosition, TrInst, false);
}


void UInterpTrackFloatAnimBPParam::UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump)
{
	UInterpTrackInstFloatAnimBPParam* ParamTrackInst = Cast<UInterpTrackInstFloatAnimBPParam>(TrInst);
	if (ParamTrackInst != NULL)
	{
		float NewFloatValue = FloatTrack.Eval(NewPosition, 0.f);
		// set value
		ParamTrackInst->SetValue(NewFloatValue);
	}
}

UInterpTrackInstFloatAnimBPParam::UInterpTrackInstFloatAnimBPParam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AnimScriptInstance(nullptr)
	, ResetFloat(0.f)
	, ParamProperty(nullptr)
{
}

void UInterpTrackInstFloatAnimBPParam::InitTrackInst(UInterpTrack* Track)
{
	Super::InitTrackInst(Track);
	RefreshParameter(Track);
}

void UInterpTrackInstFloatAnimBPParam::RefreshParameter(UInterpTrack* Track)
{
	// if currently has correct setup, restore actor state
	RestoreActorState(Track);

	AnimScriptInstance = nullptr;
	ParamProperty = nullptr;

	UInterpTrackFloatAnimBPParam* ParamTrack = Cast<UInterpTrackFloatAnimBPParam>(Track);
	if(ParamTrack != nullptr && ParamTrack->ParamName != NAME_None)
	{
		AActor* Actor=GetGroupActor();
		if(Actor)
		{
			TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshComponents;
			Actor->GetComponents(SkeletalMeshComponents);

			if(SkeletalMeshComponents.Num() > 0)
			{
				UAnimInstance* NewAnimInstance = SkeletalMeshComponents[0]->GetAnimInstance();

				if (NewAnimInstance && NewAnimInstance->GetClass() == ParamTrack->AnimClass)
				{
					AnimScriptInstance = NewAnimInstance;
					// make sure the class has the parameter
					ParamProperty = NewAnimInstance->GetClass()->FindPropertyByName(ParamTrack->ParamName);
				}
			}
		}
	}

	// save actor state since now property might have changed
	SaveActorState(Track);
}

void UInterpTrackInstFloatAnimBPParam::SetValue(float InValue)
{
	if (AnimScriptInstance && ParamProperty)
	{
		float* Value = ParamProperty->ContainerPtrToValuePtr<float>(AnimScriptInstance);
		if(Value)
		{
			*Value = InValue;
		}
	}
}

float UInterpTrackInstFloatAnimBPParam::GetValue() const
{
	if(AnimScriptInstance && ParamProperty)
	{
		float* Value = ParamProperty->ContainerPtrToValuePtr<float>(AnimScriptInstance);
		if(Value)
		{
			return *Value;
		}
	}

	return 0.f;
}

void UInterpTrackInstFloatAnimBPParam::SaveActorState(UInterpTrack* Track) 
{
	ResetFloat = GetValue();
}
void UInterpTrackInstFloatAnimBPParam::RestoreActorState(UInterpTrack* Track)
{
	SetValue(ResetFloat);
}
