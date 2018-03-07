// Copyright 2017 Google Inc.

#include "GoogleARCorePlaneRendererComponent.h"
#include "GoogleARCoreFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "GoogleARCorePlane.h"

UGoogleARCorePlaneRendererComponent::UGoogleARCorePlaneRendererComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	PlaneIndices.AddUninitialized(6);
	PlaneIndices[0] = 0; PlaneIndices[1] = 1; PlaneIndices[2] = 2;
	PlaneIndices[3] = 0; PlaneIndices[4] = 2; PlaneIndices[5] = 3;
}

void UGoogleARCorePlaneRendererComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
	DrawPlanes();
}

void UGoogleARCorePlaneRendererComponent::DrawPlanes()
{
	UWorld* World = GetWorld();
#if PLATFORM_ANDROID
	if (UGoogleARCoreSessionFunctionLibrary::GetSessionStatus() == EGoogleARCoreSessionStatus::Tracking)
	{
		TArray<UGoogleARCorePlane*> PlaneList;
		UGoogleARCoreFrameFunctionLibrary::GetAllPlanes(PlaneList);
		for (UGoogleARCorePlane* Plane : PlaneList)
		{
			if (Plane->GetTrackingState() != EGoogleARCorePlaneTrackingState::Tracking)
			{
				continue;
			}

			if (bRenderPlane)
			{
				FTransform BoundingBoxTransform = Plane->GetBoundingBoxWorldTransform();
				FVector BoundingBoxLocation = BoundingBoxTransform.GetLocation();
				FVector2D BoundingBoxSize = Plane->GetBoundingBoxSize();

				PlaneVertices.Empty();
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(-BoundingBoxSize.X / 2.0f, -BoundingBoxSize.Y / 2.0f, 0)));
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(-BoundingBoxSize.X / 2.0f, BoundingBoxSize.Y / 2.0f, 0)));
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(BoundingBoxSize.X / 2.0f, BoundingBoxSize.Y / 2.0f, 0)));
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(BoundingBoxSize.X / 2.0f, -BoundingBoxSize.Y / 2.0f, 0)));
				// plane quad
				DrawDebugMesh(World, PlaneVertices, PlaneIndices, PlaneColor);
			}

			if (bRenderBoundaryPolygon)
			{
				const TArray<FVector>& BoundaryPolygonData = Plane->GetWorldSpaceBoundaryPolygon();
				for (int i = 0; i < BoundaryPolygonData.Num(); i++)
				{
					FVector Start = BoundaryPolygonData[i];
					FVector End = BoundaryPolygonData[(i + 1) % BoundaryPolygonData.Num()];
					DrawDebugLine(World, Start, End, BoundaryPolygonColor, false, -1.f, 0, BoundaryPolygonThickness);
				}
			}
		}
	}
#endif
}
