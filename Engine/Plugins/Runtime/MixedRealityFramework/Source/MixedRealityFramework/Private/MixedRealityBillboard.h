// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialBillboardComponent.h"
#include "GameFramework/Actor.h"
#include "MixedRealityBillboard.generated.h"

class APawn;
class UMaterialInstance;

UCLASS()
class UMixedRealityBillboard : public UMaterialBillboardComponent
{
	GENERATED_UCLASS_BODY()

public:
	//~ UActorComponent interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

public:
	//~ UPrimitiveComponent interface
#if WITH_EDITOR
	virtual uint64 GetHiddenEditorViews() const override
	{
		// we don't want this billboard crowding the editor window, so hide it from all editor
		// views (however, we do want to see it in preview windows, which this doesn't affect)
		return 0xFFFFFFFFFFFFFFFF;
	}
#endif // WITH_EDITOR
};

/* AMixedRealityBillboardActor
 *****************************************************************************/

UCLASS(notplaceable)
class AMixedRealityProjectionActor : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UMixedRealityBillboard* ProjectionComponent;

public:
	//~ AActor interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

public:
	void SetProjectionMaterial(UMaterialInterface* VidProcessingMat);
	void SetProjectionAspectRatio(const float NewAspectRatio);
	FVector GetTargetPosition() const;

protected:
	void SetDepthTarget(const APawn* PlayerPawn);
	void RefreshTickState();

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> AttachTarget;
};