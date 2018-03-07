// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneCaptureComponent2D.h"
#include "InputCoreTypes.h" // for EControllerHand
#include "Math/Color.h" // for FLinearColor
#include "MixedRealityCaptureComponent.generated.h"

class UMediaPlayer;
class UMaterial;
class UMixedRealityBillboard;
class UChildActorComponent;
class AMixedRealityProjectionActor;
class UMotionControllerComponent;
class AStaticMeshActor;
class USceneCaptureComponent2D;
class UMixedRealityGarbageMatteCaptureComponent;
class UTextureRenderTarget2D;

/**
 *	
 */
USTRUCT(BlueprintType)
struct FChromaKeyParams
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(BlueprintReadWrite, Category = ChromaKeying)
	FLinearColor ChromaColor = FLinearColor(0.122f, 0.765f, 0.261, 1.f);

	/* 
	 * Colors matching the chroma color up to this tolerance level will be completely 
	 * cut out. The higher the value the more that is cut out. A value of zero 
	 * means that the chroma color has to be an exact match for the pixel to be 
	 * completely transparent. 
	 */
	UPROPERTY(BlueprintReadWrite, Category = ChromaKeying)
	float ChromaClipThreshold = 0.26f;

	/* 
	 * Colors that differ from the chroma color beyond this tolerance level will 
	 * be fully opaque. The higher the number, the more transparency gradient there 
	 * will be along edges. This is expected to be greater than the 'Chroma Clip 
	 * Threshold' param. If this matches the 'Chroma Clip Threshold' then there will 
	 * be no transparency gradient (what isn't clipped will be fully opaque).
	 */
	UPROPERTY(BlueprintReadWrite, Category = ChromaKeying)
	float ChromaToleranceCap = 0.53f;

	/*
	 * An exponent param that governs how soft/hard the semi-translucent edges are. 
	 * Larger numbers will cause the translucency to fall off faster, shrinking 
	 * the silhouette and smoothing it out. Larger numbers can also be used to hide 
	 * splotchy artifacts. Values under 1 will cause the transparent edges to 
	 * increase in harshness (approaching on opaque).
	 */
	UPROPERTY(BlueprintReadWrite, Category = ChromaKeying)
	float EdgeSoftness = 10.f;

public:
	void ApplyToMaterial(UMaterialInstanceDynamic* Material) const;
};

/**
 *	
 */
UCLASS(ClassGroup = Rendering, editinlinenew, config = Engine, meta = (BlueprintSpawnableComponent))
class MIXEDREALITYFRAMEWORK_API UMixedRealityCaptureComponent : public USceneCaptureComponent2D
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=VideoCapture)
	UMediaPlayer* MediaSource;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetVidProjectionMat, Category=VideoCapture)
	UMaterialInterface* VideoProcessingMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetChromaSettings, Category=VideoCapture)
	FChromaKeyParams ChromaKeySettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Tracking)
	bool bAutoTracking;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Tracking, meta=(editcondition="bAutoTracking"))
	EControllerHand TrackingDevice;

public:
	//~ UObject interface

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:
	//~ UActorComponent interface

	virtual void OnRegister() override;
	virtual void InitializeComponent() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif 

public:
	//~ USceneComponent interface

#if WITH_EDITOR
	virtual bool GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut) override;
#endif 

public:
	//~ USceneCaptureComponent interface

	virtual const AActor* GetViewOwner() const override;

public:
	//~ Blueprint API	

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration", meta = (DisplayName = "SaveAsDefaultConfiguration"))
	bool SaveAsDefaultConfiguration_K2(); // non-const to make it an exec node
	bool SaveAsDefaultConfiguration() const;

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration", meta = (DisplayName="SaveConfiguration"))
	bool SaveConfiguration_K2(const FString& SlotName, int32 UserIndex); // non-const to make it an exec node
	bool SaveConfiguration(const FString& SlotName, int32 UserIndex) const;

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration")
	bool LoadDefaultConfiguration();

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration")
	bool LoadConfiguration(const FString& SlotName, int32 UserIndex);

	/**
	* Set an external garbage matte actor to be used instead of the mixed reality component's
	* normal configuration save game based actor.  This is used during garbage matte setup to
	* preview the garbage mask in realtime.
	*/
	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration")
	void SetExternalGarbageMatteActor(AActor* Actor);

	/**
	* Clear the external garbage matte actor so that the mixed reality component goes
	* back to its normal behavior where it uses a garbage matte actor spawned based on 
	* the mixed reality configuration save file information.
	*/
	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration")
	void ClearExternalGarbageMatteActor();

	/**
	* Set color parameter in the mixed reality material with which pixels will be max combined
	* so that they are obviously visible while setting up the garbage mattes and green screen.
	*
	* @param NewColor			(in) The color used.
	*/
	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration")
	void SetUnmaskedPixelHighlightColor(const FLinearColor& NewColor);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Calibration")
	bool IsCalibrated() const { return bCalibrated; }

	UFUNCTION(BlueprintSetter)
	void SetVidProjectionMat(UMaterialInterface* NewMaterial);

	UFUNCTION(BlueprintSetter)
	void SetChromaSettings(const FChromaKeyParams& NewChromaSettings);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Tracking")
	void SetDeviceAttachment(EControllerHand DeviceId);

	UFUNCTION(BlueprintCallable, Category = "MixedReality|Tracking")
	void DetatchFromDevice();

	UFUNCTION(BlueprintCallable, Category = "MixedReality|VideoCapture")
	void SetCaptureDevice(const FString& DeviceURL);

	UFUNCTION(BlueprintPure, Category = "MixedReality|Projection", meta=(DisplayName="GetProjectionActor"))
	AActor* GetProjectionActor_K2() const;
	AMixedRealityProjectionActor* GetProjectionActor() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	UTextureRenderTarget2D* GarbageMatteCaptureTextureTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	UStaticMesh* GarbageMatteMesh;
public:

	/**
	 *
	 */
	void RefreshCameraFeed();

	/**
	 *
	 */
	void RefreshDevicePairing();

private:
	void AttachMediaListeners() const;
	void DetachMediaListeners() const;

	UFUNCTION() // needs to be a UFunction for binding purposes
	void OnVideoFeedOpened(FString MediaUrl);

	void  RefreshProjectionDimensions();
	float GetDesiredAspectRatio() const;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	UStaticMesh* ProxyMesh;

	UPROPERTY(Transient)
	UStaticMeshComponent* ProxyMeshComponent;
#endif
	bool bCalibrated = false;

	UPROPERTY(Transient)
	UChildActorComponent* ProjectionActor;

	UPROPERTY(Transient)
	UMotionControllerComponent* PairedTracker;

	UPROPERTY(Transient)
	UMixedRealityGarbageMatteCaptureComponent* GarbageMatteCaptureComponent;

	FString CaptureDeviceURL;
};
