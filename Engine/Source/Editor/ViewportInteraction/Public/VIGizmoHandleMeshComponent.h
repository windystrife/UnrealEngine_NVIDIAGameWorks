// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/StaticMeshComponent.h"
#include "VIGizmoHandleMeshComponent.generated.h"

UCLASS(hidecategories = Object)
class UGizmoHandleMeshComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	/** Default constructor */
	UGizmoHandleMeshComponent();

	//~ Begin UPrimitiveComponent Interface
	virtual class FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface
};