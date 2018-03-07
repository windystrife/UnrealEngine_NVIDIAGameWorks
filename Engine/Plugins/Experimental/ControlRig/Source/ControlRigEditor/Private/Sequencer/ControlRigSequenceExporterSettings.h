// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ControlRigSequenceExporterSettings.generated.h"

class UControlRigSequence;
class UAnimSequence;
class USkeletalMesh;

/** Settings object used to display a details panel for export settings */
UCLASS()
class UControlRigSequenceExporterSettings : public UObject
{
	GENERATED_BODY()

	UControlRigSequenceExporterSettings()
		: FrameRate(30.0f)
	{}

public:
	/** The skeletal mesh to use when exporting */
	UPROPERTY(EditAnywhere, Category = "Export Settings", meta = (DisplayThumbnail = "true"))
	UControlRigSequence* Sequence;

	/** The skeletal mesh to use when exporting */
	UPROPERTY(EditAnywhere, Category = "Export Settings", meta = (DisplayThumbnail = "true"))
	USkeletalMesh* SkeletalMesh;

	/** Animation sequence to export to */
	UPROPERTY(EditAnywhere, Category = "Export Settings", meta = (DisplayThumbnail = "true"))
	UAnimSequence* AnimationSequence;

	/** The frame rate (in Hz) to export the animation at */
	UPROPERTY(EditAnywhere, Category = "Export Settings", meta=(UIMin=0.00000001, ClampMin=0.00000001))
	float FrameRate;
};