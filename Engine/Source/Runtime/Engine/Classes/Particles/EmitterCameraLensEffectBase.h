// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Base class for Camera Lens Effects.  Needed so we can have AnimNotifies be able to show camera effects
 * in a nice drop down.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Particles/Emitter.h"
#include "EmitterCameraLensEffectBase.generated.h"

class APlayerCameraManager;

UCLASS(abstract, Blueprintable)
class ENGINE_API AEmitterCameraLensEffectBase : public AEmitter
{
	GENERATED_UCLASS_BODY()

protected:
	/** Particle System to use */
	UPROPERTY(EditDefaultsOnly, Category = EmitterCameraLensEffectBase)
	class UParticleSystem* PS_CameraEffect;

	/** The effect to use for non extreme content */
	UPROPERTY()
	class UParticleSystem* PS_CameraEffectNonExtremeContent_DEPRECATED;

	/** Camera this emitter is attached to, will be notified when emitter is destroyed */
	UPROPERTY(transient)
	class APlayerCameraManager* BaseCamera;

	/** 
	 * Effect-to-camera transform to allow arbitrary placement of the particle system .
	 * Note the X component of the location will be scaled with camera fov to keep the lens effect the same apparent size.
	 */
	UPROPERTY(EditDefaultsOnly, Category = EmitterCameraLensEffectBase)
	FTransform RelativeTransform;

public:
	/** This is the assumed FOV for which the effect was authored. The code will make automatic adjustments to make it look the same at different FOVs */
	UPROPERTY(EditDefaultsOnly, Category = EmitterCameraLensEffectBase)
	float BaseFOV;

	/** true if multiple instances of this emitter can exist simultaneously, false otherwise.  */
	UPROPERTY(EditAnywhere, Category = EmitterCameraLensEffectBase)
	uint8 bAllowMultipleInstances:1;

	/** If bAllowMultipleInstances is true and this effect is retriggered, the particle system will be reset if this is true */
	UPROPERTY(EditAnywhere, Category = EmitterCameraLensEffectBase)
	uint8 bResetWhenRetriggered:1;

	/** 
	 *  If an emitter class in this array is currently playing, do not play this effect.
	 *  Useful for preventing multiple similar or expensive camera effects from playing simultaneously.
	 */
	UPROPERTY(EditDefaultsOnly, Category = EmitterCameraLensEffectBase)
	TArray<TSubclassOf<class AEmitterCameraLensEffectBase> > EmittersToTreatAsSame;

public:
	//~ Begin AActor Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;
	virtual void PostLoad() override;
	//~ End AActor Interface

	/** Tell the emitter what camera it is attached to. */
	virtual void RegisterCamera(APlayerCameraManager* C);
	
	/** Called when this emitter is re-triggered, for bAllowMultipleInstances=false emitters. */
	virtual void NotifyRetriggered();

	/** This will actually activate the lens Effect.  We want this separated from PostInitializeComponents so we can cache these emitters **/
	virtual void ActivateLensEffect();
	
	/** Deactivtes the particle system. If bDestroyOnSystemFinish is true, actor will die after particles are all dead. */
	virtual void DeactivateLensEffect();
	
	/** Given updated camera information, adjust this effect to display appropriately. */
	virtual void UpdateLocation(const FVector& CamLoc, const FRotator& CamRot, float CamFOVDeg);

	static FTransform GetAttachedEmitterTransform(AEmitterCameraLensEffectBase const* Emitter, const FVector& CamLoc, const FRotator& CamRot, float CamFOVDeg);

	/** Returns true if either particle system would loop forever when played */
	bool IsLooping() const;
private:
	/** DEPRECATED(4.11) */
	UPROPERTY()
	float DistFromCamera_DEPRECATED;
};


