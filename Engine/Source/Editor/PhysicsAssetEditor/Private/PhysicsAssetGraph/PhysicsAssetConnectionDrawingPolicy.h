// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConnectionDrawingPolicy.h"

class FPhysicsAssetConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FPhysicsAssetConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements)
		: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	{
		// We dont draw arrows here
		ArrowImage = nullptr;
		ArrowRadius = FVector2D(0.0f, 0.0f);
	}

	virtual FVector2D ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const override
	{
		const int32 Tension = FMath::Abs<int32>(Start.X - End.X);
		return Tension * FVector2D(1.0f, 0);
	}
};
