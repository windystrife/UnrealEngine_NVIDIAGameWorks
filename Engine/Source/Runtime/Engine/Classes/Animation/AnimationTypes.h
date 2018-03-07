// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectMacros.h"
#include "AnimationTypes.generated.h"

/** A named float */
USTRUCT(BlueprintType)
struct ENGINE_API FNamedFloat
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Float")
	float Value;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Float")
	FName Name;
};

/** A named float */
USTRUCT(BlueprintType)
struct ENGINE_API FNamedVector
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vector")
	FVector Value;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vector")
	FName Name;
};

/** A named color */
USTRUCT(BlueprintType)
struct ENGINE_API FNamedColor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color")
	FColor Value;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color")
	FName Name;
};

/** A named transform */
USTRUCT(BlueprintType)
struct ENGINE_API FNamedTransform
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transform")
	FTransform Value;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transform")
	FName Name;
};

/** A pose in local space (i.e. each transform is relative to its parent) */
USTRUCT(BlueprintType)
struct FLocalSpacePose
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transforms")
	TArray<FTransform> Transforms;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Names")
	TArray<FName> Names;
};

/** A pose in component space (i.e. each transform is relative to the component's transform) */
USTRUCT(BlueprintType)
struct FComponentSpacePose
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transforms")
	TArray<FTransform> Transforms;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Names")
	TArray<FName> Names;
};
