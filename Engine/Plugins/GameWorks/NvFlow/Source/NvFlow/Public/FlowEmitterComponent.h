
/**
 *	NvFlow - Emitter Component
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "Containers/List.h"

#include "FlowTimeStepper.h"

#include "FlowEmitterComponent.generated.h"

UCLASS(ClassGroup = Physics, config = Engine, editinlinenew, HideCategories = (Activation, PhysX), meta = (BlueprintSpawnableComponent), MinimalAPI)
class UFlowEmitterComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	/** Linear emission velocity*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter)
	FVector		LinearVelocity;

	/** Angular emission velocity*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter)
	FVector		AngularVelocity;

	/** Factor between 0 and 1 to blend in physical velocity of actor*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, UIMax = 1.0f))
	float 		BlendInPhysicalVelocity;

	UPROPERTY()
	float		Density_DEPRECATED;

	/**  Target smoke*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (UIMin = 0.0f, UIMax = 10.0f))
	float		Smoke;

	/** Target temperature*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (UIMin = 0.0f, UIMax = 10.0f))
	float		Temperature;

	/** Target fuel*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (UIMin = -2.0f, UIMax = 2.0f))
	float		Fuel;

	/** Minimum temperature to release FuelRelease additional fuel*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (UIMin = -2.0f, UIMax = 2.0f))
	float		FuelReleaseTemp;

	/** Fuel released when temperature exceeds release temperature*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (UIMin = -2.0f, UIMax = 2.0f))
	float		FuelRelease;

	/** Time factor used for pre-allocation of grid cells for fast emitters*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, UIMax = 1.0f))
	float		AllocationPredict;

	/** Controls emitter allocation behavior. 0.0 turns emitter allocation off. 1.0 is default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, UIMax = 2.0f))
	float		AllocationScale;

	/** 0.0 is pure emitter. 1.0 makes entire shape interior a collider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float		CollisionFactor;

	/** Allows inflation of emitter outside of shape surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float		EmitterInflate;

	/** Rate at which grid cells move to emitter target values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, UIMax = 10.0f))
	float		CoupleRate;

	/** 1.0 makes velocity change based on CoupleRate. 0.0 makes emitter have no effect on velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, UIMax = 1.0f))
	float VelocityMask;

	UPROPERTY()
	float DensityMask_DEPRECATED;

	/** 1.0 makes smoke change based on CoupleRate. 0.0 makes emitter have no effect on smoke. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, UIMax = 1.0f))
	float SmokeMask;

	/** 1.0 makes temperature change based on CoupleRate. 0.0 makes emitter have no effect on temperature. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, UIMax = 1.0f))
	float TemperatureMask;

	/** 1.0 makes fuel change based on CoupleRate. 0.0 makes emitter have no effect on fuel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 0.0f, UIMax = 1.0f))
	float FuelMask;

	/** Super sampling of emitter shape along it's path onto flow grid*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter, meta = (ClampMin = 1, UIMax = 10))
	int32		NumSubsteps;

	/** If true, emitter will disable velocity/density emission and will allocate based on the shape instead of the bounding box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter)
	uint32		bAllocShapeOnly : 1;

	FFlowTimeStepper EmitTimeStepper;
	uint32		bPreviousStateInitialized : 1;
	FTransform	PreviousTransform;
	FVector PreviousLinearVelocity;
	FVector PreviousAngularVelocity;

	/** Flow material, if null then taken default material from grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	class UFlowMaterial* FlowMaterial;

	/** If true, emitter will use DistanceField from a StaticMeshComponent in the same Actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Emitter)
	uint32		bUseDistanceField : 1;

	virtual void PostLoad() override;
};

