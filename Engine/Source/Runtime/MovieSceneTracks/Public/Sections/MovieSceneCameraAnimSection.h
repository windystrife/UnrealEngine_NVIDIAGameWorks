// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "Camera/CameraTypes.h"
#include "MovieSceneCameraAnimSection.generated.h"

class UCameraAnim;

USTRUCT()
struct FMovieSceneCameraAnimSectionData
{
	GENERATED_BODY()

	FMovieSceneCameraAnimSectionData()
		: CameraAnim(nullptr)
		, PlayRate(1.f)
		, PlayScale(1.f)
		, BlendInTime(0.f)
		, BlendOutTime(0.f)
		, bLooping(false)
		, bRandomStartTime(false)
	{
	}

	/** The camera anim to play */
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	UCameraAnim* CameraAnim;

	/** How fast to play back the animation. */
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	float PlayRate;
	
	/** Scalar to control intensity of the animation. */
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	float PlayScale;
	
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	float BlendInTime;
	
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	float BlendOutTime;
	
	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	bool bLooping;
	
// @todo: Why is this not EditAnywhere?
// 	UPROPERTY(EditAnywhere, Category = "Camera Anim")
	bool bRandomStartTime;
};

/**
 *
 */
UCLASS(MinimalAPI)
class UMovieSceneCameraAnimSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:

	UMovieSceneCameraAnimSection(const FObjectInitializer& ObjectInitializer);

	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;
	virtual void PostLoad() override;

	UPROPERTY(EditAnywhere, Category="Camera Anim", meta=(ShowOnlyInnerProperties))
	FMovieSceneCameraAnimSectionData AnimData;

private:

	/** Deprecated members */
	UPROPERTY()
	UCameraAnim* CameraAnim_DEPRECATED;

	UPROPERTY()
	float PlayRate_DEPRECATED;
	
	UPROPERTY()
	float PlayScale_DEPRECATED;
	
	UPROPERTY()
	float BlendInTime_DEPRECATED;
	
	UPROPERTY()
	float BlendOutTime_DEPRECATED;
	
	UPROPERTY()
	bool bLooping_DEPRECATED;
};
