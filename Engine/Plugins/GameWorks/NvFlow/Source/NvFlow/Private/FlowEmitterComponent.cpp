#include "FlowEmitterComponent.h"

#include "NvFlowCommon.h"
#include "FlowGridAsset.h"

/*=============================================================================
	FlowEmitterComponent.cpp: UFlowEmitterComponent methods.
=============================================================================*/

// NvFlow begin

#include "PhysicsEngine/PhysXSupport.h"

static const float Density_DEPRECATED_Default = 0.5f;
static const float DensityMask_DEPRECATED_Default = 1.0f;

UFlowEmitterComponent::UFlowEmitterComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Density_DEPRECATED(Density_DEPRECATED_Default)
	, DensityMask_DEPRECATED(DensityMask_DEPRECATED_Default)
{
	bIsActive = true;
	bAutoActivate = true;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	NvFlowGridEmitParams FlowGridEmitParams;
	NvFlowGridEmitParamsDefaultsInline(&FlowGridEmitParams);

	LinearVelocity = *(FVector*)(&FlowGridEmitParams.velocityLinear.x) * UFlowGridAsset::GetFlowToUE4Scale();
	AngularVelocity = *(FVector*)(&FlowGridEmitParams.velocityAngular.x);
	Smoke = FlowGridEmitParams.smoke;
	Temperature = FlowGridEmitParams.temperature;
	Fuel = FlowGridEmitParams.fuel;
	FuelReleaseTemp = FlowGridEmitParams.fuelReleaseTemp;
	FuelRelease = FlowGridEmitParams.fuelRelease;
	AllocationPredict = FlowGridEmitParams.allocationPredict;
	AllocationScale = FlowGridEmitParams.allocationScale.x;
	CollisionFactor = 0.f;
	EmitterInflate = 0.f;
	CoupleRate = 0.5f;
	VelocityMask = 1.f;
	SmokeMask = 1.f;
	TemperatureMask = 1.f;
	FuelMask = 1.f;

	BlendInPhysicalVelocity = 1.0f;
	NumSubsteps = 1;

	bAllocShapeOnly = 0u;

	FlowMaterial = nullptr;

	bUseDistanceField = false;

	bPreviousStateInitialized = false;
	PreviousTransform = FTransform::Identity;
}

void UFlowEmitterComponent::PostLoad()
{
	Super::PostLoad();

	if (Density_DEPRECATED != Density_DEPRECATED_Default)
	{
		Smoke = Density_DEPRECATED;
	}
	if (DensityMask_DEPRECATED != DensityMask_DEPRECATED_Default)
	{
		SmokeMask = DensityMask_DEPRECATED;
	}
}

// NvFlow end