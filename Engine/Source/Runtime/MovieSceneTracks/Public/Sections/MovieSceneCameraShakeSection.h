// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Camera/CameraShake.h"
#include "MovieSceneSection.h"
#include "MovieSceneCameraShakeSection.generated.h"

USTRUCT()
struct FMovieSceneCameraShakeSectionData
{
	GENERATED_BODY()

	FMovieSceneCameraShakeSectionData()
		: ShakeClass(nullptr)
		, PlayScale(1.f)
	{
	}

	/** Class of the camera shake to play */
	UPROPERTY(EditAnywhere, Category = "Camera Shake")
	TSubclassOf<UCameraShake> ShakeClass;

	/** Scalar that affects shake intensity */
	UPROPERTY(EditAnywhere, Category = "Camera Shake")
	float PlayScale;

	UPROPERTY(EditAnywhere, Category = "Camera Shake")
	TEnumAsByte<ECameraAnimPlaySpace::Type> PlaySpace;

	UPROPERTY(EditAnywhere, Category = "Camera Shake")
	FRotator UserDefinedPlaySpace;
};

/**
 *
 */
UCLASS(MinimalAPI)
class UMovieSceneCameraShakeSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:

	UMovieSceneCameraShakeSection(const FObjectInitializer& ObjectInitializer);

	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;
	virtual void PostLoad() override;
	
	UPROPERTY(EditAnywhere, Category="Cmaera Shake", meta=(ShowOnlyInnerProperties))
	FMovieSceneCameraShakeSectionData ShakeData;

public:
	UPROPERTY()
	TSubclassOf<UCameraShake> ShakeClass_DEPRECATED;
	
	UPROPERTY()
	float PlayScale_DEPRECATED;

	UPROPERTY()
	TEnumAsByte<ECameraAnimPlaySpace::Type> PlaySpace_DEPRECATED;

	UPROPERTY()
	FRotator UserDefinedPlaySpace_DEPRECATED;
};
