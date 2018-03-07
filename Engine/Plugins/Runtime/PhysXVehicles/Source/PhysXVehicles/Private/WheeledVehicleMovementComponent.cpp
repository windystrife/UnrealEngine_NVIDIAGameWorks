// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WheeledVehicleMovementComponent.h"
#include "EngineGlobals.h"
#include "GameFramework/Pawn.h"
#include "PhysxUserData.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Net/UnrealNetwork.h"
#include "VehicleAnimInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Physics/PhysicsFiltering.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Logging/MessageLog.h"
#include "TireConfig.h"
#include "DisplayDebugHelpers.h"

#include "PhysXPublic.h"
#include "PhysXVehicleManager.h"

#include "AI/Navigation/AvoidanceManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#define LOCTEXT_NAMESPACE "UWheeledVehicleMovementComponent"


/**
 * PhysX shader for tire friction forces
 * tireFriction - friction value of the tire contact.
 * longSlip - longitudinal slip of the tire.
 * latSlip - lateral slip of the tire.
 * camber - camber angle of the tire
 * wheelOmega - rotational speed of the wheel.
 * wheelRadius - the distance from the tire surface and the center of the wheel.
 * recipWheelRadius - the reciprocal of wheelRadius.
 * restTireLoad - the load force experienced by the tire when the vehicle is at rest.
 * normalisedTireLoad - a value equal to the load force on the tire divided by the restTireLoad.
 * tireLoad - the load force currently experienced by the tire.
 * gravity - magnitude of gravitational acceleration.
 * recipGravity - the reciprocal of the magnitude of gravitational acceleration.
 * wheelTorque - the torque to be applied to the wheel around the wheel axle.
 * tireLongForceMag - the magnitude of the longitudinal tire force to be applied to the vehicle's rigid body.
 * tireLatForceMag - the magnitude of the lateral tire force to be applied to the vehicle's rigid body.
 * tireAlignMoment - the aligning moment of the tire that is to be applied to the vehicle's rigid body (not currently used). 
 */
void PTireShader(const void* shaderData, const PxF32 tireFriction,
	const PxF32 longSlip, const PxF32 latSlip,
	const PxF32 camber, const PxF32 wheelOmega, const PxF32 wheelRadius, const PxF32 recipWheelRadius,
	const PxF32 restTireLoad, const PxF32 normalisedTireLoad, const PxF32 tireLoad,
	const PxF32 gravity, const PxF32 recipGravity,
	PxF32& wheelTorque, PxF32& tireLongForceMag, PxF32& tireLatForceMag, PxF32& tireAlignMoment)
{
	UVehicleWheel* Wheel = (UVehicleWheel*)shaderData;

	FTireShaderInput Input;

	Input.TireFriction = tireFriction;
	Input.LongSlip = longSlip;
	Input.LatSlip = latSlip;
	Input.WheelOmega = wheelOmega;
	Input.WheelRadius = wheelRadius;
	Input.RecipWheelRadius = recipWheelRadius;
	Input.NormalizedTireLoad = normalisedTireLoad;
	Input.RestTireLoad = restTireLoad;
	Input.TireLoad = tireLoad;
	Input.Gravity = gravity;
	Input.RecipGravity = recipGravity;

	FTireShaderOutput Output(0.0f);

	Wheel->VehicleSim->GenerateTireForces( Wheel, Input, Output );

	wheelTorque = Output.WheelTorque;
	tireLongForceMag = Output.LongForce;
	tireLatForceMag = Output.LatForce;
	
	Wheel->DebugLongSlip = longSlip;
	Wheel->DebugLatSlip = latSlip;
	Wheel->DebugNormalizedTireLoad = normalisedTireLoad;
	Wheel->DebugTireLoad = tireLoad;
	Wheel->DebugWheelTorque = wheelTorque;
	Wheel->DebugLongForce = tireLongForceMag;
	Wheel->DebugLatForce = tireLatForceMag;
}

UWheeledVehicleMovementComponent::UWheeledVehicleMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mass = 1500.0f;
	DragCoefficient = 0.3f;
	ChassisWidth = 180.f;
	ChassisHeight = 140.f;
	InertiaTensorScale = FVector( 1.0f, 1.0f, 1.0f );
	AngErrorAccumulator = 0.0f;
	MinNormalizedTireLoad = 0.0f;
	MaxNormalizedTireLoad = 10.0f;
	
	IdleBrakeInput = 0.0f;
	StopThreshold = 10.0f; 
	WrongDirectionThreshold = 100.f;
	ThrottleInputRate.RiseRate = 6.0f;
	ThrottleInputRate.FallRate = 10.0f;
	BrakeInputRate.RiseRate = 6.0f;
	BrakeInputRate.FallRate = 10.0f;
	HandbrakeInputRate.RiseRate = 12.0f;
	HandbrakeInputRate.FallRate = 12.0f;
	SteeringInputRate.RiseRate = 2.5f;
	SteeringInputRate.FallRate = 5.0f;

	bDeprecatedSpringOffsetMode = false;	//This is just for backwards compat. Newly tuned content should never need to use this

	bUseRVOAvoidance = false;
	AvoidanceVelocity = FVector::ZeroVector;
	AvoidanceLockVelocity = FVector::ZeroVector;
	AvoidanceLockTimer = 0.0f;
	AvoidanceGroup.bGroup0 = true;
	GroupsToAvoid.Packed = 0xFFFFFFFF;
	GroupsToIgnore.Packed = 0;
	RVOAvoidanceRadius = 400.0f;
	RVOAvoidanceHeight = 200.0f;
	AvoidanceConsiderationRadius = 2000.0f;
	RVOSteeringStep = 0.5f;
	RVOThrottleStep = 0.25f;
    
    ThresholdLongitudinalSpeed = 5.f;
    LowForwardSpeedSubStepCount = 3;
    HighForwardSpeedSubStepCount = 1;
	
	bReverseAsBrake = true;	//Treats reverse button as break for a more arcade feel (also automatically goes into reverse)

	// tire load filtering
	PxVehicleTireLoadFilterData PTireLoadFilterDef;
	MinNormalizedTireLoad = PTireLoadFilterDef.mMinNormalisedLoad;
	MinNormalizedTireLoadFiltered = PTireLoadFilterDef.mMinFilteredNormalisedLoad;
	MaxNormalizedTireLoad = PTireLoadFilterDef.mMaxNormalisedLoad;
	MaxNormalizedTireLoadFiltered = PTireLoadFilterDef.mMaxFilteredNormalisedLoad;
}

void UWheeledVehicleMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	//Skip PawnMovementComponent and simply set PawnOwner to null if we don't have a PawnActor as owner
	UNavMovementComponent::SetUpdatedComponent(NewUpdatedComponent);
	PawnOwner = NewUpdatedComponent ? Cast<APawn>(NewUpdatedComponent->GetOwner()) : nullptr;

	if(USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(NewUpdatedComponent))
	{
		//TODO: this is a hack until we get proper local space kinematic support
		SKC->bLocalSpaceKinematics = true;
	}
}

bool UWheeledVehicleMovementComponent::CanCreateVehicle() const
{
	if ( UpdatedComponent == NULL )
	{
		UE_LOG( LogVehicles, Warning, TEXT("Cannot create vehicle for %s. UpdatedComponent is not set."),
			*GetPathName() );
		return false;
	}

	if (UpdatedPrimitive == NULL)
	{
		UE_LOG(LogVehicles, Warning, TEXT("Cannot create vehicle for %s. UpdatedComponent is not a PrimitiveComponent."),
			*GetPathName());
		return false;
	}

	if (UpdatedPrimitive->GetBodyInstance() == NULL)
	{
		UE_LOG( LogVehicles, Warning, TEXT("Cannot create vehicle for %s. UpdatedComponent has not initialized its rigid body actor."),
			*GetPathName() );
		return false;
	}

	for ( int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx )
	{
		if ( WheelSetups[WheelIdx].WheelClass == NULL )
		{
			UE_LOG( LogVehicles, Warning, TEXT("Cannot create vehicle for %s. Wheel %d is not set."),
				*GetPathName(), WheelIdx );
			return false;
		}
	}

	return true;
}

void UWheeledVehicleMovementComponent::CreateVehicle()
{
	ComputeConstants();

	if ( PVehicle == NULL )
	{
		if ( CanCreateVehicle() )
		{
			check(UpdatedComponent);
			if (ensure(UpdatedPrimitive != nullptr))
			{
				check(UpdatedPrimitive->GetBodyInstance()->IsDynamic());
				
				SetupVehicle();

				if ( PVehicle != NULL )
				{
					PostSetupVehicle();
				}
			}
		}
	}
}

void UWheeledVehicleMovementComponent::SetupVehicleShapes()
{
	if (!UpdatedPrimitive)
	{
		return;
	}

	static PxMaterial* WheelMaterial = GPhysXSDK->createMaterial(0.0f, 0.0f, 0.0f);

	ExecuteOnPxRigidDynamicReadWrite(UpdatedPrimitive->GetBodyInstance(), [&](PxRigidDynamic* PVehicleActor)
	{
		// Add wheel shapes to actor
		for (int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx)
		{
			FWheelSetup& WheelSetup = WheelSetups[WheelIdx];
			UVehicleWheel* Wheel = WheelSetup.WheelClass.GetDefaultObject();
			check(Wheel);

			const FVector WheelOffset = GetWheelRestingPosition(WheelSetup);
			const PxTransform PLocalPose = PxTransform(U2PVector(WheelOffset));
			PxShape* PWheelShape = NULL;
			
			// Prepare shape
			const UBodySetup* WheelBodySetup = nullptr;
			FVector MeshScaleV(1.f, 1.f, 1.f);
			if (Wheel->bDontCreateShape)
			{
				//don't create shape so grab it directly from the bodies associated with the vehicle
				if(USkinnedMeshComponent* SkinnedMesh = GetMesh())
				{
					if(const FBodyInstance* WheelBI = SkinnedMesh->GetBodyInstance(WheelSetup.BoneName))
					{
						WheelBodySetup = WheelBI->BodySetup.Get();
					}
				}
				
			}
			else if (Wheel->CollisionMesh && Wheel->CollisionMesh->BodySetup)
			{
				WheelBodySetup = Wheel->CollisionMesh->BodySetup;

				FBoxSphereBounds MeshBounds = Wheel->CollisionMesh->GetBounds();
				if (Wheel->bAutoAdjustCollisionSize)
				{
					MeshScaleV.X = Wheel->ShapeRadius / MeshBounds.BoxExtent.X;
					MeshScaleV.Y = Wheel->ShapeWidth / MeshBounds.BoxExtent.Y;
					MeshScaleV.Z = Wheel->ShapeRadius / MeshBounds.BoxExtent.Z;
				}
			}

			if(WheelBodySetup)
			{
				PxMeshScale MeshScale(U2PVector(UpdatedComponent->RelativeScale3D * MeshScaleV), PxQuat(physx::PxIdentity));

				if (WheelBodySetup->AggGeom.ConvexElems.Num() == 1)
				{
					PxConvexMesh* ConvexMesh = WheelBodySetup->AggGeom.ConvexElems[0].GetConvexMesh();
					PWheelShape = GPhysXSDK->createShape(PxConvexMeshGeometry(ConvexMesh, MeshScale), *WheelMaterial, /*bIsExclusive=*/true);
					PVehicleActor->attachShape(*PWheelShape);
					PWheelShape->release();
				}
				else if (WheelBodySetup->TriMeshes.Num())
				{
					PxTriangleMesh* TriMesh = WheelBodySetup->TriMeshes[0];

					// No eSIMULATION_SHAPE flag for wheels
					PWheelShape = GPhysXSDK->createShape(PxTriangleMeshGeometry(TriMesh, MeshScale), *WheelMaterial, /*bIsExclusive=*/true, PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eVISUALIZATION);
					PWheelShape->setLocalPose(PLocalPose);
					PVehicleActor->attachShape(*PWheelShape);
					PWheelShape->release();
				}
			}
			
			if(!PWheelShape)
			{
				//fallback onto simple spheres
				PWheelShape = GPhysXSDK->createShape(PxSphereGeometry(Wheel->ShapeRadius), *WheelMaterial, /*bIsExclusive=*/true);
				PWheelShape->setLocalPose(PLocalPose);
				PVehicleActor->attachShape(*PWheelShape);
				PWheelShape->release();
			}

			// Init filter data
			FCollisionResponseContainer CollisionResponse;
			CollisionResponse.SetAllChannels(ECR_Ignore);

			PxFilterData PWheelQueryFilterData, PDummySimData;
			CreateShapeFilterData(ECC_Vehicle, FMaskFilter(0), UpdatedComponent->GetOwner()->GetUniqueID(), CollisionResponse, UpdatedComponent->GetUniqueID(), 0, PWheelQueryFilterData, PDummySimData, false, false, false);

			if (Wheel->SweepType != EWheelSweepType::Complex)
			{
				PWheelQueryFilterData.word3 |= EPDF_SimpleCollision;
			}

			if (Wheel->SweepType != EWheelSweepType::Simple)
			{
				PWheelQueryFilterData.word3 |= EPDF_ComplexCollision;
			}

			//// Give suspension raycasts the same group ID as the chassis so that they don't hit each other
			PWheelShape->setQueryFilterData(PWheelQueryFilterData);
		}
	});
}



void UWheeledVehicleMovementComponent::UpdateMassProperties(FBodyInstance* BI)
{
	ExecuteOnPxRigidDynamicReadWrite(BI, [&](PxRigidDynamic* PVehicleActor)
	{
		// Override mass
		const float MassRatio = Mass > 0.0f ? Mass / PVehicleActor->getMass() : 1.0f;

		PxVec3 PInertiaTensor = PVehicleActor->getMassSpaceInertiaTensor();

		PInertiaTensor.x *= InertiaTensorScale.X * MassRatio;
		PInertiaTensor.y *= InertiaTensorScale.Y * MassRatio;
		PInertiaTensor.z *= InertiaTensorScale.Z * MassRatio;

		PVehicleActor->setMassSpaceInertiaTensor(PInertiaTensor);
		PVehicleActor->setMass(Mass);

		const PxVec3 PCOMOffset = U2PVector(GetLocalCOM());
		PVehicleActor->setCMassLocalPose(PxTransform(PCOMOffset, PxQuat(physx::PxIdentity)));	//ignore the mass reference frame. TODO: expose this to the user

		if (PVehicle)
		{
			PxVehicleWheelsSimData& WheelData = PVehicle->mWheelsSimData;
			SetupWheelMassProperties_AssumesLocked(WheelData.getNbWheels(), &WheelData, PVehicleActor);
		}
	});
}


void UWheeledVehicleMovementComponent::SetupVehicleMass()
{
	if (!UpdatedPrimitive)
	{
		return;
	}

	//Ensure that if mass properties ever change we set them back to our override
	UpdatedPrimitive->GetBodyInstance()->OnRecalculatedMassProperties.AddUObject(this, &UWheeledVehicleMovementComponent::UpdateMassProperties);

	UpdateMassProperties(UpdatedPrimitive->GetBodyInstance());
}

void UWheeledVehicleMovementComponent::SetupWheelMassProperties_AssumesLocked(const uint32 NumWheels, PxVehicleWheelsSimData* PWheelsSimData, PxRigidBody* PVehicleActor)
{
	if (!ensure(PWheelsSimData) ||
		!ensure(PVehicleActor) ||
		!ensure(NumWheels > 0 && NumWheels <= 32))
	{
		return;
	}

	// Prealloc data for the sprung masses
	TArray<PxVec3> WheelOffsets;
	WheelOffsets.Init(PxVec3(), NumWheels);

	TArray<float> SprungMasses;
	SprungMasses.Init(0.f, NumWheels);

	// Calculate wheel offsets first, necessary for sprung masses
	for (uint32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
	{
		WheelOffsets[WheelIdx] = U2PVector(GetWheelRestingPosition(WheelSetups[WheelIdx]));
	}

	// Now that we have all the wheel offsets, calculate the sprung masses
	const PxTransform PLocalCOM = PVehicleActor->getCMassLocalPose();
	PxVehicleComputeSprungMasses(NumWheels, WheelOffsets.GetData(), PLocalCOM.p, PVehicleActor->getMass(), /*gravityDirection=*/2, SprungMasses.GetData());

	for (uint32 WheelIdx = 0; WheelIdx < NumWheels; WheelIdx++)
	{
		UVehicleWheel* Wheel = WheelSetups[WheelIdx].WheelClass.GetDefaultObject();

		// init suspension data
		PxVehicleSuspensionData PSuspensionData;
		PSuspensionData.mSprungMass = SprungMasses[WheelIdx];
		PSuspensionData.mMaxCompression = Wheel->SuspensionMaxRaise;
		PSuspensionData.mMaxDroop = Wheel->SuspensionMaxDrop;
		PSuspensionData.mSpringStrength = FMath::Square(Wheel->SuspensionNaturalFrequency) * PSuspensionData.mSprungMass;
		PSuspensionData.mSpringDamperRate = Wheel->SuspensionDampingRatio * 2.0f * FMath::Sqrt(PSuspensionData.mSpringStrength * PSuspensionData.mSprungMass);

		const PxVec3 PWheelOffset = WheelOffsets[WheelIdx];

		PxVec3 PSuspTravelDirection = PLocalCOM.rotate(PxVec3(0.0f, 0.0f, -1.0f));
		PxVec3 PWheelCentreCMOffset = PLocalCOM.transformInv(PWheelOffset);
		PxVec3 PSuspForceAppCMOffset = !bDeprecatedSpringOffsetMode ? PxVec3(PWheelCentreCMOffset.x, PWheelCentreCMOffset.y, Wheel->SuspensionForceOffset + PWheelCentreCMOffset.z)
			: PxVec3(PWheelCentreCMOffset.x, PWheelCentreCMOffset.y, Wheel->SuspensionForceOffset);
		PxVec3 PTireForceAppCMOffset = PSuspForceAppCMOffset;

		PWheelsSimData->setSuspensionData(WheelIdx, PSuspensionData);
		PWheelsSimData->setSuspTravelDirection(WheelIdx, PSuspTravelDirection);
		PWheelsSimData->setWheelCentreOffset(WheelIdx, PWheelCentreCMOffset);
		PWheelsSimData->setSuspForceAppPointOffset(WheelIdx, PSuspForceAppCMOffset);
		PWheelsSimData->setTireForceAppPointOffset(WheelIdx, PTireForceAppCMOffset);
	}
}

void UWheeledVehicleMovementComponent::SetupWheels(PxVehicleWheelsSimData* PWheelsSimData)
{
	if (!UpdatedPrimitive)
	{
		return;
	}

	ExecuteOnPxRigidDynamicReadWrite(UpdatedPrimitive->GetBodyInstance(), [&](PxRigidDynamic* PVehicleActor)
	{
		const PxReal LengthScale = 100.f; // Convert default from m to cm

		// Control substepping
		PWheelsSimData->setSubStepCount(ThresholdLongitudinalSpeed * LengthScale, LowForwardSpeedSubStepCount, HighForwardSpeedSubStepCount);
		PWheelsSimData->setMinLongSlipDenominator(4.f * LengthScale);

		const int32 NumWheels = FMath::Min(32, WheelSetups.Num());

		for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
		{
			UVehicleWheel* Wheel = WheelSetups[WheelIdx].WheelClass.GetDefaultObject();

			// init wheel data
			PxVehicleWheelData PWheelData;
			PWheelData.mRadius = Wheel->ShapeRadius;
			PWheelData.mWidth = Wheel->ShapeWidth;
			PWheelData.mMaxSteer = FMath::DegreesToRadians(Wheel->SteerAngle);
			PWheelData.mMaxBrakeTorque = M2ToCm2(Wheel->MaxBrakeTorque);
			PWheelData.mMaxHandBrakeTorque = Wheel->bAffectedByHandbrake ? M2ToCm2(Wheel->MaxHandBrakeTorque) : 0.0f;

			PWheelData.mDampingRate = M2ToCm2(Wheel->DampingRate);
			PWheelData.mMass = Wheel->Mass;
			PWheelData.mMOI = 0.5f * PWheelData.mMass * FMath::Square(PWheelData.mRadius);

			// init tire data
			PxVehicleTireData PTireData;
			PTireData.mType = Wheel->TireConfig ? Wheel->TireConfig->GetTireConfigID() : FPhysXVehicleManager::GetDefaultTireConfig()->GetTireConfigID();
			//PTireData.mCamberStiffnessPerUnitGravity = 0.0f;
			PTireData.mLatStiffX = Wheel->LatStiffMaxLoad;
			PTireData.mLatStiffY = Wheel->LatStiffValue;
			PTireData.mLongitudinalStiffnessPerUnitGravity = Wheel->LongStiffValue;

			// finalize sim data
			PWheelsSimData->setWheelData(WheelIdx, PWheelData);
			PWheelsSimData->setTireData(WheelIdx, PTireData);
		}

		SetupWheelMassProperties_AssumesLocked(NumWheels, PWheelsSimData, PVehicleActor);

		const int32 NumShapes = PVehicleActor->getNbShapes();
		const int32 NumChassisShapes = NumShapes - NumWheels;
		if(NumChassisShapes >= 1)
		{
		TArray<PxShape*> Shapes;
		Shapes.AddZeroed(NumShapes);

		PVehicleActor->getShapes(Shapes.GetData(), NumShapes);

			for (int32 WheelIdx = 0; WheelIdx < NumWheels; ++WheelIdx)
		{
			const int32 WheelShapeIndex = NumChassisShapes + WheelIdx;

			PWheelsSimData->setWheelShapeMapping(WheelIdx, WheelShapeIndex);
			PWheelsSimData->setSceneQueryFilterData(WheelIdx, Shapes[WheelShapeIndex]->getQueryFilterData());
		}
		}
		else
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			UE_LOG(LogPhysics, Warning, TEXT("Missing wheel shapes. Please ensure there's a body associated with each wheel, or deselect Don'tCreateShape in your wheel class for vehicle''%s''"), *GetPathNameSafe(this));
#endif
		}

		

		// tire load filtering
		PxVehicleTireLoadFilterData PTireLoadFilter;
		PTireLoadFilter.mMinNormalisedLoad = MinNormalizedTireLoad;
		PTireLoadFilter.mMinFilteredNormalisedLoad = MinNormalizedTireLoadFiltered;
		PTireLoadFilter.mMaxNormalisedLoad = MaxNormalizedTireLoad;
		PTireLoadFilter.mMaxFilteredNormalisedLoad = MaxNormalizedTireLoadFiltered;
		PWheelsSimData->setTireLoadFilterData(PTireLoadFilter);
	});
}
 ////////////////////////////////////////////////////////////////////////////
//Default tire force shader function.
//Taken from Michigan tire model.
//Computes tire long and lat forces plus the aligning moment arising from 
//the lat force and the torque to apply back to the wheel arising from the 
//long force (application of Newton's 3rd law).
////////////////////////////////////////////////////////////////////////////


#define ONE_TWENTYSEVENTH 0.037037f
#define ONE_THIRD 0.33333f
PX_FORCE_INLINE PxF32 smoothingFunction1(const PxF32 K)
{
	//Equation 20 in CarSimEd manual Appendix F.
	//Looks a bit like a curve of sqrt(x) for 0<x<1 but reaching 1.0 on y-axis at K=3. 
	PX_ASSERT(K>=0.0f);
	return PxMin(1.0f, K - ONE_THIRD*K*K + ONE_TWENTYSEVENTH*K*K*K);
}
PX_FORCE_INLINE PxF32 smoothingFunction2(const PxF32 K)
{
	//Equation 21 in CarSimEd manual Appendix F.
	//Rises to a peak at K=0.75 and falls back to zero by K=3
	PX_ASSERT(K>=0.0f);
	return (K - K*K + ONE_THIRD*K*K*K - ONE_TWENTYSEVENTH*K*K*K*K);
}

void PxVehicleComputeTireForceDefault
(const void* tireShaderData, 
 const PxF32 tireFriction,
 const PxF32 longSlip, const PxF32 latSlip, const PxF32 camber,
 const PxF32 wheelOmega, const PxF32 wheelRadius, const PxF32 recipWheelRadius,
 const PxF32 restTireLoad, const PxF32 normalisedTireLoad, const PxF32 tireLoad,
 const PxF32 gravity, const PxF32 recipGravity,
 PxF32& wheelTorque, PxF32& tireLongForceMag, PxF32& tireLatForceMag, PxF32& tireAlignMoment)
{
	PX_UNUSED(wheelOmega);
	PX_UNUSED(recipWheelRadius);

	const PxVehicleTireData& tireData=*((PxVehicleTireData*)tireShaderData);

	PX_ASSERT(tireFriction>0);
	PX_ASSERT(tireLoad>0);

	wheelTorque=0.0f;
	tireLongForceMag=0.0f;
	tireLatForceMag=0.0f;
	tireAlignMoment=0.0f;

	//If long slip/lat slip/camber are all zero than there will be zero tire force.
	if (FMath::IsNearlyZero(latSlip) && FMath::IsNearlyZero(longSlip) && FMath::IsNearlyZero(camber))
	{
		return;
	}

	//Compute the lateral stiffness
	const PxF32 latStiff=restTireLoad*tireData.mLatStiffY*smoothingFunction1(normalisedTireLoad*3.0f/tireData.mLatStiffX);

	//Get the longitudinal stiffness
	const PxF32 longStiff=tireData.mLongitudinalStiffnessPerUnitGravity*gravity;
	const PxF32 recipLongStiff=tireData.getRecipLongitudinalStiffnessPerUnitGravity()*recipGravity;

	//Get the camber stiffness.
	const PxF32 camberStiff=tireData.mCamberStiffnessPerUnitGravity*gravity;

	//Carry on and compute the forces.
	const PxF32 TEff = PxTan(latSlip - camber*camberStiff/latStiff);
	const PxF32 K = PxSqrt(latStiff*TEff*latStiff*TEff + longStiff*longSlip*longStiff*longSlip) /(tireFriction*tireLoad);
	//const PxF32 KAbs=PxAbs(K);
	PxF32 FBar = smoothingFunction1(K);//K - ONE_THIRD*PxAbs(K)*K + ONE_TWENTYSEVENTH*K*K*K;
	PxF32 MBar = smoothingFunction2(K); //K - KAbs*K + ONE_THIRD*K*K*K - ONE_TWENTYSEVENTH*KAbs*K*K*K;
	//Mbar = PxMin(Mbar, 1.0f);
	PxF32 nu=1;
	if(K <= 2.0f*PxPi)
	{
		const PxF32 latOverlLong=latStiff*recipLongStiff;
		nu = 0.5f*(1.0f + latOverlLong - (1.0f - latOverlLong)*PxCos(K*0.5f));
	}
	const PxF32 FZero = tireFriction*tireLoad / (PxSqrt(longSlip*longSlip + nu*TEff*nu*TEff));
	const PxF32 fz = longSlip*FBar*FZero;
	const PxF32 fx = -nu*TEff*FBar*FZero;
	//TODO: pneumatic trail.
	const PxF32 pneumaticTrail=1.0f;
	const PxF32	fMy= nu * pneumaticTrail * TEff * MBar * FZero;

	//We can add the torque to the wheel.
	wheelTorque=-fz*wheelRadius;
	tireLongForceMag=fz;
	tireLatForceMag=fx;
	tireAlignMoment=fMy;
}

void UWheeledVehicleMovementComponent::GenerateTireForces( UVehicleWheel* Wheel, const FTireShaderInput& Input, FTireShaderOutput& Output )
{
	const void* realShaderData = &PVehicle->mWheelsSimData.getTireData(Wheel->WheelIndex);

	float Dummy;

	PxVehicleComputeTireForceDefault(
		realShaderData, Input.TireFriction,
		Input.LongSlip, Input.LatSlip,
		0.0f, Input.WheelOmega, Input.WheelRadius, Input.RecipWheelRadius,
		Input.RestTireLoad, Input.NormalizedTireLoad, Input.TireLoad,
		Input.Gravity, Input.RecipGravity,
		Output.WheelTorque, Output.LongForce, Output.LatForce, Dummy
		);
	
	ensureMsgf(Output.WheelTorque == Output.WheelTorque, TEXT("Output.WheelTorque is bad: %f"), Output.WheelTorque);
	ensureMsgf(Output.LongForce == Output.LongForce, TEXT("Output.LongForce is bad: %f"), Output.LongForce);
	ensureMsgf(Output.LatForce == Output.LatForce, TEXT("Output.LatForce is bad: %f"), Output.LatForce);

	//UE_LOG( LogVehicles, Warning, TEXT("Friction = %f	LongSlip = %f	LatSlip = %f"), Input.TireFriction, Input.LongSlip, Input.LatSlip );	
	//UE_LOG( LogVehicles, Warning, TEXT("WheelTorque= %f	LongForce = %f	LatForce = %f"), Output.WheelTorque, Output.LongForce, Output.LatForce );
	//UE_LOG( LogVehicles, Warning, TEXT("RestLoad= %f	NormLoad = %f	TireLoad = %f"),Input.RestTireLoad, Input.NormalizedTireLoad, Input.TireLoad );
}

void UWheeledVehicleMovementComponent::PostSetupVehicle()
{
	if (bUseRVOAvoidance)
	{
		UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
		if (AvoidanceManager)
		{
			AvoidanceManager->RegisterMovementComponent(this, AvoidanceWeight);
		}
	}
}

FVector UWheeledVehicleMovementComponent::GetWheelRestingPosition( const FWheelSetup& WheelSetup )
{
	FVector Offset = WheelSetup.WheelClass.GetDefaultObject()->Offset + WheelSetup.AdditionalOffset;

	if ( WheelSetup.BoneName != NAME_None )
	{
		USkinnedMeshComponent* Mesh = GetMesh();
		if ( Mesh && Mesh->SkeletalMesh )
		{
			const FVector BonePosition = Mesh->SkeletalMesh->GetComposedRefPoseMatrix( WheelSetup.BoneName ).GetOrigin() * Mesh->RelativeScale3D;
			//BonePosition is local for the root BONE of the skeletal mesh - however, we are using the Root BODY which may have its own transform, so we need to return the position local to the root BODY
			const FMatrix RootBodyMTX = Mesh->SkeletalMesh->GetComposedRefPoseMatrix(Mesh->GetBodyInstance()->BodySetup->BoneName);
			const FVector LocalBonePosition = RootBodyMTX.InverseTransformPosition(BonePosition);
			Offset += LocalBonePosition;

		}
	}

	return Offset;
}

FVector UWheeledVehicleMovementComponent::GetLocalCOM() const
{
	FVector LocalCOM = FVector::ZeroVector;
	if (UpdatedPrimitive)
	{
		if (const FBodyInstance* BodyInst = UpdatedPrimitive->GetBodyInstance())
		{
			ExecuteOnPxRigidDynamicReadOnly(BodyInst, [&](const PxRigidDynamic* PVehicleActor)
			{
				PxTransform PCOMTransform = PVehicleActor->getCMassLocalPose();
				LocalCOM = P2UVector(PCOMTransform.p);
			});
		}
	}

	return LocalCOM;
}

USkinnedMeshComponent* UWheeledVehicleMovementComponent::GetMesh()
{
	return Cast<USkinnedMeshComponent>(UpdatedComponent);
}

void LogVehicleSettings( PxVehicleWheels* Vehicle )
{
	const float VehicleMass = Vehicle->getRigidDynamicActor()->getMass();
	const FVector VehicleMOI = P2UVector( Vehicle->getRigidDynamicActor()->getMassSpaceInertiaTensor() );

	UE_LOG( LogPhysics, Warning, TEXT("Vehicle Mass: %f"), VehicleMass );
	UE_LOG( LogPhysics, Warning, TEXT("Vehicle MOI: %s"), *VehicleMOI.ToString() );

	const PxVehicleWheelsSimData& SimData = Vehicle->mWheelsSimData;
	for ( int32 WheelIdx = 0; WheelIdx < 4; ++WheelIdx )
	{
		const  PxVec3& suspTravelDir = SimData.getSuspTravelDirection(WheelIdx);
		const PxVec3& suspAppPointOffset = SimData.getSuspForceAppPointOffset(WheelIdx);
		const PxVec3& tireForceAppPointOffset = SimData.getTireForceAppPointOffset(WheelIdx);
		const PxVec3& wheelCenterOffset = SimData.getWheelCentreOffset(WheelIdx);			
		const PxVehicleSuspensionData& SuspensionData = SimData.getSuspensionData( WheelIdx );
		const PxVehicleWheelData& WheelData = SimData.getWheelData( WheelIdx );
		const PxVehicleTireData& TireData = SimData.getTireData( WheelIdx );

		UE_LOG( LogPhysics, Warning, TEXT("Wheel %d suspension: travelDir ={%f, %f, %f} "), WheelIdx, suspTravelDir.x, suspTravelDir.y, suspTravelDir.z );
		UE_LOG( LogPhysics, Warning, TEXT("Wheel %d suspension: suspAppPointOffset ={%f, %f, %f} "), WheelIdx, suspAppPointOffset.x, suspAppPointOffset.y, suspAppPointOffset.z );
		UE_LOG( LogPhysics, Warning, TEXT("Wheel %d suspension: tireForceAppPointOffset ={%f, %f, %f} "), WheelIdx, tireForceAppPointOffset.x, tireForceAppPointOffset.y, tireForceAppPointOffset.z );
		UE_LOG( LogPhysics, Warning, TEXT("Wheel %d suspension: wheelCenterOffset ={%f, %f, %f} "), WheelIdx, wheelCenterOffset.x, wheelCenterOffset.y, wheelCenterOffset.z );
		UE_LOG( LogPhysics, Warning, TEXT("Wheel %d suspension: MaxCompress=%f, MaxDroop=%f, Damper=%f, Strength=%f, SprungMass=%f"),
			WheelIdx, SuspensionData.mMaxCompression, SuspensionData.mMaxDroop, SuspensionData.mSpringDamperRate, SuspensionData.mSpringStrength, SuspensionData.mSprungMass );

		UE_LOG( LogPhysics, Warning, TEXT("Wheel %d wheel: Damping=%f, Mass=%f, MOI=%f, Radius=%f"),
			WheelIdx, WheelData.mDampingRate, WheelData.mMass, WheelData.mMOI, WheelData.mRadius );

		UE_LOG( LogPhysics, Warning, TEXT("Wheel %d tire: LatStiffX=%f, LatStiffY=%f, LongStiff=%f"),
			WheelIdx, TireData.mLatStiffX, TireData.mLatStiffY, TireData.mLongitudinalStiffnessPerUnitGravity );
	}
}

void UWheeledVehicleMovementComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	VehicleSetupTag = FPhysXVehicleManager::VehicleSetupTag;

	// only create physx vehicle in game
	UWorld* World = GetWorld();
	if ( World->IsGameWorld() )
	{
		FPhysScene* PhysScene = World->GetPhysicsScene();

		if ( PhysScene && FPhysXVehicleManager::GetVehicleManagerFromScene(PhysScene) )
		{
			FixupSkeletalMesh();
			CreateVehicle();

			if ( PVehicle )
			{
				FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(PhysScene);
				VehicleManager->AddVehicle( this );

				CreateWheels();

				//LogVehicleSettings( PVehicle );
				SCOPED_SCENE_WRITE_LOCK(VehicleManager->GetScene());
				PVehicle->getRigidDynamicActor()->wakeUp();

				// Need to bind to the notify delegate on the mesh incase physics state is changed
				if(USkeletalMeshComponent* MeshComp = Cast<USkeletalMeshComponent>(GetMesh()))
				{
					MeshOnPhysicsStateChangeHandle = MeshComp->RegisterOnPhysicsCreatedDelegate(FOnSkelMeshPhysicsCreated::CreateUObject(this, &UWheeledVehicleMovementComponent::RecreatePhysicsState));
					if(UVehicleAnimInstance* VehicleAnimInstance = Cast<UVehicleAnimInstance>(MeshComp->GetAnimInstance()))
					{
						VehicleAnimInstance->SetWheeledVehicleMovementComponent(this);
					}
				}
			}
		}
	}
}

void UWheeledVehicleMovementComponent::OnDestroyPhysicsState()
{
	Super::OnDestroyPhysicsState();

	if ( PVehicle )
	{
		DestroyWheels();

		FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(GetWorld()->GetPhysicsScene());
		VehicleManager->RemoveVehicle( this );
		PVehicle = NULL;

		if(MeshOnPhysicsStateChangeHandle.IsValid())
		{
			if(USkeletalMeshComponent* MeshComp = Cast<USkeletalMeshComponent>(GetMesh()))
			{
				MeshComp->UnregisterOnPhysicsCreatedDelegate(MeshOnPhysicsStateChangeHandle);
			}
		}

		if ( UpdatedComponent )
		{
			UpdatedComponent->RecreatePhysicsState();
		}
	}
}

bool UWheeledVehicleMovementComponent::ShouldCreatePhysicsState() const
{
	if (!IsRegistered() || IsBeingDestroyed())
	{
		return false;
	}

	// only create physx vehicle in game
	UWorld* World = GetWorld();
	if (World->IsGameWorld())
	{
		FPhysScene* PhysScene = World->GetPhysicsScene();

		if (PhysScene && FPhysXVehicleManager::GetVehicleManagerFromScene(PhysScene))
		{
			if (CanCreateVehicle())
			{
				return true;
			}
		}
	}

	return false;
}

bool UWheeledVehicleMovementComponent::HasValidPhysicsState() const
{
	return PVehicle != NULL;
}

void UWheeledVehicleMovementComponent::CreateWheels()
{
	// Wheels num is getting copied when blueprint recompiles, so we have to manually reset here
	Wheels.Reset();

	PVehicle->mWheelsDynData.setTireForceShaderFunction( PTireShader );

	// Instantiate the wheels
	for ( int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx )
	{
		UVehicleWheel* Wheel = NewObject<UVehicleWheel>( this, WheelSetups[WheelIdx].WheelClass );
		check(Wheel);

		Wheels.Add( Wheel );
	}

	// Initialize the wheels
	for ( int32 WheelIdx = 0; WheelIdx < Wheels.Num(); ++WheelIdx )
	{
		PVehicle->mWheelsDynData.setTireForceShaderData( WheelIdx, Wheels[WheelIdx] );

		Wheels[WheelIdx]->Init( this, WheelIdx );
	}
}

void UWheeledVehicleMovementComponent::DestroyWheels()
{
	for ( int32 i = 0; i < Wheels.Num(); ++i )
	{
		Wheels[i]->Shutdown();
	}

	Wheels.Reset();
}

void UWheeledVehicleMovementComponent::TickVehicle( float DeltaTime )
{
	if (AvoidanceLockTimer > 0.0f)
	{
		AvoidanceLockTimer -= DeltaTime;
	}

	// movement updates and replication
	if (PVehicle && UpdatedComponent)
	{
		APawn* MyOwner = Cast<APawn>(UpdatedComponent->GetOwner());
		if (MyOwner)
		{
			UpdateSimulation(DeltaTime);
		}
	}

	// update wheels
	for (int32 i = 0; i < Wheels.Num(); i++)
	{
		Wheels[i]->Tick(DeltaTime);
	}

	UpdateDrag(DeltaTime);
}

void UWheeledVehicleMovementComponent::UpdateDrag(float DeltaTime)
{
	if (PVehicle && UpdatedPrimitive)
	{
		float ForwardSpeed = GetForwardSpeed();
		if (FMath::Abs(ForwardSpeed) > 1.f)
		{
			FVector GlobalForwardVector = UpdatedComponent->GetForwardVector();
			FVector DragVector = -GlobalForwardVector;
			float SpeedSquared = ForwardSpeed > 0.f ? ForwardSpeed * ForwardSpeed : -ForwardSpeed * ForwardSpeed;
			float ChassisDragArea = ChassisHeight * ChassisWidth;
			float AirDensity = 1.25 / (100 * 100 * 100); //kg/cm^3
			float DragMag = 0.5f * AirDensity * SpeedSquared * DragCoefficient * ChassisDragArea;
			DebugDragMagnitude = DragMag;
			DragVector *= DragMag;
			FBodyInstance* BodyInstance = UpdatedPrimitive->GetBodyInstance();
			BodyInstance->AddForce(DragVector, false);
		}
		
	}
}

void UWheeledVehicleMovementComponent::PreTick(float DeltaTime)
{
	// movement updates and replication
	if (PVehicle && UpdatedComponent)
	{
		APawn* MyOwner = Cast<APawn>(UpdatedComponent->GetOwner());
		if (MyOwner)
		{
			UpdateState(DeltaTime);
		}
	}

	if (VehicleSetupTag != FPhysXVehicleManager::VehicleSetupTag)
	{
		RecreatePhysicsState();
	}
}

void UWheeledVehicleMovementComponent::SetupVehicle()
{
	if (!UpdatedPrimitive)
	{
		return;
	}

	if (WheelSetups.Num() == 0)
	{
		PVehicle = nullptr;
		PVehicleDrive = nullptr;
		return;
	}

	for (int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx)
	{
		const FWheelSetup& WheelSetup = WheelSetups[WheelIdx];
		if (WheelSetup.BoneName == NAME_None)
		{
			return;
		}
	}

	// Setup the chassis and wheel shapes
	SetupVehicleShapes();

	// Setup mass properties
	SetupVehicleMass();

	// Setup the wheels
	PxVehicleWheelsSimData* PWheelsSimData = PxVehicleWheelsSimData::allocate(WheelSetups.Num());
	SetupWheels(PWheelsSimData);

	SetupVehicleDrive(PWheelsSimData);
}

void UWheeledVehicleMovementComponent::SetupVehicleDrive(PxVehicleWheelsSimData* PWheelsSimData)
{	
}

void UWheeledVehicleMovementComponent::UpdateSimulation( float DeltaTime )
{
}

void UWheeledVehicleMovementComponent::UpdateAvoidance(float DeltaTime)
{
	UpdateDefaultAvoidance();
}

void UWheeledVehicleMovementComponent::UpdateDefaultAvoidance()
{
	if (!bUseRVOAvoidance)
	{
		return;
	}

	UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
	if (AvoidanceManager && !bWasAvoidanceUpdated)
	{
		AvoidanceManager->UpdateRVO(this);
		
		//Consider this a clean move because we didn't even try to avoid.
		SetAvoidanceVelocityLock(AvoidanceManager, AvoidanceManager->LockTimeAfterClean);
	}

	bWasAvoidanceUpdated = false;		//Reset for next frame
}

void UWheeledVehicleMovementComponent::SetAvoidanceVelocityLock(class UAvoidanceManager* Avoidance, float Duration)
{
	Avoidance->OverrideToMaxWeight(AvoidanceUID, Duration);
	AvoidanceLockVelocity = AvoidanceVelocity;
	AvoidanceLockTimer = Duration;
}

void UWheeledVehicleMovementComponent::UpdateState( float DeltaTime )
{
	// update input values
	APawn* MyOwner = UpdatedComponent ? Cast<APawn>(UpdatedComponent->GetOwner()) : NULL;
	
	// TODO: IsLocallyControlled will fail if the owner is unpossessed (i.e. MyOwner->GetController() == nullptr);
	// Should we remove input instead of relying on replicated state in that case?
	if (MyOwner && MyOwner->IsLocallyControlled())
	{
		if(bReverseAsBrake)
		{
			//for reverse as state we want to automatically shift between reverse and first gear
			if (FMath::Abs(GetForwardSpeed()) < WrongDirectionThreshold)	//we only shift between reverse and first if the car is slow enough. This isn't 100% correct since we really only care about engine speed, but good enough
			{
				if (RawThrottleInput < 0.f && GetCurrentGear() >= 0 && GetTargetGear() >= 0)
				{
					SetTargetGear(-1, true);
				}
				else if (RawThrottleInput > 0.f && GetCurrentGear() <= 0 && GetTargetGear() <= 0)
				{
					SetTargetGear(1, true);
				}
			}
		}
		
		
		if (bUseRVOAvoidance)
		{
			CalculateAvoidanceVelocity(DeltaTime);
			UpdateAvoidance(DeltaTime);
		}

		SteeringInput = SteeringInputRate.InterpInputValue(DeltaTime, SteeringInput, CalcSteeringInput());
		ThrottleInput = ThrottleInputRate.InterpInputValue( DeltaTime, ThrottleInput, CalcThrottleInput() );
		BrakeInput = BrakeInputRate.InterpInputValue(DeltaTime, BrakeInput, CalcBrakeInput());
		HandbrakeInput = HandbrakeInputRate.InterpInputValue(DeltaTime, HandbrakeInput, CalcHandbrakeInput());

		// and send to server
		ServerUpdateState(SteeringInput, ThrottleInput, BrakeInput, HandbrakeInput, GetCurrentGear());
	}
	else
	{
		// use replicated values for remote pawns
		SteeringInput = ReplicatedState.SteeringInput;
		ThrottleInput = ReplicatedState.ThrottleInput;
		BrakeInput = ReplicatedState.BrakeInput;
		HandbrakeInput = ReplicatedState.HandbrakeInput;
		SetTargetGear(ReplicatedState.CurrentGear, true);
	}
}

/// @cond DOXYGEN_WARNINGS

bool UWheeledVehicleMovementComponent::ServerUpdateState_Validate(float InSteeringInput, float InThrottleInput, float InBrakeInput, float InHandbrakeInput, int32 InCurrentGear)
{
	return true;
}

void UWheeledVehicleMovementComponent::ServerUpdateState_Implementation(float InSteeringInput, float InThrottleInput, float InBrakeInput, float InHandbrakeInput, int32 InCurrentGear)
{
	SteeringInput = InSteeringInput;
	ThrottleInput = InThrottleInput;
	BrakeInput = InBrakeInput;
	HandbrakeInput = InHandbrakeInput;

	if (!GetUseAutoGears())
	{
		SetTargetGear(InCurrentGear, true);
	}

	// update state of inputs
	ReplicatedState.SteeringInput = InSteeringInput;
	ReplicatedState.ThrottleInput = InThrottleInput;
	ReplicatedState.BrakeInput = InBrakeInput;
	ReplicatedState.HandbrakeInput = InHandbrakeInput;
	ReplicatedState.CurrentGear = InCurrentGear;
}

/// @endcond

float UWheeledVehicleMovementComponent::CalcSteeringInput()
{
	if (bUseRVOAvoidance)
	{
		const float AngleDiff = AvoidanceVelocity.HeadingAngle() - GetVelocityForRVOConsideration().HeadingAngle();
		if (AngleDiff > 0.0f)
		{
			RawSteeringInput = FMath::Clamp(RawSteeringInput + RVOSteeringStep, 0.0f, 1.0f);
		}
		else if (AngleDiff < 0.0f)
		{
			RawSteeringInput = FMath::Clamp(RawSteeringInput - RVOSteeringStep, -1.0f, 0.0f);
		}
	}

	return RawSteeringInput;
}

float UWheeledVehicleMovementComponent::CalcBrakeInput()
{	
	if(bReverseAsBrake)
	{
	const float ForwardSpeed = GetForwardSpeed();

	float NewBrakeInput = 0.0f;

	// if player wants to move forwards...
		if (RawThrottleInput > 0.f)
	{
		// if vehicle is moving backwards, then press brake
			if (ForwardSpeed < -WrongDirectionThreshold)
		{
				NewBrakeInput = 1.0f;
		}

	}

	// if player wants to move backwards...
		else if (RawThrottleInput < 0.f)
	{
		// if vehicle is moving forwards, then press brake
		if (ForwardSpeed > WrongDirectionThreshold)
		{
			NewBrakeInput = 1.0f;			// Seems a bit severe to have 0 or 1 braking. Better control can be had by allowing continuous brake input values
			}
	}

	// if player isn't pressing forward or backwards...
	else
	{
		if (ForwardSpeed < StopThreshold && ForwardSpeed > -StopThreshold)	//auto break 
		{
			NewBrakeInput = 1.f;
		}
		else
		{
			NewBrakeInput = IdleBrakeInput;
		}
	}

	return FMath::Clamp<float>(NewBrakeInput, 0.0, 1.0);
	}
	else
	{
		return FMath::Abs(RawBrakeInput);
	}
	
}

float UWheeledVehicleMovementComponent::CalcHandbrakeInput()
{
	return (bRawHandbrakeInput == true) ? 1.0f : 0.0f;
}

float UWheeledVehicleMovementComponent::CalcThrottleInput()
{
	if (bUseRVOAvoidance)
	{
		const float AvoidanceSpeedSq = AvoidanceVelocity.SizeSquared();
		const float DesiredSpeedSq = GetVelocityForRVOConsideration().SizeSquared();

		if (AvoidanceSpeedSq > DesiredSpeedSq)
		{
			RawThrottleInput = FMath::Clamp(RawThrottleInput + RVOThrottleStep, -1.0f, 1.0f);
		}
		else if (AvoidanceSpeedSq < DesiredSpeedSq)
		{
			RawThrottleInput = FMath::Clamp(RawThrottleInput - RVOThrottleStep, -1.0f, 1.0f);
		}		
	}

	if(bReverseAsBrake)
	{
	//If the user is changing direction we should really be braking first and not applying any gas, so wait until they've changed gears
		if ((RawThrottleInput > 0.f && GetTargetGear() < 0) || (RawThrottleInput < 0.f && GetTargetGear() > 0))
		{
			return 0.f;
		}
	}

	return FMath::Abs(RawThrottleInput);
}

void UWheeledVehicleMovementComponent::StopMovementImmediately()
{
	Super::StopMovementImmediately();
	ClearAllInput();
}

void UWheeledVehicleMovementComponent::ClearInput()
{
	SteeringInput = 0.0f;
	ThrottleInput = 0.0f;
	BrakeInput = 0.0f;
	HandbrakeInput = 0.0f;

	// Send this immediately.
	ServerUpdateState(SteeringInput, ThrottleInput, BrakeInput, HandbrakeInput, GetCurrentGear());
}

void UWheeledVehicleMovementComponent::ClearRawInput()
{
	RawBrakeInput = 0.0f;
	RawSteeringInput = 0.0f;
	RawThrottleInput = 0.0f;
	bRawGearDownInput = false;
	bRawGearUpInput = false;
	bRawHandbrakeInput = false;
}

void UWheeledVehicleMovementComponent::SetThrottleInput( float Throttle )
{	
	RawThrottleInput = FMath::Clamp( Throttle, -1.0f, 1.0f );
}

void UWheeledVehicleMovementComponent::SetBrakeInput(float Brake)
{
	RawBrakeInput = FMath::Clamp(Brake, -1.0f, 1.0f);
}


void UWheeledVehicleMovementComponent::SetSteeringInput( float Steering )
{
	RawSteeringInput = FMath::Clamp( Steering, -1.0f, 1.0f );
}

void UWheeledVehicleMovementComponent::SetHandbrakeInput( bool bNewHandbrake )
{
	bRawHandbrakeInput = bNewHandbrake;
}

void UWheeledVehicleMovementComponent::SetGearUp(bool bNewGearUp)
{
	bRawGearUpInput = bNewGearUp;
}

void UWheeledVehicleMovementComponent::SetGearDown(bool bNewGearDown)
{
	bRawGearDownInput = bNewGearDown;
}

void UWheeledVehicleMovementComponent::SetTargetGear(int32 GearNum, bool bImmediate)
{
	//UE_LOG( LogVehicles, Warning, TEXT(" UWheeledVehicleMovementComponent::SetTargetGear::GearNum = %d, bImmediate = %d"), GearNum, bImmediate);
	const uint32 TargetGearNum = GearToPhysXGear(GearNum);
	if (PVehicleDrive && PVehicleDrive->mDriveDynData.getTargetGear() != TargetGearNum)
	{
		if (bImmediate)
		{
			PVehicleDrive->mDriveDynData.forceGearChange(TargetGearNum);			
		}
		else
		{
			PVehicleDrive->mDriveDynData.startGearChange(TargetGearNum);
		}
	}
}

void UWheeledVehicleMovementComponent::SetUseAutoGears(bool bUseAuto)
{
	if (PVehicleDrive)
	{
		PVehicleDrive->mDriveDynData.setUseAutoGears(bUseAuto);
	}
}

float UWheeledVehicleMovementComponent::GetForwardSpeed() const
{
	float ForwardSpeed = 0.f;
	if ( PVehicle )
	{
		UpdatedPrimitive->GetBodyInstance()->ExecuteOnPhysicsReadOnly([&]
		{
			ForwardSpeed = PVehicle->computeForwardSpeed();
		});
	}

	return ForwardSpeed;
}

float UWheeledVehicleMovementComponent::GetEngineRotationSpeed() const
{
	if (PVehicleDrive)
	{		
		return 9.5493 *  PVehicleDrive->mDriveDynData.getEngineRotationSpeed(); // 9.5493 = 60sec/min * (Motor Omega)/(2 * Pi); Motor Omega is in radians/sec, not RPM.
	}
	else if (PVehicle && WheelSetups.Num())
	{
		float TotalWheelSpeed = 0.0f;

		for (int32 i = 0; i < WheelSetups.Num(); i++)
		{
			const PxReal WheelSpeed = PVehicle->mWheelsDynData.getWheelRotationSpeed(i);
			TotalWheelSpeed += WheelSpeed;
		}

		const float CurrentRPM = TotalWheelSpeed / WheelSetups.Num();
		return CurrentRPM;
	}

	return 0.0f;
}

float UWheeledVehicleMovementComponent::GetEngineMaxRotationSpeed() const
{
	return MaxEngineRPM;
}


int32 UWheeledVehicleMovementComponent::GearToPhysXGear(const int32 Gear) const
{
	if (Gear < 0)
	{
		return PxVehicleGearsData::eREVERSE;
	}
	else if (Gear == 0)
	{
		return PxVehicleGearsData::eNEUTRAL;
	}

	return FMath::Min(PxVehicleGearsData::eNEUTRAL + Gear, PxVehicleGearsData::eGEARSRATIO_COUNT - 1);
}

int32 UWheeledVehicleMovementComponent::PhysXGearToGear(const int32 PhysXGear) const
{
	if (PhysXGear == PxVehicleGearsData::eREVERSE)
	{
		return -1;
	}
	else if (PhysXGear == PxVehicleGearsData::eNEUTRAL)
	{
		return 0;
	}

	return (PhysXGear - PxVehicleGearsData::eNEUTRAL);

}


int32 UWheeledVehicleMovementComponent::GetCurrentGear() const
{
	if (PVehicleDrive)
	{
		const int32 PhysXGearNum = PVehicleDrive->mDriveDynData.getCurrentGear();
		return PhysXGearToGear(PhysXGearNum);
	}

	return 0;
}

int32 UWheeledVehicleMovementComponent::GetTargetGear() const
{
	if (PVehicleDrive)
	{
		const int32 PhysXGearNum = PVehicleDrive->mDriveDynData.getTargetGear();
		return PhysXGearToGear(PhysXGearNum);
	}

	return 0;
}

bool UWheeledVehicleMovementComponent::GetUseAutoGears() const
{
	if (PVehicleDrive)
	{
		return PVehicleDrive->mDriveDynData.getUseAutoGears();
	}

	return false;
}

void UWheeledVehicleMovementComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::WheelOffsetIsFromWheel)
	{
		bDeprecatedSpringOffsetMode = true;	//Existing content is tuned with the old way of applying spring force offset. There's no easy way to re-compute this at the wheel level since it's a shared asset
	}
}


void DrawTelemetryGraph( uint32 Channel, const PxVehicleGraph& PGraph, UCanvas* Canvas, float GraphX, float GraphY, float GraphWidth, float GraphHeight, float& OutX )
{
	PxF32 PGraphXY[2*PxVehicleGraph::eMAX_NB_SAMPLES];
	PxVec3 PGraphColor[PxVehicleGraph::eMAX_NB_SAMPLES];
	char PGraphTitle[PxVehicleGraph::eMAX_NB_TITLE_CHARS];

	PGraph.computeGraphChannel( Channel, PGraphXY, PGraphColor, PGraphTitle );

	FString Label = ANSI_TO_TCHAR(PGraphTitle);
	Canvas->SetDrawColor( FColor( 255, 255, 0 ) );
	UFont* Font = GEngine->GetSmallFont();
	Canvas->DrawText( Font, Label, GraphX, GraphY );

	float XL, YL;
	Canvas->TextSize( Font, Label, XL, YL );

	float LineGraphHeight = GraphHeight - YL - 4.0f;
	float LineGraphY = GraphY + YL + 4.0f;

	FCanvasTileItem TileItem( FVector2D(GraphX, LineGraphY), GWhiteTexture, FVector2D( GraphWidth, GraphWidth ), FLinearColor( 0.0f, 0.125f, 0.0f, 0.25f ) );
	TileItem.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem( TileItem );
	
	Canvas->SetDrawColor( FColor( 0, 32, 0, 128 ) );	
	for ( uint32 i = 2; i < 2 * PxVehicleGraph::eMAX_NB_SAMPLES; i += 2 )
	{
		float x1 = PGraphXY[i-2];
		float y1 = PGraphXY[i-1];
		float x2 = PGraphXY[i];
		float y2 = PGraphXY[i+1];

		x1 = FMath::Clamp( x1 + 0.50f, 0.0f, 1.0f );
		x2 = FMath::Clamp( x2 + 0.50f, 0.0f, 1.0f );
		y1 = 1.0f - FMath::Clamp( y1 + 0.50f, 0.0f, 1.0f );
		y2 = 1.0f - FMath::Clamp( y2 + 0.50f, 0.0f, 1.0f );

		FCanvasLineItem LineItem( FVector2D( GraphX + x1 * GraphWidth, LineGraphY + y1 * LineGraphHeight ), FVector2D( GraphX + x2 * GraphWidth, LineGraphY + y2 * LineGraphHeight ) );
		LineItem.SetColor( FLinearColor( 1.0f, 0.5f, 0.0f, 1.0f ) );
		LineItem.Draw( Canvas->Canvas );
	}

	OutX = FMath::Max(XL,GraphWidth);
}

bool UWheeledVehicleMovementComponent::CheckSlipThreshold(float AbsLongSlipThreshold, float AbsLatSlipThreshold) const
{
	if (PVehicle == NULL)
	{
		return false;
	}

	FPhysXVehicleManager* MyVehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(GetWorld()->GetPhysicsScene());
	SCOPED_SCENE_READ_LOCK(MyVehicleManager->GetScene());

	PxWheelQueryResult * WheelsStates = MyVehicleManager->GetWheelsStates_AssumesLocked(this);
	check(WheelsStates);

	PxReal MaxLongSlip = 0.f;
	PxReal MaxLatSlip = 0.f;

	// draw wheel data
	for (uint32 w = 0; w < PVehicle->mWheelsSimData.getNbWheels(); ++w)
	{
		const PxReal AbsLongSlip = FMath::Abs(WheelsStates[w].longitudinalSlip);
		const PxReal AbsLatSlip = FMath::Abs(WheelsStates[w].lateralSlip);

		if (AbsLongSlip > AbsLongSlipThreshold)
		{
			return true;
		}

		if (AbsLatSlip > AbsLatSlipThreshold)
		{
			return true;
		}
	}

	return false;
}

float UWheeledVehicleMovementComponent::GetMaxSpringForce() const
{
	if (PVehicle == NULL)
	{
		return false;
	}

	FPhysXVehicleManager* MyVehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(GetWorld()->GetPhysicsScene());
	SCOPED_SCENE_READ_LOCK(MyVehicleManager->GetScene());

	PxWheelQueryResult * WheelsStates = MyVehicleManager->GetWheelsStates_AssumesLocked(this);
	check(WheelsStates);

	PxReal MaxSpringCompression = 0.f;

	// draw wheel data
	for (uint32 w = 0; w < PVehicle->mWheelsSimData.getNbWheels(); ++w)
	{
		MaxSpringCompression = WheelsStates[w].suspSpringForce > MaxSpringCompression ? WheelsStates[w].suspSpringForce : MaxSpringCompression;
	}

	return MaxSpringCompression;

}

void UWheeledVehicleMovementComponent::DrawDebug(UCanvas* Canvas, float& YL, float& YPos)
{
	if (PVehicle == NULL)
	{
		return;
	}

	FPhysXVehicleManager* MyVehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(GetWorld()->GetPhysicsScene());

	MyVehicleManager->SetRecordTelemetry(this, true);

	UFont* RenderFont = GEngine->GetSmallFont();
	// draw drive data
	{
		Canvas->SetDrawColor(FColor::White);
		float forwardSpeedKmH = CmSToKmH(GetForwardSpeed());
		YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Speed (km/h): %d"), (int32)forwardSpeedKmH), 4, YPos);
		YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Steering: %.1f"), SteeringInput), 4, YPos);
		YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Throttle: %.1f"), ThrottleInput), 4, YPos);
		YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Brake: %.1f"), BrakeInput), 4, YPos);
		YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("RPM: %.1f"), GetEngineRotationSpeed()), 4, YPos);
		YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Gear: %d"), GetCurrentGear()), 4, YPos);
		YPos += Canvas->DrawText(RenderFont, FString::Printf(TEXT("Drag: %.1f"), DebugDragMagnitude), 4, YPos);

	}

	SCOPED_SCENE_READ_LOCK(MyVehicleManager->GetScene());
	PxWheelQueryResult* WheelsStates = MyVehicleManager->GetWheelsStates_AssumesLocked(this);
	check(WheelsStates);

	float XPos = 4.f;
	auto GetXPos = [&XPos](float Amount) -> float
	{
		float Ret = XPos;
		XPos += Amount;
		return Ret;
	};

	// draw wheel data
	for (uint32 w = 0; w < PVehicle->mWheelsSimData.getNbWheels(); ++w)
	{
		XPos = 4.f;
		const PxMaterial* ContactSurface = WheelsStates[w].tireSurfaceMaterial;
		const PxReal TireFriction = WheelsStates[w].tireFriction;
		const PxReal LatSlip = WheelsStates[w].lateralSlip;
		const PxReal LongSlip = WheelsStates[w].longitudinalSlip;
		const PxReal WheelRPM = OmegaToRPM(PVehicle->mWheelsDynData.getWheelRotationSpeed(w));

		UPhysicalMaterial* ContactSurfaceMaterial = ContactSurface ? FPhysxUserData::Get<UPhysicalMaterial>(ContactSurface->userData) : NULL;
		const FString ContactSurfaceString = ContactSurfaceMaterial ? ContactSurfaceMaterial->GetName() : FString(TEXT("NONE"));

		Canvas->SetDrawColor(FColor::White);

		Canvas->DrawText(RenderFont, FString::Printf(TEXT("[%d]"), w), GetXPos(20.f), YPos);
		Canvas->DrawText(RenderFont, FString::Printf(TEXT("RPM: %.1f"), WheelRPM), GetXPos(80.f), YPos);
		Canvas->DrawText(RenderFont, FString::Printf(TEXT("Slip Ratio: %.2f"), LongSlip), GetXPos(100.f), YPos);
		Canvas->DrawText(RenderFont, FString::Printf(TEXT("Slip Angle (degrees): %.1f"), FMath::RadiansToDegrees(LatSlip)), GetXPos(180.f), YPos);
		Canvas->DrawText(RenderFont, FString::Printf(TEXT("Contact Surface: %s"), *ContactSurfaceString), GetXPos(200.f), YPos);

		YPos += YL;
		XPos = 24.f;
		if ((int32)w < Wheels.Num())
		{
			UVehicleWheel* Wheel = Wheels[w];
			Canvas->DrawText(RenderFont, FString::Printf(TEXT("Normalized Load: %.1f"), Wheel->DebugNormalizedTireLoad), GetXPos(150.f), YPos);
			Canvas->DrawText(RenderFont, FString::Printf(TEXT("Torque (Nm): %.1f"), Cm2ToM2(Wheel->DebugWheelTorque)), GetXPos(150.f), YPos);
			Canvas->DrawText(RenderFont, FString::Printf(TEXT("Long Force: %.1fN (%.1f%%)"), Wheel->DebugLongForce / 100.f, 100.f * Wheel->DebugLongForce / Wheel->DebugTireLoad), GetXPos(200.f), YPos);
			Canvas->DrawText(RenderFont, FString::Printf(TEXT("Lat Force: %.1fN (%.1f%%)"), Wheel->DebugLatForce / 100.f, 100.f * Wheel->DebugLatForce / Wheel->DebugTireLoad), GetXPos(200.f), YPos);
		}
		else
		{
			Canvas->DrawText(RenderFont, TEXT("Wheels array insufficiently sized!"), YL * 50, YPos);
		}

		YPos += YL * 1.2f;
	}

	// draw wheel graphs
	PxVehicleTelemetryData* TelemetryData = MyVehicleManager->GetTelemetryData_AssumesLocked();

	if (TelemetryData)
	{
		const float GraphWidth(100.0f), GraphHeight(100.0f);

		int GraphChannels[] = {
			PxVehicleWheelGraphChannel::eWHEEL_OMEGA,
			PxVehicleWheelGraphChannel::eSUSPFORCE,
			PxVehicleWheelGraphChannel::eTIRE_LONG_SLIP,
			PxVehicleWheelGraphChannel::eNORM_TIRE_LONG_FORCE,
			PxVehicleWheelGraphChannel::eTIRE_LAT_SLIP,
			PxVehicleWheelGraphChannel::eNORM_TIRE_LAT_FORCE,
			PxVehicleWheelGraphChannel::eNORMALIZED_TIRELOAD,
			PxVehicleWheelGraphChannel::eTIRE_FRICTION
		};

		for (uint32 w = 0; w < PVehicle->mWheelsSimData.getNbWheels(); ++w)
		{
			float CurX = 4;
			for (uint32 i = 0; i < ARRAY_COUNT(GraphChannels); ++i)
			{
				float OutX = GraphWidth;
				DrawTelemetryGraph(GraphChannels[i], TelemetryData->getWheelGraph(w), Canvas, CurX, YPos, GraphWidth, GraphHeight, OutX);
				CurX += OutX + 10.f;
			}

			YPos += GraphHeight + 10.f;
			YPos += YL;
		}
	}

	DrawDebugLines();
}


void UWheeledVehicleMovementComponent::FixupSkeletalMesh()
{
	if (USkeletalMeshComponent * Mesh = Cast<USkeletalMeshComponent>(GetMesh()))
	{

					//in skeletal mesh case we must set the offset on the PrimitiveComponent's BodyInstance, which will later update the actual root body
					//this is needed for UI

		if (UPhysicsAsset * PhysicsAsset = Mesh->GetPhysicsAsset())
		{
			for (int32 WheelIdx = 0; WheelIdx < WheelSetups.Num(); ++WheelIdx)
			{
				FWheelSetup& WheelSetup = WheelSetups[WheelIdx];
				if (WheelSetup.BoneName != NAME_None)
				{
					int32 BodySetupIdx = PhysicsAsset->FindBodyIndex(WheelSetup.BoneName);
					
					if (BodySetupIdx >= 0)
					{
						FBodyInstance* BodyInstance = Mesh->Bodies[BodySetupIdx];
						BodyInstance->SetResponseToAllChannels(ECR_Ignore);	//turn off collision for wheel automatically

						if (UBodySetup * BodySetup = PhysicsAsset->SkeletalBodySetups[BodySetupIdx])
						{

							if (BodySetup->PhysicsType == PhysType_Default) 	//if they set it to unfixed we don't fixup because they are explicitely saying Unfixed
							{
								BodyInstance->SetInstanceSimulatePhysics(false);
							}

							//and get rid of constraints on the wheels. TODO: right now we remove all wheel constraints, we probably only want to remove parent constraints
							TArray<int32> WheelConstraints;
							PhysicsAsset->BodyFindConstraints(BodySetupIdx, WheelConstraints);
							for (int32 ConstraintIdx = 0; ConstraintIdx < WheelConstraints.Num(); ++ConstraintIdx)
							{
								FConstraintInstance * ConstraintInstance = Mesh->Constraints[WheelConstraints[ConstraintIdx]];
								ConstraintInstance->TermConstraint();
							}
						}
					}
				}
			}
		}

		Mesh->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipSimulatingBones;
		
	}


}

void UWheeledVehicleMovementComponent::DrawDebugLines()
{
#if ENABLE_DRAW_DEBUG

	if ( PVehicle == NULL )
	{
		return;
	}

	UWorld* World = GetWorld();

	FPhysXVehicleManager* MyVehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(World->GetPhysicsScene());

	MyVehicleManager->SetRecordTelemetry( this, true );

	PxRigidDynamic* PActor = PVehicle->getRigidDynamicActor();

	// gather wheel shapes
	PxShape* PShapeBuffer[32];
	PActor->getShapes( PShapeBuffer, 32 );
	const uint32 PNumWheels = PVehicle->mWheelsSimData.getNbWheels();

	// draw chassis orientation
	const PxTransform GlobalT = PActor->getGlobalPose();
	const PxTransform T = GlobalT.transform( PActor->getCMassLocalPose() );
	const PxVec3 ChassisExtent = PActor->getWorldBounds().getExtents();
	const float ChassisSize = ChassisExtent.magnitude();
	DrawDebugLine(World, P2UVector(T.p), P2UVector(T.p + T.rotate(PxVec3(ChassisSize, 0, 0))), FColor::Red);
	DrawDebugLine(World, P2UVector(T.p), P2UVector(T.p + T.rotate(PxVec3(0, ChassisSize, 0))), FColor::Green);
	DrawDebugLine(World, P2UVector(T.p), P2UVector(T.p + T.rotate(PxVec3(0, 0, ChassisSize))), FColor::Blue);

	SCOPED_SCENE_READ_LOCK(MyVehicleManager->GetScene());
	PxVehicleTelemetryData* TelemetryData = MyVehicleManager->GetTelemetryData_AssumesLocked();
	
	PxWheelQueryResult* WheelsStates = MyVehicleManager->GetWheelsStates_AssumesLocked(this);
	check(WheelsStates);

	for ( uint32 w = 0; w < PNumWheels; ++w )
	{
		// render suspension raycast
	
		const FVector SuspensionStart = P2UVector( WheelsStates[w].suspLineStart );
		const FVector SuspensionEnd = P2UVector( WheelsStates[w].suspLineStart + WheelsStates[w].suspLineDir * WheelsStates[w].suspLineLength );
		const FColor SuspensionColor = WheelsStates[w].tireSurfaceMaterial == NULL ? FColor(255,64,64) : FColor(64,255,64);
		DrawDebugLine(World, SuspensionStart, SuspensionEnd, SuspensionColor );

		// render wheel radii
		const int32 ShapeIndex = PVehicle->mWheelsSimData.getWheelShapeMapping( w );
		const PxF32 WheelRadius = PVehicle->mWheelsSimData.getWheelData(w).mRadius;
		const PxF32 WheelWidth = PVehicle->mWheelsSimData.getWheelData(w).mWidth;
		const FTransform WheelTransform = P2UTransform( PActor->getGlobalPose().transform( PShapeBuffer[ShapeIndex]->getLocalPose() ) );
		const FVector WheelLocation = WheelTransform.GetLocation();
		const FVector WheelLatDir = WheelTransform.TransformVector( FVector( 0.0f, 1.0f, 0.0f ) );
		const FVector WheelLatOffset = WheelLatDir * WheelWidth * 0.50f;
		//const FVector WheelRotDir = FQuat( WheelLatDir, PVehicle->mWheelsDynData.getWheelRotationAngle(w) ) * FVector( 1.0f, 0.0f, 0.0f );
		const FVector WheelRotDir = WheelTransform.TransformVector( FVector( 1.0f, 0.0f, 0.0f ) );
		const FVector WheelRotOffset = WheelRotDir * WheelRadius;

		const FVector CylinderStart = WheelLocation + WheelLatOffset;
		const FVector CylinderEnd = WheelLocation - WheelLatOffset;

		DrawDebugCylinder( World, CylinderStart, CylinderEnd, WheelRadius, 16, SuspensionColor );
		DrawDebugLine( World, WheelLocation, WheelLocation + WheelRotOffset, SuspensionColor );

		const FVector ContactPoint = P2UVector( WheelsStates[w].tireContactPoint );
		DrawDebugBox( World, ContactPoint, FVector(4.0f), FQuat::Identity, SuspensionColor );

		if ( TelemetryData )
		{
			// Draw all tire force app points.
			const PxVec3& PAppPoint = TelemetryData->getTireforceAppPoints()[w];
			const FVector AppPoint = P2UVector( PAppPoint );
			DrawDebugBox( World, AppPoint, FVector(5.0f), FQuat::Identity, FColor( 255, 0, 255 ) );

			// Draw all susp force app points.
			const PxVec3& PAppPoint2 = TelemetryData->getSuspforceAppPoints()[w];
			const FVector AppPoint2 = P2UVector( PAppPoint2 );
			DrawDebugBox( World, AppPoint2, FVector(5.0f), FQuat::Identity, FColor( 0, 255, 255 ) );
		}
	}
#endif // ENABLE_DRAW_DEBUG
}

#if WITH_EDITOR

void UWheeledVehicleMovementComponent::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty( PropertyChangedEvent );

	// Trigger a runtime rebuild of the PhysX vehicle
	FPhysXVehicleManager::VehicleSetupTag++;
}

#endif // WITH_EDITOR

/// @cond DOXYGEN_WARNINGS

void UWheeledVehicleMovementComponent::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( UWheeledVehicleMovementComponent, ReplicatedState );
}

/// @endcond

void UWheeledVehicleMovementComponent::ComputeConstants()
{
	DragArea = ChassisWidth * ChassisHeight;
	MaxEngineRPM = 5000.f;
}

void UWheeledVehicleMovementComponent::CalculateAvoidanceVelocity(float DeltaTime)
{
	if (!bUseRVOAvoidance)
	{
		return;
	}
	
	UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
	APawn* MyOwner = UpdatedComponent ? Cast<APawn>(UpdatedComponent->GetOwner()) : NULL;

	// since we don't assign the avoidance velocity but instead use it to adjust steering and throttle,
	// always reset the avoidance velocity to the current velocity
	AvoidanceVelocity = GetVelocityForRVOConsideration();

	if (AvoidanceWeight >= 1.0f || AvoidanceManager == NULL || MyOwner == NULL)
	{
		return;
	}
	
	if (MyOwner->Role != ROLE_Authority)
	{	
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bShowDebug = AvoidanceManager->IsDebugEnabled(AvoidanceUID);
#endif

	if (!AvoidanceVelocity.IsZero())
	{
		//See if we're doing a locked avoidance move already, and if so, skip the testing and just do the move.
		if (AvoidanceLockTimer > 0.0f)
		{
			AvoidanceVelocity = AvoidanceLockVelocity;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bShowDebug)
			{
				DrawDebugLine(GetWorld(), GetRVOAvoidanceOrigin(), GetRVOAvoidanceOrigin() + AvoidanceVelocity, FColor::Blue, true, 0.5f, SDPG_MAX);
			}
#endif
		}
		else
		{
			FVector NewVelocity = AvoidanceManager->GetAvoidanceVelocityForComponent(this);
			if (!NewVelocity.Equals(AvoidanceVelocity))		//Really want to branch hint that this will probably not pass
			{
				//Had to divert course, lock this avoidance move in for a short time. This will make us a VO, so unlocked others will know to avoid us.
				AvoidanceVelocity = NewVelocity;
				SetAvoidanceVelocityLock(AvoidanceManager, AvoidanceManager->LockTimeAfterAvoid);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (bShowDebug)
				{
					DrawDebugLine(GetWorld(), GetRVOAvoidanceOrigin(), GetRVOAvoidanceOrigin() + AvoidanceVelocity, FColor::Red, true, 20.0f, SDPG_MAX, 10.0f);
				}
#endif
			}
			else
			{
				//Although we didn't divert course, our velocity for this frame is decided. We will not reciprocate anything further, so treat as a VO for the remainder of this frame.
				SetAvoidanceVelocityLock(AvoidanceManager, AvoidanceManager->LockTimeAfterClean);	//10 ms of lock time should be adequate.
			}
		}

		AvoidanceManager->UpdateRVO(this);
		bWasAvoidanceUpdated = true;
	}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	else if (bShowDebug)
	{
		DrawDebugLine(GetWorld(), GetRVOAvoidanceOrigin(), GetRVOAvoidanceOrigin() + GetVelocityForRVOConsideration(), FColor::Yellow, true, 0.05f, SDPG_MAX);
	}

	if (bShowDebug)
	{
		FVector UpLine(0, 0, 500);
		DrawDebugLine(GetWorld(), GetRVOAvoidanceOrigin(), GetRVOAvoidanceOrigin() + UpLine, (AvoidanceLockTimer > 0.01f) ? FColor::Red : FColor::Blue, true, 0.05f, SDPG_MAX, 5.0f);
	}
#endif
}

void UWheeledVehicleMovementComponent::SetAvoidanceGroup(int32 GroupFlags)
{
	AvoidanceGroup.SetFlagsDirectly(GroupFlags);
}

void UWheeledVehicleMovementComponent::SetAvoidanceGroupMask(const FNavAvoidanceMask& GroupMask)
{
	AvoidanceGroup.SetFlagsDirectly(GroupMask.Packed);
}

void UWheeledVehicleMovementComponent::SetGroupsToAvoid(int32 GroupFlags)
{
	GroupsToAvoid.SetFlagsDirectly(GroupFlags);
}

void UWheeledVehicleMovementComponent::SetGroupsToAvoidMask(const FNavAvoidanceMask& GroupMask)
{
	GroupsToAvoid.SetFlagsDirectly(GroupMask.Packed);
}

void UWheeledVehicleMovementComponent::SetGroupsToIgnore(int32 GroupFlags)
{
	GroupsToIgnore.SetFlagsDirectly(GroupFlags);
}

void UWheeledVehicleMovementComponent::SetGroupsToIgnoreMask(const FNavAvoidanceMask& GroupMask)
{
	GroupsToIgnore.SetFlagsDirectly(GroupMask.Packed);
}

void UWheeledVehicleMovementComponent::SetAvoidanceEnabled(bool bEnable)
{
	if (bUseRVOAvoidance != bEnable)
	{
		bUseRVOAvoidance = bEnable;
		
		// reset id, RegisterMovementComponent call is required to initialize update timers in avoidance manager
		AvoidanceUID = 0;

		UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
		if (AvoidanceManager && bEnable)
		{
			AvoidanceManager->RegisterMovementComponent(this, AvoidanceWeight);
		}
	}
}


void UWheeledVehicleMovementComponent::SetRVOAvoidanceUID(int32 UID)
{
	AvoidanceUID = UID;
}

int32 UWheeledVehicleMovementComponent::GetRVOAvoidanceUID()
{
	return AvoidanceUID;
}

void UWheeledVehicleMovementComponent::SetRVOAvoidanceWeight(float Weight)
{
	AvoidanceWeight = Weight;
}

float UWheeledVehicleMovementComponent::GetRVOAvoidanceWeight()
{
	return AvoidanceWeight;
}

FVector UWheeledVehicleMovementComponent::GetRVOAvoidanceOrigin()
{
	return UpdatedComponent->GetComponentLocation();
}

float UWheeledVehicleMovementComponent::GetRVOAvoidanceRadius()
{
	return RVOAvoidanceRadius;
}

float UWheeledVehicleMovementComponent::GetRVOAvoidanceHeight()
{
	return RVOAvoidanceHeight;
}

float UWheeledVehicleMovementComponent::GetRVOAvoidanceConsiderationRadius()
{
	return AvoidanceConsiderationRadius;
}

FVector UWheeledVehicleMovementComponent::GetVelocityForRVOConsideration()
{
	FVector Velocity2D = UpdatedComponent->GetComponentVelocity();
	Velocity2D.Z = 0.f;

	return Velocity2D;
}

int32 UWheeledVehicleMovementComponent::GetAvoidanceGroupMask()
{
	return AvoidanceGroup.Packed;
}

int32 UWheeledVehicleMovementComponent::GetGroupsToAvoidMask()
{
	return GroupsToAvoid.Packed;
}

int32 UWheeledVehicleMovementComponent::GetGroupsToIgnoreMask()
{
	return GroupsToIgnore.Packed;
}

#undef LOCTEXT_NAMESPACE

FWheelSetup::FWheelSetup()
: WheelClass(UVehicleWheel::StaticClass())
, BoneName(NAME_None)
, AdditionalOffset(0.0f)
{

}
