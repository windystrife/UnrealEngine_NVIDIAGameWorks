// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Components/StaticMeshComponent.h"

#include "LandscapeMeshProxyComponent.generated.h"

class ALandscapeProxy;
class FPrimitiveSceneProxy;

UCLASS(MinimalAPI)
class ULandscapeMeshProxyComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	ULandscapeMeshProxyComponent(const FObjectInitializer& ObjectInitializer);

private:
	/* The landscape this proxy was generated for */
	UPROPERTY()
	FGuid LandscapeGuid;

	/* The components this proxy was generated for */
	UPROPERTY()
	TArray<FIntPoint> ProxyComponentBases;

	/* LOD level proxy was generated for */
	UPROPERTY()
	int8 ProxyLOD;

public:
	LANDSCAPE_API void InitializeForLandscape(ALandscapeProxy* Landscape, int8 InProxyLOD);
	
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};

