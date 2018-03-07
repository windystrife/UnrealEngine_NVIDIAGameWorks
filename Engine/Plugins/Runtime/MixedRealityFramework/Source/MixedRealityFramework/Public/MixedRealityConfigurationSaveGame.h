// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/SaveGame.h"
#include "Math/Color.h" // for FLinearColor
#include "MixedRealityCaptureComponent.h" // for FChromaKeyParams
#include "MixedRealityConfigurationSaveGame.generated.h"


USTRUCT(BlueprintType)
struct FAlignmentSaveData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FVector CameraOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FVector LookAtDir = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	float FOV = 90.f;
};

USTRUCT(BlueprintType)
struct FGarbageMatteSaveData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FTransform Transform;
};

USTRUCT(BlueprintType)
struct FCompositingSaveData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FChromaKeyParams ChromaKeySettings;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	FString CaptureDeviceURL;

	UPROPERTY(BlueprintReadWrite, Category = Data)
	float DepthOffset = 0.0f;
};

/**
 * 
 */
UCLASS(BlueprintType, config = Engine)
class MIXEDREALITYFRAMEWORK_API UMixedRealityConfigurationSaveGame : public USaveGame
{
	GENERATED_UCLASS_BODY()

public:
	// Metadata about the save file

	UPROPERTY(BlueprintReadWrite, config, Category = SaveMetadata)
	FString SaveSlotName;

	UPROPERTY(BlueprintReadWrite, config, Category = SaveMetadata)
	int32 UserIndex;
	
	UPROPERTY(BlueprintReadOnly, Category = SaveMetadata)
	int32 ConfigurationSaveVersion;


	// Configuration data that is saved

	UPROPERTY(BlueprintReadWrite, Category = SaveData)
	FAlignmentSaveData AlignmentData;

	UPROPERTY(BlueprintReadWrite, Category = SaveData)
	TArray<FGarbageMatteSaveData> GarbageMatteSaveDatas;

	UPROPERTY(BlueprintReadWrite, Category = SaveData)
	FCompositingSaveData CompositingData;
};