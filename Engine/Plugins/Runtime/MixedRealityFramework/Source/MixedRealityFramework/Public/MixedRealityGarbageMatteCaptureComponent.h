// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneCaptureComponent2D.h"
#include "MixedRealityGarbageMatteCaptureComponent.generated.h"

class AActor;

/**
 *	
 */
UCLASS(ClassGroup = Rendering, editinlinenew, config = Engine)
class MIXEDREALITYFRAMEWORK_API UMixedRealityGarbageMatteCaptureComponent : public USceneCaptureComponent2D
{
	GENERATED_UCLASS_BODY()

public:
	//~ UActorComponent interface
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:
	//~ USceneCaptureComponent interface
	virtual const AActor* GetViewOwner() const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

public:
	void ApplyConfiguration(const class UMixedRealityConfigurationSaveGame& SaveGameInstance);

	// During garbage matte setup one may want to use a different actor for garbage matte capture, so that it live updates as the garbage matte is setup.
	void SetExternalGarbageMatteActor(AActor* Actor);
	void ClearExternalGarbageMatteActor();

public:
	UPROPERTY(Transient)
	UStaticMesh* GarbageMatteMesh;

private:
	UPROPERTY(Transient)
	AActor* GarbageMatteActor;

	UPROPERTY(Transient)
	AActor* ExternalGarbageMatteActor;

};
