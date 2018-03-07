// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Components/PrimitiveComponent.h"

#include "FlexFluidSurfaceComponent.generated.h"

/**
 *	Used to render a screen space fluid surface for particles.
 */
UCLASS(ClassGroup = Rendering, collapsecategories, hidecategories = (Object, Mobility, Collision, Physics, PhysicsVolume, Actor))
class ENGINE_API UFlexFluidSurfaceComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:
	void SetEnabledReferenceCounting(bool bEnable);
	bool GetEnabledReferenceCounting();
	void RegisterEmitterInstance(struct FParticleEmitterInstance* EmitterInstance);
	void UnregisterEmitterInstance(struct FParticleEmitterInstance* EmitterInstance);

	void SendRenderEmitterDynamicData_Concurrent(class FParticleSystemSceneProxy* ParticleSystemSceneProxy,
		struct FDynamicEmitterDataBase* DynamicEmitterData);
	
	UFlexFluidSurface* FlexFluidSurface;
	TArray<struct FParticleEmitterInstance*> EmitterInstances;

	// Begin UActorComponent interface.
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

protected:
	virtual void SendRenderDynamicData_Concurrent() override;
	// End UActorComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	// Begin USceneComponent interface.

	// Begin UPrimitiveComponent interface.
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// End UPrimitiveComponent interface.

private:
	bool bReferenceCountingEnabled;
};


