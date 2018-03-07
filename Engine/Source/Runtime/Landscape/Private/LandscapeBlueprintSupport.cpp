// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LandscapeBlueprintSupport.cpp: Landscape blueprint functions
  =============================================================================*/

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplineRaster.h"
#include "Components/SplineComponent.h"

void ALandscapeProxy::EditorApplySpline(USplineComponent* InSplineComponent, float StartWidth, float EndWidth, float StartSideFalloff, float EndSideFalloff, float StartRoll, float EndRoll, int32 NumSubdivisions, bool bRaiseHeights, bool bLowerHeights, ULandscapeLayerInfoObject* PaintLayer)
{
#if WITH_EDITOR
	if (InSplineComponent && !GetWorld()->IsGameWorld())
	{
		TArray<FLandscapeSplineInterpPoint> Points;
		LandscapeSplineRaster::Pointify(InSplineComponent->SplineCurves.Position, Points, NumSubdivisions, 0.0f, 0.0f, StartWidth, EndWidth, StartSideFalloff, EndSideFalloff, StartRoll, EndRoll);

		FTransform SplineToWorld = InSplineComponent->GetComponentTransform();
		LandscapeSplineRaster::RasterizeSegmentPoints(GetLandscapeInfo(), MoveTemp(Points), SplineToWorld, bRaiseHeights, bLowerHeights, PaintLayer);
	}
#endif
}
