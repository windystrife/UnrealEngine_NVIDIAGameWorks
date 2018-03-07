// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavModifierComponent.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "AI/NavigationModifier.h"
#include "AI/Navigation/NavAreas/NavArea_Null.h"
#include "AI/NavigationOctree.h"
#include "PhysicsEngine/BodySetup.h"

UNavModifierComponent::UNavModifierComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AreaClass = UNavArea_Null::StaticClass();
	FailsafeExtent = FVector(100, 100, 100);
}

void UNavModifierComponent::CalcAndCacheBounds() const
{
	const AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
		MyOwner->GetComponents(PrimComponents);

		Bounds = FBox(ForceInit);
		ComponentBounds.Reset();
		for (int32 Idx = 0; Idx < PrimComponents.Num(); Idx++)
		{
			UPrimitiveComponent* PrimComp = PrimComponents[Idx];
			if (PrimComp && PrimComp->IsRegistered() && PrimComp->IsCollisionEnabled())
			{
				UBodySetup* BodySetup = PrimComp->GetBodySetup();
				if (BodySetup)
				{
					FTransform ParentTM = PrimComp->GetComponentTransform();
					const FVector Scale3D = ParentTM.GetScale3D();
					ParentTM.RemoveScaling();
					Bounds += PrimComp->Bounds.GetBox();

					for (int32 SphereIdx = 0; SphereIdx < BodySetup->AggGeom.SphereElems.Num(); SphereIdx++)
					{
						const FKSphereElem& ElemInfo = BodySetup->AggGeom.SphereElems[SphereIdx];
						FTransform ElemTM = ElemInfo.GetTransform();
						ElemTM.ScaleTranslation(Scale3D);
						ElemTM *= ParentTM;

						const FBox SphereBounds = FBox::BuildAABB(ElemTM.GetLocation(), ElemInfo.Radius * Scale3D);
						ComponentBounds.Add(FRotatedBox(SphereBounds, ElemTM.GetRotation()));
					}

					for (int32 BoxIdx = 0; BoxIdx < BodySetup->AggGeom.BoxElems.Num(); BoxIdx++)
					{
						const FKBoxElem& ElemInfo = BodySetup->AggGeom.BoxElems[BoxIdx];
						FTransform ElemTM = ElemInfo.GetTransform();
						ElemTM.ScaleTranslation(Scale3D);
						ElemTM *= ParentTM;

						const FBox BoxBounds = FBox::BuildAABB(ElemTM.GetLocation(), FVector(ElemInfo.X, ElemInfo.Y, ElemInfo.Z) * Scale3D * 0.5f);
						ComponentBounds.Add(FRotatedBox(BoxBounds, ElemTM.GetRotation()));
					}

					for (int32 SphylIdx = 0; SphylIdx < BodySetup->AggGeom.SphylElems.Num(); SphylIdx++)
					{
						const FKSphylElem& ElemInfo = BodySetup->AggGeom.SphylElems[SphylIdx];
						FTransform ElemTM = ElemInfo.GetTransform();
						ElemTM.ScaleTranslation(Scale3D);
						ElemTM *= ParentTM;

						const FBox SphylBounds = FBox::BuildAABB(ElemTM.GetLocation(), FVector(ElemInfo.Radius, ElemInfo.Radius, ElemInfo.Length) * Scale3D);
						ComponentBounds.Add(FRotatedBox(SphylBounds, ElemTM.GetRotation()));
					}

					for (int32 ConvexIdx = 0; ConvexIdx < BodySetup->AggGeom.ConvexElems.Num(); ConvexIdx++)
					{
						const FKConvexElem& ElemInfo = BodySetup->AggGeom.ConvexElems[ConvexIdx];
						FTransform ElemTM = ElemInfo.GetTransform();
						ElemTM.ScaleTranslation(Scale3D);
						ElemTM *= FTransform(ParentTM.GetLocation());

						const FBox ConvexBounds = ElemInfo.CalcAABB(ElemTM, Scale3D);
						ComponentBounds.Add(FRotatedBox(ConvexBounds, ElemTM.GetRotation() * ParentTM.GetRotation()));
					}
				}
			}
		}

		if (ComponentBounds.Num() == 0)
		{
			Bounds = FBox::BuildAABB(MyOwner->GetActorLocation(), FailsafeExtent);
			ComponentBounds.Add(FRotatedBox(Bounds, MyOwner->GetActorQuat()));
		}
		else
		{
			for (int32 Idx = 0; Idx < ComponentBounds.Num(); Idx++)
			{
				const FVector BoxOrigin = ComponentBounds[Idx].Box.GetCenter();
				const FVector BoxExtent = ComponentBounds[Idx].Box.GetExtent();
				
				const FVector NavModBoxOrigin = FTransform(ComponentBounds[Idx].Quat).InverseTransformPosition(BoxOrigin);
				ComponentBounds[Idx].Box = FBox::BuildAABB(NavModBoxOrigin, BoxExtent);
			}
		}
	}
}

void UNavModifierComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	for (int32 Idx = 0; Idx < ComponentBounds.Num(); Idx++)
	{
		Data.Modifiers.Add(FAreaNavModifier(ComponentBounds[Idx].Box, FTransform(ComponentBounds[Idx].Quat), AreaClass));
	}
}

void UNavModifierComponent::SetAreaClass(TSubclassOf<UNavArea> NewAreaClass)
{
	if (AreaClass != NewAreaClass)
	{
		AreaClass = NewAreaClass;
		RefreshNavigationModifiers();
	}
}
