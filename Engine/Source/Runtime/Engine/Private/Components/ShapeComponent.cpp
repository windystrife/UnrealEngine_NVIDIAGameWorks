// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Components/ShapeComponent.h"
#include "AI/Navigation/NavAreas/NavArea_Obstacle.h"
#include "AI/NavigationOctree.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_PHYSX
	#include "PhysXPublic.h"
#endif // WITH_PHYSX



UShapeComponent::UShapeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static const FName CollisionProfileName(TEXT("OverlapAllDynamic"));
	BodyInstance.SetCollisionProfileName(CollisionProfileName);
	// when we deprecated this variable, we switched on for the shapecomponent collision profile
	// the problem with adding collision profile later, the instanced data will be wiped
	// since shape component is so popular for BP and so on, I'm adding manual support for compatibility
	// this only works since this variable is getting deprecated. 
	BodyInstance.ResponseToChannels_DEPRECATED.SetAllChannels(ECR_Block);
	BodyInstance.bAutoWeld = true;	//UShapeComponent by default has auto welding

	bHiddenInGame = true;
	bCastDynamicShadow = false;
	ShapeColor = FColor(223, 149, 157, 255);
	bShouldCollideWhenPlacing = false;

	bUseArchetypeBodySetup = !IsTemplate();
	
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;
	bCanEverAffectNavigation = true;
	bDynamicObstacle = false;
	AreaClass = UNavArea_Obstacle::StaticClass();
}

FPrimitiveSceneProxy* UShapeComponent::CreateSceneProxy()
{
	check( false && "Subclass needs to Implement this" );
	return NULL;
}

FBoxSphereBounds UShapeComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	check( false && "Subclass needs to Implement this" );
	return FBoxSphereBounds();
}

void UShapeComponent::UpdateBodySetup()
{
	check( false && "Subclass needs to Implement this" );
}

UBodySetup* UShapeComponent::GetBodySetup()
{
	UpdateBodySetup();
	return ShapeBodySetup;
}

#if WITH_EDITOR
void UShapeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!IsTemplate())
	{
		UpdateBodySetup(); // do this before reregistering components so that new values are used for collision
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

bool UShapeComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	return !bDynamicObstacle;
}

void UShapeComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	if (bDynamicObstacle)
	{
		Data.Modifiers.CreateAreaModifiers(this, AreaClass);
	}
}

bool UShapeComponent::IsNavigationRelevant() const
{
	// failed CanEverAffectNavigation() always takes priority
	// dynamic obstacle overrides collision check

	return (bDynamicObstacle && CanEverAffectNavigation()) || Super::IsNavigationRelevant();
}

template <> void UShapeComponent::AddShapeToGeomArray<FKBoxElem>() { ShapeBodySetup->AggGeom.BoxElems.Add(FKBoxElem()); }
template <> void UShapeComponent::AddShapeToGeomArray<FKSphereElem>() { ShapeBodySetup->AggGeom.SphereElems.Add(FKSphereElem()); }
template <> void UShapeComponent::AddShapeToGeomArray<FKSphylElem>() { ShapeBodySetup->AggGeom.SphylElems.Add(FKSphylElem()); }

#if WITH_PHYSX
template <> void UShapeComponent::SetShapeToNewGeom<FKBoxElem>(PxShape* PShape) { PShape->userData = (void*)ShapeBodySetup->AggGeom.BoxElems[0].GetUserData(); }
template <> void UShapeComponent::SetShapeToNewGeom<FKSphereElem>(PxShape* PShape) { PShape->userData = (void*)ShapeBodySetup->AggGeom.SphereElems[0].GetUserData(); }
template <> void UShapeComponent::SetShapeToNewGeom<FKSphylElem>(PxShape* PShape) { PShape->userData = (void*)ShapeBodySetup->AggGeom.SphylElems[0].GetUserData(); }
#endif

template <typename ShapeElemType>
void UShapeComponent::CreateShapeBodySetupIfNeeded()
{
	if (ShapeBodySetup == nullptr || ShapeBodySetup->IsPendingKill())
	{
		ShapeBodySetup = NewObject<UBodySetup>(this, NAME_None, RF_Transient);
		if (GUObjectArray.IsDisregardForGC(this))
		{
			ShapeBodySetup->AddToRoot();
		}

		// If this component is in GC cluster, make sure we add the body setup to it to
		ShapeBodySetup->AddToCluster(this);
		
		ShapeBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		AddShapeToGeomArray<ShapeElemType>();
		ShapeBodySetup->bNeverNeedsCookedCollisionData = true;
		bUseArchetypeBodySetup = false;	//We're making our own body setup, so don't use the archetype's.

		//Update bodyinstance and shapes
		BodyInstance.BodySetup = ShapeBodySetup;
		{
			if(BodyInstance.IsValidBodyInstance())
			{
#if WITH_PHYSX
				BodyInstance.ExecuteOnPhysicsReadWrite([this]
				{
					TArray<PxShape *> PShapes;
					BodyInstance.GetAllShapes_AssumesLocked(PShapes);

					for(PxShape* PShape : PShapes)	//The reason we iterate is we may have multiple scenes and thus multiple shapes, but they are all pointing to the same geometry
					{
						//Update shape with the new body setup. Make sure to only update shapes owned by this body instance
						if(BodyInstance.IsShapeBoundToBody(PShape))
						{
							SetShapeToNewGeom<ShapeElemType>(PShape);
						}
					}
				});
#endif
			}
		}
	}
}

//Explicit instantiation of the different shape components
template void UShapeComponent::CreateShapeBodySetupIfNeeded<FKSphylElem>();
template void UShapeComponent::CreateShapeBodySetupIfNeeded<FKBoxElem>();
template void UShapeComponent::CreateShapeBodySetupIfNeeded<FKSphereElem>();
