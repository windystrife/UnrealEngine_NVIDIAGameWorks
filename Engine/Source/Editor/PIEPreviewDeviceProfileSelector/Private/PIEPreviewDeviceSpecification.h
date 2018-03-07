// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PIEPreviewDeviceSpecification.generated.h"

UENUM()
enum class EPIEPreviewDeviceType : uint8
{
	Unset,
	Android,
	IOS,
	TVOS,
	MAX,
};

UCLASS()
class PIEPREVIEWDEVICEPROFILESELECTOR_API UPIEPreviewDeviceSpecification : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
	EPIEPreviewDeviceType PreviewDeviceType;

	UPROPERTY()
	FString GPUFamily;
	UPROPERTY()
	FString GLVersion;
	UPROPERTY()
	FString VulkanVersion;
	UPROPERTY()
	FString AndroidVersion;
	UPROPERTY()
	FString DeviceMake;
	UPROPERTY()
	FString DeviceModel;
	UPROPERTY()
	bool UsingHoudini;
};

USTRUCT()
struct FPIERHIOverrideState
{
public:
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
	int32 MaxShadowDepthBufferSizeX;
	UPROPERTY()
	int32 MaxShadowDepthBufferSizeY;
	UPROPERTY()
	int32 MaxTextureDimensions;
	UPROPERTY()
	int32 MaxCubeTextureDimensions;
	UPROPERTY()
	bool SupportsRenderTargetFormat_PF_G8;
	UPROPERTY()
	bool SupportsRenderTargetFormat_PF_FloatRGBA;
	UPROPERTY()
	bool SupportsMultipleRenderTargets;
	UPROPERTY()
	bool SupportsInstancing;
};

USTRUCT()
struct FPIEAndroidDeviceProperties
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString GPUFamily;
	UPROPERTY()
	FString GLVersion;
	UPROPERTY()
	FString VulkanVersion;
	UPROPERTY()
	FString AndroidVersion;
	UPROPERTY()
	FString DeviceMake;
	UPROPERTY()
	FString DeviceModel;
	UPROPERTY()
	bool UsingHoudini;

	UPROPERTY()
	FPIERHIOverrideState GLES2RHIState;

	UPROPERTY()
	FPIERHIOverrideState GLES31RHIState;

// 	UPROPERTY()
// 	FPIERHIOverrideState VulkanRHIState;
};

USTRUCT()
struct FPIEIOSDeviceProperties
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString DeviceModel;

	UPROPERTY()
	FPIERHIOverrideState GLES2RHIState;

	UPROPERTY()
	FPIERHIOverrideState MetalRHIState;
};



USTRUCT()
struct FPIEPreviewDeviceSpecifications
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	EPIEPreviewDeviceType DevicePlatform;

	UPROPERTY()
	FPIEAndroidDeviceProperties AndroidProperties;

	UPROPERTY()
	FPIEIOSDeviceProperties IOSProperties;
};
