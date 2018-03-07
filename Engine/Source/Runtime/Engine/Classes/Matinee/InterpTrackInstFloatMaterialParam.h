// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInst.h"
#include "Engine/EngineTypes.h"
#include "InterpTrackInstFloatMaterialParam.generated.h"

class UInterpTrack;

UCLASS()
class UInterpTrackInstFloatMaterialParam : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	/** MIDs we're using to set the desired parameter. */
	UPROPERTY()
	TArray<class UMaterialInstanceDynamic*> MaterialInstances;

	/** Saved values for restoring state when exiting Matinee. */
	UPROPERTY()
	TArray<float> ResetFloats;

	/** Primitive components on which materials have been overridden. */
	UPROPERTY()
	TArray<struct FPrimitiveMaterialRef> PrimitiveMaterialRefs;

	/** track we are an instance of - used in the editor to propagate changes to the track's Materials array immediately */
	UPROPERTY()
	class UInterpTrackFloatMaterialParam* InstancedTrack;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void TermTrackInst(UInterpTrack* Track) override;
	virtual void SaveActorState(UInterpTrack* Track) override;
	virtual void RestoreActorState(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

