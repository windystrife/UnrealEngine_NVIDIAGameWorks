// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LandscapeSpline.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "Misc/Guid.h"
#include "Math/RandomStream.h"
#include "UObject/ObjectMacros.h"
#include "Templates/Casts.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/EngineTypes.h"
#include "HitProxies.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "LandscapeProxy.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "LandscapeComponent.h"
#include "LandscapeVersion.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "LandscapeSplineProxies.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplineRaster.h"
#include "LandscapeSplineControlPoint.h"
#include "ControlPointMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMeshSocket.h"
#include "EngineGlobals.h"
#if WITH_EDITOR
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#endif

IMPLEMENT_HIT_PROXY(HLandscapeSplineProxy, HHitProxy);
IMPLEMENT_HIT_PROXY(HLandscapeSplineProxy_Segment, HLandscapeSplineProxy);
IMPLEMENT_HIT_PROXY(HLandscapeSplineProxy_ControlPoint, HLandscapeSplineProxy);
IMPLEMENT_HIT_PROXY(HLandscapeSplineProxy_Tangent, HLandscapeSplineProxy);

#define LOCTEXT_NAMESPACE "Landscape.Splines"

//////////////////////////////////////////////////////////////////////////
// LANDSCAPE SPLINES SCENE PROXY

/** Represents a ULandscapeSplinesComponent to the scene manager. */
#if WITH_EDITOR
class FLandscapeSplinesSceneProxy : public FPrimitiveSceneProxy
{
private:
	const FLinearColor	SplineColor;

	const UTexture2D*	ControlPointSprite;
	const bool			bDrawControlPointSprite;
	const bool			bDrawFalloff;

	struct FSegmentProxy
	{
		ULandscapeSplineSegment* Owner;
		TRefCountPtr<HHitProxy> HitProxy;
		TArray<FLandscapeSplineInterpPoint> Points;
		uint32 bSelected : 1;
	};
	TArray<FSegmentProxy> Segments;

	struct FControlPointProxy
	{
		ULandscapeSplineControlPoint* Owner;
		TRefCountPtr<HHitProxy> HitProxy;
		FVector Location;
		TArray<FLandscapeSplineInterpPoint> Points;
		float SpriteScale;
		uint32 bSelected : 1;
	};
	TArray<FControlPointProxy> ControlPoints;

public:

	~FLandscapeSplinesSceneProxy()
	{
	}

	FLandscapeSplinesSceneProxy(ULandscapeSplinesComponent* Component):
		FPrimitiveSceneProxy(Component),
		SplineColor(Component->SplineColor),
		ControlPointSprite(Component->ControlPointSprite),
		bDrawControlPointSprite(Component->bShowSplineEditorMesh),
		bDrawFalloff(Component->bShowSplineEditorMesh)
	{
		Segments.Reserve(Component->Segments.Num());
		for (ULandscapeSplineSegment* Segment : Component->Segments)
		{
			FSegmentProxy SegmentProxy;
			SegmentProxy.Owner = Segment;
			SegmentProxy.HitProxy = nullptr;
			SegmentProxy.Points = Segment->GetPoints();
			SegmentProxy.bSelected = Segment->IsSplineSelected();
			Segments.Add(SegmentProxy);
		}

		ControlPoints.Reserve(Component->ControlPoints.Num());
		for (ULandscapeSplineControlPoint* ControlPoint : Component->ControlPoints)
		{
			FControlPointProxy ControlPointProxy;
			ControlPointProxy.Owner = ControlPoint;
			ControlPointProxy.HitProxy = nullptr;
			ControlPointProxy.Location = ControlPoint->Location;
			ControlPointProxy.Points = ControlPoint->GetPoints();
			ControlPointProxy.SpriteScale = FMath::Clamp<float>(ControlPoint->Width != 0 ? ControlPoint->Width / 2 : ControlPoint->SideFalloff / 4, 10, 1000);
			ControlPointProxy.bSelected = ControlPoint->IsSplineSelected();
			ControlPoints.Add(ControlPointProxy);
		}
	}

	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override
	{
		OutHitProxies.Reserve(OutHitProxies.Num() + Segments.Num() + ControlPoints.Num());
		for (FSegmentProxy& Segment : Segments)
		{
			Segment.HitProxy = new HLandscapeSplineProxy_Segment(Segment.Owner);
			OutHitProxies.Add(Segment.HitProxy);
		}
		for (FControlPointProxy& ControlPoint : ControlPoints)
		{
			ControlPoint.HitProxy = new HLandscapeSplineProxy_ControlPoint(ControlPoint.Owner);
			OutHitProxies.Add(ControlPoint.HitProxy);
		}
		return nullptr;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		// Slight Depth Bias so that the splines show up when they exactly match the target surface
		// e.g. someone playing with splines on a newly-created perfectly-flat landscape
		static const float DepthBias = 0.0001;

		const FMatrix& MyLocalToWorld = GetLocalToWorld();

		const FLinearColor SelectedSplineColor = GEngine->GetSelectedMaterialColor();
		const FLinearColor SelectedControlPointSpriteColor = FLinearColor::White + (GEngine->GetSelectedMaterialColor() * GEngine->SelectionHighlightIntensityBillboards * 10); 

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

				for (const FSegmentProxy& Segment : Segments)
				{
					const FLinearColor SegmentColor = Segment.bSelected ? SelectedSplineColor : SplineColor;

					if (Segment.Points.Num() == 0 || !Segment.Points.IsValidIndex(0)) // for some reason the segment do not have valid points, prevent possible crash, by simply not rendering this segment
						continue;

					FLandscapeSplineInterpPoint OldPoint = Segment.Points[0];
					OldPoint.Center       = MyLocalToWorld.TransformPosition(OldPoint.Center);
					OldPoint.Left         = MyLocalToWorld.TransformPosition(OldPoint.Left);
					OldPoint.Right        = MyLocalToWorld.TransformPosition(OldPoint.Right);
					OldPoint.FalloffLeft  = MyLocalToWorld.TransformPosition(OldPoint.FalloffLeft);
					OldPoint.FalloffRight = MyLocalToWorld.TransformPosition(OldPoint.FalloffRight);
					for (int32 i = 1; i < Segment.Points.Num(); i++)
					{
						FLandscapeSplineInterpPoint NewPoint = Segment.Points[i];
						NewPoint.Center       = MyLocalToWorld.TransformPosition(NewPoint.Center);
						NewPoint.Left         = MyLocalToWorld.TransformPosition(NewPoint.Left);
						NewPoint.Right        = MyLocalToWorld.TransformPosition(NewPoint.Right);
						NewPoint.FalloffLeft  = MyLocalToWorld.TransformPosition(NewPoint.FalloffLeft);
						NewPoint.FalloffRight = MyLocalToWorld.TransformPosition(NewPoint.FalloffRight);

						// Draw lines from the last keypoint.
						PDI->SetHitProxy(Segment.HitProxy);

						// center line
						PDI->DrawLine(OldPoint.Center, NewPoint.Center, SegmentColor, GetDepthPriorityGroup(View), 0.0f, DepthBias);

						// draw sides
						PDI->DrawLine(OldPoint.Left, NewPoint.Left, SegmentColor, GetDepthPriorityGroup(View), 0.0f, DepthBias);
						PDI->DrawLine(OldPoint.Right, NewPoint.Right, SegmentColor, GetDepthPriorityGroup(View), 0.0f, DepthBias);

						PDI->SetHitProxy(nullptr);

						// draw falloff sides
						if (bDrawFalloff)
						{
							DrawDashedLine(PDI, OldPoint.FalloffLeft, NewPoint.FalloffLeft, SegmentColor, 100, GetDepthPriorityGroup(View), DepthBias);
							DrawDashedLine(PDI, OldPoint.FalloffRight, NewPoint.FalloffRight, SegmentColor, 100, GetDepthPriorityGroup(View), DepthBias);
						}

						OldPoint = NewPoint;
					}
				}

				for (const FControlPointProxy& ControlPoint : ControlPoints)
				{
					const FVector ControlPointLocation = MyLocalToWorld.TransformPosition(ControlPoint.Location);

					// Draw Sprite
					if (bDrawControlPointSprite)
					{
						const float ControlPointSpriteScale = MyLocalToWorld.GetScaleVector().X * ControlPoint.SpriteScale;
						const FVector ControlPointSpriteLocation = ControlPointLocation + FVector(0, 0, ControlPointSpriteScale * 0.75f);
						const FLinearColor ControlPointSpriteColor = ControlPoint.bSelected ? SelectedControlPointSpriteColor : FLinearColor::White;

						PDI->SetHitProxy(ControlPoint.HitProxy);

						PDI->DrawSprite(
							ControlPointSpriteLocation,
							ControlPointSpriteScale,
							ControlPointSpriteScale,
							ControlPointSprite->Resource,
							ControlPointSpriteColor,
							GetDepthPriorityGroup(View),
							0, ControlPointSprite->Resource->GetSizeX(),
							0, ControlPointSprite->Resource->GetSizeY(),
							SE_BLEND_Masked);
					}


					// Draw Lines
					const FLinearColor ControlPointColor = ControlPoint.bSelected ? SelectedSplineColor : SplineColor;

					if (ControlPoint.Points.Num() == 1)
					{
						FLandscapeSplineInterpPoint NewPoint = ControlPoint.Points[0];
						NewPoint.Center = MyLocalToWorld.TransformPosition(NewPoint.Center);
						NewPoint.Left   = MyLocalToWorld.TransformPosition(NewPoint.Left);
						NewPoint.Right  = MyLocalToWorld.TransformPosition(NewPoint.Right);
						NewPoint.FalloffLeft  = MyLocalToWorld.TransformPosition(NewPoint.FalloffLeft);
						NewPoint.FalloffRight = MyLocalToWorld.TransformPosition(NewPoint.FalloffRight);

						// draw end for spline connection
						PDI->DrawPoint(NewPoint.Center, ControlPointColor, 6.0f, GetDepthPriorityGroup(View));
						PDI->DrawLine(NewPoint.Left, NewPoint.Center, ControlPointColor, GetDepthPriorityGroup(View), 0.0f, DepthBias);
						PDI->DrawLine(NewPoint.Right, NewPoint.Center, ControlPointColor, GetDepthPriorityGroup(View), 0.0f, DepthBias);
						if (bDrawFalloff)
						{
							DrawDashedLine(PDI, NewPoint.FalloffLeft, NewPoint.Left, ControlPointColor, 100, GetDepthPriorityGroup(View), DepthBias);
							DrawDashedLine(PDI, NewPoint.FalloffRight, NewPoint.Right, ControlPointColor, 100, GetDepthPriorityGroup(View), DepthBias);
						}
					}
					else if (ControlPoint.Points.Num() >= 2)
					{
						FLandscapeSplineInterpPoint OldPoint = ControlPoint.Points.Last();
						//OldPoint.Left   = MyLocalToWorld.TransformPosition(OldPoint.Left);
						OldPoint.Right  = MyLocalToWorld.TransformPosition(OldPoint.Right);
						//OldPoint.FalloffLeft  = MyLocalToWorld.TransformPosition(OldPoint.FalloffLeft);
						OldPoint.FalloffRight = MyLocalToWorld.TransformPosition(OldPoint.FalloffRight);

						for (const FLandscapeSplineInterpPoint& Point : ControlPoint.Points)
						{
							FLandscapeSplineInterpPoint NewPoint = Point;
							NewPoint.Center = MyLocalToWorld.TransformPosition(NewPoint.Center);
							NewPoint.Left   = MyLocalToWorld.TransformPosition(NewPoint.Left);
							NewPoint.Right  = MyLocalToWorld.TransformPosition(NewPoint.Right);
							NewPoint.FalloffLeft  = MyLocalToWorld.TransformPosition(NewPoint.FalloffLeft);
							NewPoint.FalloffRight = MyLocalToWorld.TransformPosition(NewPoint.FalloffRight);

							PDI->SetHitProxy(ControlPoint.HitProxy);

							// center line
							PDI->DrawLine(ControlPointLocation, NewPoint.Center, ControlPointColor, GetDepthPriorityGroup(View), 0.0f, DepthBias);

							// draw sides
							PDI->DrawLine(OldPoint.Right, NewPoint.Left, ControlPointColor, GetDepthPriorityGroup(View), 0.0f, DepthBias);

							PDI->SetHitProxy(nullptr);

							// draw falloff sides
							if (bDrawFalloff)
							{
								DrawDashedLine(PDI, OldPoint.FalloffRight, NewPoint.FalloffLeft, ControlPointColor, 100, GetDepthPriorityGroup(View), DepthBias);
							}

							// draw end for spline connection
							PDI->DrawPoint(NewPoint.Center, ControlPointColor, 6.0f, GetDepthPriorityGroup(View));
							PDI->DrawLine(NewPoint.Left, NewPoint.Center, ControlPointColor, GetDepthPriorityGroup(View), 0.0f, DepthBias);
							PDI->DrawLine(NewPoint.Right, NewPoint.Center, ControlPointColor, GetDepthPriorityGroup(View), 0.0f, DepthBias);
							if (bDrawFalloff)
							{
								DrawDashedLine(PDI, NewPoint.FalloffLeft, NewPoint.Left, ControlPointColor, 100, GetDepthPriorityGroup(View), DepthBias);
								DrawDashedLine(PDI, NewPoint.FalloffRight, NewPoint.Right, ControlPointColor, 100, GetDepthPriorityGroup(View), DepthBias);
							}

							//OldPoint = NewPoint;
							OldPoint.Right = NewPoint.Right;
							OldPoint.FalloffRight = NewPoint.FalloffRight;
						}
					}
				}

				PDI->SetHitProxy(nullptr);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.Splines;
		Result.bDynamicRelevance = true;
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}
	uint32 GetAllocatedSize() const
	{
		uint32 AllocatedSize = FPrimitiveSceneProxy::GetAllocatedSize() + Segments.GetAllocatedSize() + ControlPoints.GetAllocatedSize();
		for (const FSegmentProxy& Segment : Segments)
		{
			AllocatedSize += Segment.Points.GetAllocatedSize();
		}
		for (const FControlPointProxy& ControlPoint : ControlPoints)
		{
			AllocatedSize += ControlPoint.Points.GetAllocatedSize();
		}
		return AllocatedSize;
	}
};
#endif

//////////////////////////////////////////////////////////////////////////
// SPLINE COMPONENT

ULandscapeSplinesComponent::ULandscapeSplinesComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	SplineResolution = 512;
	SplineColor = FColor(0, 192, 48);

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinder<UTexture2D> SpriteTexture;
			ConstructorHelpers::FObjectFinder<UStaticMesh> SplineEditorMesh;
			FConstructorStatics()
				: SpriteTexture(TEXT("/Engine/EditorResources/S_Terrain.S_Terrain"))
				, SplineEditorMesh(TEXT("/Engine/EditorLandscapeResources/SplineEditorMesh"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		ControlPointSprite = ConstructorStatics.SpriteTexture.Object;
		SplineEditorMesh = ConstructorStatics.SplineEditorMesh.Object;
	}
#endif
	//RelativeScale3D = FVector(1/100.0f, 1/100.0f, 1/100.0f); // cancel out landscape scale. The scale is set up when component is created, but for a default landscape it's this
}

void ULandscapeSplinesComponent::CheckSplinesValid()
{
#if DO_CHECK
	// This shouldn't happen, but it has somehow (TTP #334549) so we have to fix it
	ensure(!ControlPoints.Contains(nullptr));
	ensure(!Segments.Contains(nullptr));

	// Remove all null control points/segments
	ControlPoints.Remove(nullptr);
	Segments.Remove(nullptr);

	// Check for cross-spline connections, as this is a potential source of nulls
	// this may be allowed in future, but is not currently
	for (ULandscapeSplineControlPoint* ControlPoint : ControlPoints)
	{
		ensure(ControlPoint->GetOuterULandscapeSplinesComponent() == this);
		for (const FLandscapeSplineConnection& Connection : ControlPoint->ConnectedSegments)
		{
			ensure(Connection.Segment->GetOuterULandscapeSplinesComponent() == this);
		}
	}
	for (ULandscapeSplineSegment* Segment : Segments)
	{
		ensure(Segment->GetOuterULandscapeSplinesComponent() == this);
		for (const FLandscapeSplineSegmentConnection& Connection : Segment->Connections)
		{
			ensure(Connection.ControlPoint->GetOuterULandscapeSplinesComponent() == this);
		}
	}
#endif
}

void ULandscapeSplinesComponent::OnRegister()
{
	CheckSplinesValid();

	Super::OnRegister();
}

#if WITH_EDITOR
FPrimitiveSceneProxy* ULandscapeSplinesComponent::CreateSceneProxy()
{
	CheckSplinesValid();

	return new FLandscapeSplinesSceneProxy(this);
}
#endif

FBoxSphereBounds ULandscapeSplinesComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox NewBoundsCalc(ForceInit);

	for (ULandscapeSplineControlPoint* ControlPoint : ControlPoints)
	{
		// TTP #334549: Somehow we're getting nulls in the ControlPoints array
		if (ControlPoint)
		{
			NewBoundsCalc += ControlPoint->GetBounds();
		}
	}

	for (ULandscapeSplineSegment* Segment : Segments)
	{
		if (Segment)
		{
			NewBoundsCalc += Segment->GetBounds();
		}
	}

	FBoxSphereBounds NewBounds;
	if (NewBoundsCalc.IsValid)
	{
		NewBoundsCalc = NewBoundsCalc.TransformBy(LocalToWorld);
		NewBounds = FBoxSphereBounds(NewBoundsCalc);
	}
	else
	{
		// There's no such thing as an "invalid" FBoxSphereBounds (unlike FBox)
		// try to return something that won't modify the parent bounds
		if (GetAttachParent())
		{
			NewBounds = FBoxSphereBounds(GetAttachParent()->Bounds.Origin, FVector::ZeroVector, 0.0f);
		}
		else
		{
			NewBounds = FBoxSphereBounds(LocalToWorld.GetTranslation(), FVector::ZeroVector, 0.0f);
		}
	}
	return NewBounds;
}

bool ULandscapeSplinesComponent::ModifySplines(bool bAlwaysMarkDirty /*= true*/)
{
	bool bSavedToTransactionBuffer = Modify(bAlwaysMarkDirty);

	for (ULandscapeSplineControlPoint* ControlPoint : ControlPoints)
	{
		bSavedToTransactionBuffer = ControlPoint->Modify(bAlwaysMarkDirty) || bSavedToTransactionBuffer;
	}
	for (ULandscapeSplineSegment* Segment : Segments)
	{
		bSavedToTransactionBuffer = Segment->Modify(bAlwaysMarkDirty) || bSavedToTransactionBuffer;
	}

	return bSavedToTransactionBuffer;
}

#if WITH_EDITORONLY_DATA
// legacy ForeignWorldSplineDataMap serialization
FArchive& operator<<(FArchive& Ar, FForeignSplineSegmentData& Value)
{
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << Value.ModificationKey << Value.MeshComponents;
	}
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FForeignWorldSplineData& Value)
{
	if (!Ar.IsFilterEditorOnly())
	{
		// note: ForeignControlPointDataMap is missing in legacy serialization
		Ar << Value.ForeignSplineSegmentDataMap_DEPRECATED;
	}
	return Ar;
}
#endif

void ULandscapeSplinesComponent::Serialize(FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	// Cooking is a save-time operation, so has to be done before Super::Serialize
	if (Ar.IsCooking())
	{
		CookedForeignMeshComponents.Reset();

		for (const auto& ForeignWorldSplineDataPair : ForeignWorldSplineDataMap)
		{
			const auto& ForeignWorldSplineData = ForeignWorldSplineDataPair.Value;

			for (const auto& ForeignControlPointData : ForeignWorldSplineData.ForeignControlPointData)
			{
				CookedForeignMeshComponents.Add(ForeignControlPointData.MeshComponent);
			}

			for (const auto& ForeignSplineSegmentData : ForeignWorldSplineData.ForeignSplineSegmentData)
			{
				CookedForeignMeshComponents.Append(ForeignSplineSegmentData.MeshComponents);
			}
		}
	}
#endif

	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (Ar.UE4Ver() >= VER_UE4_LANDSCAPE_SPLINE_CROSS_LEVEL_MESHES &&
		!Ar.IsFilterEditorOnly())
	{
		Ar.UsingCustomVersion(FLandscapeCustomVersion::GUID);

		if (Ar.CustomVer(FLandscapeCustomVersion::GUID) < FLandscapeCustomVersion::NewSplineCrossLevelMeshSerialization)
		{
			Ar << ForeignWorldSplineDataMap;
		}

		if (Ar.IsLoading() && Ar.CustomVer(FLandscapeCustomVersion::GUID) < FLandscapeCustomVersion::SplineForeignDataLazyObjectPtrFix)
		{
			for (auto& SplineData : ForeignWorldSplineDataMap)
			{
				for (auto& ControlPoint : SplineData.Value.ForeignControlPointDataMap_DEPRECATED)
				{
					ControlPoint.Value.Identifier = ControlPoint.Key;
					SplineData.Value.ForeignControlPointData.Add(ControlPoint.Value);
				}

				SplineData.Value.ForeignControlPointDataMap_DEPRECATED.Empty();

				for (auto& SegmentData : SplineData.Value.ForeignSplineSegmentDataMap_DEPRECATED)
				{
					SegmentData.Value.Identifier = SegmentData.Key;
					SplineData.Value.ForeignSplineSegmentData.Add(SegmentData.Value);
				}

				SplineData.Value.ForeignSplineSegmentDataMap_DEPRECATED.Empty();
			}
		}
	}

	if (!Ar.IsPersistent())
	{
		Ar << MeshComponentLocalOwnersMap;
		Ar << MeshComponentForeignOwnersMap;
	}
#endif
}

#if WITH_EDITOR
void ULandscapeSplinesComponent::AutoFixMeshComponentErrors(UWorld* OtherWorld)
{
	UWorld* ThisOuterWorld = GetTypedOuter<UWorld>();

	TSoftObjectPtr<UWorld> OtherWorldSoftPtr = OtherWorld;
	ULandscapeSplinesComponent* StreamingSplinesComponent = GetStreamingSplinesComponentForLevel(OtherWorld->PersistentLevel);
	auto* ForeignWorldSplineData = StreamingSplinesComponent ? StreamingSplinesComponent->ForeignWorldSplineDataMap.Find(ThisOuterWorld) : nullptr;

	// Fix control point meshes
	for (ULandscapeSplineControlPoint* ControlPoint : ControlPoints)
	{
		if (ControlPoint->GetForeignWorld() == OtherWorld)
		{
			auto* ForeignControlPointData = ForeignWorldSplineData ? ForeignWorldSplineData->FindControlPoint(ControlPoint) : nullptr;
			if (!ForeignControlPointData || ForeignControlPointData->ModificationKey != ControlPoint->GetModificationKey())
			{
				// We don't pass true for update segments to avoid them being updated multiple times
				ControlPoint->UpdateSplinePoints(true, false);
			}
		}
	}

	// Fix spline segment meshes
	for (ULandscapeSplineSegment* Segment : Segments)
	{
		if (Segment->GetForeignWorlds().Contains(OtherWorld))
		{
			auto* ForeignSplineSegmentData = ForeignWorldSplineData ? ForeignWorldSplineData->FindSegmentData(Segment) : nullptr;
			if (!ForeignSplineSegmentData || ForeignSplineSegmentData->ModificationKey != Segment->GetModificationKey())
			{
				Segment->UpdateSplinePoints(true);
			}
		}
	}

	if (StreamingSplinesComponent)
	{
		StreamingSplinesComponent->DestroyOrphanedForeignMeshComponents(ThisOuterWorld);
	}
}

void ULandscapeSplinesComponent::CheckForErrors()
{
	Super::CheckForErrors();

	UWorld* ThisOuterWorld = GetTypedOuter<UWorld>();
	check(ThisOuterWorld->WorldType == EWorldType::Editor);

	TSet<UWorld*> OutdatedWorlds;
	TMap<UWorld*, FForeignWorldSplineData*> ForeignWorldSplineDataMapCache;

	// Check control point meshes
	for (ULandscapeSplineControlPoint* ControlPoint : ControlPoints)
	{
		UWorld* ForeignWorld = ControlPoint->GetForeignWorld().Get();
		if (ForeignWorld && !OutdatedWorlds.Contains(ForeignWorld))
		{
			auto** ForeignWorldSplineDataCachedPtr = ForeignWorldSplineDataMapCache.Find(ForeignWorld);
			auto* ForeignWorldSplineData = ForeignWorldSplineDataCachedPtr ? *ForeignWorldSplineDataCachedPtr : nullptr;
			if (!ForeignWorldSplineDataCachedPtr)
			{
				ULandscapeSplinesComponent* StreamingSplinesComponent = GetStreamingSplinesComponentForLevel(ForeignWorld->PersistentLevel);
				ForeignWorldSplineData = StreamingSplinesComponent ? StreamingSplinesComponent->ForeignWorldSplineDataMap.Find(ThisOuterWorld) : nullptr;
				ForeignWorldSplineDataMapCache.Add(ForeignWorld, ForeignWorldSplineData);
			}
			auto* ForeignControlPointData = ForeignWorldSplineData ? ForeignWorldSplineData->FindControlPoint(ControlPoint) : nullptr;
			if (!ForeignControlPointData || ForeignControlPointData->ModificationKey != ControlPoint->GetModificationKey())
			{
				OutdatedWorlds.Add(ForeignWorld);
			}
		}
	}

	// Check spline segment meshes
	for (ULandscapeSplineSegment* Segment : Segments)
	{
		for (auto& ForeignWorldSoftPtr : Segment->GetForeignWorlds())
		{
			UWorld* ForeignWorld = ForeignWorldSoftPtr.Get();

			if (ForeignWorld && !OutdatedWorlds.Contains(ForeignWorld))
			{
				auto** ForeignWorldSplineDataCachedPtr = ForeignWorldSplineDataMapCache.Find(ForeignWorld);
				auto* ForeignWorldSplineData = ForeignWorldSplineDataCachedPtr ? *ForeignWorldSplineDataCachedPtr : nullptr;
				if (!ForeignWorldSplineDataCachedPtr)
				{
					ULandscapeSplinesComponent* StreamingSplinesComponent = GetStreamingSplinesComponentForLevel(ForeignWorld->PersistentLevel);
					ForeignWorldSplineData = StreamingSplinesComponent ? StreamingSplinesComponent->ForeignWorldSplineDataMap.Find(ThisOuterWorld) : nullptr;
					ForeignWorldSplineDataMapCache.Add(ForeignWorld, ForeignWorldSplineData);
				}
				auto* ForeignSplineSegmentData = ForeignWorldSplineData ? ForeignWorldSplineData->FindSegmentData(Segment) : nullptr;
				if (!ForeignSplineSegmentData || ForeignSplineSegmentData->ModificationKey != Segment->GetModificationKey())
				{
					OutdatedWorlds.Add(ForeignWorld);
				}
			}
		}
	}
	ForeignWorldSplineDataMapCache.Empty();

	for (UWorld* OutdatedWorld : OutdatedWorlds)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("MeshMap"), FText::FromName(OutdatedWorld->GetFName()));
		Arguments.Add(TEXT("SplineMap"), FText::FromName(ThisOuterWorld->GetFName()));

		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(GetOwner()))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_MeshesOutDated", "Meshes in {MeshMap} out of date compared to landscape spline in {SplineMap}"), Arguments)))
			->AddToken(FActionToken::Create(LOCTEXT("MapCheck_ActionName_MeshesOutDated", "Rebuild landscape splines"), FText(),
			FOnActionTokenExecuted::CreateUObject(this, &ULandscapeSplinesComponent::AutoFixMeshComponentErrors, OutdatedWorld), true));
	}

	// check for orphaned components
	for (auto& ForeignWorldSplineDataPair : ForeignWorldSplineDataMap)
	{
		auto& ForeignWorldSoftPtr = ForeignWorldSplineDataPair.Key;
		auto& ForeignWorldSplineData = ForeignWorldSplineDataPair.Value;

		// World is not loaded
		if (ForeignWorldSoftPtr.IsPending())
		{
			continue;
		}

		UWorld* ForeignWorld = ForeignWorldSoftPtr.Get();
		for (auto& ForeignSplineSegmentData : ForeignWorldSplineData.ForeignSplineSegmentData)
		{
			const ULandscapeSplineSegment* ForeignSplineSegment = ForeignSplineSegmentData.Identifier.Get();

			// No such segment or segment doesn't match our meshes
			if (!ForeignSplineSegment)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("MeshMap"),   FText::FromName(ThisOuterWorld->GetFName()));
				Arguments.Add(TEXT("SplineMap"), FText::FromName(ForeignWorld->GetFName()));

				FMessageLog("MapCheck").Error()
					->AddToken(FUObjectToken::Create(GetOwner()))
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_OrphanedMeshes", "{MeshMap} contains orphaned meshes due to mismatch with landscape splines in {SplineMap}"), Arguments)))
					->AddToken(FActionToken::Create(LOCTEXT("MapCheck_ActionName_OrphanedMeshes", "Clean up orphaned meshes"), FText(),
					FOnActionTokenExecuted::CreateUObject(this, &ULandscapeSplinesComponent::DestroyOrphanedForeignMeshComponents, ForeignWorld), true));

				break;
			}
		}
	}
}
#endif

void ULandscapeSplinesComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (GIsEditor && GetWorld()->WorldType == EWorldType::Editor)
	{
		// Build MeshComponentForeignOwnersMap (Component->Spline) from ForeignWorldSplineDataMap (World->Spline->Component)
		for (auto& ForeignWorldSplineDataPair : ForeignWorldSplineDataMap)
		{
			auto& ForeignWorld = ForeignWorldSplineDataPair.Key;
			auto& ForeignWorldSplineData = ForeignWorldSplineDataPair.Value;

			for (auto& ForeignControlPointData : ForeignWorldSplineData.ForeignControlPointData)
			{
				MeshComponentForeignOwnersMap.Add(ForeignControlPointData.MeshComponent, ForeignControlPointData.Identifier);
			}

			for (auto& ForeignSplineSegmentData : ForeignWorldSplineData.ForeignSplineSegmentData)
			{
				for (auto* MeshComponent : ForeignSplineSegmentData.MeshComponents)
				{
					MeshComponentForeignOwnersMap.Add(MeshComponent, ForeignSplineSegmentData.Identifier);
				}
			}
		}
	}
#endif

	CheckSplinesValid();

#if WITH_EDITOR
	if (GIsEditor && GetWorld()->WorldType == EWorldType::Editor)
	{
		CheckForErrors();
	}
#endif
}

#if WITH_EDITOR
static bool bHackIsUndoingSplines = false;
void ULandscapeSplinesComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Don't update splines when undoing, not only is it unnecessary and expensive,
	// it also causes failed asserts in debug builds when trying to register components
	// (because the actor hasn't reset its OwnedComponents array yet)
	if (!bHackIsUndoingSplines)
	{
		const bool bUpdateCollision = PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive;
		RebuildAllSplines(bUpdateCollision);
	}
}
void ULandscapeSplinesComponent::PostEditUndo()
{
	bHackIsUndoingSplines = true;
	Super::PostEditUndo();
	bHackIsUndoingSplines = false;

	MarkRenderStateDirty();
}

void ULandscapeSplinesComponent::RebuildAllSplines(bool bUpdateCollision)
{
	for (ULandscapeSplineControlPoint* ControlPoint : ControlPoints)
	{
		ControlPoint->UpdateSplinePoints(true, false);
	}

	for (ULandscapeSplineSegment* Segment : Segments)
	{
		Segment->UpdateSplinePoints(true);
	}
}

void ULandscapeSplinesComponent::ShowSplineEditorMesh(bool bShow)
{
	bShowSplineEditorMesh = bShow;

	for (ULandscapeSplineSegment* Segment : Segments)
	{
		Segment->UpdateSplineEditorMesh();
	}

	MarkRenderStateDirty();
}

bool FForeignWorldSplineData::IsEmpty()
{
	return ForeignControlPointData.Num() == 0 && ForeignSplineSegmentData.Num() == 0;
}

FForeignControlPointData* FForeignWorldSplineData::FindControlPoint(ULandscapeSplineControlPoint* InIdentifer)
{
	for (auto& ControlPoint : ForeignControlPointData)
	{
		if (ControlPoint.Identifier == InIdentifer)
		{
			return &ControlPoint;
		}
	}

	return nullptr;
}

FForeignSplineSegmentData* FForeignWorldSplineData::FindSegmentData(ULandscapeSplineSegment* InIdentifer)
{
	for (auto& SegmentData : ForeignSplineSegmentData)
	{
		if (SegmentData.Identifier == InIdentifer)
		{
			return &SegmentData;
		}
	}

	return nullptr;
}

ULandscapeSplinesComponent* ULandscapeSplinesComponent::GetStreamingSplinesComponentByLocation(const FVector& LocalLocation, bool bCreate /* = true*/)
{
	ALandscapeProxy* OuterLandscape = Cast<ALandscapeProxy>(GetOwner());
	if (OuterLandscape &&
		// when copy/pasting this can get called with a null guid on the parent landscape
		// this is fine, we won't have any cross-level meshes in this case anyway
		OuterLandscape->GetLandscapeGuid().IsValid())
	{
		FVector LandscapeLocalLocation = GetComponentTransform().GetRelativeTransform(OuterLandscape->LandscapeActorToWorld()).TransformPosition(LocalLocation);
		const int32 ComponentIndexX = (LandscapeLocalLocation.X >= 0.0f) ? FMath::FloorToInt(LandscapeLocalLocation.X / OuterLandscape->ComponentSizeQuads) : FMath::CeilToInt(LandscapeLocalLocation.X / OuterLandscape->ComponentSizeQuads);
		const int32 ComponentIndexY = (LandscapeLocalLocation.Y >= 0.0f) ? FMath::FloorToInt(LandscapeLocalLocation.Y / OuterLandscape->ComponentSizeQuads) : FMath::CeilToInt(LandscapeLocalLocation.Y / OuterLandscape->ComponentSizeQuads);
		ULandscapeComponent* LandscapeComponent = OuterLandscape->GetLandscapeInfo()->XYtoComponentMap.FindRef(FIntPoint(ComponentIndexX, ComponentIndexY));
		if (LandscapeComponent)
		{
			ALandscapeProxy* ComponentLandscapeProxy = LandscapeComponent->GetLandscapeProxy();
			if (!ComponentLandscapeProxy->SplineComponent && bCreate)
			{
				ComponentLandscapeProxy->Modify();
				ComponentLandscapeProxy->SplineComponent = NewObject<ULandscapeSplinesComponent>(ComponentLandscapeProxy, NAME_None, RF_Transactional);
				ComponentLandscapeProxy->SplineComponent->RelativeScale3D = RelativeScale3D;
				ComponentLandscapeProxy->SplineComponent->AttachToComponent(ComponentLandscapeProxy->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			}
			if (ComponentLandscapeProxy->SplineComponent)
			{
				return ComponentLandscapeProxy->SplineComponent;
			}
		}
	}

	return this;
}

ULandscapeSplinesComponent* ULandscapeSplinesComponent::GetStreamingSplinesComponentForLevel(ULevel* Level, bool bCreate /* = true*/)
{
	ALandscapeProxy* OuterLandscape = Cast<ALandscapeProxy>(GetOwner());
	if (OuterLandscape)
	{
		ULandscapeInfo* LandscapeInfo = OuterLandscape->GetLandscapeInfo();
		check(LandscapeInfo);

		ALandscapeProxy* Proxy = LandscapeInfo->GetLandscapeProxyForLevel(Level);
		if (Proxy)
		{
			if (!Proxy->SplineComponent && bCreate)
			{
				Proxy->Modify();
				Proxy->SplineComponent = NewObject<ULandscapeSplinesComponent>(Proxy, NAME_None, RF_Transactional);
				Proxy->SplineComponent->RelativeScale3D = RelativeScale3D;
				Proxy->SplineComponent->AttachToComponent(Proxy->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			}
			return Proxy->SplineComponent;
		}
	}

	return nullptr;
}

TArray<ULandscapeSplinesComponent*> ULandscapeSplinesComponent::GetAllStreamingSplinesComponents()
{
	ALandscapeProxy* OuterLandscape = Cast<ALandscapeProxy>(GetOwner());
	if (OuterLandscape &&
		// when copy/pasting this can get called with a null guid on the parent landscape
		// this is fine, we won't have any cross-level meshes in this case anyway
		OuterLandscape->GetLandscapeGuid().IsValid())
	{
		ULandscapeInfo* LandscapeInfo = OuterLandscape->GetLandscapeInfo();

		if (LandscapeInfo)
		{
			TArray<ULandscapeSplinesComponent*> SplinesComponents;
			LandscapeInfo->ForAllLandscapeProxies([&SplinesComponents](ALandscapeProxy* Proxy)
			{
				if (Proxy->SplineComponent)
				{
					SplinesComponents.Add(Proxy->SplineComponent);
				}
			});
			return SplinesComponents;
		}
	}

	return {};
}

void ULandscapeSplinesComponent::UpdateModificationKey(ULandscapeSplineSegment* Owner)
{
	UWorld* OwnerWorld = Owner->GetTypedOuter<UWorld>();
	checkSlow(OwnerWorld != GetTypedOuter<UWorld>());

	auto* ForeignWorldSplineData = ForeignWorldSplineDataMap.Find(OwnerWorld);
	checkSlow(ForeignWorldSplineData);

	if (ForeignWorldSplineData)
	{
		auto* ForeignSplineSegmentData = ForeignWorldSplineData->FindSegmentData(Owner);
		
		if (ForeignSplineSegmentData != nullptr)
		{
			ForeignSplineSegmentData->ModificationKey = Owner->GetModificationKey();
		}
	}
}

void ULandscapeSplinesComponent::UpdateModificationKey(ULandscapeSplineControlPoint* Owner)
{
	UWorld* OwnerWorld = Owner->GetTypedOuter<UWorld>();
	checkSlow(OwnerWorld != GetTypedOuter<UWorld>());

	auto* ForeignWorldSplineData = ForeignWorldSplineDataMap.Find(OwnerWorld);
	checkSlow(ForeignWorldSplineData);

	if (ForeignWorldSplineData)
	{
		auto* ForeignControlPointData = ForeignWorldSplineData->FindControlPoint(Owner);
		
		if (ForeignControlPointData != nullptr)
		{
			ForeignControlPointData->ModificationKey = Owner->GetModificationKey();
		}
	}
}

void ULandscapeSplinesComponent::AddForeignMeshComponent(ULandscapeSplineSegment* Owner, USplineMeshComponent* Component)
{
	UWorld* OwnerWorld = Owner->GetTypedOuter<UWorld>();

#if DO_GUARD_SLOW
	UWorld* ThisOuterWorld = GetTypedOuter<UWorld>();
	UWorld* ComponentOuterWorld = Component->GetTypedOuter<UWorld>();
	checkSlow(ComponentOuterWorld == ThisOuterWorld);
	checkSlow(OwnerWorld != ThisOuterWorld);
#endif

	auto& ForeignWorldSplineData = ForeignWorldSplineDataMap.FindOrAdd(OwnerWorld);
	FForeignSplineSegmentData* ForeignSplineSegmentData = ForeignWorldSplineData.FindSegmentData(Owner);

	if (ForeignSplineSegmentData == nullptr)
	{
		int32 AddedIndex = ForeignWorldSplineData.ForeignSplineSegmentData.Add(FForeignSplineSegmentData());
		ForeignSplineSegmentData = &ForeignWorldSplineData.ForeignSplineSegmentData[AddedIndex];
	}

	ForeignSplineSegmentData->MeshComponents.Add(Component);
	ForeignSplineSegmentData->ModificationKey = Owner->GetModificationKey();
	ForeignSplineSegmentData->Identifier = Owner;

	MeshComponentForeignOwnersMap.Add(Component, Owner);
}

void ULandscapeSplinesComponent::RemoveForeignMeshComponent(ULandscapeSplineSegment* Owner, USplineMeshComponent* Component)
{
	UWorld* OwnerWorld = Owner->GetTypedOuter<UWorld>();

#if DO_GUARD_SLOW
	UWorld* ThisOuterWorld = GetTypedOuter<UWorld>();
	UWorld* ComponentOuterWorld = Component->GetTypedOuter<UWorld>();
	checkSlow(ComponentOuterWorld == ThisOuterWorld);
	checkSlow(OwnerWorld != ThisOuterWorld);
#endif

	auto* ForeignWorldSplineData = ForeignWorldSplineDataMap.Find(OwnerWorld);
	checkSlow(ForeignWorldSplineData);
	checkSlow(MeshComponentForeignOwnersMap.FindRef(Component) == Owner);
	verifySlow(MeshComponentForeignOwnersMap.Remove(Component) == 1);

	if (ForeignWorldSplineData)
	{
		FForeignSplineSegmentData* SegmentData = ForeignWorldSplineData->FindSegmentData(Owner);
		verifySlow(SegmentData->MeshComponents.RemoveSingle(Component) == 1);
		if (SegmentData->MeshComponents.Num() == 0)
		{
			verifySlow(ForeignWorldSplineData->ForeignSplineSegmentData.RemoveSingle(*SegmentData) == 1);

			if (ForeignWorldSplineData->IsEmpty())
			{
				verifySlow(ForeignWorldSplineDataMap.Remove(OwnerWorld) == 1);
			}
		}
		else
		{
			SegmentData->ModificationKey = Owner->GetModificationKey();
		}
	}
}

void ULandscapeSplinesComponent::RemoveAllForeignMeshComponents(ULandscapeSplineSegment* Owner)
{
	UWorld* OwnerWorld = Owner->GetTypedOuter<UWorld>();
	checkSlow(OwnerWorld != GetTypedOuter<UWorld>());

	auto* ForeignWorldSplineData = ForeignWorldSplineDataMap.Find(OwnerWorld);
	checkSlow(ForeignWorldSplineData);

	if (ForeignWorldSplineData)
	{
		auto* ForeignSplineSegmentData = ForeignWorldSplineData->FindSegmentData(Owner);

		for (auto* MeshComponent : ForeignSplineSegmentData->MeshComponents)
		{
			checkSlow(MeshComponentForeignOwnersMap.FindRef(MeshComponent) == Owner);
			verifySlow(MeshComponentForeignOwnersMap.Remove(MeshComponent) == 1);
		}
		ForeignSplineSegmentData->MeshComponents.Empty();
		verifySlow(ForeignWorldSplineData->ForeignSplineSegmentData.RemoveSingle(*ForeignSplineSegmentData) == 1);
		if (ForeignWorldSplineData->IsEmpty())
		{
			verifySlow(ForeignWorldSplineDataMap.Remove(OwnerWorld) == 1);
		}
	}
}

void ULandscapeSplinesComponent::AddForeignMeshComponent(ULandscapeSplineControlPoint* Owner, UControlPointMeshComponent* Component)
{
	UWorld* OwnerWorld = Owner->GetTypedOuter<UWorld>();

#if DO_GUARD_SLOW
	UWorld* ThisOuterWorld = GetTypedOuter<UWorld>();
	UWorld* ComponentOuterWorld = Component->GetTypedOuter<UWorld>();
	checkSlow(ComponentOuterWorld == ThisOuterWorld);
	checkSlow(OwnerWorld != ThisOuterWorld);
#endif

	auto& ForeignWorldSplineData = ForeignWorldSplineDataMap.FindOrAdd(OwnerWorld);
	checkSlow(ForeignWorldSplineData.FindControlPoint(Owner) == nullptr);
	int32 AddedIndex = ForeignWorldSplineData.ForeignControlPointData.Add(FForeignControlPointData());
	auto& ForeignControlPointData = ForeignWorldSplineData.ForeignControlPointData[AddedIndex];

	ForeignControlPointData.MeshComponent = Component;
	ForeignControlPointData.ModificationKey = Owner->GetModificationKey();
	ForeignControlPointData.Identifier = Owner;

	MeshComponentForeignOwnersMap.Add(Component, Owner);
}

void ULandscapeSplinesComponent::RemoveForeignMeshComponent(ULandscapeSplineControlPoint* Owner, UControlPointMeshComponent* Component)
{
	UWorld* OwnerWorld = Owner->GetTypedOuter<UWorld>();

#if DO_GUARD_SLOW
	UWorld* ThisOuterWorld = GetTypedOuter<UWorld>();
	UWorld* ComponentOuterWorld = Component->GetTypedOuter<UWorld>();
	checkSlow(ComponentOuterWorld == ThisOuterWorld);
	checkSlow(OwnerWorld != ThisOuterWorld);
#endif

	auto* ForeignWorldSplineData = ForeignWorldSplineDataMap.Find(OwnerWorld);
	checkSlow(ForeignWorldSplineData);
	checkSlow(MeshComponentForeignOwnersMap.FindRef(Component) == Owner);
	verifySlow(MeshComponentForeignOwnersMap.Remove(Component) == 1);

	if (ForeignWorldSplineData)
	{
		auto* ForeignControlPointData = ForeignWorldSplineData->FindControlPoint(Owner);
		checkSlow(ForeignControlPointData);
		checkSlow(ForeignControlPointData->MeshComponent == Component);

		verifySlow(ForeignWorldSplineData->ForeignControlPointData.RemoveSingle(*ForeignControlPointData) == 1);
		if (ForeignWorldSplineData->IsEmpty())
		{
			verifySlow(ForeignWorldSplineDataMap.Remove(OwnerWorld) == 1);
		}
	}
}

void ULandscapeSplinesComponent::DestroyOrphanedForeignMeshComponents(UWorld* OwnerWorld)
{
	auto* ForeignWorldSplineData = ForeignWorldSplineDataMap.Find(OwnerWorld);

	if (ForeignWorldSplineData)
	{
		for (int32 i = ForeignWorldSplineData->ForeignSplineSegmentData.Num() - 1; i >= 0; --i)
		{
			FForeignSplineSegmentData& SegmentData = ForeignWorldSplineData->ForeignSplineSegmentData[i];
			const auto& ForeignSplineSegment = SegmentData.Identifier;

			if (!ForeignSplineSegment)
			{
				for (auto* MeshComponent : SegmentData.MeshComponents)
				{
					checkSlow(!MeshComponentForeignOwnersMap.FindRef(MeshComponent).IsValid());
					verifySlow(MeshComponentForeignOwnersMap.Remove(MeshComponent) == 1);
					MeshComponent->DestroyComponent();
				}
				SegmentData.MeshComponents.Empty();

				ForeignWorldSplineData->ForeignSplineSegmentData.RemoveSingle(SegmentData);
			}
		}

		if (ForeignWorldSplineData->IsEmpty())
		{
			verifySlow(ForeignWorldSplineDataMap.Remove(OwnerWorld) == 1);
		}
	}
}

UControlPointMeshComponent* ULandscapeSplinesComponent::GetForeignMeshComponent(ULandscapeSplineControlPoint* Owner)
{
	UWorld* OwnerWorld = Owner->GetTypedOuter<UWorld>();
	checkSlow(OwnerWorld != GetTypedOuter<UWorld>());

	FForeignWorldSplineData* ForeignWorldSplineData = ForeignWorldSplineDataMap.Find(OwnerWorld);
	if (ForeignWorldSplineData)
	{
		FForeignControlPointData* ForeignControlPointData = ForeignWorldSplineData->FindControlPoint(Owner);
		if (ForeignControlPointData)
		{
			return ForeignControlPointData->MeshComponent;
		}
	}

	return nullptr;
}

TArray<USplineMeshComponent*> ULandscapeSplinesComponent::GetForeignMeshComponents(ULandscapeSplineSegment* Owner)
{
	UWorld* OwnerWorld = Owner->GetTypedOuter<UWorld>();
	checkSlow(OwnerWorld != GetTypedOuter<UWorld>());

	auto* ForeignWorldSplineData = ForeignWorldSplineDataMap.Find(OwnerWorld);
	if (ForeignWorldSplineData)
	{
		auto* ForeignSplineSegmentData = ForeignWorldSplineData->FindSegmentData(Owner);
		if (ForeignSplineSegmentData)
		{
			return ForeignSplineSegmentData->MeshComponents;
		}
	}

	return {};
}

UObject* ULandscapeSplinesComponent::GetOwnerForMeshComponent(const UMeshComponent* SplineMeshComponent)
{
	UObject* LocalOwner = MeshComponentLocalOwnersMap.FindRef(SplineMeshComponent);
	if (LocalOwner)
	{
		return LocalOwner;
	}

	TLazyObjectPtr<UObject>* ForeignOwner = MeshComponentForeignOwnersMap.Find(SplineMeshComponent);
	if (ForeignOwner)
	{
		// this will be null if ForeignOwner isn't currently loaded
		return ForeignOwner->Get();
	}

	return nullptr;
}
#endif // WITH_EDITOR

//////////////////////////////////////////////////////////////////////////
// CONTROL POINT MESH COMPONENT

UControlPointMeshComponent::UControlPointMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	bSelected = false;
#endif
}

//////////////////////////////////////////////////////////////////////////
// SPLINE CONTROL POINT

ULandscapeSplineControlPoint::ULandscapeSplineControlPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Width = 1000;
	SideFalloff = 1000;
	EndFalloff = 2000;

#if WITH_EDITORONLY_DATA
	Mesh = nullptr;
	MeshScale = FVector(1);

	LDMaxDrawDistance = 0;
	TranslucencySortPriority = 0;

	LayerName = NAME_None;
	bRaiseTerrain = true;
	bLowerTerrain = true;

	LocalMeshComponent = nullptr;
	bPlaceSplineMeshesInStreamingLevels = true;
	bEnableCollision = true;
	bCastShadow = true;

	// transients
	bSelected = false;
#endif
}

void ULandscapeSplineControlPoint::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.UE4Ver() < VER_UE4_LANDSCAPE_SPLINE_CROSS_LEVEL_MESHES)
	{
		bPlaceSplineMeshesInStreamingLevels = false;
	}
#endif
}

void ULandscapeSplineControlPoint::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (GIsEditor)
	{
		if (LocalMeshComponent != nullptr)
		{
			ULandscapeSplinesComponent* OuterSplines = GetOuterULandscapeSplinesComponent();
			OuterSplines->MeshComponentLocalOwnersMap.Add(LocalMeshComponent, this);
		}
	}

	if (GetLinkerUE4Version() < VER_UE4_LANDSCAPE_SPLINE_CROSS_LEVEL_MESHES)
	{
		// Fix collision profile
		if (LocalMeshComponent != nullptr) // ForeignMeshComponents didn't exist yet
		{
			const FName CollisionProfile = bEnableCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName;
			if (LocalMeshComponent->GetCollisionProfileName() != CollisionProfile)
			{
				LocalMeshComponent->SetCollisionProfileName(CollisionProfile);
			}

			LocalMeshComponent->SetFlags(RF_TextExportTransient);
		}
	}
#endif
}

FLandscapeSplineSegmentConnection& FLandscapeSplineConnection::GetNearConnection() const
{
	return Segment->Connections[End];
}

FLandscapeSplineSegmentConnection& FLandscapeSplineConnection::GetFarConnection() const
{
	return Segment->Connections[1 - End];
}

#if WITH_EDITOR
FName ULandscapeSplineControlPoint::GetBestConnectionTo(FVector Destination) const
{
	FName BestSocket = NAME_None;
	float BestScore = -FLT_MAX;

	if (Mesh != nullptr)
	{
		for (const UStaticMeshSocket* Socket : Mesh->Sockets)
		{
			FTransform SocketTransform = FTransform(Socket->RelativeRotation, Socket->RelativeLocation) * FTransform(Rotation, Location, MeshScale);
			FVector SocketLocation = SocketTransform.GetTranslation();
			FRotator SocketRotation = SocketTransform.GetRotation().Rotator();

			float Score = (Destination - Location).Size() - (Destination - SocketLocation).Size(); // Score closer sockets higher
			Score *= FMath::Abs(FVector::DotProduct((Destination - SocketLocation), SocketRotation.Vector())); // score closer rotation higher

			if (Score > BestScore)
			{
				BestSocket = Socket->SocketName;
				BestScore = Score;
			}
		}
	}

	return BestSocket;
}

void ULandscapeSplineControlPoint::GetConnectionLocalLocationAndRotation(FName SocketName, OUT FVector& OutLocation, OUT FRotator& OutRotation) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;

	if (Mesh != nullptr)
	{
		const UStaticMeshSocket* Socket = Mesh->FindSocket(SocketName);
		if (Socket != nullptr)
		{
			OutLocation = Socket->RelativeLocation;
			OutRotation = Socket->RelativeRotation;
		}
	}
}

void ULandscapeSplineControlPoint::GetConnectionLocationAndRotation(FName SocketName, OUT FVector& OutLocation, OUT FRotator& OutRotation) const
{
	OutLocation = Location;
	OutRotation = Rotation;

	if (Mesh != nullptr)
	{
		const UStaticMeshSocket* Socket = Mesh->FindSocket(SocketName);
		if (Socket != nullptr)
		{
			FTransform SocketTransform = FTransform(Socket->RelativeRotation, Socket->RelativeLocation) * FTransform(Rotation, Location, MeshScale);
			OutLocation = SocketTransform.GetTranslation();
			OutRotation = SocketTransform.GetRotation().Rotator().GetNormalized();
		}
	}
}

void ULandscapeSplineControlPoint::SetSplineSelected(bool bInSelected)
{
	bSelected = bInSelected;
	GetOuterULandscapeSplinesComponent()->MarkRenderStateDirty();

	if (LocalMeshComponent != nullptr)
	{
		LocalMeshComponent->bSelected = bInSelected;
		LocalMeshComponent->PushSelectionToProxy();
	}

	auto ForeignMeshComponentsMap = GetForeignMeshComponents();
	for (auto& ForeignMeshComponentsPair : ForeignMeshComponentsMap)
	{
		ULandscapeSplinesComponent* MeshComponentOuterSplines = ForeignMeshComponentsPair.Key;
		auto* MeshComponent = ForeignMeshComponentsPair.Value;
		MeshComponent->bSelected = bInSelected;
		MeshComponent->PushSelectionToProxy();
	}
}

void ULandscapeSplineControlPoint::AutoCalcRotation()
{
	Modify();

	FRotator Delta = FRotator::ZeroRotator;

	for (const FLandscapeSplineConnection& Connection : ConnectedSegments)
	{
		// Get the start and end location/rotation of this connection
		FVector StartLocation; FRotator StartRotation;
		this->GetConnectionLocationAndRotation(Connection.GetNearConnection().SocketName, StartLocation, StartRotation);
		FVector StartLocalLocation; FRotator StartLocalRotation;
		this->GetConnectionLocalLocationAndRotation(Connection.GetNearConnection().SocketName, StartLocalLocation, StartLocalRotation);
		FVector EndLocation; FRotator EndRotation;
		Connection.GetFarConnection().ControlPoint->GetConnectionLocationAndRotation(Connection.GetFarConnection().SocketName, EndLocation, EndRotation);

		// Find the delta between the direction of the tangent at the connection point and
		// the direction to the other end's control point
		FQuat SocketLocalRotation = StartLocalRotation.Quaternion();
		if (FMath::Sign(Connection.GetNearConnection().TangentLen) < 0)
		{
			SocketLocalRotation = SocketLocalRotation * FRotator(0, 180, 0).Quaternion();
		}
		const FVector  DesiredDirection = (EndLocation - StartLocation);
		const FQuat    DesiredSocketRotation = DesiredDirection.Rotation().Quaternion();
		const FRotator DesiredRotation = (DesiredSocketRotation * SocketLocalRotation.Inverse()).Rotator().GetNormalized();
		const FRotator DesiredRotationDelta = (DesiredRotation - Rotation).GetNormalized();

		Delta += DesiredRotationDelta;
	}

	// Average delta of all connections
	Delta *= 1.0f / ConnectedSegments.Num();

	// Apply Delta and normalize
	Rotation = (Rotation + Delta).GetNormalized();
}

void ULandscapeSplineControlPoint::AutoFlipTangents()
{
	for (const FLandscapeSplineConnection& Connection : ConnectedSegments)
	{
		Connection.Segment->AutoFlipTangents();
	}
}

void ULandscapeSplineControlPoint::AutoSetConnections(bool bIncludingValid)
{
	for (const FLandscapeSplineConnection& Connection : ConnectedSegments)
	{
		FLandscapeSplineSegmentConnection& NearConnection = Connection.GetNearConnection();
		if (bIncludingValid ||
			(Mesh != nullptr && Mesh->FindSocket(NearConnection.SocketName) == nullptr) ||
			(Mesh == nullptr && NearConnection.SocketName != NAME_None))
		{
			FLandscapeSplineSegmentConnection& FarConnection = Connection.GetFarConnection();
			FVector EndLocation; FRotator EndRotation;
			FarConnection.ControlPoint->GetConnectionLocationAndRotation(FarConnection.SocketName, EndLocation, EndRotation);

			NearConnection.SocketName = GetBestConnectionTo(EndLocation);
			NearConnection.TangentLen = FMath::Abs(NearConnection.TangentLen);

			// Allow flipping tangent on the null connection
			if (NearConnection.SocketName == NAME_None)
			{
				FVector StartLocation; FRotator StartRotation;
				NearConnection.ControlPoint->GetConnectionLocationAndRotation(NearConnection.SocketName, StartLocation, StartRotation);

				if (FVector::DotProduct((EndLocation - StartLocation).GetSafeNormal(), StartRotation.Vector()) < 0)
				{
					NearConnection.TangentLen = -NearConnection.TangentLen;
				}
			}
		}
	}
}
#endif

#if WITH_EDITOR
TMap<ULandscapeSplinesComponent*, UControlPointMeshComponent*> ULandscapeSplineControlPoint::GetForeignMeshComponents()
{
	TMap<ULandscapeSplinesComponent*, UControlPointMeshComponent*> ForeignMeshComponentsMap;

	ULandscapeSplinesComponent* OuterSplines = GetOuterULandscapeSplinesComponent();
	TArray<ULandscapeSplinesComponent*> SplineComponents = OuterSplines->GetAllStreamingSplinesComponents();

	for (ULandscapeSplinesComponent* SplineComponent : SplineComponents)
	{
		if (SplineComponent != OuterSplines)
		{
			auto* ForeignMeshComponent = SplineComponent->GetForeignMeshComponent(this);
			if (ForeignMeshComponent)
			{
				ForeignMeshComponent->Modify(false);
				ForeignMeshComponentsMap.Add(SplineComponent, ForeignMeshComponent);
			}
		}
	}

	return ForeignMeshComponentsMap;
}

void ULandscapeSplineControlPoint::UpdateSplinePoints(bool bUpdateCollision, bool bUpdateAttachedSegments)
{
	Modify();

	ULandscapeSplinesComponent* OuterSplines = GetOuterULandscapeSplinesComponent();

	auto ForeignMeshComponentsMap = GetForeignMeshComponents();

	ModificationKey = FGuid::NewGuid();

	UControlPointMeshComponent* MeshComponent = LocalMeshComponent;
	ULandscapeSplinesComponent* MeshComponentOuterSplines = OuterSplines;

	if (Mesh != nullptr)
	{
		// Attempt to place mesh components into the appropriate landscape streaming levels based on the components under the spline
		if (bPlaceSplineMeshesInStreamingLevels)
		{
			MeshComponentOuterSplines = OuterSplines->GetStreamingSplinesComponentByLocation(Location);

			if (MeshComponentOuterSplines != OuterSplines)
			{
				MeshComponent = MeshComponentOuterSplines->GetForeignMeshComponent(this);
				if (MeshComponent)
				{
					MeshComponentOuterSplines->Modify();
					MeshComponentOuterSplines->UpdateModificationKey(this);
				}
			}
		}

		// Create mesh component if needed
		bool bComponentNeedsRegistering = false;
		if (MeshComponent == nullptr)
		{
			AActor* MeshComponentOuterActor = MeshComponentOuterSplines->GetOwner();
			MeshComponentOuterSplines->Modify();
			MeshComponentOuterActor->Modify();
			MeshComponent = NewObject<UControlPointMeshComponent>(MeshComponentOuterActor, NAME_None, RF_Transactional | RF_TextExportTransient);
			MeshComponent->bSelected = bSelected;
			MeshComponent->AttachToComponent(MeshComponentOuterSplines, FAttachmentTransformRules::KeepRelativeTransform);
			bComponentNeedsRegistering = true;

			if (MeshComponentOuterSplines == OuterSplines)
			{
				MeshComponentOuterSplines->MeshComponentLocalOwnersMap.Add(MeshComponent, this);
				LocalMeshComponent = MeshComponent;
			}
			else
			{
				MeshComponentOuterSplines->AddForeignMeshComponent(this, MeshComponent);
				ForeignWorld = MeshComponentOuterSplines->GetTypedOuter<UWorld>();
			}
		}

		FVector MeshLocation = Location;
		FRotator MeshRotation = Rotation;
		if (MeshComponentOuterSplines != OuterSplines)
		{
			const FTransform RelativeTransform = OuterSplines->GetComponentTransform().GetRelativeTransform(MeshComponentOuterSplines->GetComponentTransform());
			MeshLocation = RelativeTransform.TransformPosition(MeshLocation);
		}

		if (MeshComponent->RelativeLocation != MeshLocation ||
			MeshComponent->RelativeRotation != MeshRotation ||
			MeshComponent->RelativeScale3D != MeshScale)
		{
			MeshComponent->Modify();
			MeshComponent->SetRelativeTransform(FTransform(MeshRotation, MeshLocation, MeshScale));
			MeshComponent->InvalidateLightingCache();
		}

		if (MeshComponent->GetStaticMesh() != Mesh)
		{
			MeshComponent->Modify();
			MeshComponent->UnregisterComponent();
			bComponentNeedsRegistering = true;
			MeshComponent->SetStaticMesh(Mesh);

			AutoSetConnections(false);
		}

		if (MeshComponent->OverrideMaterials != MaterialOverrides)
		{
			MeshComponent->Modify();
			MeshComponent->OverrideMaterials = MaterialOverrides;
			MeshComponent->MarkRenderStateDirty();
			if (MeshComponent->BodyInstance.IsValidBodyInstance())
			{
				MeshComponent->BodyInstance.UpdatePhysicalMaterials();
			}
		}

		if (MeshComponent->TranslucencySortPriority != TranslucencySortPriority)
		{
			MeshComponent->Modify();
			MeshComponent->TranslucencySortPriority = TranslucencySortPriority;
			MeshComponent->MarkRenderStateDirty();
		}

		if (MeshComponent->LDMaxDrawDistance != LDMaxDrawDistance)
		{
			MeshComponent->Modify();
			MeshComponent->LDMaxDrawDistance = LDMaxDrawDistance;
			MeshComponent->CachedMaxDrawDistance = 0;
			MeshComponent->MarkRenderStateDirty();
		}

		if (MeshComponent->CastShadow != bCastShadow)
		{
			MeshComponent->Modify();
			MeshComponent->SetCastShadow(bCastShadow);
		}

		const FName CollisionProfile = bEnableCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName;
		if (MeshComponent->BodyInstance.GetCollisionProfileName() != CollisionProfile)
		{
			MeshComponent->Modify();
			MeshComponent->BodyInstance.SetCollisionProfileName(CollisionProfile);
		}

		if (bComponentNeedsRegistering)
		{
			MeshComponent->RegisterComponent();
		}
	}
	else
	{
		MeshComponent = nullptr;
		ForeignWorld = nullptr;
	}

	// Destroy any unused components
	bool bDestroyedAnyComponents = false;
	if (LocalMeshComponent && LocalMeshComponent != MeshComponent)
	{
		OuterSplines->Modify();
		LocalMeshComponent->Modify();
		checkSlow(OuterSplines->MeshComponentLocalOwnersMap.FindRef(LocalMeshComponent) == this);
		verifySlow(OuterSplines->MeshComponentLocalOwnersMap.Remove(LocalMeshComponent) == 1);
		LocalMeshComponent->DestroyComponent();
		LocalMeshComponent = nullptr;
	}
	for (auto& ForeignMeshComponentsPair : ForeignMeshComponentsMap)
	{
		ULandscapeSplinesComponent* ForeignMeshComponentOuterSplines = ForeignMeshComponentsPair.Key;
		auto* ForeignMeshComponent = ForeignMeshComponentsPair.Value;
		if (ForeignMeshComponent != MeshComponent)
		{
			ForeignMeshComponentOuterSplines->Modify();
			ForeignMeshComponent->Modify();
			ForeignMeshComponentOuterSplines->RemoveForeignMeshComponent(this, ForeignMeshComponent);
			ForeignMeshComponent->DestroyComponent();
		}
	}
	ForeignMeshComponentsMap.Empty();
	if (bDestroyedAnyComponents)
	{
		AutoSetConnections(false);
	}

	// Update "Points" array
	if (Mesh != nullptr)
	{
		Points.Reset(ConnectedSegments.Num());

		for (const FLandscapeSplineConnection& Connection : ConnectedSegments)
		{
			FVector StartLocation; FRotator StartRotation;
			GetConnectionLocationAndRotation(Connection.GetNearConnection().SocketName, StartLocation, StartRotation);

			const float Roll = FMath::DegreesToRadians(StartRotation.Roll);
			const FVector Tangent = StartRotation.Vector();
			const FVector BiNormal = FQuat(Tangent, -Roll).RotateVector((Tangent ^ FVector(0, 0, -1)).GetSafeNormal());
			const FVector LeftPos = StartLocation - BiNormal * Width;
			const FVector RightPos = StartLocation + BiNormal * Width;
			const FVector FalloffLeftPos = StartLocation - BiNormal * (Width + SideFalloff);
			const FVector FalloffRightPos = StartLocation + BiNormal * (Width + SideFalloff);

			Points.Emplace(StartLocation, LeftPos, RightPos, FalloffLeftPos, FalloffRightPos, 1.0f);
		}

		const FVector CPLocation = Location;
		Points.Sort([&CPLocation](const FLandscapeSplineInterpPoint& x, const FLandscapeSplineInterpPoint& y){return (x.Center - CPLocation).Rotation().Yaw < (y.Center - CPLocation).Rotation().Yaw;});
	}
	else
	{
		Points.Reset(1);

		FVector StartLocation; FRotator StartRotation;
		GetConnectionLocationAndRotation(NAME_None, StartLocation, StartRotation);

		const float Roll = FMath::DegreesToRadians(StartRotation.Roll);
		const FVector Tangent = StartRotation.Vector();
		const FVector BiNormal = FQuat(Tangent, -Roll).RotateVector((Tangent ^ FVector(0, 0, -1)).GetSafeNormal());
		const FVector LeftPos = StartLocation - BiNormal * Width;
		const FVector RightPos = StartLocation + BiNormal * Width;
		const FVector FalloffLeftPos = StartLocation - BiNormal * (Width + SideFalloff);
		const FVector FalloffRightPos = StartLocation + BiNormal * (Width + SideFalloff);

		Points.Emplace(StartLocation, LeftPos, RightPos, FalloffLeftPos, FalloffRightPos, 1.0f);
	}

	// Update bounds
	Bounds = FBox(ForceInit);

	// Sprite bounds
	float SpriteScale = FMath::Clamp<float>(Width != 0 ? Width / 2 : SideFalloff / 4, 10, 1000);
	Bounds += Location + FVector(0, 0, 0.75f * SpriteScale);
	Bounds = Bounds.ExpandBy(SpriteScale);

	// Points bounds
	for (const FLandscapeSplineInterpPoint& Point : Points)
	{
		Bounds += Point.FalloffLeft;
		Bounds += Point.FalloffRight;
	}

	OuterSplines->MarkRenderStateDirty();

	if (bUpdateAttachedSegments)
	{
		for (const FLandscapeSplineConnection& Connection : ConnectedSegments)
		{
			Connection.Segment->UpdateSplinePoints(bUpdateCollision);
		}
	}
}

void ULandscapeSplineControlPoint::DeleteSplinePoints()
{
	Modify();

	ULandscapeSplinesComponent* OuterSplines = CastChecked<ULandscapeSplinesComponent>(GetOuter());

	Points.Reset();
	Bounds = FBox(ForceInit);

	OuterSplines->MarkRenderStateDirty();

	if (LocalMeshComponent != nullptr)
	{
		OuterSplines->Modify();
		LocalMeshComponent->Modify();
		checkSlow(OuterSplines->MeshComponentLocalOwnersMap.FindRef(LocalMeshComponent) == this);
		verifySlow(OuterSplines->MeshComponentLocalOwnersMap.Remove(LocalMeshComponent) == 1);
		LocalMeshComponent->DestroyComponent();
		LocalMeshComponent = nullptr;
	}

	auto ForeignMeshComponentsMap = GetForeignMeshComponents();
	for (auto& ForeignMeshComponentsPair : ForeignMeshComponentsMap)
	{
		ULandscapeSplinesComponent* MeshComponentOuterSplines = ForeignMeshComponentsPair.Key;
		MeshComponentOuterSplines->Modify();
		auto* MeshComponent = ForeignMeshComponentsPair.Value;
		MeshComponent->Modify();
		MeshComponentOuterSplines->RemoveForeignMeshComponent(this, MeshComponent);
		MeshComponent->DestroyComponent();
	}
}

void ULandscapeSplineControlPoint::PostEditUndo()
{
	bHackIsUndoingSplines = true;
	Super::PostEditUndo();
	bHackIsUndoingSplines = false;

	GetOuterULandscapeSplinesComponent()->MarkRenderStateDirty();
}

void ULandscapeSplineControlPoint::PostDuplicate(bool bDuplicateForPIE)
{
	if (!bDuplicateForPIE)
	{
		// if we get duplicated but our local mesh doesn't, then clear our reference to the mesh - it's not ours
		if (LocalMeshComponent != nullptr)
		{
			ULandscapeSplinesComponent* OuterSplines = CastChecked<ULandscapeSplinesComponent>(GetOuter());
			if (LocalMeshComponent->GetOuter() != OuterSplines->GetOwner())
			{
				LocalMeshComponent = nullptr;
			}
		}

		UpdateSplinePoints();
	}

	Super::PostDuplicate(bDuplicateForPIE);
}

void ULandscapeSplineControlPoint::PostEditImport()
{
	Super::PostEditImport();

	GetOuterULandscapeSplinesComponent()->ControlPoints.AddUnique(this);
}

void ULandscapeSplineControlPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	Width = FMath::Max(Width, 0.001f);
	SideFalloff = FMath::Max(SideFalloff, 0.0f);
	EndFalloff = FMath::Max(EndFalloff, 0.0f);

	// Don't update splines when undoing, not only is it unnecessary and expensive,
	// it also causes failed asserts in debug builds when trying to register components
	// (because the actor hasn't reset its OwnedComponents array yet)
	if (!bHackIsUndoingSplines)
	{
		const bool bUpdateCollision = PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive;
		UpdateSplinePoints(bUpdateCollision);
	}
}
#endif // WITH_EDITOR

//////////////////////////////////////////////////////////////////////////
// SPLINE SEGMENT

ULandscapeSplineSegment::ULandscapeSplineSegment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	Connections[0].ControlPoint = nullptr;
	Connections[0].TangentLen = 0;
	Connections[1].ControlPoint = nullptr;
	Connections[1].TangentLen = 0;

#if WITH_EDITORONLY_DATA
	LayerName = NAME_None;
	bRaiseTerrain = true;
	bLowerTerrain = true;

	// SplineMesh properties
	SplineMeshes.Empty();
	LDMaxDrawDistance = 0;
	TranslucencySortPriority = 0;
	bPlaceSplineMeshesInStreamingLevels = true;
	bEnableCollision = true;
	bCastShadow = true;

	// transients
	bSelected = false;
#endif
}

void ULandscapeSplineSegment::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) &&
		!HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
	{
		// create a new random seed for all new objects
		RandomSeed = FMath::Rand();
	}
#endif
}

void ULandscapeSplineSegment::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.UE4Ver() < VER_UE4_SPLINE_MESH_ORIENTATION)
	{
		for (FLandscapeSplineMeshEntry& MeshEntry : SplineMeshes)
		{
			switch (MeshEntry.Orientation_DEPRECATED)
			{
			case LSMO_XUp:
				MeshEntry.ForwardAxis = ESplineMeshAxis::Z;
				MeshEntry.UpAxis = ESplineMeshAxis::X;
				break;
			case LSMO_YUp:
				MeshEntry.ForwardAxis = ESplineMeshAxis::Z;
				MeshEntry.UpAxis = ESplineMeshAxis::Y;
				break;
			}
		}
	}

	if (Ar.UE4Ver() < VER_UE4_LANDSCAPE_SPLINE_CROSS_LEVEL_MESHES)
	{
		bPlaceSplineMeshesInStreamingLevels = false;
	}
#endif
}

void ULandscapeSplineSegment::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (GIsEditor)
	{
		if (GetLinkerUE4Version() < VER_UE4_ADDED_LANDSCAPE_SPLINE_EDITOR_MESH &&
			LocalMeshComponents.Num() == 0) // ForeignMeshComponents didn't exist yet
		{
			UpdateSplinePoints();
		}

		// Replace null meshes with the editor mesh
		// Otherwise the spline will have no mesh and won't be easily selectable
		ULandscapeSplinesComponent* OuterSplines = GetOuterULandscapeSplinesComponent();
		if (OuterSplines->SplineEditorMesh != nullptr)
		{
			for (auto* LocalMeshComponent : LocalMeshComponents)
			{
				if (LocalMeshComponent->GetStaticMesh() == nullptr)
				{
					LocalMeshComponent->ConditionalPostLoad();
					LocalMeshComponent->SetStaticMesh(OuterSplines->SplineEditorMesh);
					LocalMeshComponent->SetHiddenInGame(true);
					LocalMeshComponent->SetVisibility(OuterSplines->bShowSplineEditorMesh);
					LocalMeshComponent->BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				}
			}
		}

		for (auto* LocalMeshComponent : LocalMeshComponents)
		{
			OuterSplines->MeshComponentLocalOwnersMap.Add(LocalMeshComponent, this);
		}
	}

	if (GetLinkerUE4Version() < VER_UE4_LANDSCAPE_SPLINE_CROSS_LEVEL_MESHES)
	{
		// Fix collision profile
		for (auto* LocalMeshComponent : LocalMeshComponents) // ForeignMeshComponents didn't exist yet
		{
			const bool bUsingEditorMesh = LocalMeshComponent->bHiddenInGame;
			const FName CollisionProfile = (bEnableCollision && !bUsingEditorMesh) ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName;
			if (LocalMeshComponent->GetCollisionProfileName() != CollisionProfile)
			{
				LocalMeshComponent->SetCollisionProfileName(CollisionProfile);
			}

			LocalMeshComponent->SetFlags(RF_TextExportTransient);
		}
	}
#endif
}

/**  */
#if WITH_EDITOR
void ULandscapeSplineSegment::SetSplineSelected(bool bInSelected)
{
	bSelected = bInSelected;
	GetOuterULandscapeSplinesComponent()->MarkRenderStateDirty();

	for (auto* LocalMeshComponent : LocalMeshComponents)
	{
		LocalMeshComponent->bSelected = bInSelected;
		LocalMeshComponent->PushSelectionToProxy();
	}

	auto ForeignMeshComponentsMap = GetForeignMeshComponents();
	for (auto& ForeignMeshComponentsPair : ForeignMeshComponentsMap)
	{
		ULandscapeSplinesComponent* MeshComponentOuterSplines = ForeignMeshComponentsPair.Key;
		for (auto* ForeignMeshComponent : ForeignMeshComponentsPair.Value)
		{
			ForeignMeshComponent->bSelected = bInSelected;
			ForeignMeshComponent->PushSelectionToProxy();
		}
	}
}

void ULandscapeSplineSegment::AutoFlipTangents()
{
	FVector StartLocation; FRotator StartRotation;
	Connections[0].ControlPoint->GetConnectionLocationAndRotation(Connections[0].SocketName, StartLocation, StartRotation);
	FVector EndLocation; FRotator EndRotation;
	Connections[1].ControlPoint->GetConnectionLocationAndRotation(Connections[1].SocketName, EndLocation, EndRotation);

	// Flipping the tangent is only allowed if not using a socket
	if (Connections[0].SocketName == NAME_None && FVector::DotProduct((EndLocation - StartLocation).GetSafeNormal() * Connections[0].TangentLen, StartRotation.Vector()) < 0)
	{
		Connections[0].TangentLen = -Connections[0].TangentLen;
	}
	if (Connections[1].SocketName == NAME_None && FVector::DotProduct((StartLocation - EndLocation).GetSafeNormal() * Connections[1].TangentLen, EndRotation.Vector()) < 0)
	{
		Connections[1].TangentLen = -Connections[1].TangentLen;
	}
}
#endif

static float ApproxLength(const FInterpCurveVector& SplineInfo, const float Start = 0.0f, const float End = 1.0f, const int32 ApproxSections = 4)
{
	float SplineLength = 0;
	FVector OldPos = SplineInfo.Eval(Start, FVector::ZeroVector);
	for (int32 i = 1; i <= ApproxSections; i++)
	{
		FVector NewPos = SplineInfo.Eval(FMath::Lerp(Start, End, (float)i / (float)ApproxSections), FVector::ZeroVector);
		SplineLength += (NewPos - OldPos).Size();
		OldPos = NewPos;
	}

	return SplineLength;
}

static ESplineMeshAxis::Type CrossAxis(ESplineMeshAxis::Type InForwardAxis, ESplineMeshAxis::Type InUpAxis)
{
	check(InForwardAxis != InUpAxis);
	return (ESplineMeshAxis::Type)(3 ^ InForwardAxis ^ InUpAxis);
}

bool FLandscapeSplineMeshEntry::IsValid() const
{
	return Mesh != nullptr && ForwardAxis != UpAxis && Scale.GetAbsMin() > KINDA_SMALL_NUMBER;
}

#if WITH_EDITOR
TMap<ULandscapeSplinesComponent*, TArray<USplineMeshComponent*>> ULandscapeSplineSegment::GetForeignMeshComponents()
{
	TMap<ULandscapeSplinesComponent*, TArray<USplineMeshComponent*>> ForeignMeshComponentsMap;

	ULandscapeSplinesComponent* OuterSplines = GetOuterULandscapeSplinesComponent();
	TArray<ULandscapeSplinesComponent*> SplineComponents = OuterSplines->GetAllStreamingSplinesComponents();

	for (ULandscapeSplinesComponent* SplineComponent : SplineComponents)
	{
		if (SplineComponent != OuterSplines)
		{
			auto ForeignMeshComponents = SplineComponent->GetForeignMeshComponents(this);
			if (ForeignMeshComponents.Num() > 0)
			{
				for (auto* ForeignMeshComponent : ForeignMeshComponents)
				{
					ForeignMeshComponent->Modify(false);
				}
				ForeignMeshComponentsMap.Add(SplineComponent, MoveTemp(ForeignMeshComponents));
			}
		}
	}

	return ForeignMeshComponentsMap;
}

void ULandscapeSplineSegment::UpdateSplinePoints(bool bUpdateCollision)
{
	Modify();

	ULandscapeSplinesComponent* OuterSplines = GetOuterULandscapeSplinesComponent();

	SplineInfo.Points.Empty(2);
	Points.Reset();

	if (Connections[0].ControlPoint == nullptr
		|| Connections[1].ControlPoint == nullptr)
	{
		return;
	}

	// Set up BSpline
	FVector StartLocation; FRotator StartRotation;
	Connections[0].ControlPoint->GetConnectionLocationAndRotation(Connections[0].SocketName, StartLocation, StartRotation);
	SplineInfo.Points.Emplace(0.0f, StartLocation, StartRotation.Vector() * Connections[0].TangentLen, StartRotation.Vector() * Connections[0].TangentLen, CIM_CurveUser);
	FVector EndLocation; FRotator EndRotation;
	Connections[1].ControlPoint->GetConnectionLocationAndRotation(Connections[1].SocketName, EndLocation, EndRotation);
	SplineInfo.Points.Emplace(1.0f, EndLocation, EndRotation.Vector() * -Connections[1].TangentLen, EndRotation.Vector() * -Connections[1].TangentLen, CIM_CurveUser);

	// Pointify

	// Calculate spline length
	const float SplineLength = ApproxLength(SplineInfo, 0.0f, 1.0f, 4);

	const float StartFalloffFraction = ((Connections[0].ControlPoint->ConnectedSegments.Num() > 1) ? 0 : (Connections[0].ControlPoint->EndFalloff / SplineLength));
	const float EndFalloffFraction = ((Connections[1].ControlPoint->ConnectedSegments.Num() > 1) ? 0 : (Connections[1].ControlPoint->EndFalloff / SplineLength));
	const float StartWidth = Connections[0].ControlPoint->Width;
	const float EndWidth = Connections[1].ControlPoint->Width;
	const float StartSideFalloff = Connections[0].ControlPoint->SideFalloff;
	const float EndSideFalloff = Connections[1].ControlPoint->SideFalloff;
	const float StartRollDegrees = StartRotation.Roll * (Connections[0].TangentLen > 0 ? 1 : -1);
	const float EndRollDegrees = EndRotation.Roll * (Connections[1].TangentLen > 0 ? -1 : 1);
	const float StartRoll = FMath::DegreesToRadians(StartRollDegrees);
	const float EndRoll = FMath::DegreesToRadians(EndRollDegrees);
	const float StartMeshOffset = Connections[0].ControlPoint->SegmentMeshOffset;
	const float EndMeshOffset = Connections[1].ControlPoint->SegmentMeshOffset;

	int32 NumPoints = FMath::CeilToInt(SplineLength / OuterSplines->SplineResolution);
	NumPoints = FMath::Clamp(NumPoints, 1, 1000);

	LandscapeSplineRaster::Pointify(SplineInfo, Points, NumPoints, StartFalloffFraction, EndFalloffFraction, StartWidth, EndWidth, StartSideFalloff, EndSideFalloff, StartRollDegrees, EndRollDegrees);

	// Update Bounds
	Bounds = FBox(ForceInit);
	for (const FLandscapeSplineInterpPoint& Point : Points)
	{
		Bounds += Point.FalloffLeft;
		Bounds += Point.FalloffRight;
	}

	OuterSplines->MarkRenderStateDirty();

	// Spline mesh components
	TArray<const FLandscapeSplineMeshEntry*> UsableMeshes;
	UsableMeshes.Reserve(SplineMeshes.Num());
	for (const FLandscapeSplineMeshEntry& MeshEntry : SplineMeshes)
	{
		if (MeshEntry.IsValid())
		{
			UsableMeshes.Add(&MeshEntry);
		}
	}

	// Editor mesh
	bool bUsingEditorMesh = false;
	FLandscapeSplineMeshEntry SplineEditorMeshEntry;
	if (UsableMeshes.Num() == 0 && OuterSplines->SplineEditorMesh != nullptr)
	{
		SplineEditorMeshEntry.Mesh = OuterSplines->SplineEditorMesh;
		SplineEditorMeshEntry.MaterialOverrides = {};
		SplineEditorMeshEntry.bCenterH = true;
		SplineEditorMeshEntry.CenterAdjust = {0.0f, 0.5f};
		SplineEditorMeshEntry.bScaleToWidth = true;
		SplineEditorMeshEntry.Scale = {3, 1, 1};
		SplineEditorMeshEntry.ForwardAxis = ESplineMeshAxis::X;
		SplineEditorMeshEntry.UpAxis = ESplineMeshAxis::Z;
		UsableMeshes.Add(&SplineEditorMeshEntry);
		bUsingEditorMesh = true;
	}

	OuterSplines->Modify();

	TArray<USplineMeshComponent*> MeshComponents;

	TArray<USplineMeshComponent*> OldLocalMeshComponents = MoveTemp(LocalMeshComponents);
	LocalMeshComponents.Reserve(20);

	auto ForeignMeshComponentsMap = GetForeignMeshComponents();

	// Unregister components
	for (auto* LocalMeshComponent : OldLocalMeshComponents)
	{
		LocalMeshComponent->Modify();
		LocalMeshComponent->UnregisterComponent();
	}
	for (auto& ForeignMeshComponentsPair : ForeignMeshComponentsMap)
	{
		ForeignMeshComponentsPair.Key->Modify();
		ForeignMeshComponentsPair.Key->GetOwner()->Modify();
		for (auto* ForeignMeshComponent : ForeignMeshComponentsPair.Value)
		{
			ForeignMeshComponent->Modify();
			ForeignMeshComponent->UnregisterComponent();
		}
	}

	ModificationKey = FGuid::NewGuid();
	ForeignWorlds.Reset();

	if (SplineLength > 0 && (StartWidth > 0 || EndWidth > 0) && UsableMeshes.Num() > 0)
	{
		float T = 0;
		int32 iMesh = 0;

		struct FMeshSettings
		{
			const float T;
			const FLandscapeSplineMeshEntry* const MeshEntry;

			FMeshSettings(float InT, const FLandscapeSplineMeshEntry* const InMeshEntry) :
				T(InT), MeshEntry(InMeshEntry) { }
		};
		TArray<FMeshSettings> MeshSettings;
		MeshSettings.Reserve(21);

		FRandomStream Random(RandomSeed);

		// First pass:
		// Choose meshes, create components, calculate lengths
		while (T < 1.0f && iMesh < 20) // Max 20 meshes per spline segment
		{
			const float CosInterp = 0.5f - 0.5f * FMath::Cos(T * PI);
			const float Width = FMath::Lerp(StartWidth, EndWidth, CosInterp);

			const FLandscapeSplineMeshEntry* MeshEntry = UsableMeshes[Random.RandHelper(UsableMeshes.Num())];
			UStaticMesh* Mesh = MeshEntry->Mesh;
			const FBoxSphereBounds MeshBounds = Mesh->GetBounds();

			FVector Scale = MeshEntry->Scale;
			if (MeshEntry->bScaleToWidth)
			{
				Scale *= Width / USplineMeshComponent::GetAxisValue(MeshBounds.BoxExtent, CrossAxis(MeshEntry->ForwardAxis, MeshEntry->UpAxis));
			}

			const float MeshLength = FMath::Abs(USplineMeshComponent::GetAxisValue(MeshBounds.BoxExtent, MeshEntry->ForwardAxis) * 2 * USplineMeshComponent::GetAxisValue(Scale, MeshEntry->ForwardAxis));
			float MeshT = (MeshLength / SplineLength);

			// Improve our approximation if we're not going off the end of the spline
			if (T + MeshT <= 1.0f)
			{
				MeshT *= (MeshLength / ApproxLength(SplineInfo, T, T + MeshT, 4));
				MeshT *= (MeshLength / ApproxLength(SplineInfo, T, T + MeshT, 4));
			}

			// If it's smaller to round up than down, don't add another component
			if (iMesh != 0 && (1.0f - T) < (T + MeshT - 1.0f))
			{
				break;
			}

			ULandscapeSplinesComponent* MeshComponentOuterSplines = OuterSplines;

			// Attempt to place mesh components into the appropriate landscape streaming levels based on the components under the spline
			if (bPlaceSplineMeshesInStreamingLevels && !bUsingEditorMesh)
			{
				// Only "approx" because we rescale T for the 2nd pass based on how well our chosen meshes fit, but it should be good enough
				FVector ApproxMeshLocation = SplineInfo.Eval(T + MeshT / 2, FVector::ZeroVector);
				MeshComponentOuterSplines = OuterSplines->GetStreamingSplinesComponentByLocation(ApproxMeshLocation);
				MeshComponentOuterSplines->Modify();
			}

			USplineMeshComponent* MeshComponent = nullptr;
			if (MeshComponentOuterSplines == OuterSplines)
			{
				if (OldLocalMeshComponents.Num() > 0)
				{
					MeshComponent = OldLocalMeshComponents.Pop(false);
					LocalMeshComponents.Add(MeshComponent);
				}
			}
			else
			{
				TArray<USplineMeshComponent*>* ForeignMeshComponents = ForeignMeshComponentsMap.Find(MeshComponentOuterSplines);
				if (ForeignMeshComponents && ForeignMeshComponents->Num() > 0)
				{
					MeshComponentOuterSplines->UpdateModificationKey(this);
					MeshComponent = ForeignMeshComponents->Pop(false);
					ForeignWorlds.AddUnique(MeshComponentOuterSplines->GetTypedOuter<UWorld>());
				}
			}

			if (!MeshComponent)
			{
				AActor* MeshComponentOuterActor = MeshComponentOuterSplines->GetOwner();
				MeshComponentOuterActor->Modify();
				MeshComponent = NewObject<USplineMeshComponent>(MeshComponentOuterActor, NAME_None, RF_Transactional | RF_TextExportTransient);
				MeshComponent->bSelected = bSelected;
				MeshComponent->AttachToComponent(MeshComponentOuterSplines, FAttachmentTransformRules::KeepRelativeTransform);
				if (MeshComponentOuterSplines == OuterSplines)
				{
					LocalMeshComponents.Add(MeshComponent);
					MeshComponentOuterSplines->MeshComponentLocalOwnersMap.Add(MeshComponent, this);
				}
				else
				{
					MeshComponentOuterSplines->AddForeignMeshComponent(this, MeshComponent);
					ForeignWorlds.AddUnique(MeshComponentOuterSplines->GetTypedOuter<UWorld>());
				}
			}

			MeshComponents.Add(MeshComponent);

			MeshComponent->SetStaticMesh(Mesh);

			MeshComponent->OverrideMaterials = MeshEntry->MaterialOverrides;
			MeshComponent->MarkRenderStateDirty();
			if (MeshComponent->BodyInstance.IsValidBodyInstance())
			{
				MeshComponent->BodyInstance.UpdatePhysicalMaterials();
			}

			MeshComponent->SetHiddenInGame(bUsingEditorMesh);
			MeshComponent->SetVisibility(!bUsingEditorMesh || OuterSplines->bShowSplineEditorMesh);

			MeshSettings.Add(FMeshSettings(T, MeshEntry));
			iMesh++;
			T += MeshT;
		}
		// Add terminating key
		MeshSettings.Add(FMeshSettings(T, nullptr));

		// Destroy old unwanted components now
		for (UMeshComponent* LocalMeshComponent : OldLocalMeshComponents)
		{
			checkSlow(OuterSplines->MeshComponentLocalOwnersMap.FindRef(LocalMeshComponent) == this);
			verifySlow(OuterSplines->MeshComponentLocalOwnersMap.Remove(LocalMeshComponent) == 1);
			LocalMeshComponent->DestroyComponent();
		}
		OldLocalMeshComponents.Empty();

		for (auto& ForeignMeshComponentsPair : ForeignMeshComponentsMap)
		{
			ULandscapeSplinesComponent* MeshComponentOuterSplines = ForeignMeshComponentsPair.Key;
			for (auto* ForeignMeshComponent : ForeignMeshComponentsPair.Value)
			{
				MeshComponentOuterSplines->RemoveForeignMeshComponent(this, ForeignMeshComponent);
				ForeignMeshComponent->DestroyComponent();
			}
		}
		ForeignMeshComponentsMap.Empty();

		// Second pass:
		// Rescale components to fit a whole number to the spline, set up final parameters
		const float Rescale = 1.0f / T;
		for (int32 i = 0; i < MeshComponents.Num(); i++)
		{
			USplineMeshComponent* const MeshComponent = MeshComponents[i];
			const UStaticMesh* const Mesh = MeshComponent->GetStaticMesh();
			const FBoxSphereBounds MeshBounds = Mesh->GetBounds();

			const float RescaledT = MeshSettings[i].T * Rescale;
			const FLandscapeSplineMeshEntry* MeshEntry = MeshSettings[i].MeshEntry;
			const ESplineMeshAxis::Type SideAxis = CrossAxis(MeshEntry->ForwardAxis, MeshEntry->UpAxis);

			const float TEnd = MeshSettings[i + 1].T * Rescale;

			const float CosInterp = 0.5f - 0.5f * FMath::Cos(RescaledT * PI);
			const float Width = FMath::Lerp(StartWidth, EndWidth, CosInterp);
			const bool bDoOrientationRoll = (MeshEntry->ForwardAxis == ESplineMeshAxis::X && MeshEntry->UpAxis == ESplineMeshAxis::Y) ||
			                                (MeshEntry->ForwardAxis == ESplineMeshAxis::Y && MeshEntry->UpAxis == ESplineMeshAxis::Z) ||
			                                (MeshEntry->ForwardAxis == ESplineMeshAxis::Z && MeshEntry->UpAxis == ESplineMeshAxis::X);
			const float Roll = FMath::Lerp(StartRoll, EndRoll, CosInterp) + (bDoOrientationRoll ? -HALF_PI : 0);
			const float MeshOffset = FMath::Lerp(StartMeshOffset, EndMeshOffset, CosInterp);

			FVector Scale = MeshEntry->Scale;
			if (MeshEntry->bScaleToWidth)
			{
				Scale *= Width / USplineMeshComponent::GetAxisValue(MeshBounds.BoxExtent, SideAxis);
			}

			FVector2D Offset = MeshEntry->CenterAdjust;
			if (MeshEntry->bCenterH)
			{
				if (bDoOrientationRoll)
				{
					Offset.Y -= USplineMeshComponent::GetAxisValue(MeshBounds.Origin, SideAxis);
				}
				else
				{
					Offset.X -= USplineMeshComponent::GetAxisValue(MeshBounds.Origin, SideAxis);
				}
			}

			FVector2D Scale2D;
			switch (MeshEntry->ForwardAxis)
			{
			case ESplineMeshAxis::X:
				Scale2D = FVector2D(Scale.Y, Scale.Z);
				break;
			case ESplineMeshAxis::Y:
				Scale2D = FVector2D(Scale.Z, Scale.X);
				break;
			case ESplineMeshAxis::Z:
				Scale2D = FVector2D(Scale.X, Scale.Y);
				break;
			default:
				check(0);
				break;
			}
			Offset *= Scale2D;
			Offset.Y += MeshOffset;
			Offset = Offset.GetRotated(-Roll);

			MeshComponent->SplineParams.StartPos = SplineInfo.Eval(RescaledT, FVector::ZeroVector);
			MeshComponent->SplineParams.StartTangent = SplineInfo.EvalDerivative(RescaledT, FVector::ZeroVector) * (TEnd - RescaledT);
			MeshComponent->SplineParams.StartScale = Scale2D;
			MeshComponent->SplineParams.StartRoll = Roll;
			MeshComponent->SplineParams.StartOffset = Offset;

			const float CosInterpEnd = 0.5f - 0.5f * FMath::Cos(TEnd * PI);
			const float WidthEnd = FMath::Lerp(StartWidth, EndWidth, CosInterpEnd);
			const float RollEnd = FMath::Lerp(StartRoll, EndRoll, CosInterpEnd) + (bDoOrientationRoll ? -HALF_PI : 0);
			const float MeshOffsetEnd = FMath::Lerp(StartMeshOffset, EndMeshOffset, CosInterpEnd);

			FVector ScaleEnd = MeshEntry->Scale;
			if (MeshEntry->bScaleToWidth)
			{
				ScaleEnd *= WidthEnd / USplineMeshComponent::GetAxisValue(MeshBounds.BoxExtent, SideAxis);
			}

			FVector2D OffsetEnd = MeshEntry->CenterAdjust;
			if (MeshEntry->bCenterH)
			{
				if (bDoOrientationRoll)
				{
					OffsetEnd.Y -= USplineMeshComponent::GetAxisValue(MeshBounds.Origin, SideAxis);
				}
				else
				{
					OffsetEnd.X -= USplineMeshComponent::GetAxisValue(MeshBounds.Origin, SideAxis);
				}
			}

			FVector2D Scale2DEnd;
			switch (MeshEntry->ForwardAxis)
			{
			case ESplineMeshAxis::X:
				Scale2DEnd = FVector2D(ScaleEnd.Y, ScaleEnd.Z);
				break;
			case ESplineMeshAxis::Y:
				Scale2DEnd = FVector2D(ScaleEnd.Z, ScaleEnd.X);
				break;
			case ESplineMeshAxis::Z:
				Scale2DEnd = FVector2D(ScaleEnd.X, ScaleEnd.Y);
				break;
			default:
				check(0);
				break;
			}
			OffsetEnd *= Scale2DEnd;
			OffsetEnd.Y += MeshOffsetEnd;
			OffsetEnd = OffsetEnd.GetRotated(-RollEnd);

			MeshComponent->SplineParams.EndPos = SplineInfo.Eval(TEnd, FVector::ZeroVector);
			MeshComponent->SplineParams.EndTangent = SplineInfo.EvalDerivative(TEnd, FVector::ZeroVector) * (TEnd - RescaledT);
			MeshComponent->SplineParams.EndScale = Scale2DEnd;
			MeshComponent->SplineParams.EndRoll = RollEnd;
			MeshComponent->SplineParams.EndOffset = OffsetEnd;

			MeshComponent->SplineUpDir = FVector(0,0,1); // Up, to be consistent between joined meshes. We rotate it to horizontal using roll if using Z Forward X Up or X Forward Y Up
			MeshComponent->ForwardAxis = MeshEntry->ForwardAxis;

			auto* const MeshComponentOuterSplines = MeshComponent->GetAttachParent();
			if (MeshComponentOuterSplines != nullptr && MeshComponentOuterSplines != OuterSplines)
			{
				const FTransform RelativeTransform = OuterSplines->GetComponentTransform().GetRelativeTransform(MeshComponentOuterSplines->GetComponentTransform());
				MeshComponent->SplineParams.StartPos = RelativeTransform.TransformPosition(MeshComponent->SplineParams.StartPos);
				MeshComponent->SplineParams.EndPos   = RelativeTransform.TransformPosition(MeshComponent->SplineParams.EndPos);
			}

			if (USplineMeshComponent::GetAxisValue(MeshEntry->Scale, MeshEntry->ForwardAxis) < 0)
			{
				Swap(MeshComponent->SplineParams.StartPos, MeshComponent->SplineParams.EndPos);
				Swap(MeshComponent->SplineParams.StartTangent, MeshComponent->SplineParams.EndTangent);
				Swap(MeshComponent->SplineParams.StartScale, MeshComponent->SplineParams.EndScale);
				Swap(MeshComponent->SplineParams.StartRoll, MeshComponent->SplineParams.EndRoll);
				Swap(MeshComponent->SplineParams.StartOffset, MeshComponent->SplineParams.EndOffset);

				MeshComponent->SplineParams.StartTangent = -MeshComponent->SplineParams.StartTangent;
				MeshComponent->SplineParams.EndTangent = -MeshComponent->SplineParams.EndTangent;
				MeshComponent->SplineParams.StartScale.X = -MeshComponent->SplineParams.StartScale.X;
				MeshComponent->SplineParams.EndScale.X = -MeshComponent->SplineParams.EndScale.X;
				MeshComponent->SplineParams.StartRoll = -MeshComponent->SplineParams.StartRoll;
				MeshComponent->SplineParams.EndRoll = -MeshComponent->SplineParams.EndRoll;
				MeshComponent->SplineParams.StartOffset.X = -MeshComponent->SplineParams.StartOffset.X;
				MeshComponent->SplineParams.EndOffset.X = -MeshComponent->SplineParams.EndOffset.X;
			}

			// Set Mesh component's location to half way between the start and end points. Improves the bounds and allows LDMaxDrawDistance to work
			MeshComponent->RelativeLocation = (MeshComponent->SplineParams.StartPos + MeshComponent->SplineParams.EndPos) / 2;
			MeshComponent->SplineParams.StartPos -= MeshComponent->RelativeLocation;
			MeshComponent->SplineParams.EndPos -= MeshComponent->RelativeLocation;

			if (MeshComponent->LDMaxDrawDistance != LDMaxDrawDistance)
			{
				MeshComponent->LDMaxDrawDistance = LDMaxDrawDistance;
				MeshComponent->CachedMaxDrawDistance = 0;
			}
			MeshComponent->TranslucencySortPriority = TranslucencySortPriority;

			MeshComponent->SetCastShadow(bCastShadow);
			MeshComponent->InvalidateLightingCache();

			MeshComponent->BodyInstance.SetCollisionProfileName((bEnableCollision && !bUsingEditorMesh) ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);

#if WITH_EDITOR
			if (bUpdateCollision)
			{
				MeshComponent->RecreateCollision();
			}
			else
			{
				if (MeshComponent->BodySetup)
				{
					MeshComponent->BodySetup->InvalidatePhysicsData();
					MeshComponent->BodySetup->AggGeom.EmptyElements();
				}
			}
#endif
		}

		// Finally, register components
		for (auto* MeshComponent : MeshComponents)
		{
			MeshComponent->RegisterComponent();
		}
	}
	else
	{
		// Spline needs no mesh components (0 length or no meshes to use) so destroy any we have
		for (auto* LocalMeshComponent : OldLocalMeshComponents)
		{
			checkSlow(OuterSplines->MeshComponentLocalOwnersMap.FindRef(LocalMeshComponent) == this);
			verifySlow(OuterSplines->MeshComponentLocalOwnersMap.Remove(LocalMeshComponent) == 1);
			LocalMeshComponent->DestroyComponent();
		}
		OldLocalMeshComponents.Empty();
		for (auto& ForeignMeshComponentsPair : ForeignMeshComponentsMap)
		{
			for (USplineMeshComponent* MeshComponent : ForeignMeshComponentsPair.Value)
			{
				ULandscapeSplinesComponent* MeshComponentOuterSplines = dynamic_cast<ULandscapeSplinesComponent*>(MeshComponent->GetAttachParent());
				if (MeshComponentOuterSplines)
				{
					MeshComponentOuterSplines->RemoveForeignMeshComponent(this, MeshComponent);
				}
				MeshComponent->DestroyComponent();
			}
		}
		ForeignMeshComponentsMap.Empty();
	}
}

void ULandscapeSplineSegment::UpdateSplineEditorMesh()
{
	ULandscapeSplinesComponent* OuterSplines = CastChecked<ULandscapeSplinesComponent>(GetOuter());

	for (auto* LocalMeshComponent : LocalMeshComponents)
	{
		if (LocalMeshComponent->bHiddenInGame)
		{
			LocalMeshComponent->SetVisibility(OuterSplines->bShowSplineEditorMesh);
		}
	}

	auto ForeignMeshComponentsMap = GetForeignMeshComponents();
	for (auto& ForeignMeshComponentsPair : ForeignMeshComponentsMap)
	{
		for (auto* ForeignMeshComponent : ForeignMeshComponentsPair.Value)
		{
			if (ForeignMeshComponent->bHiddenInGame)
			{
				ForeignMeshComponent->SetVisibility(OuterSplines->bShowSplineEditorMesh);
			}
		}
	}
}

void ULandscapeSplineSegment::DeleteSplinePoints()
{
	Modify();

	ULandscapeSplinesComponent* OuterSplines = GetOuterULandscapeSplinesComponent();

	SplineInfo.Reset();
	Points.Reset();
	Bounds = FBox(ForceInit);

	OuterSplines->MarkRenderStateDirty();

	// Destroy mesh components
	if (LocalMeshComponents.Num() > 0)
	{
		OuterSplines->Modify();
		for (auto* LocalMeshComponent : LocalMeshComponents)
		{
			checkSlow(OuterSplines->MeshComponentLocalOwnersMap.FindRef(LocalMeshComponent) == this);
			verifySlow(OuterSplines->MeshComponentLocalOwnersMap.Remove(LocalMeshComponent) == 1);
			LocalMeshComponent->Modify();
			LocalMeshComponent->DestroyComponent();
		}
		LocalMeshComponents.Empty();
	}

	auto ForeignMeshComponentsMap = GetForeignMeshComponents();
	for (auto& ForeignMeshComponentsPair : ForeignMeshComponentsMap)
	{
		ULandscapeSplinesComponent* MeshComponentOuterSplines = ForeignMeshComponentsPair.Key;
		MeshComponentOuterSplines->Modify();
		MeshComponentOuterSplines->GetOwner()->Modify();
		for (auto* ForeignMeshComponent : ForeignMeshComponentsPair.Value)
		{
			ForeignMeshComponent->Modify();
			MeshComponentOuterSplines->RemoveForeignMeshComponent(this, ForeignMeshComponent);
			ForeignMeshComponent->DestroyComponent();
		}
	}

	ModificationKey.Invalidate();
	ForeignWorlds.Empty();
}
#endif

void ULandscapeSplineSegment::FindNearest( const FVector& InLocation, float& t, FVector& OutLocation, FVector& OutTangent )
{
	float TempOutDistanceSq;
	t = SplineInfo.InaccurateFindNearest(InLocation, TempOutDistanceSq);
	OutLocation = SplineInfo.Eval(t, FVector::ZeroVector);
	OutTangent = SplineInfo.EvalDerivative(t, FVector::ZeroVector);
}

bool ULandscapeSplineSegment::Modify(bool bAlwaysMarkDirty /*= true*/)
{
	bool bSavedToTransactionBuffer = Super::Modify(bAlwaysMarkDirty);

	//for (auto MeshComponent : MeshComponents)
	//{
	//	if (MeshComponent)
	//	{
	//		bSavedToTransactionBuffer = MeshComponent->Modify(bAlwaysMarkDirty) || bSavedToTransactionBuffer;
	//	}
	//}

	return bSavedToTransactionBuffer;
}

#if WITH_EDITOR
void ULandscapeSplineSegment::PostEditUndo()
{
	bHackIsUndoingSplines = true;
	Super::PostEditUndo();
	bHackIsUndoingSplines = false;

	GetOuterULandscapeSplinesComponent()->MarkRenderStateDirty();
}

void ULandscapeSplineSegment::PostDuplicate(bool bDuplicateForPIE)
{
	if (!bDuplicateForPIE)
	{
		// if we get duplicated but our local meshes don't, then clear our reference to the meshes - they're not ours
		if (LocalMeshComponents.Num() > 0)
		{
			ULandscapeSplinesComponent* OuterSplines = CastChecked<ULandscapeSplinesComponent>(GetOuter());

			// we assume all meshes are duplicated or none are, to avoid testing every one
			if (LocalMeshComponents[0]->GetOuter() != OuterSplines->GetOwner())
			{
				LocalMeshComponents.Empty();
			}
		}

		UpdateSplinePoints();
	}

	Super::PostDuplicate(bDuplicateForPIE);
}

void ULandscapeSplineSegment::PostEditImport()
{
	Super::PostEditImport();

	GetOuterULandscapeSplinesComponent()->Segments.AddUnique(this);

	if (Connections[0].ControlPoint != nullptr)
	{
		Connections[0].ControlPoint->ConnectedSegments.AddUnique(FLandscapeSplineConnection(this, 0));
		Connections[1].ControlPoint->ConnectedSegments.AddUnique(FLandscapeSplineConnection(this, 1));
	}
}

void ULandscapeSplineSegment::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Flipping the tangent is only allowed if not using a socket
	if (Connections[0].SocketName != NAME_None)
	{
		Connections[0].TangentLen = FMath::Abs(Connections[0].TangentLen);
	}
	if (Connections[1].SocketName != NAME_None)
	{
		Connections[1].TangentLen = FMath::Abs(Connections[1].TangentLen);
	}

	// Don't update splines when undoing, not only is it unnecessary and expensive,
	// it also causes failed asserts in debug builds when trying to register components
	// (because the actor hasn't reset its OwnedComponents array yet)
	if (!bHackIsUndoingSplines)
	{
		const bool bUpdateCollision = PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive;
		UpdateSplinePoints(bUpdateCollision);
	}
}
#endif // WITH_EDITOR


#undef LOCTEXT_NAMESPACE
