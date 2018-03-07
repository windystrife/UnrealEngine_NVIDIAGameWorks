// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISnappingPolicy.h"

class FPrimitiveDrawInterface;
class FSceneView;

//////////////////////////////////////////////////////////////////////////
// FPlanarConstraintSnapPolicy

class FPlanarConstraintSnapPolicy : public ISnappingPolicy
{
public:
	FPlane SnapPlane;

	virtual bool IsEnabled() const;
	void ToggleEnabled();

private:
	bool bIsEnabled;
public:
	FPlanarConstraintSnapPolicy();

	// ISnappingPolicy interface
	virtual void SnapScale(FVector& Point, const FVector& GridBase) override;
	virtual void SnapPointToGrid(FVector& Point, const FVector& GridBase) override;
	virtual void SnapRotatorToGrid(FRotator& Rotation) override;
	virtual void ClearSnappingHelpers(bool bClearImmediately) override;
	virtual void DrawSnappingHelpers(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	// End of ISnappingPolicy interface
};
