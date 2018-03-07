// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DrawFrsutumComponent.cpp: UDrawFrsutumComponent implementation.
=============================================================================*/

#include "MediaPlaneFrustumComponent.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Engine/CollisionProfile.h"
#include "SceneManagement.h"
#include "MediaPlaneComponent.h"


/** Represents a draw frustum to the scene manager. */
class FMediaPlaneFrustumSceneProxy : public FPrimitiveSceneProxy
{
public:

	FMediaPlaneFrustumSceneProxy(const UMediaPlaneFrustumComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, ViewTarget(nullptr)
	{
		bWillEverBeLit = false;

		UMediaPlaneComponent* Parent = Cast<UMediaPlaneComponent>(InComponent->GetAttachParent());
		if (Parent && Parent->GetPlane().bFillScreen)
		{
			ViewTarget = Parent->FindViewTarget();
			InvViewProjectionMatrix = Parent->GetCachedInvViewProjectionMatrix();
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		if (!ViewTarget)
		{
			return;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}

			const FSceneView* View = Views[ViewIndex];

			// Don't draw frustums on views that are set as the current view target
			if (View->ViewActor == ViewTarget)
			{
				continue;
			}

			// These are all in world space
			const float NearPlaneZ = 1.f;
			FVector NearPlaneTopLeft 		= UMediaPlaneComponent::TransfromFromProjection(InvViewProjectionMatrix, FVector4(-1.f, -1.f, NearPlaneZ, 1.0f));
			FVector NearPlaneTopRight 		= UMediaPlaneComponent::TransfromFromProjection(InvViewProjectionMatrix, FVector4( 1.f, -1.f, NearPlaneZ, 1.0f));
			FVector NearPlaneBottomLeft 	= UMediaPlaneComponent::TransfromFromProjection(InvViewProjectionMatrix, FVector4(-1.f,  1.f, NearPlaneZ, 1.0f));
			FVector NearPlaneBottomRight	= UMediaPlaneComponent::TransfromFromProjection(InvViewProjectionMatrix, FVector4( 1.f,  1.f, NearPlaneZ, 1.0f));

			FVector DestinationTopLeft 		=  GetLocalToWorld().TransformPosition(FVector(0.f, -1.f, -1.f));
			FVector DestinationTopRight 	=  GetLocalToWorld().TransformPosition(FVector(0.f,  1.f, -1.f));
			FVector DestinationBottomLeft 	=  GetLocalToWorld().TransformPosition(FVector(0.f, -1.f,  1.f));
			FVector DestinationBottomRight	=  GetLocalToWorld().TransformPosition(FVector(0.f,  1.f,  1.f));

			FColor LineColor(255, 0, 255, 128);

			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			const uint8 DepthPriorityGroup = GetDepthPriorityGroup(View);
			PDI->DrawLine(NearPlaneTopLeft, DestinationTopLeft, LineColor, DepthPriorityGroup );
			PDI->DrawLine(NearPlaneTopRight, DestinationTopRight, LineColor, DepthPriorityGroup );
			PDI->DrawLine(NearPlaneBottomLeft, DestinationBottomLeft, LineColor, DepthPriorityGroup );
			PDI->DrawLine(NearPlaneBottomRight, DestinationBottomRight, LineColor, DepthPriorityGroup );
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		return Result;
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	AActor* ViewTarget;
	FMatrix InvViewProjectionMatrix;
};

UMediaPlaneFrustumComponent::UMediaPlaneFrustumComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseEditorCompositing = true;
	bHiddenInGame = true;
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bGenerateOverlapEvents = false;
}

FPrimitiveSceneProxy* UMediaPlaneFrustumComponent::CreateSceneProxy()
{
	return new FMediaPlaneFrustumSceneProxy(this);
}

FBoxSphereBounds UMediaPlaneFrustumComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	UMediaPlaneComponent* Parent = Cast<UMediaPlaneComponent>(GetAttachParent());
	
	FBox MaxBox({
		FVector(0,  1,  1),
		FVector(0, -1,  1),
		FVector(0,  1, -1),
		FVector(0, -1, -1),
		});

	MaxBox = MaxBox.TransformBy(LocalToWorld);

	// Include the near view plane if possible
	if (Parent && Parent->GetPlane().bFillScreen)
	{
		const FMatrix& InvViewProjectionMatrix = Parent->GetCachedInvViewProjectionMatrix();
		
		// These are all in world space
		const float NearPlaneZ = 1.f;
		MaxBox += UMediaPlaneComponent::TransfromFromProjection(InvViewProjectionMatrix, FVector4(0.f, 0.f, NearPlaneZ, 1.0f));
	}

	return FBoxSphereBounds(MaxBox);
}