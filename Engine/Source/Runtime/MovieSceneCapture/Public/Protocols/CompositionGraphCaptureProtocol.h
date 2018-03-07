// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "IMovieSceneCaptureProtocol.h"
#include "MovieSceneCaptureProtocolSettings.h"
#include "CompositionGraphCaptureProtocol.generated.h"

class FSceneViewport;
struct FMovieSceneCaptureSettings;
struct FFrameCaptureViewExtension;

/** Used by UCompositionGraphCaptureSettings. Matches gamut oreder in TonemapCommon.usf OuputGamutMappingMatrix()*/
UENUM()
enum EHDRCaptureGamut
{
	HCGM_Rec709 UMETA(DisplayName = "Rec.709 / sRGB"),
	HCGM_P3DCI UMETA(DisplayName = "P3 D65"),
	HCGM_Rec2020 UMETA(DisplayName = "Rec.2020"),
	HCGM_ACES UMETA(DisplayName = "ACES"),
	HCGM_ACEScg UMETA(DisplayName = "ACEScg"),
	HCGM_MAX,
};

USTRUCT()
struct MOVIESCENECAPTURE_API FCompositionGraphCapturePasses
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Composition Graph Settings")
	TArray<FString> Value;
};

UCLASS(config=EditorPerProjectUserSettings, DisplayName="Composition Graph Options")
class MOVIESCENECAPTURE_API UCompositionGraphCaptureSettings : public UMovieSceneCaptureProtocolSettings
{
public:
	GENERATED_BODY()
	
	/**~ UMovieSceneCaptureProtocolSettings implementation */
	virtual void OnReleaseConfig(FMovieSceneCaptureSettings& InSettings) override;
	virtual void OnLoadConfig(FMovieSceneCaptureSettings& InSettings) override;

	/** A list of render passes to include in the capture. Leave empty to export all available passes. */
	UPROPERTY(config, EditAnywhere, Category="Composition Graph Options")
	FCompositionGraphCapturePasses IncludeRenderPasses;

	/** Whether to capture the frames as HDR textures (*.exr format) */
	UPROPERTY(config, EditAnywhere, Category="Composition Graph Options")
	bool bCaptureFramesInHDR;

	/** Compression Quality for HDR Frames (0 for no compression, 1 for default compression which can be slow) */
	UPROPERTY(config, EditAnywhere, Category="Composition Graph Options", meta = (EditCondition = "bCaptureFramesInHDR"))
	int32 HDRCompressionQuality;

	/** The color gamut to use when storing HDR captured data. The gamut depends on whether the bCaptureFramesInHDR option is enabled. */
	UPROPERTY(config, EditAnywhere, Category="Composition Graph Options", meta = (EditCondition = "bCaptureFramesInHDR"))
	TEnumAsByte<enum EHDRCaptureGamut> CaptureGamut;

	/** Custom post processing material to use for rendering */
	UPROPERTY(config, EditAnywhere, Category="Composition Graph Options", meta=(AllowedClasses=""))
	FSoftObjectPath PostProcessingMaterial;
};

struct MOVIESCENECAPTURE_API FCompositionGraphCaptureProtocol : IMovieSceneCaptureProtocol
{
	/**~ IMovieSceneCaptureProtocol implementation */
	virtual bool Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host);
	virtual void CaptureFrame(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host);
	virtual void Tick() override;
	virtual void Finalize() override;
	virtual bool HasFinishedProcessing() const override;
	/**~ End IMovieSceneCaptureProtocol implementation */

private:
	/** The viewport we are capturing from */
	TWeakPtr<FSceneViewport> SceneViewport;

	/** A view extension that we use to ensure we dump out the composition graph frames with the correct settings */
	TSharedPtr<FFrameCaptureViewExtension, ESPMode::ThreadSafe> ViewExtension;

	/** The render passes we want to export */
	TArray<FString> RenderPasses;
};
