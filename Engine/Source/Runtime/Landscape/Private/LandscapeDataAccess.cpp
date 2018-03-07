// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeDataAccess.h"
#include "LandscapeComponent.h"

#if WITH_EDITOR


LANDSCAPE_API FLandscapeComponentDataInterface::FLandscapeComponentDataInterface(ULandscapeComponent* InComponent, int32 InMipLevel) :
	Component(InComponent),
	HeightMipData(NULL),
	XYOffsetMipData(NULL),
	MipLevel(InMipLevel)
{
	// Offset and stride for this component's data in heightmap texture
	HeightmapStride = Component->HeightmapTexture->Source.GetSizeX() >> MipLevel;
	HeightmapComponentOffsetX = FMath::RoundToInt((float)(Component->HeightmapTexture->Source.GetSizeX() >> MipLevel) * Component->HeightmapScaleBias.Z);
	HeightmapComponentOffsetY = FMath::RoundToInt((float)(Component->HeightmapTexture->Source.GetSizeY() >> MipLevel) * Component->HeightmapScaleBias.W);
	HeightmapSubsectionOffset = (Component->SubsectionSizeQuads + 1) >> MipLevel;

	ComponentSizeVerts = (Component->ComponentSizeQuads + 1) >> MipLevel;
	SubsectionSizeVerts = (Component->SubsectionSizeQuads + 1) >> MipLevel;
	ComponentNumSubsections = Component->NumSubsections;

	if (MipLevel < Component->HeightmapTexture->Source.GetNumMips())
	{
		HeightMipData = (FColor*)DataInterface.LockMip(Component->HeightmapTexture, MipLevel);
		if (Component->XYOffsetmapTexture)
		{
			XYOffsetMipData = (FColor*)DataInterface.LockMip(Component->XYOffsetmapTexture, MipLevel);
		}
	}
}

LANDSCAPE_API FLandscapeComponentDataInterface::~FLandscapeComponentDataInterface()
{
	if (HeightMipData)
	{
		DataInterface.UnlockMip(Component->HeightmapTexture, MipLevel);
		if (Component->XYOffsetmapTexture)
		{
			DataInterface.UnlockMip(Component->XYOffsetmapTexture, MipLevel);
		}
	}
}

LANDSCAPE_API void FLandscapeComponentDataInterface::GetHeightmapTextureData(TArray<FColor>& OutData, bool bOkToFail)
{
	if (bOkToFail && !HeightMipData)
	{
		OutData.Empty();
		return;
	}
#if LANDSCAPE_VALIDATE_DATA_ACCESS
	check(HeightMipData);
#endif
	int32 HeightmapSize = ((Component->SubsectionSizeQuads + 1) * Component->NumSubsections) >> MipLevel;
	OutData.Empty(FMath::Square(HeightmapSize));
	OutData.AddUninitialized(FMath::Square(HeightmapSize));

	for (int32 SubY = 0; SubY < HeightmapSize; SubY++)
	{
		// X/Y of the vertex we're looking at in component's coordinates.
		int32 CompY = SubY;

		// UV coordinates of the data offset into the texture
		int32 TexV = SubY + HeightmapComponentOffsetY;

		// Copy the data
		FMemory::Memcpy(&OutData[CompY * HeightmapSize], &HeightMipData[HeightmapComponentOffsetX + TexV * HeightmapStride], HeightmapSize * sizeof(FColor));
	}
}

LANDSCAPE_API bool FLandscapeComponentDataInterface::GetWeightmapTextureData(ULandscapeLayerInfoObject* LayerInfo, TArray<uint8>& OutData)
{
	int32 LayerIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < Component->WeightmapLayerAllocations.Num(); Idx++)
	{
		if (Component->WeightmapLayerAllocations[Idx].LayerInfo == LayerInfo)
		{
			LayerIdx = Idx;
			break;
		}
	}
	if (LayerIdx < 0)
	{
		return false;
	}
	if (Component->WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex >= Component->WeightmapTextures.Num())
	{
		return false;
	}
	if (Component->WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel >= 4)
	{
		return false;
	}

	int32 WeightmapSize = ((Component->SubsectionSizeQuads + 1) * Component->NumSubsections) >> MipLevel;
	OutData.Empty(FMath::Square(WeightmapSize));
	OutData.AddUninitialized(FMath::Square(WeightmapSize));

	FColor* WeightMipData = (FColor*)DataInterface.LockMip(Component->WeightmapTextures[Component->WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex], MipLevel);

	// Channel remapping
	int32 ChannelOffsets[4] = { (int32)STRUCT_OFFSET(FColor, R), (int32)STRUCT_OFFSET(FColor, G), (int32)STRUCT_OFFSET(FColor, B), (int32)STRUCT_OFFSET(FColor, A) };

	uint8* SrcTextureData = (uint8*)WeightMipData + ChannelOffsets[Component->WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel];

	for (int32 i = 0; i < FMath::Square(WeightmapSize); i++)
	{
		OutData[i] = SrcTextureData[i * 4];
	}

	DataInterface.UnlockMip(Component->WeightmapTextures[Component->WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex], MipLevel);
	return true;
}

LANDSCAPE_API FColor* FLandscapeComponentDataInterface::GetXYOffsetData(int32 LocalX, int32 LocalY) const
{
#if LANDSCAPE_VALIDATE_DATA_ACCESS
	check(Component);
	check(LocalX >= 0 && LocalY >= 0 && LocalX < Component->ComponentSizeQuads + 1 && LocalY < Component->ComponentSizeQuads + 1);
#endif

	const int32 WeightmapSize = (Component->SubsectionSizeQuads + 1) * Component->NumSubsections;
	int32 SubNumX;
	int32 SubNumY;
	int32 SubX;
	int32 SubY;
	ComponentXYToSubsectionXY(LocalX, LocalY, SubNumX, SubNumY, SubX, SubY);

	return &XYOffsetMipData[SubX + SubNumX*SubsectionSizeVerts + (SubY + SubNumY*SubsectionSizeVerts)*WeightmapSize];
}

LANDSCAPE_API FVector FLandscapeComponentDataInterface::GetLocalVertex(int32 LocalX, int32 LocalY) const
{
	const float ScaleFactor = (float)Component->ComponentSizeQuads / (float)(ComponentSizeVerts - 1);
	float XOffset, YOffset;
	GetXYOffset(LocalX, LocalY, XOffset, YOffset);
	return FVector(LocalX * ScaleFactor + XOffset, LocalY * ScaleFactor + YOffset, LandscapeDataAccess::GetLocalHeight(GetHeight(LocalX, LocalY)));
}

LANDSCAPE_API FVector FLandscapeComponentDataInterface::GetWorldVertex(int32 LocalX, int32 LocalY) const
{
	return Component->GetComponentTransform().TransformPosition(GetLocalVertex(LocalX, LocalY));
}

LANDSCAPE_API void FLandscapeComponentDataInterface::GetWorldTangentVectors(int32 LocalX, int32 LocalY, FVector& WorldTangentX, FVector& WorldTangentY, FVector& WorldTangentZ) const
{
	FColor* Data = GetHeightData(LocalX, LocalY);
	WorldTangentZ.X = 2.f * (float)Data->B / 255.f - 1.f;
	WorldTangentZ.Y = 2.f * (float)Data->A / 255.f - 1.f;
	WorldTangentZ.Z = FMath::Sqrt(1.f - (FMath::Square(WorldTangentZ.X) + FMath::Square(WorldTangentZ.Y)));
	WorldTangentX = FVector(-WorldTangentZ.Z, 0.f, WorldTangentZ.X);
	WorldTangentY = FVector(0.f, WorldTangentZ.Z, -WorldTangentZ.Y);

	WorldTangentX = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentX);
	WorldTangentY = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentY);
	WorldTangentZ = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentZ);
}

LANDSCAPE_API void FLandscapeComponentDataInterface::GetWorldPositionTangents(int32 LocalX, int32 LocalY, FVector& WorldPos, FVector& WorldTangentX, FVector& WorldTangentY, FVector& WorldTangentZ) const
{
	FColor* Data = GetHeightData(LocalX, LocalY);

	WorldTangentZ.X = 2.f * (float)Data->B / 255.f - 1.f;
	WorldTangentZ.Y = 2.f * (float)Data->A / 255.f - 1.f;
	WorldTangentZ.Z = FMath::Sqrt(1.f - (FMath::Square(WorldTangentZ.X) + FMath::Square(WorldTangentZ.Y)));
	WorldTangentX = FVector(WorldTangentZ.Z, 0.f, -WorldTangentZ.X);
	WorldTangentY = WorldTangentZ ^ WorldTangentX;

	uint16 Height = (Data->R << 8) + Data->G;

	const float ScaleFactor = (float)Component->ComponentSizeQuads / (float)(ComponentSizeVerts - 1);
	float XOffset, YOffset;
	GetXYOffset(LocalX, LocalY, XOffset, YOffset);
	WorldPos = Component->GetComponentTransform().TransformPosition(FVector(LocalX * ScaleFactor + XOffset, LocalY * ScaleFactor + YOffset, LandscapeDataAccess::GetLocalHeight(Height)));
	WorldTangentX = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentX);
	WorldTangentY = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentY);
	WorldTangentZ = Component->GetComponentTransform().TransformVectorNoScale(WorldTangentZ);
}

#endif // WITH_EDITOR
