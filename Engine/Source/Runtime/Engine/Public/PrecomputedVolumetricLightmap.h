// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrecomputedVolumetricLightmap.h: Declarations for precomputed volumetric lightmap.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Math/SHMath.h"
#include "ResourceArray.h"
#include "PixelFormat.h"
#include "Math/PackedVector.h"
#include "RHI.h"
#include "RenderResource.h"

class FSceneInterface;

class FVolumetricLightmapDataLayer : public FResourceBulkDataInterface
{
public:

	FVolumetricLightmapDataLayer() :
		DataSize(0),
		Format(PF_Unknown)
	{}

	friend FArchive& operator<<(FArchive& Ar, FVolumetricLightmapDataLayer& Volume);

	virtual const void* GetResourceBulkData() const override
	{
		return Data.GetData();
	}

	virtual uint32 GetResourceBulkDataSize() const override
	{
		return Data.Num() * Data.GetTypeSize();
	}

	virtual void Discard() override
	{
		// Keep backing data in the editor for saving
		if (!GIsEditor)
		{
			Data.Empty();
		}
	}

	void Resize(int32 NewSize)
	{
		Data.Empty(NewSize);
		Data.AddUninitialized(NewSize);
		DataSize = NewSize;
	}

	ENGINE_API void CreateTexture(FIntVector Dimensions);

	TArray<uint8> Data;
	// Stored redundantly for stats after Data has been discarded
	int32 DataSize;
	EPixelFormat Format;
	FTexture3DRHIRef Texture;
};

class FVolumetricLightmapBrickData
{
public:
	FVolumetricLightmapDataLayer AmbientVector;
	FVolumetricLightmapDataLayer SHCoefficients[6];
	FVolumetricLightmapDataLayer SkyBentNormal;
	FVolumetricLightmapDataLayer DirectionalLightShadowing;

	ENGINE_API int32 GetMinimumVoxelSize() const;

	SIZE_T GetAllocatedBytes() const
	{
		SIZE_T NumBytes = AmbientVector.DataSize + SkyBentNormal.DataSize + DirectionalLightShadowing.DataSize;

		for (int32 i = 0; i < ARRAY_COUNT(SHCoefficients); i++)
		{
			NumBytes += SHCoefficients[i].DataSize;
		}

		return NumBytes;
	}
};

/** 
 * Data for a Volumetric Lightmap, built during import from Lightmass.
 * Its lifetime is managed by UMapBuildDataRegistry. 
 */
class FPrecomputedVolumetricLightmapData : public FRenderResource
{
public:

	ENGINE_API FPrecomputedVolumetricLightmapData();
	virtual ~FPrecomputedVolumetricLightmapData();

	friend FArchive& operator<<(FArchive& Ar, FPrecomputedVolumetricLightmapData& Volume);
	friend FArchive& operator<<(FArchive& Ar, FPrecomputedVolumetricLightmapData*& Volume);

	ENGINE_API void InitializeOnImport(const FBox& NewBounds, int32 InBrickSize);
	ENGINE_API void FinalizeImport();

	ENGINE_API virtual void InitRHI() override;
	ENGINE_API virtual void ReleaseRHI() override;

	SIZE_T GetAllocatedBytes() const;

	const FBox& GetBounds() const
	{
		return Bounds;
	}

	FBox Bounds;

	FIntVector IndirectionTextureDimensions;
	FVolumetricLightmapDataLayer IndirectionTexture;

	int32 BrickSize;
	FIntVector BrickDataDimensions;
	FVolumetricLightmapBrickData BrickData;

private:

	friend class FPrecomputedVolumetricLightmap;
};


/** 
 * Represents the Volumetric Lightmap for a specific ULevel.  
 */
class FPrecomputedVolumetricLightmap
{
public:

	ENGINE_API FPrecomputedVolumetricLightmap();
	~FPrecomputedVolumetricLightmap();

	ENGINE_API void AddToScene(class FSceneInterface* Scene, class UMapBuildDataRegistry* Registry, FGuid LevelBuildDataId);

	ENGINE_API void RemoveFromScene(FSceneInterface* Scene);
	
	ENGINE_API void SetData(FPrecomputedVolumetricLightmapData* NewData, FSceneInterface* Scene);

	bool IsAddedToScene() const
	{
		return bAddedToScene;
	}

	ENGINE_API void ApplyWorldOffset(const FVector& InOffset);

	// Owned by rendering thread
	// ULevel's MapBuildData GC-visible property guarantees that the FPrecomputedVolumetricLightmapData will not be deleted during the lifetime of FPrecomputedVolumetricLightmap.
	FPrecomputedVolumetricLightmapData* Data;

private:

	bool bAddedToScene;

	/** Offset from world origin. Non-zero only when world origin was rebased */
	FVector WorldOriginOffset;
};

template<typename T>
inline FLinearColor ConvertToLinearColor(T InColor)
{
	return FLinearColor(InColor);
};

template<typename T>
inline T ConvertFromLinearColor(const FLinearColor& InColor)
{
	return T(InColor);
};

template<>
inline FLinearColor ConvertToLinearColor<FColor>(FColor InColor)
{
	return InColor.ReinterpretAsLinear();
};

template<>
inline FColor ConvertFromLinearColor<FColor>(const FLinearColor& InColor)
{
	return InColor.QuantizeRound();
};

template<>
inline FLinearColor ConvertToLinearColor<FFloat3Packed>(FFloat3Packed InColor)
{
	return InColor.ToLinearColor();
};

template<>
inline FFloat3Packed ConvertFromLinearColor<FFloat3Packed>(const FLinearColor& InColor)
{
	return FFloat3Packed(InColor);
};

template<>
inline FLinearColor ConvertToLinearColor<FFixedRGBASigned8>(FFixedRGBASigned8 InColor)
{
	return InColor.ToLinearColor();
};

template<>
inline FFixedRGBASigned8 ConvertFromLinearColor<FFixedRGBASigned8>(const FLinearColor& InColor)
{
	return FFixedRGBASigned8(InColor);
};

template<>
inline uint8 ConvertFromLinearColor<uint8>(const FLinearColor& InColor)
{
	return (uint8)FMath::Clamp<int32>(FMath::RoundToInt(InColor.R * MAX_uint8), 0, MAX_uint8);
};

template<>
inline FLinearColor ConvertToLinearColor<uint8>(uint8 InColor)
{
	const float Scale = 1.0f / MAX_uint8;
	return FLinearColor(InColor * Scale, 0, 0, 0);
};

static const float GPointFilteringThreshold = .001f;

template<typename VoxelDataType>
FLinearColor FilteredVolumeLookup(FVector Coordinate, FIntVector DataDimensions, const VoxelDataType* Data)
{
	FVector CoordinateFraction(FMath::Frac(Coordinate.X), FMath::Frac(Coordinate.Y), FMath::Frac(Coordinate.Z));
	FIntVector FilterNeighborSize(CoordinateFraction.X > GPointFilteringThreshold ? 2 : 1, CoordinateFraction.Y > GPointFilteringThreshold ? 2 : 1, CoordinateFraction.Z > GPointFilteringThreshold ? 2 : 1);
	FIntVector CoordinateInt000(Coordinate);

	FLinearColor FilteredValue(0, 0, 0, 0);
	FVector FilterWeight(1.0f, 1.0f, 1.0f);

	for (int32 Z = 0; Z < FilterNeighborSize.Z; Z++)
	{
		if (FilterNeighborSize.Z > 1)
		{
			FilterWeight.Z = (Z == 0 ? 1.0f - CoordinateFraction.Z : CoordinateFraction.Z);
		}

		for (int32 Y = 0; Y < FilterNeighborSize.Y; Y++)
		{
			if (FilterNeighborSize.Y > 1)
			{
				FilterWeight.Y = (Y == 0 ? 1.0f - CoordinateFraction.Y : CoordinateFraction.Y);
			}

			for (int32 X = 0; X < FilterNeighborSize.X; X++)
			{
				if (FilterNeighborSize.X > 1)
				{
					FilterWeight.X = (X == 0 ? 1.0f - CoordinateFraction.X : CoordinateFraction.X);
				}

				const FIntVector CoordinateInt = CoordinateInt000 + FIntVector(X, Y, Z);
				const int32 LinearIndex = ((CoordinateInt.Z * DataDimensions.Y) + CoordinateInt.Y) * DataDimensions.X + CoordinateInt.X;

				FilteredValue += ConvertToLinearColor<VoxelDataType>(Data[LinearIndex]) * FilterWeight.X * FilterWeight.Y * FilterWeight.Z;
			}
		}
	}

	return FilteredValue;
}

template<typename VoxelDataType>
VoxelDataType FilteredVolumeLookupReconverted(FVector Coordinate, FIntVector DataDimensions, const VoxelDataType* Data)
{
	FVector CoordinateFraction(FMath::Frac(Coordinate.X), FMath::Frac(Coordinate.Y), FMath::Frac(Coordinate.Z));
	FIntVector FilterNeighborSize(CoordinateFraction.X > GPointFilteringThreshold ? 2 : 1, CoordinateFraction.Y > GPointFilteringThreshold ? 2 : 1, CoordinateFraction.Z > GPointFilteringThreshold ? 2 : 1);

	if (FilterNeighborSize.X == 1 && FilterNeighborSize.Y == 1 && FilterNeighborSize.Z == 1)
	{
		FIntVector CoordinateInt000(Coordinate);
		const int32 LinearIndex = ((CoordinateInt000.Z * DataDimensions.Y) + CoordinateInt000.Y) * DataDimensions.X + CoordinateInt000.X;
		return Data[LinearIndex];
	}
	else
	{
		FLinearColor FilteredValue = FilteredVolumeLookup<VoxelDataType>(Coordinate, DataDimensions, Data);
		return ConvertFromLinearColor<VoxelDataType>(FilteredValue);
	}
}

template<typename VoxelDataType>
VoxelDataType NearestVolumeLookup(FVector Coordinate, FIntVector DataDimensions, const VoxelDataType* Data)
{
	FIntVector NearestCoordinateInt(FMath::RoundToInt(Coordinate.X), FMath::RoundToInt(Coordinate.Y), FMath::RoundToInt(Coordinate.Z));
	const int32 LinearIndex = ((NearestCoordinateInt.Z * DataDimensions.Y) + NearestCoordinateInt.Y) * DataDimensions.X + NearestCoordinateInt.X;
	return Data[LinearIndex];
}

extern ENGINE_API void SampleIndirectionTexture(
	FVector IndirectionDataSourceCoordinate,
	FIntVector IndirectionTextureDimensions,
	const uint8* IndirectionTextureData,
	FIntVector& OutIndirectionBrickOffset,
	int32& OutIndirectionBrickSize);

extern ENGINE_API FVector ComputeBrickTextureCoordinate(
	FVector IndirectionDataSourceCoordinate,
	FIntVector IndirectionBrickOffset,
	int32 IndirectionBrickSize,
	int32 BrickSize);
