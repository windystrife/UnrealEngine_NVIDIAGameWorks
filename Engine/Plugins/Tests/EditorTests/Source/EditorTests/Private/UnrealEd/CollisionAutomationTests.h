// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "CollisionAutomationTests.generated.h"

/**
 * Container for detailing collision automated test data.
 */
USTRUCT()
struct FCollisionTestEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString RootShapeAsset;

	UPROPERTY()
	FString ShapeType;

	UPROPERTY()
	FHitResult	HitResult;

	FCollisionTestEntry()
	{
	}
};

USTRUCT()
struct FCollisionPerfTest
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString RootShapeAsset;

	UPROPERTY()
	FString ShapeType;

	UPROPERTY()
	FVector CreationBounds;

	UPROPERTY()
	FVector CreationElements;

	FCollisionPerfTest()
	{
	}
};

UCLASS(config=Editor)
class UCollisionAutomationTestConfigData : public UObject
{
public:
	GENERATED_BODY()
		
	UPROPERTY(config)
	TArray<FCollisionTestEntry>	ComponentSweepMultiTests;
	UPROPERTY(config)
	TArray<FCollisionTestEntry>	LineTraceSingleByChannelTests;
	
	UPROPERTY(config)
	TArray<FCollisionPerfTest>	LineTracePerformanceTests;
};
