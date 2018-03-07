// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "DebugRenderSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "VisualLoggerRenderingComponent.generated.h"

/**
*	Transient actor used to draw visual logger data on level
*/

UCLASS()
class UVisualLoggerRenderingComponent : public UPrimitiveComponent
{
public:
	GENERATED_UCLASS_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;

private:
#if WITH_EDITOR
	FDebugDrawDelegateHelper DebugDrawDelegateHelper;
#endif
};

