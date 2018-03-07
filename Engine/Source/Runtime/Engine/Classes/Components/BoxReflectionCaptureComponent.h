// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ReflectionCaptureComponent.h"
#include "BoxReflectionCaptureComponent.generated.h"

	// -> will be exported to EngineDecalClasses.h
UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent), MinimalAPI)
class UBoxReflectionCaptureComponent : public UReflectionCaptureComponent
{
	GENERATED_UCLASS_BODY()

	/** Adjust capture transition distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReflectionCapture, meta=(UIMin = "1", UIMax = "1000"))
	float BoxTransitionDistance;

	UPROPERTY()
	class UBoxComponent* PreviewInfluenceBox;

	UPROPERTY()
	class UBoxComponent* PreviewCaptureBox;

public:
	virtual void UpdatePreviewShape() override;
	virtual float GetInfluenceBoundingRadius() const override;

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface
};

