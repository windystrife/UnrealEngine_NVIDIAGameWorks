// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Interpolation.h"
#include "InterpGroup.generated.h"

class AActor;
class UInterpTrack;

/**
 *
 * A group, associated with a particular  AActor  or set of Actors, which contains a set of InterpTracks for interpolating 
 * properties of the  AActor  over time.
 * The Outer of an UInterpGroup is an InterpData.
 */
USTRUCT()
struct FInterpEdSelKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	class UInterpGroup* Group;

	UPROPERTY()
	class UInterpTrack* Track;

	UPROPERTY()
	int32 KeyIndex;

	UPROPERTY()
	float UnsnappedPosition;



		FInterpEdSelKey()
		{
			Group = NULL;
			Track = NULL;
			KeyIndex = INDEX_NONE;
			UnsnappedPosition = 0.f;
		}
		FInterpEdSelKey(class UInterpGroup* InGroup, class UInterpTrack* InTrack, int32 InKey)
		{
			Group = InGroup;
			Track = InTrack;
			KeyIndex = InKey;
		}
		bool operator==(const FInterpEdSelKey& Other) const
		{
			if(	Group == Other.Group &&
				Track == Other.Track &&
				KeyIndex == Other.KeyIndex )
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		/** 
		 * Returns the parent track of this key.  If this track isn't a subtrack, Track is returned (it owns itself)
		 */
		class UInterpTrack* GetOwningTrack();

		/** 
		 * Returns the sub group name of the parent track of this key. If this track isn't a subtrack, nothing is returned
		 */
		ENGINE_API FString GetOwningTrackSubGroupName( int32* piSubTrack = NULL );

	private:
		/** 
		 * Recursive function used by GetOwningTrack();  to search through all subtracks
		 */
		class UInterpTrack* GetOwningTrack( UInterpTrack* pTrack );
	
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UInterpGroup : public UObject, public FInterpEdInputInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(export, BlueprintReadOnly, Category=InterpGroup)
	TArray<class UInterpTrack*> InterpTracks;

	/** 
	 *	Within an InterpData, all GroupNames must be unique. 
	 *	Used for naming Variable connectors on the Action in Kismet and finding each groups object.
	 */
	UPROPERTY()
	FName GroupName;

	/** Colour used for drawing tracks etc. related to this group. */
	UPROPERTY(EditAnywhere, Category=InterpGroup)
	FColor GroupColor;

	/** Whether or not this group is folded away in the editor. */
	UPROPERTY()
	uint32 bCollapsed:1;

	/** Whether or not this group is visible in the editor. */
	UPROPERTY(transient)
	uint32 bVisible:1;

	/** When enabled, this group is treated like a folder in the editor, which should only be used for organization.  Folders are never associated with actors and don't have a presence in the Kismet graph. */
	UPROPERTY()
	uint32 bIsFolder:1;

	/** When true, this group is considered a 'visual child' of another group.  This doesn't at all affect the behavior of the group, it's only for visual organization.  Also, it's implied that the parent is the next prior group in the array that doesn't have a parent. */
	UPROPERTY()
	uint32 bIsParented:1;

private:
	/** When enabled, this group will be selected in the interp editor. */
	UPROPERTY(transient)
	uint32 bIsSelected:1;

public:
	//~ Begin UObject Interface.
	virtual void PostLoad() override;
	//~ End UObject Interface.


	/** Iterate over all InterpTracks in this UInterpGroup, doing any actions to bring the state to the specified time. */
	virtual void UpdateGroup(float NewPosition, class UInterpGroupInst* GrInst, bool bPreview, bool bJump);

	/**
	 * Selects this group
	 *
	 * @param bInSelected				True if the group should be selected, false to deselect
	 */
	virtual void SetSelected( bool bInSelected ) { bIsSelected = bInSelected; }

	/** Returns true if this group has been selected.  */
	bool IsSelected() const { return bIsSelected; }

	/** @return true if this group contains selected tracks.  */
	ENGINE_API bool HasSelectedTracks() const;

	/** Ensure this group name is unique within this InterpData (its Outer). */
	ENGINE_API void EnsureUniqueName();

	/** 
	 *	Find all the tracks in this group of a specific class.
	 *	Tracks are in the output array in the order they appear in the group.
	 */
	ENGINE_API void FindTracksByClass(UClass* TrackClass, TArray<class UInterpTrack*>& OutputTracks);

	/** Returns whether this Group contains at least one AnimControl track. */
	ENGINE_API bool HasAnimControlTrack() const;

	/** Returns whether this Group contains a movement track. */
	ENGINE_API bool HasMoveTrack() const;

	/** Iterate over AnimControl tracks in this Group, build the anim blend info structures, and pass to the AActor via (Preview)SetAnimWeights. */
	void UpdateAnimWeights(float NewPosition, class UInterpGroupInst* GrInst, bool bPreview, bool bJump);

	/** Util for determining how many AnimControl tracks within this group are using the Slot with the supplied name. */
	ENGINE_API int32 GetAnimTracksUsingSlot(FName InSlotName);

	/**
	 * Selects the group actor associated with the interp group. 
	 *
	 * @param	GrInst	The group corresponding to the referenced actor to select. 
	 * @param	bDeselectActors	If true, deselects all other actors first
	 */
	virtual AActor* SelectGroupActor( class UInterpGroupInst* GrInst, bool bDeselectActors = false );

	/** Deselects the group actor associated with the interp group. */
	virtual AActor* DeselectGroupActor( class UInterpGroupInst* GrInst );
};



