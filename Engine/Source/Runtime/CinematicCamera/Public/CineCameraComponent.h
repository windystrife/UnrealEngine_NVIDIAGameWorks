// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Camera/CameraComponent.h"
#include "CineCameraComponent.generated.h"

class AActor;
class UMaterial;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

/** #note, this struct has a details customization in CameraFilmbackSettingsCustomization.cpp/h */
USTRUCT(BlueprintType)
struct FCameraFilmbackSettings
{
	GENERATED_USTRUCT_BODY()

	/** Horizontal size of filmback or digital sensor, in mm. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Filmback", meta = (ClampMin = "0.001", ForceUnits = mm))
	float SensorWidth;

	/** Vertical size of filmback or digital sensor, in mm. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Filmback", meta = (ClampMin = "0.001", ForceUnits = mm))
	float SensorHeight;

	/** Read-only. Computed from Sensor dimensions. */
	UPROPERTY(Interp, VisibleAnywhere, BlueprintReadOnly, Category = "Filmback")
	float SensorAspectRatio;

	bool operator==(const FCameraFilmbackSettings& Other) const
	{
		return (SensorWidth == Other.SensorWidth)
			&& (SensorHeight == Other.SensorHeight);
	}
};

/** A named bundle of filmback settings used to implement filmback presets */
USTRUCT()
struct FNamedFilmbackPreset
{
	GENERATED_USTRUCT_BODY()

	/** Name for the preset. */
	UPROPERTY()
	FString Name;

	UPROPERTY()
	FCameraFilmbackSettings FilmbackSettings;
};

/** 
 * #note, this struct has a details customization in CameraLensSettingsCustomization.cpp/h
 */
USTRUCT(BlueprintType)
struct FCameraLensSettings
{
	GENERATED_USTRUCT_BODY()

	/** Minimum focal length for this lens */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ForceUnits = mm))
	float MinFocalLength;

	/** Maximum focal length for this lens */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ForceUnits = mm))
	float MaxFocalLength;

	/** Minimum aperture for this lens (e.g. 2.8 for an f/2.8 lens) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MinFStop;

	/** Minimum aperture for this lens (e.g. 2.8 for an f/2.8 lens) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MaxFStop;

	/** Shortest distance this lens can focus on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ForceUnits = mm))
	float MinimumFocusDistance;

	bool operator==(const FCameraLensSettings& Other) const
	{
		return (MinFocalLength == Other.MinFocalLength)
			&& (MaxFocalLength == Other.MaxFocalLength)
			&& (MinFStop == Other.MinFStop)
			&& (MaxFStop == Other.MaxFStop)
			&& (MinimumFocusDistance == Other.MinimumFocusDistance);
	}
};

/** A named bundle of lens settings used to implement lens presets. */
USTRUCT()
struct FNamedLensPreset
{
	GENERATED_USTRUCT_BODY()

	/** Name for the preset. */
	UPROPERTY()
	FString Name;

	UPROPERTY()
	FCameraLensSettings LensSettings;
};

/** Supported methods for focusing the camera. */
UENUM()
enum class ECameraFocusMethod : uint8
{
	/** Disables DoF entirely. */
	None,

	/** Allows for specifying or animating exact focus distances. */
	Manual,

	/** Locks focus to specific object. */
	Tracking,
};

/** Settings to control tracking-focus mode. */
USTRUCT(BlueprintType)
struct FCameraTrackingFocusSettings
{
	GENERATED_USTRUCT_BODY()

	/** Focus distance will be tied to this actor's location. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus")
	AActor* ActorToTrack;

	/** Offset from actor position to track. Relative to actor if tracking an actor, relative to world otherwise. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus")
	FVector RelativeOffset;

	/** True to draw a debug representation of the tracked position. */
	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus")
	uint8 bDrawDebugTrackingFocusPoint : 1;

	FCameraTrackingFocusSettings()
		: ActorToTrack(nullptr),
		bDrawDebugTrackingFocusPoint(false)
	{}
};

/** Settings to control camera focus */
USTRUCT(BlueprintType)
struct FCameraFocusSettings
{
	GENERATED_USTRUCT_BODY()

	/** Which method to use to handle camera focus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Method")
	ECameraFocusMethod FocusMethod;
	
	/** Manually-controlled focus distance (manual focus mode only) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Manual Focus Settings", meta=(Units=cm))
	float ManualFocusDistance;

	/** Settings to control tracking focus (tracking focus mode only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus Settings")
	FCameraTrackingFocusSettings TrackingFocusSettings;

//~ TODO: Make this editor only again once UE-43122 has been completed.
//~	#if WITH_EDITORONLY_DATA
	/** True to draw a translucent plane at the current focus depth, for easy tweaking. */
	UPROPERTY(Transient, EditAnywhere, Category = "Focus Settings")
	uint8 bDrawDebugFocusPlane : 1;

	/** For customizing the focus plane color, in case the default doesn't show up well in your scene. */
	UPROPERTY(EditAnywhere, Category = "Focus Settings", meta = (EditCondition = "bDrawDebugFocusPlane"))
	FColor DebugFocusPlaneColor;
//~	#endif 

	/** True to use interpolation to smooth out changes in focus distance, false for focus distance changes to be instantaneous. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Settings")
	uint8 bSmoothFocusChanges : 1;
	
	/** Controls interpolation speed when smoothing focus distance changes. Ignored if bSmoothFocusChanges is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Settings", meta = (EditCondition = "bSmoothFocusChanges"))
	float FocusSmoothingInterpSpeed;

	/** Additional focus depth offset, used for manually tweaking if your chosen focus method needs adjustment */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Focus Settings")
	float FocusOffset;

	FCameraFocusSettings() : 
		FocusMethod(ECameraFocusMethod::Manual),
		ManualFocusDistance(100000.f),
#if WITH_EDITORONLY_DATA
		bDrawDebugFocusPlane(false),
		DebugFocusPlaneColor(102, 26, 204, 153),		// purple
#endif
		bSmoothFocusChanges(false),
		FocusSmoothingInterpSpeed(8.f),
		FocusOffset(0.f)
	{}
};

/**
 * A specialized version of a camera component, geared toward cinematic usage.
 */
UCLASS(HideCategories = (CameraSettings), HideFunctions = (SetFieldOfView, SetAspectRatio, SetConstraintAspectRatio), Blueprintable, ClassGroup = Camera, meta = (BlueprintSpawnableComponent), Config = Engine)
class CINEMATICCAMERA_API UCineCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	/** Default constuctor. */
	UCineCameraComponent();

	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	/** Controls the filmback of the camera. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	FCameraFilmbackSettings FilmbackSettings;

	/** Controls the camera's lens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	FCameraLensSettings LensSettings;

	/** Controls the camera's focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	FCameraFocusSettings FocusSettings;

	/** Current focal length of the camera (i.e. controls FoV, zoom) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	float CurrentFocalLength;

	/** Current aperture, in terms of f-stop (e.g. 2.8 for f/2.8) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	float CurrentAperture;
	
	/** Read-only. Control this value via FocusSettings. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Current Camera Settings")
	float CurrentFocusDistance;

#if WITH_EDITORONLY_DATA
	/** Read-only. Control this value with CurrentFocalLength (and filmback settings). */
	UPROPERTY(VisibleAnywhere, Category = "Current Camera Settings")
	float CurrentHorizontalFOV;
#endif
	
	/** Returns the horizonal FOV of the camera with current settings. */
	UFUNCTION(BlueprintCallable, Category = "Cine Camera")
	float GetHorizontalFieldOfView() const;
	
	/** Returns the vertical FOV of the camera with current settings. */
	UFUNCTION(BlueprintCallable, Category = "Cine Camera")
	float GetVerticalFieldOfView() const;

	/** Returns the filmback name of the camera with the current settings. */
	UFUNCTION(BlueprintCallable, Category = "Cine Camera")
	FString GetFilmbackPresetName() const;

	/** Set the current preset settings by preset name. */
	UFUNCTION(BlueprintCallable, Category = "Cine Camera")
	void SetFilmbackPresetByName(const FString& InPresetName);

	/** Returns the lens name of the camera with the current settings. */
	UFUNCTION(BlueprintCallable, Category = "Cine Camera")
	FString GetLensPresetName() const;

	/** Set the current lens settings by preset name. */
	UFUNCTION(BlueprintCallable, Category = "Cine Camera")
	void SetLensPresetByName(const FString& InPresetName);

	/** Returns a list of available filmback presets. */
	static TArray<FNamedFilmbackPreset> const& GetFilmbackPresets();
	
	/** Returns a list of available lens presets. */
	static TArray<FNamedLensPreset> const& GetLensPresets();

	/** Update the debug focus plane position and orientation. */
	void UpdateDebugFocusPlane();

protected:

	/** Most recent calculated focus distance. Used for interpolation. */
	float LastFocusDistance;

	/** Set to true to skip any interpolations on the next update. Resets to false automatically. */
	uint8 bResetInterpolation : 1;

	/// @cond DOXYGEN_WARNINGS
	
	virtual void PostLoad() override;
	
	/// @endcond
	
	virtual void PostInitProperties() override;
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#endif

#if WITH_EDITORONLY_DATA

	/** Mesh used for debug focus plane visualization */
	UPROPERTY(transient)
	UStaticMesh* DebugFocusPlaneMesh;

	/** Material used for debug focus plane visualization */
	UPROPERTY(transient)
	UMaterial* DebugFocusPlaneMaterial;

	/** Component for the debug focus plane visualization */
	UPROPERTY(transient)
	UStaticMeshComponent* DebugFocusPlaneComponent;

	/** Dynamic material instance for the debug focus plane visualization */
	UPROPERTY(transient)
	UMaterialInstanceDynamic* DebugFocusPlaneMID;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void ResetProxyMeshTransform() override;
#endif

	/** List of available filmback presets */
	UPROPERTY(config)
	TArray<FNamedFilmbackPreset> FilmbackPresets;

	/** List of available lens presets */
	UPROPERTY(config)
	TArray<FNamedLensPreset> LensPresets;

	/** Name of the default filmback preset */
	UPROPERTY(config)
	FString DefaultFilmbackPresetName;

	/** Name of the default lens preset */
	UPROPERTY(config)
	FString DefaultLensPresetName;
	
	/** Default focal length (will be constrained by default lens) */
	UPROPERTY(config)
	float DefaultLensFocalLength;
	
	/** Default aperture (will be constrained by default lens) */
	UPROPERTY(config)
	float DefaultLensFStop;

	virtual void UpdateCameraLens(float DeltaTime, FMinimalViewInfo& DesiredView);

	virtual void NotifyCameraCut() override;
	
private:
	void RecalcDerivedData();
	float GetDesiredFocusDistance(const FVector& InLocation) const;
	float GetWorldToMetersScale() const;

	void CreateDebugFocusPlane();
	void DestroyDebugFocusPlane();
};
