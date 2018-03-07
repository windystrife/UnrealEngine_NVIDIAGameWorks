// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/**
 * This interface deals with Matinee Track for Anim Control 
 * 
 * If you have an actor that needs to support Anim Control Track, implemented this interface
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "UObject/Interface.h"
#include "MatineeAnimInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UMatineeAnimInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IMatineeAnimInterface
{
	GENERATED_IINTERFACE_BODY()


	
	//
	// Editor preview support.
	//

	/**
	 *	Called by Matinee when we open it to start controlling animation on this  AActor  for editor preview
	 *	Is also called again when the GroupAnimSets array changes in Matinee, so must support multiple calls.
	 * @param UInterpGroup	UInterpGroup of this track
	 */
	virtual void PreviewBeginAnimControl(class UInterpGroup* InInterpGroup) PURE_VIRTUAL(UMatineeAnimInterface::PreviewBeginAnimControl,);

	/** 
	 * Called each frame by Matinee to update the desired sequence by name and position within it. 
	 * @param SlotName	Slot Node Name that this track is setting the anim position for
	 * @param ChannelIndex	Index of the child that it wants to play
	 * @param InAnimSequence	Anim sequence to play
	 * @param InPosition	Position of the animation
	 * @param bLooping		true if looping. false otherwise
	 * @param bFireNotifiers	true if it wants to fire AnimNotifiers
	 * @param AdvanceTime		Used if bEnableRootMotion == true as a delta time
	 */
	virtual void PreviewSetAnimPosition(FName SlotName, int32 ChannelIndex, class UAnimSequence* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies, float AdvanceTime) PURE_VIRTUAL(UMatineeAnimInterface::PreviewSetAnimPosition,);

	/** 
	 * Called each frame by Matinee to update the desired animation channel weights for this Actor. 
	 * @param SlotInfos	Array of SlotInfos : pair <slot node name, children weight>
	 */
	virtual void PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos) PURE_VIRTUAL(UMatineeAnimInterface::PreviewSetAnimWeights,);

	/** 
	 * Called by Matinee when we close it after we have been controlling animation on this Actor. 
	 * @param UInterpGroup	UInterpGroup of this track
	 */
	virtual void PreviewFinishAnimControl(class UInterpGroup* InInterpGroup) PURE_VIRTUAL(UMatineeAnimInterface::PreviewFinishAnimControl,);

	//
	// Other.
	//

	/** 
	 * Used to provide information on the slots that this  AActor  provides for animation to Matinee. 
	 * @param OutSlotDescs	Fill up all slot nodes of this actor : pair <slot node name, total number of channel>
	 */
	virtual void GetAnimControlSlotDesc(TArray<struct FAnimSlotDesc>& OutSlotDescs) PURE_VIRTUAL(UMatineeAnimInterface::GetAnimControlSlotDesc,);

	/**
	 * Called each from while the Matinee action is running, to set the animation weights for the actor. 
	 * @param SlotInfos	Array of SlotInfos : pair <slot node name, children weight>
	 */
	virtual void SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos ) PURE_VIRTUAL(UMatineeAnimInterface::SetAnimWeights,);

	/** 
	 * Called when we start an AnimControl track operating on this Actor. Supplied is the set of AnimSets we are going to want to play from. 
	 * @param UInterpGroup	UInterpGroup of this track
	 */
	virtual void BeginAnimControl(class UInterpGroup* InInterpGroup)=0;

	/** 
	 * Called each from while the Matinee action is running, with the desired sequence name and position we want to be at. 
	 *
	 * @param SlotName	Slot Node Name that this track is setting the anim position for
	 * @param ChannelIndex	Index of the child that it wants to play
	 * @param InAnimSequence	Anim Sequence to play
	 * @param InPosition	Position of the animation
	 * @param bLooping		true if looping. false otherwise
	 * @param bFireNotifiers	true if it wants to fire AnimNotifiers
	 */
	virtual void SetAnimPosition(FName SlotName, int32 ChannelIndex, class UAnimSequence* InAnimSequence, float InPosition, bool bFireNotifies, bool bLooping)=0;

	/** 
	 * Called when we are done with the AnimControl track. 
	 * 
	 * @param UInterpGroup	UInterpGroup of this track
	 */
	virtual void FinishAnimControl(class UInterpGroup* InInterpGroup)=0;
};

