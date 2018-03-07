// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SlateVectorArtData.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

USTRUCT()
struct FSlateMeshVertex
{
	GENERATED_USTRUCT_BODY()

	static const int32 MaxNumUVs = 6;

	FSlateMeshVertex()
	{
	}

	FSlateMeshVertex(
		  FVector2D InPos
		, FColor InColor
		, FVector2D InUV0
		, FVector2D InUV1
		, FVector2D InUV2
		, FVector2D InUV3
		, FVector2D InUV4
		, FVector2D InUV5
		)
		: Position(InPos)
		, Color(InColor)
		, UV0(InUV0)
		, UV1(InUV1)
		, UV2(InUV2)
		, UV3(InUV3)
		, UV4(InUV4)
		, UV5(InUV5)	
	{
	}

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FColor Color;

	UPROPERTY()
	FVector2D UV0;

	UPROPERTY()
	FVector2D UV1;

	UPROPERTY()
	FVector2D UV2;

	UPROPERTY()
	FVector2D UV3;

	UPROPERTY()
	FVector2D UV4;

	UPROPERTY()
	FVector2D UV5;
};

/**
 * Turn static mesh data into Slate's simple vector art format.
 */
UCLASS()
class UMG_API USlateVectorArtData : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Access the slate vertexes. */
	const TArray<FSlateMeshVertex>& GetVertexData() const;
	
	/** Access the indexes for the order in which to draw the vertexes. */
	const TArray<uint32>& GetIndexData() const;
	
	/** Material to be used with the specified vector art data. */
	UMaterialInterface* GetMaterial() const;
	
	/** Convert the material into an MID and get a pointer to the MID so that parameters can be set on it. */
	UMaterialInstanceDynamic* ConvertToMaterialInstanceDynamic();

	/** Convert the static mesh data into slate vector art on demand. Does nothing in a cooked build. */
	void EnsureValidData();

	FVector2D GetDesiredSize() const;

	FVector2D GetExtentMin() const;

	FVector2D GetExtentMax() const;

private:
	// ~ UObject Interface
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	// ~ UObject Interface

#if WITH_EDITORONLY_DATA
	/** Does the actual work of converting mesh data into slate vector art */
	void InitFromStaticMesh(const UStaticMesh& InSourceMesh);

	/** The mesh data asset from which the vector art is sourced */
	UPROPERTY(EditAnywhere, Category="Vector Art" )
	UStaticMesh* MeshAsset;

	/** The material which we are using, or the material from with the MIC was constructed. */
	UPROPERTY(Transient)
	UMaterialInterface* SourceMaterial;
#endif

	/** @see GetVertexData() */
	UPROPERTY()
	TArray<FSlateMeshVertex> VertexData;

	/** @see GetIndexData() */
	UPROPERTY()
	TArray<uint32> IndexData;

	/** @see GetMaterial() */
	UPROPERTY()
	UMaterialInterface* Material;

	UPROPERTY()
	FVector2D ExtentMin;

	UPROPERTY()
	FVector2D ExtentMax;
};
