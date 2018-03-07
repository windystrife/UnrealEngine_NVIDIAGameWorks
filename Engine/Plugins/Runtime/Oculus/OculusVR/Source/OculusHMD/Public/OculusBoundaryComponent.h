// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ActorComponent.h"
#include "OculusFunctionLibrary.h"
#include "IOculusHMDModule.h"
#include "OculusBoundaryComponent.generated.h"

/* Boundary types corresponding to ovrBoundaryType enum*/
UENUM(BlueprintType)
enum class EBoundaryType : uint8
{
	Boundary_Outer	UMETA(DisplayName = "Outer Boundary"),
	Boundary_PlayArea	UMETA(DisplayName = "Play Area"),
};

/*
 * Information about relationships between a triggered boundary (EBoundaryType::Boundary_Outer or
 * EBoundaryType::Boundary_PlayArea) and a device or point in the world.
 * All dimensions, points, and vectors are returned in Unreal world coordinate space.
 */
USTRUCT(BlueprintType)
struct FBoundaryTestResult
{
	GENERATED_BODY()

	/** Is there a triggering interaction between the device/point and specified boundary? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	bool IsTriggering;

	/** Device type triggering boundary (ETrackedDeviceType::None if BoundaryTestResult corresponds to a point rather than a device) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	ETrackedDeviceType DeviceType;

	/** Distance of device/point to surface of boundary specified by BoundaryType */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	float ClosestDistance;

	/** Closest point on surface corresponding to specified boundary */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	FVector ClosestPoint;

	/** Normal of closest point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary Test Result")
	FVector ClosestPointNormal;
};

UCLASS(MinimalAPI, meta = (BlueprintSpawnableComponent), ClassGroup = OculusHMD)
class UOculusBoundaryComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	void BeginPlay() override;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** @return true if outer boundary is currently displayed, i.e., Boundary System is triggered, application has sucessfully requested visible boundaries, or user has forced boundaries on */
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	bool IsOuterBoundaryDisplayed();

	/** @return true only if Boundary System (outer boundary) is actually being triggered by a tracked device */
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	bool IsOuterBoundaryTriggered();

	/**
	 * Sets the RGB color of the outer boundary walls that are displayed automatically when HMD or
	 * Touch controllers near edges of tracking space or when RequestBoundaryVisible(true) is called.
	 * @param InBoundaryColor is desired boundary color. Alpha value is ignored by LibOVR
	 * @return true if setting color succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "Oculus Boundary System")
	bool SetOuterBoundaryColor(const FColor InBoundaryColor);

	/**
	 * Restores the default color of the outer boundary walls
	 * @return true on success, false on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Oculus Boundary System")
	bool ResetOuterBoundaryColor();

	/**
	 * @return Array of 3D points (clockwise order) defining play area at floor height
	 * Points are returned in Unreal world coordinate space.
	 */
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	TArray<FVector> GetPlayAreaPoints();

	/**
	 * @return Array of 3D points (clockwise order) defining outer boundary at floor height.
	 * Points are returned in Unreal world coordinate space.
	 */
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	TArray<FVector> GetOuterBoundaryPoints();

	/** @return Vector of play area's axis aligned dimensions (width, height, length) in Unreal world coordinate space */
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	FVector GetPlayAreaDimensions();

	/** @return Vector of outer boundary's axis aligned dimensions (width, height, length) in Unreal world coordinate space */
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	FVector GetOuterBoundaryDimensions();

	/**
	* @param Point is 3D point in Unreal world coordinate space
	* @return Information about distance from play area boundary, whether the boundary is triggered, etc.
	*/
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	FBoundaryTestResult CheckIfPointWithinPlayArea(const FVector Point);

	/**
	 * @param Point is 3D point in Unreal world coordinate space
	 * @return Information about distance from outer boundary, whether the boundary is triggered, etc.
	 */
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	FBoundaryTestResult CheckIfPointWithinOuterBounds(const FVector Point);

	/**
	* Request that boundary system (outer boundary) become visible or cancel a previous request. Can be overridden by boundary
	* system or by user. Application cannot force outer boundaries to be invisible.
	* @param BoundaryVisible: true to request outer boundaries be visible, false to cancel such a request
	* @return true on success, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "Oculus Boundary System")
	bool RequestOuterBoundaryVisible(bool BoundaryVisible);

	/**
	* Tests for interaction between DeviceType and EBoundaryType::PlayArea
	* @return BoundaryTestResult struct containing information about device/play area interaction
	*/
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	FBoundaryTestResult GetTriggeredPlayAreaInfo(ETrackedDeviceType DeviceType);

	/**
	* Getter for OuterBoundsInteractionList, an array of FBoundaryTestResults with info about interactions between outer boundary and devices triggering it.
	* Should only be called when outer boundary is being triggered. Otherwise, will return empty TArray.
	* @return TArray of structs containing information about device/outer boundary interactions for each device triggering outer boundary
	*/
	UFUNCTION(BlueprintPure, Category = "Oculus Boundary System")
	TArray<FBoundaryTestResult> GetTriggeredOuterBoundaryInfo();

	/** @param Array of structs containing information about device/outer boundary interactions for each device triggering outer boundary */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOculusOuterBoundaryTriggeredEvent, const TArray<FBoundaryTestResult>&, OuterBoundsInteractionList);

	/** When player returns within outer bounds */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOculusOuterBoundaryReturnedEvent);

	/**
	 * For outer boundary only. Devs can bind delegates via something like: BoundaryComponent->OnOuterBoundaryTriggered.AddDynamic(this, &UCameraActor::PauseGameForBoundarySystem) where
	 * PauseGameForBoundarySystem() takes a TArray<FBoundaryTestResult> parameter.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Oculus Boundary System")
	FOculusOuterBoundaryTriggeredEvent OnOuterBoundaryTriggered;

	/** For outer boundary only. Devs can bind delegates via something like: BoundaryComponent->OnOuterBoundaryReturned.AddDynamic(this, &UCameraActor::ResumeGameForBoundarySystem) */
	UPROPERTY(BlueprintAssignable, Category = "Oculus Boundary System")
	FOculusOuterBoundaryReturnedEvent OnOuterBoundaryReturned;

private:
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	/** True only if player is triggering Boundary System (outer boundary) */
	bool bIsOuterBoundaryTriggered;

	/**  Stores a list of FBoundaryTestResults passed to OnOuterBoundaryTriggered delegate functions and by GetTriggeredOuterBoundaryInfo() */
	TArray<FBoundaryTestResult> OuterBoundsInteractionList;
#endif
};