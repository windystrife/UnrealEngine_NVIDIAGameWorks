// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * Abstract base class for a track of interpolated data. Contains the actual data.
 * The Outer of an InterpTrack is the UInterpGroup.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Interpolation.h"
#include "InterpTrack.generated.h"

class FCanvas;
class FPrimitiveDrawInterface;
class FSceneView;
class UInterpGroup;
class UInterpTrackInst;
class UTexture2D;

//
// Forward declarations
//
class FCanvas;
class UInterpGroup;

/** Required condition for this track to be enabled */
UENUM()
enum ETrackActiveCondition
{
	/** Track is always active */
	ETAC_Always,
	/** Track is active when extreme content (gore) is enabled */
	ETAC_GoreEnabled,
	/** Track is active when extreme content (gore) is disabled */
	ETAC_GoreDisabled,
	ETAC_MAX,
};

/** Helper struct for creating sub tracks supported by this track */
USTRUCT()
struct FSupportedSubTrackInfo
{
	GENERATED_USTRUCT_BODY()

	/** The sub track class which is supported by this track */
	UPROPERTY()
	TSubclassOf<class UInterpTrack>  SupportedClass;

	/** The name of the subtrack */
	UPROPERTY()
	FString SubTrackName;

	/** Index into the any subtrack group this subtrack belongs to (can be -1 for no group) */
	UPROPERTY()
	int32 GroupIndex;


	FSupportedSubTrackInfo()
		: SupportedClass(NULL)
		, GroupIndex(0)
	{
	}

};

/** A small structure holding data for grouping subtracks. (For UI drawing purposes) */
USTRUCT()
struct FSubTrackGroup
{
	GENERATED_USTRUCT_BODY()

	/** Name of the subtrack  group */
	UPROPERTY()
	FString GroupName;

	/** Indices to tracks in the parent track subtrack array. */
	UPROPERTY()
	TArray<int32> TrackIndices;

	/** If this group is collapsed */
	UPROPERTY()
	uint32 bIsCollapsed:1;

	/** If this group is selected */
	UPROPERTY(transient)
	uint32 bIsSelected:1;


	FSubTrackGroup()
		: bIsCollapsed(false)
		, bIsSelected(false)
	{
	}

};

UCLASS(collapsecategories, hidecategories=Object, abstract, MinimalAPI)
class UInterpTrack : public UObject, public FCurveEdInterface, public FInterpEdInputInterface
{
	GENERATED_UCLASS_BODY()

	/** A list of subtracks that belong to this track */
	UPROPERTY(BlueprintReadOnly, Category=InterpTrack)
	TArray<class UInterpTrack*> SubTracks;

#if WITH_EDITORONLY_DATA
	/** A list of subtrack groups (for editor UI organization only) */
	UPROPERTY()
	TArray<struct FSubTrackGroup> SubTrackGroups;

	/** A list of supported tracks that can be a subtrack of this track. */
	UPROPERTY(transient)
	TArray<struct FSupportedSubTrackInfo> SupportedSubTracks;

#endif // WITH_EDITORONLY_DATA
	UPROPERTY()
	TSubclassOf<class UInterpTrackInst>  TrackInstClass;

	/** Sets the condition that must be met for this track to be enabled */
	UPROPERTY(EditAnywhere, Category=InterpTrack)
	TEnumAsByte<enum ETrackActiveCondition> ActiveCondition;

	/** Title of track type. */
	UPROPERTY()
	FString TrackTitle;

	/** Whether there may only be one of this track in an UInterpGroup. */
	UPROPERTY()
	uint32 bOnePerGroup:1;

	/** If this track can only exist inside the Director group. */
	UPROPERTY()
	uint32 bDirGroupOnly:1;

private:
	/** Whether or not this track should actually update the target actor. */
	UPROPERTY()
	uint32 bDisableTrack:1;

	/** Whether or not this track is selected in the editor. */
	UPROPERTY(transient)
	uint32 bIsSelected:1;
	
public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	UTexture2D* TrackIcon;
#endif // WITH_EDITORONLY_DATA

	/** If true, the  AActor  this track is working on will have BeginAnimControl/FinishAnimControl called on it. */
	UPROPERTY()
	uint32 bIsAnimControlTrack:1;

	/** If this track can only exist as a sub track. */
	UPROPERTY()
	uint32 bSubTrackOnly:1;

	/** Whether or not this track is visible in the editor. */
	UPROPERTY(transient)
	uint32 bVisible:1;

	/** Whether or not this track is recording in the editor. */
	UPROPERTY(transient)
	uint32 bIsRecording:1;

#if WITH_EDITORONLY_DATA
	/** If this track is collapsed. (Only applies  to tracks with subtracks). */
	UPROPERTY()
	uint32 bIsCollapsed:1;
#endif // WITH_EDITORONLY_DATA


public:
	//~ Begin FInterpEdInputInterface Interface
	virtual UObject* GetUObject() override {return this;}
	//~ End FInterpEdInputInterface Interface

	/** @return  The total number of keyframes currently in this track. */
	virtual int32 GetNumKeyframes() const { return 0; }

	/**
	 * Gathers the range that spans all keyframes.
	 * @param   StartTime   [out] The time of the first keyframe on this track.
	 * @param   EndTime     [out] The time of the last keyframe on this track. 
	 */
	virtual void GetTimeRange(float& StartTime, float& EndTime) const { StartTime = 0.f; EndTime = 0.f; }

	/** Get the time of the keyframe with the given index. 
	 * @param   KeyIndex    The index of the key to retrieve the time in the track's key array. 
	 * @return  The time of the given key in the track on the timeline. 
	 */
	virtual float GetKeyframeTime(int32 KeyIndex) const { return 0.f; }

	/** Get the index of the keyframe with the given time. */
	virtual int32 GetKeyframeIndex( float KeyTime ) const { return INDEX_NONE; }

	/**
	 * Adds a keyframe at the given time to the track.
	 * 
	 * @param	Time			The time to place the key in the track timeline.
	 * @param	TrackInst		The instance of this track. 
	 * @param	InitInterpMode	The interp mode of the newly-added keyframe.
	 */
	virtual int32 AddKeyframe( float Time, class UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode ) { return INDEX_NONE; }

	/**
	 * Adds a keyframe to a child track
	 *
	 * @param ChildTrack		The child track where the keyframe should be added
	 * @param Time				What time the keyframe is located at
	 * @param TrackInst			The track instance of the parent track(this track)
	 * @param InitInterpMode	The initial interp mode for the keyframe?
	 */
	virtual int32 AddChildKeyframe( class UInterpTrack* ChildTrack, float Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode ) { return INDEX_NONE; }

	/**
	 * Can a keyframe be added to the track?
	 * 
	 * @param	TrackInst		The instance of this track. 
	 */
	virtual bool CanAddKeyframe( class UInterpTrackInst* TrackInst ) { return true; }

	/**
	 * Can a keyframe be added to a child track?
	 *
	 * @param TrackInst			The track instance of the parent track(this track)
	 */
	virtual bool CanAddChildKeyframe( class UInterpTrackInst* TrackInst ) { return true; }

	/**
	 * Changes the value of an existing keyframe.
	 *
	 * @param	KeyIndex	The index of the key to update in the track's key array. 
	 * @param	TrackInst	The instance of this track to update. 
	 */
	virtual void UpdateKeyframe(int32 KeyIndex, class UInterpTrackInst* TrInst) {}

	/**
	 * Updates a child track keyframe
	 *
	 * @param ChildTrack		The child track with keyframe to update
	 * @param KeyIndex			The index of the key to be updated
	 * @param TrackInst			The track instance of the parent track(this track)
	 */
	virtual void UpdateChildKeyframe( class UInterpTrack* ChildTrack, int32 KeyIndex, UInterpTrackInst* TrackInst ) {};

	/**
	 * Changes the time of the given key with the new given time.
	 * 
	 * @param   KeyIndex        The index key to change in the track's key array.
	 * @param   NewKeyTime      The new time for the given key in the timeline.
	 * @param   bUpdateOrder    When true, moves the key to be in chronological order in the array. 
	 * 
	 * @return  This can change the index of the keyframe - the new index is returned. 
	 */
	virtual int32 SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder=true) { return INDEX_NONE; }

	/**
	 * Removes the given key from the array of keys in the track.
	 * 
	 * @param   KeyIndex    The index of the key to remove in this track's array of keys. 
	 */
	virtual void RemoveKeyframe(int32 KeyIndex) {}

	/**
	 * Duplicates the given key.
	 * 
	 * @param   KeyIndex    The index of the key to duplicate in this track's array of keys.
	 * @param   NewKeyTime  The time to assign to the duplicated key.
	 * @param   ToTrack		Optional, if specified duplicates the key this track instead of 'this'
	 * 
	 * @return  The new index for the given key.
	 */
	virtual int32 DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL) { return INDEX_NONE; }

	/**
	 * Gets the position of the closest key with snapping incorporated.
	 * 
	 * @param   InPosition  The current position in the timeline.
	 * @param   IgnoreKeys  The set of keys to ignore when searching for the closest key.
	 * @param   OutPosition The position of the closest key with respect to snapping and ignoring the given set of keys.
	 * 
	 * @return  true if a keyframe was found; false if no keyframe was found. 
	 */
	virtual bool GetClosestSnapPosition(float InPosition, TArray<int32> &IgnoreKeys, float& OutPosition) { return false; }

	/**
	 *	Conditionally calls PreviewUpdateTrack depending on whether or not the track is enabled.
	 */
	ENGINE_API virtual void ConditionalPreviewUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst);

	/**
	 *	Conditionally calls UpdateTrack depending on whether or not the track is enabled.
	 */
	ENGINE_API virtual void ConditionalUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst, bool bJump);


	/**
	 * Updates the instance of this track based on the new position. This is for editor preview.
	 *
	 * @param	NewPosition	The position of the track in the timeline. 
	 * @param	TrackInst	The instance of this track to update. 
	 */
	virtual void PreviewUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst) {} // This is called in the editor, when scrubbing/previewing etc

	/** 
	 * Updates the instance of this track based on the new position. This is called in the game, when a MatineeActor is ticked.
	 *
	 * @param	NewPosition	The position of the track in the timeline. 
	 * @param	TrackInst	The instance of this track to update. 
	 * @param	bJump		Indicates if this is a sudden jump instead of a smooth move to the new position.
	 */
	virtual void UpdateTrack(float NewPosition, class UInterpTrackInst* TrInst, bool bJump) {} // This is called in the game, when a MatineeActor is ticked

	/** Called when playback is stopped in Matinee. Useful for stopping sounds etc. */
	virtual void PreviewStopPlayback(class UInterpTrackInst* TrInst) {}

	/** 
	 * Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
	 * 
	 * @return	String name of the helper class.
	 */
	virtual const FString	GetEdHelperClassName() const;

	/**
	 * Get the name of the class used to help out when adding tracks, keys, etc. in Slate.
	 */
	virtual const FString	GetSlateHelperClassName() const;

#if WITH_EDITORONLY_DATA
	/** @return	The icon to draw for this track in Matinee. */
	virtual class UTexture2D* GetTrackIcon() const;
#endif // WITH_EDITORONLY_DATA

	/** @return  true if this track type works with static actors; false, otherwise. */
	virtual bool AllowStaticActors() { return false; }

	/** Draw this track with the specified parameters */
	ENGINE_API virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );

	/** @return color to draw each keyframe in Matinee. */
	virtual FColor GetKeyframeColor(int32 KeyIndex) const;

	/**
	 * @return	The ending time of the track.
	 */
	virtual float GetTrackEndTime() const { return 0.0f; }

	/**
	 *	For drawing track information into the 3D scene.
	 *	TimeRes is how often to draw an event (eg. resoltion of spline path) in seconds.
	 */
	virtual void Render3DTrack(class UInterpTrackInst* TrInst,
		const FSceneView* View,
		FPrimitiveDrawInterface* PDI,
		int32 TrackIndex,
		const FColor& TrackColor,
		TArray<struct FInterpEdSelKey>& SelectedKeys) {}

	/** Set this track to sensible default values. Called when track is first created. */
	virtual void SetTrackToSensibleDefault() {}


	/**
	 * @return the outer group of this track.  If this track is a subtrack, the group of its parent track is returned
	 */
	ENGINE_API UInterpGroup* GetOwningGroup();

	/**
	 * Enables this track and optionally, all subtracks.
	 *
	 * @param bInEnable				True if the track should be enabled, false to disable
	 * @param bPropagateToSubTracks	True to propagate the state to all subtracks
	 */
	ENGINE_API void EnableTrack( bool bInEnable, bool bPropagateToSubTracks = true );

	/** Returns true if this track has been disabled.  */
	bool IsDisabled() const { return bDisableTrack; }

	/**
	 * Selects this track
	 *
	 * @param bInSelected				True if the track should be selected, false to deselect
	 */
	virtual void SetSelected( bool bInSelected ) { bIsSelected = bInSelected; }

	/** Returns true if this track has been selected.  */
	bool IsSelected() const { return bIsSelected; }

	/**
	 * Creates and adds subtracks to this track
	 *
	 * @param bCopy	If subtracks are being added as a result of a copy
	 */
	virtual void CreateSubTracks( bool bCopy ) {};

	/**
	 * Reduce Keys within Tolerance
	 *
	 * @param bIntervalStart	start of the key to reduce
	 * @param bIntervalEnd		end of the key to reduce
	 * @param Tolerance			tolerance
	 */
	virtual void ReduceKeys( float IntervalStart, float IntervalEnd, float Tolerance ){};

	/**
	 * Called by owning actor on position shifting
	 *
	 * @param InOffset		Offset vector from previous actor position
	 * @param bWorldShift	Whether this call is part of whole world shifting
	 */
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) {};
};



