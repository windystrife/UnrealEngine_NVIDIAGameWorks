// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * Group for controlling properties of a 'CameraAnim' in the game. Used for CameraAnim Previews
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Matinee/InterpGroup.h"
#include "InterpGroupCamera.generated.h"

/**
 *	Preview APawn class for this track
 */
USTRUCT()
struct FCameraPreviewInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=CameraPreviewInfo)
	TSubclassOf<class APawn>  PawnClass;

	UPROPERTY(EditAnywhere, Category=CameraPreviewInfo)
	class UAnimSequence* AnimSeq;

	/* for now this is read-only. It has maintenance issue to be resolved if I enable this.*/
	UPROPERTY(VisibleAnywhere, Category=CameraPreviewInfo)
	FVector Location;

	UPROPERTY(VisibleAnywhere, Category=CameraPreviewInfo)
	FRotator Rotation;

	/** APawn Inst - CameraAnimInst doesn't really exist in editor **/
	UPROPERTY(transient)
	class APawn* PawnInst;



		FCameraPreviewInfo()
		: PawnClass(NULL)
		, AnimSeq(NULL)
		, Location(ForceInit)
		, Rotation(ForceInit)
		, PawnInst(NULL)
		{
		}
	
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UInterpGroupCamera : public UInterpGroup
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(transient)
	class UCameraAnim* CameraAnimInst;

#if WITH_EDITORONLY_DATA
	// this is interaction property info for CameraAnim
	// this information isn't really saved with it
	UPROPERTY(EditAnywhere, Category=InterpGroupCamera)
	struct FCameraPreviewInfo Target;

#endif // WITH_EDITORONLY_DATA
	//editable() editoronly bool                           EnableInteractionTarget;
	//editable() editoronly CameraAnim.CameraPreviewInfo   InteractionTarget;
	
	/** When compress, tolerance option **/
	UPROPERTY(EditAnywhere, Category=InterpGroupCamera)
	float CompressTolerance;
};



