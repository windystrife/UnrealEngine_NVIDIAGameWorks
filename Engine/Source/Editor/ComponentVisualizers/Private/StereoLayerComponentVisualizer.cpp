// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StereoLayerComponentVisualizer.h"
#include "SceneManagement.h"

#include "Components/StereoLayerComponent.h"


void FStereoLayerComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	const UStereoLayerComponent* StereoLayerComp = Cast<const UStereoLayerComponent>(Component);
	if(StereoLayerComp != NULL)
	{
        FLinearColor YellowColor = FColor(231, 239, 0, 255);
        if(StereoLayerComp->StereoLayerShape == EStereoLayerShape::SLSH_QuadLayer)
        {
            const FVector2D QuadSize = StereoLayerComp->GetQuadSize() / 2.0f;
            const FBox QuadBox(FVector(0.0f, -QuadSize.X, -QuadSize.Y), FVector(0.0f, QuadSize.X, QuadSize.Y));

            DrawWireBox(PDI, StereoLayerComp->GetComponentTransform().ToMatrixWithScale(), QuadBox, YellowColor, 0);
        }
        else if(StereoLayerComp->StereoLayerShape == EStereoLayerShape::SLSH_CylinderLayer)
        {
            float ArcAngle = StereoLayerComp->CylinderOverlayArc * 180 / (StereoLayerComp->CylinderRadius * PI);
            
            FVector X = StereoLayerComp->GetComponentTransform().GetUnitAxis(EAxis::Type::X);
            FVector Y = StereoLayerComp->GetComponentTransform().GetUnitAxis(EAxis::Type::Y);
            FVector Base = StereoLayerComp->GetComponentTransform().GetLocation();
            FVector HalfHeight = FVector(0, 0, StereoLayerComp->CylinderHeight/2);
            
            FVector LeftVertex = Base + StereoLayerComp->CylinderRadius * ( FMath::Cos(ArcAngle/2 * (PI/180.0f)) * X + FMath::Sin(ArcAngle/2 * (PI/180.0f)) * Y );
            FVector RightVertex = Base + StereoLayerComp->CylinderRadius * ( FMath::Cos(-ArcAngle/2 * (PI/180.0f)) * X + FMath::Sin(-ArcAngle/2 * (PI/180.0f)) * Y );

            DrawArc(PDI, Base + HalfHeight, X, Y, -ArcAngle/2, ArcAngle/2, StereoLayerComp->CylinderRadius, 10, YellowColor, 0);
            
            DrawArc(PDI, Base - HalfHeight, X, Y, -ArcAngle/2, ArcAngle/2, StereoLayerComp->CylinderRadius, 10, YellowColor, 0);

            PDI->DrawLine( LeftVertex - HalfHeight, LeftVertex + HalfHeight, YellowColor, 0 );
            
            PDI->DrawLine( RightVertex - HalfHeight, RightVertex + HalfHeight, YellowColor, 0 );

        }
    }
}
