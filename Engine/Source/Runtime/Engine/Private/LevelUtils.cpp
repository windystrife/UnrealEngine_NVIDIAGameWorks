// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "LevelUtils.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "EditorSupportDelegates.h"
#include "EngineGlobals.h"
#include "Misc/FeedbackContext.h"
#include "GameFramework/WorldSettings.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "LevelUtils"


#if WITH_EDITOR
// Structure to hold the state of the Level file on disk, the goal is to query it only one time per frame.
struct FLevelReadOnlyData
{
	FLevelReadOnlyData()
		:IsReadOnly(false)
		,LastUpdateTime(-1.0f)
	{}
	/** the current level file state */
	bool IsReadOnly;
	/** Last time when the level file state was update */
	float LastUpdateTime;
};
// Map to link the level data with a level
static TMap<ULevel*, FLevelReadOnlyData> LevelReadOnlyCache;

#endif

/////////////////////////////////////////////////////////////////////////////////////////
//
//	FindStreamingLevel methods.
//
/////////////////////////////////////////////////////////////////////////////////////////


#if WITH_EDITOR
bool FLevelUtils::bMovingLevel = false;
#endif

/**
 * Returns the streaming level corresponding to the specified ULevel, or NULL if none exists.
 *
 * @param		Level		The level to query.
 * @return					The level's streaming level, or NULL if none exists.
 */
ULevelStreaming* FLevelUtils::FindStreamingLevel(const ULevel* Level)
{
	ULevelStreaming* MatchingLevel = NULL;

	if (Level && Level->OwningWorld)
	{
		for( int32 LevelIndex = 0 ; LevelIndex < Level->OwningWorld->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming* CurStreamingLevel = Level->OwningWorld->StreamingLevels[ LevelIndex ];
			if( CurStreamingLevel && CurStreamingLevel->GetLoadedLevel() == Level )
			{
				MatchingLevel = CurStreamingLevel;
				break;
			}
		}
	}

	return MatchingLevel;
}

/**
 * Returns the streaming level by package name, or NULL if none exists.
 *
 * @param		PackageName		Name of the package containing the ULevel to query
 * @return						The level's streaming level, or NULL if none exists.
 */
ULevelStreaming* FLevelUtils::FindStreamingLevel(UWorld* InWorld, const TCHAR* InPackageName)
{
	const FName PackageName( InPackageName );
	ULevelStreaming* MatchingLevel = NULL;
	if( InWorld)
	{
		for( int32 LevelIndex = 0 ; LevelIndex< InWorld->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming* CurStreamingLevel = InWorld->StreamingLevels[ LevelIndex ];
			if( CurStreamingLevel && CurStreamingLevel->GetWorldAssetPackageFName() == PackageName )
			{
				MatchingLevel = CurStreamingLevel;
				break;
			}
		}
	}
	return MatchingLevel;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//	Level locking/unlocking.
//
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns true if the specified level is locked for edit, false otherwise.
 *
 * @param	Level		The level to query.
 * @return				true if the level is locked, false otherwise.
 */
bool FLevelUtils::IsLevelLocked(ULevel* Level)
{
//We should not check file status on disk if we are not running the editor
#if WITH_EDITOR
	// Don't permit spawning in read only levels if they are locked
	if ( GIsEditor && !GIsEditorLoadingPackage )
	{
		if ( GEngine && GEngine->bLockReadOnlyLevels )
		{
			if (!LevelReadOnlyCache.Contains(Level))
			{
				LevelReadOnlyCache.Add(Level, FLevelReadOnlyData());
			}
			check(LevelReadOnlyCache.Contains(Level));
			FLevelReadOnlyData &LevelData = LevelReadOnlyCache[Level];
			//Make sure we test if the level file on disk is readonly only once a frame,
			//when the frame time get updated.
			if (LevelData.LastUpdateTime < Level->OwningWorld->GetRealTimeSeconds())
			{
				LevelData.LastUpdateTime = Level->OwningWorld->GetRealTimeSeconds();
				//If we dont find package we dont consider it as readonly
				LevelData.IsReadOnly = false;
				const UPackage* pPackage = Level->GetOutermost();
				if (pPackage)
				{
					FString PackageFileName;
					if (FPackageName::DoesPackageExist(pPackage->GetName(), NULL, &PackageFileName))
					{
						LevelData.IsReadOnly = IFileManager::Get().IsReadOnly(*PackageFileName);
					}
				}
			}

			if (LevelData.IsReadOnly)
			{
				return true;
			}
		}
	}
#endif //#if WITH_EDITOR

	// PIE levels, the persistent level, and transient move levels are usually never locked.
	if ( Level->RootPackageHasAnyFlags(PKG_PlayInEditor) || Level->IsPersistentLevel() || Level->GetName() == TEXT("TransLevelMoveBuffer") )
	{
		return false;
	}

	ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
	if ( StreamingLevel != NULL )
	{
		return StreamingLevel->bLocked;
	}
	else
	{
		return Level->bLocked;
	}
}
bool FLevelUtils::IsLevelLocked( AActor* Actor )
{
	return Actor != NULL && !Actor->IsTemplate() && Actor->GetLevel() != NULL && IsLevelLocked(Actor->GetLevel());
}

/**
 * Sets a level's edit lock.
 *
 * @param	Level		The level to modify.
 */
void FLevelUtils::ToggleLevelLock(ULevel* Level)
{
	if ( !Level || Level->IsPersistentLevel() )
	{
		return;
	}

	ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
	if ( StreamingLevel != NULL )
	{
		// We need to set the RF_Transactional to make a streaming level serialize itself. so store the original ones, set the flag, and put the original flags back when done
		EObjectFlags cachedFlags = StreamingLevel->GetFlags();
		StreamingLevel->SetFlags( RF_Transactional );
		StreamingLevel->Modify();			
		StreamingLevel->SetFlags( cachedFlags );

		StreamingLevel->bLocked = !StreamingLevel->bLocked;
	}
	else
	{
		Level->Modify();
		Level->bLocked = !Level->bLocked;	
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//	Level loading/unloading.
//
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns true if the level is currently loaded in the editor, false otherwise.
 *
 * @param	Level		The level to query.
 * @return				true if the level is loaded, false otherwise.
 */
bool FLevelUtils::IsLevelLoaded(ULevel* Level)
{
	if ( Level && Level->IsPersistentLevel() )
	{
		// The persistent level is always loaded.
		return true;
	}

	ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
	checkf( StreamingLevel, TEXT("Couldn't find streaming level" ) );

	// @todo: Dave, please come talk to me before implementing anything like this.
	return true;
}

/**
 * Flags an unloaded level for loading.
 *
 * @param	Level		The level to modify.
 */
void FLevelUtils::MarkLevelForLoading(ULevel* Level)
{
	// If the level is valid and not the persistent level (which is always loaded) . . .
	if ( Level && !Level->IsPersistentLevel() )
	{
		// Mark the level's stream for load.
		ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
		checkf( StreamingLevel, TEXT("Couldn't find streaming level" ) );
		// @todo: Dave, please come talk to me before implementing anything like this.
	}
}

/**
 * Flags a loaded level for unloading.
 *
 * @param	Level		The level to modify.
 */
void FLevelUtils::MarkLevelForUnloading(ULevel* Level)
{
	// If the level is valid and not the persistent level (which is always loaded) . . .
	if ( Level && !Level->IsPersistentLevel() )
	{
		ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
		checkf( StreamingLevel, TEXT("Couldn't find streaming level" ) );
		// @todo: Dave, please come talk to me before implementing anything like this.
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//	Level visibility.
//
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns true if the specified level is visible in the editor, false otherwise.
 *
 * @param	StreamingLevel		The level to query.
 */
bool FLevelUtils::IsLevelVisible(const ULevelStreaming* StreamingLevel)
{
	const bool bVisible = StreamingLevel && StreamingLevel->bShouldBeVisibleInEditor;
	return bVisible;
}

/**
 * Returns true if the specified level is visible in the editor, false otherwise.
 *
 * @param	Level		The level to query.
 */
bool FLevelUtils::IsLevelVisible(const ULevel* Level)
{
	if (!Level)
	{
		return false;
	}

	// P-level is specially handled
	if ( Level->IsPersistentLevel() )
	{
#if WITH_EDITORONLY_DATA
		return !( Level->OwningWorld->PersistentLevel->GetWorldSettings()->bHiddenEdLevel );
#else
		return true;
#endif
	}

	static const FName NAME_TransLevelMoveBuffer(TEXT("TransLevelMoveBuffer"));
	if (Level->GetFName() == NAME_TransLevelMoveBuffer)
	{
		// The TransLevelMoveBuffer does not exist in the streaming list and is never visible
		return false;
	}

	return Level->bIsVisible;
}

#if WITH_EDITOR
/////////////////////////////////////////////////////////////////////////////////////////
//
//	Level editor transforms.
//
/////////////////////////////////////////////////////////////////////////////////////////

void FLevelUtils::SetEditorTransform(ULevelStreaming* StreamingLevel, const FTransform& Transform, bool bDoPostEditMove )
{
	check(StreamingLevel);

	// Check we are actually changing the value
	if(StreamingLevel->LevelTransform.Equals(Transform))
	{
		return;
	}

	// Setup an Undo transaction
	const FScopedTransaction LevelOffsetTransaction( LOCTEXT( "ChangeEditorLevelTransform", "Edit Level Transform" ) );
	StreamingLevel->Modify();

	// Apply new transform
	RemoveEditorTransform(StreamingLevel, false );
	StreamingLevel->LevelTransform = Transform;
	ApplyEditorTransform(StreamingLevel, bDoPostEditMove);

	// Redraw the viewports to see this change
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

void FLevelUtils::ApplyEditorTransform(const ULevelStreaming* StreamingLevel, bool bDoPostEditMove )
{
	check(StreamingLevel);
	ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
	if( LoadedLevel != NULL )
	{	
		ApplyLevelTransform( LoadedLevel, StreamingLevel->LevelTransform, bDoPostEditMove );
	}
}

void FLevelUtils::RemoveEditorTransform(const ULevelStreaming* StreamingLevel, bool bDoPostEditMove )
{
	check(StreamingLevel);
	ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
	if( LoadedLevel != NULL )
	{
		ApplyLevelTransform( LoadedLevel, StreamingLevel->LevelTransform.Inverse(), bDoPostEditMove );
	}
}

void FLevelUtils::ApplyPostEditMove( ULevel* Level )
{	
	check(Level)	
	GWarn->BeginSlowTask( LOCTEXT( "ApplyPostEditMove", "Updating all actors in level after move" ), true);

	const int32 NumActors = Level->Actors.Num();

	// Iterate over all actors in the level and transform them
	bMovingLevel = true;
	for( int32 ActorIndex=0; ActorIndex < NumActors; ++ActorIndex )
	{
		GWarn->UpdateProgress( ActorIndex, NumActors );
		AActor* Actor = Level->Actors[ActorIndex];
		if( Actor )
		{
			if (!Actor->GetWorld()->IsGameWorld() )
			{
				Actor->PostEditMove(true);				
			}
		}
	}
	bMovingLevel = false;
	GWarn->EndSlowTask();	
}


bool FLevelUtils::IsMovingLevel()
{
	return bMovingLevel;
}

#endif // WITH_EDITOR


void FLevelUtils::ApplyLevelTransform( ULevel* Level, const FTransform& LevelTransform, bool bDoPostEditMove )
{
	bool bTransformActors =  !LevelTransform.Equals(FTransform::Identity);
	if (bTransformActors)
	{
		if (!LevelTransform.GetRotation().IsIdentity())
		{
			// If there is a rotation applied, then the relative precomputed bounds become invalid.
			Level->bTextureStreamingRotationChanged = true;
		}

		// Iterate over all actors in the level and transform them
		for( int32 ActorIndex=0; ActorIndex<Level->Actors.Num(); ActorIndex++ )
		{
			AActor* Actor = Level->Actors[ActorIndex];

			// Don't want to transform children they should stay relative to there parents.
			if( Actor && Actor->GetAttachParentActor() == NULL )
			{
				// Has to modify root component directly as GetActorPosition is incorrect this early
				USceneComponent *RootComponent = Actor->GetRootComponent();
				if (RootComponent)
				{
					RootComponent->SetRelativeLocationAndRotation( LevelTransform.TransformPosition(RootComponent->RelativeLocation), (FTransform(RootComponent->RelativeRotation) * LevelTransform).Rotator());
				}			
			}
		}

#if WITH_EDITOR
		if( bDoPostEditMove )
		{
			ApplyPostEditMove( Level );						
		}
#endif // WITH_EDITOR

		Level->OnApplyLevelTransform.Broadcast(LevelTransform);
	}
}



#undef LOCTEXT_NAMESPACE
