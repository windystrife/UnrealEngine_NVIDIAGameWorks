// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/StaticMeshComponent.h"
#include "ControlPointMeshComponent.generated.h"

UCLASS(MinimalAPI)
class UControlPointMeshComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	uint32 bSelected:1;
#endif

	//Begin UPrimitiveComponent Interface
#if WITH_EDITOR
	virtual bool ShouldRenderSelected() const override
	{
		return Super::ShouldRenderSelected() || bSelected;
	}
#endif
	//End UPrimitiveComponent Interface
};
