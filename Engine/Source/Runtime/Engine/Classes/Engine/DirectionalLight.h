// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Light.h"
#include "DirectionalLight.generated.h"

class UArrowComponent;

/**
 * Implements a directional light actor.
 */
UCLASS(ClassGroup=(Lights, DirectionalLights), MinimalAPI, meta=(ChildCanTick))
class ADirectionalLight
	: public ALight
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	// Reference to editor visualization arrow
private:
	UPROPERTY()
	UArrowComponent* ArrowComponent;
#endif

public:

	// UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void LoadedFromAnotherClass(const FName& OldClassName) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:

#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	ENGINE_API UArrowComponent* GetArrowComponent() const { return ArrowComponent; }
#endif
};
