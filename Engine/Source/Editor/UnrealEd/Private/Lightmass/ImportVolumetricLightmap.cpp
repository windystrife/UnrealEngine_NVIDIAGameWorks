// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ImportVolumetricLightmap.h
=============================================================================*/

#include "Lightmass/Lightmass.h"
#include "EngineDefines.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PrecomputedVolumetricLightmap.h"
#include "Engine/MapBuildDataRegistry.h"
#include "ImportExport.h"
#include "UnrealEngine.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "RenderUtils.h"

#define LOCTEXT_NAMESPACE "ImportVolumetricLightmap"

DEFINE_LOG_CATEGORY_STATIC(LogVolumetricLightmapImport, Log, All);

void CopyBrickToAtlasVolumeTexture(int32 FormatSize, FIntVector AtlasSize, FIntVector BrickMin, FIntVector BrickSize, const uint8* SourceData, uint8* DestData)
{
	const int32 SourcePitch = BrickSize.X * FormatSize;
	const int32 Pitch = AtlasSize.X * FormatSize;
	const int32 DepthPitch = AtlasSize.X * AtlasSize.Y * FormatSize;

	// Copy each row into the correct position in the global volume texture
	for (int32 ZIndex = 0; ZIndex < BrickSize.Z; ZIndex++)
	{
		const int32 DestZIndex = (BrickMin.Z + ZIndex) * DepthPitch + BrickMin.X * FormatSize;
		const int32 SourceZIndex = ZIndex * BrickSize.Y * SourcePitch;

		for (int32 YIndex = 0; YIndex < BrickSize.Y; YIndex++)
		{
			const int32 DestIndex = DestZIndex + (BrickMin.Y + YIndex) * Pitch;
			const int32 SourceIndex = SourceZIndex + YIndex * SourcePitch;
			FMemory::Memcpy((uint8*)&DestData[DestIndex], (const uint8*)&SourceData[SourceIndex], SourcePitch);
		}
	}
}

int32 ComputeLinearVoxelIndex(FIntVector VoxelCoordinate, FIntVector VolumeDimensions)
{
	return (VoxelCoordinate.Z * VolumeDimensions.Y + VoxelCoordinate.Y) * VolumeDimensions.X + VoxelCoordinate.X;
}

struct FImportedVolumetricLightmapBrick
{
	FIntVector IndirectionTexturePosition;
	int32 TreeDepth;
	float AverageClosestGeometryDistance;
	TArray<FFloat3Packed> AmbientVector;
	TArray<FColor> SHCoefficients[6];
	TArray<FColor> SkyBentNormal;
	TArray<uint8> DirectionalLightShadowing;
	TArray<Lightmass::FIrradianceVoxelImportProcessingData> TaskVoxelImportProcessingData;
};

struct FImportedVolumetricLightmapTaskData
{
	TArray<FImportedVolumetricLightmapBrick> Bricks;
};

FIntVector ComputeBrickLayoutPosition(int32 BrickLayoutAllocation, FIntVector BrickLayoutDimensions)
{
	const FIntVector BrickPosition(
		BrickLayoutAllocation % BrickLayoutDimensions.X,
		(BrickLayoutAllocation / BrickLayoutDimensions.X) % BrickLayoutDimensions.Y,
		BrickLayoutAllocation / (BrickLayoutDimensions.X * BrickLayoutDimensions.Y));

	return BrickPosition;
}

bool CopyFromBrickmapTexel(
	FVector IndirectionDataSourceCoordinate,
	FIntVector LocalCellDestCoordinate, 
	int32 MinDestinationNumBottomLevelBricks,
	int32 BrickSize, 
	FIntVector BrickLayoutPosition,
	const FPrecomputedVolumetricLightmapData& CurrentLevelData,
	FVolumetricLightmapBrickData& BrickData)
{
	const FVector IndirectionCoordMax(FVector(CurrentLevelData.IndirectionTextureDimensions) * (1 - GPointFilteringThreshold));

	if (IndirectionDataSourceCoordinate.X < 0 || IndirectionDataSourceCoordinate.Y < 0 || IndirectionDataSourceCoordinate.Z < 0 || 
		IndirectionDataSourceCoordinate.X > IndirectionCoordMax.X || IndirectionDataSourceCoordinate.Y > IndirectionCoordMax.Y || IndirectionDataSourceCoordinate.Z > IndirectionCoordMax.Z)
	{
		return false;
	}

	FIntVector IndirectionBrickOffset;
	int32 IndirectionBrickSize;

	checkSlow(GPixelFormats[CurrentLevelData.IndirectionTexture.Format].BlockBytes == sizeof(uint8) * 4);
	SampleIndirectionTexture(IndirectionDataSourceCoordinate, CurrentLevelData.IndirectionTextureDimensions, CurrentLevelData.IndirectionTexture.Data.GetData(), IndirectionBrickOffset, IndirectionBrickSize);

	if (IndirectionBrickSize > MinDestinationNumBottomLevelBricks)
	{
		const FVector BrickTextureCoordinate = ComputeBrickTextureCoordinate(IndirectionDataSourceCoordinate, IndirectionBrickOffset, IndirectionBrickSize, BrickSize);

		const FIntVector DestCellPosition = BrickLayoutPosition + LocalCellDestCoordinate;
		const int32 LinearDestCellIndex = ComputeLinearVoxelIndex(DestCellPosition, CurrentLevelData.BrickDataDimensions);

		*(FFloat3Packed*)&BrickData.AmbientVector.Data[LinearDestCellIndex * sizeof(FFloat3Packed)] = FilteredVolumeLookupReconverted<FFloat3Packed>(BrickTextureCoordinate, CurrentLevelData.BrickDataDimensions, (const FFloat3Packed*)BrickData.AmbientVector.Data.GetData());
		
		for (int32 i = 0; i < ARRAY_COUNT(BrickData.SHCoefficients); i++)
		{
			*(FColor*)&BrickData.SHCoefficients[i].Data[LinearDestCellIndex * sizeof(FColor)] = FilteredVolumeLookupReconverted<FColor>(BrickTextureCoordinate, CurrentLevelData.BrickDataDimensions, (const FColor*)BrickData.SHCoefficients[i].Data.GetData());
		}

		if (BrickData.SkyBentNormal.Data.Num() > 0)
		{
			*(FColor*)&BrickData.SkyBentNormal.Data[LinearDestCellIndex * sizeof(FColor)] = FilteredVolumeLookupReconverted<FColor>(BrickTextureCoordinate, CurrentLevelData.BrickDataDimensions, (const FColor*)BrickData.SkyBentNormal.Data.GetData());
		}
	
		*(uint8*)&BrickData.DirectionalLightShadowing.Data[LinearDestCellIndex * sizeof(uint8)] = FilteredVolumeLookupReconverted<uint8>(BrickTextureCoordinate, CurrentLevelData.BrickDataDimensions, (const uint8*)BrickData.DirectionalLightShadowing.Data.GetData());

		return true;
	}

	return false;
}

static NSwarm::TChannelFlags LM_VOLUMETRICLIGHTMAP_CHANNEL_FLAGS = NSwarm::SWARM_JOB_CHANNEL_READ;

void FLightmassProcessor::ImportIrradianceTasks(bool& bGenerateSkyShadowing, TArray<FImportedVolumetricLightmapTaskData>& TaskDataArray)
{
	TList<FGuid>* Element = CompletedVolumetricLightmapTasks.ExtractAll();

	while (Element)
	{
		// If this task has not already been imported, import it now
		TList<FGuid>* NextElement = Element->Next;

		const FString ChannelName = Lightmass::CreateChannelName(Element->Element, Lightmass::LM_VOLUMETRICLIGHTMAP_VERSION, Lightmass::LM_VOLUMETRICLIGHTMAP_EXTENSION);
		const int32 Channel = Swarm.OpenChannel( *ChannelName, LM_VOLUMETRICLIGHTMAP_CHANNEL_FLAGS );
		if (Channel >= 0)
		{
			TaskDataArray.AddDefaulted();
			FImportedVolumetricLightmapTaskData& NewTaskData = TaskDataArray.Last();

			int32 NumBricks;
			Swarm.ReadChannel(Channel, &NumBricks, sizeof(NumBricks));

			NewTaskData.Bricks.Empty(NumBricks);

			for (int32 BrickIndex = 0; BrickIndex < NumBricks; BrickIndex++)
			{
				NewTaskData.Bricks.AddDefaulted();
				FImportedVolumetricLightmapBrick& NewBrick = NewTaskData.Bricks.Last();
				Swarm.ReadChannel(Channel, &NewBrick.IndirectionTexturePosition, sizeof(NewBrick.IndirectionTexturePosition));
				Swarm.ReadChannel(Channel, &NewBrick.TreeDepth, sizeof(NewBrick.TreeDepth));
				Swarm.ReadChannel(Channel, &NewBrick.AverageClosestGeometryDistance, sizeof(NewBrick.AverageClosestGeometryDistance));
				ReadArray(Channel, NewBrick.AmbientVector);

				for (int32 i = 0; i < ARRAY_COUNT(NewBrick.SHCoefficients); i++)
				{
					ReadArray(Channel, NewBrick.SHCoefficients[i]);
				}
				
				ReadArray(Channel, NewBrick.SkyBentNormal);
				ReadArray(Channel, NewBrick.DirectionalLightShadowing);

				bGenerateSkyShadowing = bGenerateSkyShadowing || NewBrick.SkyBentNormal.Num() > 0;

				ReadArray(Channel, NewBrick.TaskVoxelImportProcessingData);
			}

			Swarm.CloseChannel(Channel);
		}
		else
		{
			UE_LOG(LogVolumetricLightmapImport, Fatal,  TEXT("Error, failed to import volumetric lightmap %s with error code %d"), *ChannelName, Channel );
		}

		delete Element;
		Element = NextElement;
	}
}

// For debugging
bool bOverwriteVoxelsInsideGeometryWithNeighbors = true;
// Introduces artifacts especially with bright static spot lights
bool bFilterWithNeighbors = false;

struct FFilteredBrickData
{
	FFloat3Packed AmbientVector;
	FColor SHCoefficients[ARRAY_COUNT(FImportedVolumetricLightmapBrick::SHCoefficients)];
};

void FilterWithNeighbors(
	const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth,
	int32 BrickStartAllocation,
	const TArray<Lightmass::FIrradianceVoxelImportProcessingData>& VoxelImportProcessingData,
	FVector DetailCellSize,
	int32 CurrentDepth,
	FIntVector BrickLayoutDimensions,
	const Lightmass::FVolumetricLightmapSettings& VolumetricLightmapSettings,
	const FVolumetricLightmapBrickData& InputBrickData,
	FPrecomputedVolumetricLightmapData& CurrentLevelData)
{
	int32 BrickSize = VolumetricLightmapSettings.BrickSize;
	int32 PaddedBrickSize = BrickSize + 1;
	const int32 BrickSizeLog2 = FMath::FloorLog2(BrickSize);
	const float InvBrickSize = 1.0f / BrickSize;
	const float FilterWithNeighborsDistanceToSurfaceThreshold = DetailCellSize.GetMax() * 1;

	TArray<FFilteredBrickData> FilteredBrickData;
	TArray<bool> FilteredBrickDataValid;

	FilteredBrickData.Empty(BrickSize * BrickSize * BrickSize);
	FilteredBrickData.AddZeroed(BrickSize * BrickSize * BrickSize);
	FilteredBrickDataValid.Empty(BrickSize * BrickSize * BrickSize);
	FilteredBrickDataValid.AddZeroed(BrickSize * BrickSize * BrickSize);

	// Fill in voxels which are inside geometry with their valid neighbors
	for (int32 BrickIndex = 0; BrickIndex < BricksAtCurrentDepth.Num(); BrickIndex++)
	{
		const FImportedVolumetricLightmapBrick& Brick = *BricksAtCurrentDepth[BrickIndex];

		// Initialize temporary brick data to invalid
		FPlatformMemory::Memzero(FilteredBrickDataValid.GetData(), FilteredBrickDataValid.Num() * FilteredBrickDataValid.GetTypeSize());

		checkSlow(Brick.TreeDepth == CurrentDepth);

		const FIntVector BrickLayoutPosition = ComputeBrickLayoutPosition(BrickStartAllocation + BrickIndex, BrickLayoutDimensions) * PaddedBrickSize;
		const int32 DetailCellsPerCurrentLevelBrick = 1 << ((VolumetricLightmapSettings.MaxRefinementLevels - Brick.TreeDepth) * BrickSizeLog2);
		const int32 NumBottomLevelBricks = DetailCellsPerCurrentLevelBrick / BrickSize;
		const FVector IndirectionTexturePosition = FVector(Brick.IndirectionTexturePosition);

		for (int32 Z = 0; Z < BrickSize; Z++)
		{
			for (int32 Y = 0; Y < BrickSize; Y++)
			{
				for (int32 X = 0; X < BrickSize; X++)
				{
					FIntVector VoxelCoordinate(X, Y, Z);
					const int32 LinearVoxelIndex = ComputeLinearVoxelIndex(VoxelCoordinate, FIntVector(BrickSize, BrickSize, BrickSize));
					Lightmass::FIrradianceVoxelImportProcessingData VoxelImportData = Brick.TaskVoxelImportProcessingData[LinearVoxelIndex];

					if (((VoxelImportData.bInsideGeometry && bOverwriteVoxelsInsideGeometryWithNeighbors)
						|| (VoxelImportData.ClosestGeometryDistance > FilterWithNeighborsDistanceToSurfaceThreshold && bFilterWithNeighbors))
						// Don't modify border voxels
						&& !VoxelImportData.bBorderVoxel)
					{
						//@todo - filter SkyBentNormal from neighbors too
						FLinearColor AmbientVector = FLinearColor(0, 0, 0, 0);
						FLinearColor SHCoefficients[ARRAY_COUNT(Brick.SHCoefficients)];

						for (int32 i = 0; i < ARRAY_COUNT(SHCoefficients); i++)
						{
							SHCoefficients[i] = FLinearColor(0, 0, 0, 0);
						}
	
						float TotalWeight = 0.0f;

						for (int32 NeighborZ = -1; NeighborZ <= 1; NeighborZ++)
						{
							for (int32 NeighborY = -1; NeighborY <= 1; NeighborY++)
							{
								for (int32 NeighborX = -1; NeighborX <= 1; NeighborX++)
								{
									const FVector NeighborIndirectionDataSourceCoordinate = IndirectionTexturePosition + FVector(X + NeighborX, Y + NeighborY, Z + NeighborZ) * InvBrickSize * NumBottomLevelBricks;
									const FIntVector NeighborVoxelCoordinate(X + NeighborX, Y + NeighborY, Z + NeighborZ);

									if ((NeighborVoxelCoordinate != VoxelCoordinate || !VoxelImportData.bInsideGeometry)
										&& NeighborIndirectionDataSourceCoordinate.X >= 0.0f 
										&& NeighborIndirectionDataSourceCoordinate.Y >= 0.0f 
										&& NeighborIndirectionDataSourceCoordinate.Z >= 0.0f
										&& NeighborIndirectionDataSourceCoordinate.X < CurrentLevelData.IndirectionTextureDimensions.X 
										&& NeighborIndirectionDataSourceCoordinate.Y < CurrentLevelData.IndirectionTextureDimensions.Y 
										&& NeighborIndirectionDataSourceCoordinate.Z < CurrentLevelData.IndirectionTextureDimensions.Z)
									{
										FIntVector IndirectionBrickOffset;
										int32 IndirectionBrickSize;

										checkSlow(GPixelFormats[CurrentLevelData.IndirectionTexture.Format].BlockBytes == sizeof(uint8) * 4);
										SampleIndirectionTexture(NeighborIndirectionDataSourceCoordinate, CurrentLevelData.IndirectionTextureDimensions, CurrentLevelData.IndirectionTexture.Data.GetData(), IndirectionBrickOffset, IndirectionBrickSize);

										// Only filter from bricks with equal density, to avoid reading from uninitialized padding
										// This causes seams but they fall at density transitions so not noticeable
										if (IndirectionBrickSize == NumBottomLevelBricks)
										{
											const FVector BrickTextureCoordinate = ComputeBrickTextureCoordinate(NeighborIndirectionDataSourceCoordinate, IndirectionBrickOffset, IndirectionBrickSize, BrickSize);
											Lightmass::FIrradianceVoxelImportProcessingData NeighborVoxelImportData = NearestVolumeLookup<Lightmass::FIrradianceVoxelImportProcessingData>(BrickTextureCoordinate, CurrentLevelData.BrickDataDimensions, VoxelImportProcessingData.GetData());

											if (!NeighborVoxelImportData.bInsideGeometry && !NeighborVoxelImportData.bBorderVoxel)
											{
												const float Weight = 1.0f / FMath::Max<float>(FMath::Abs(NeighborX) + FMath::Abs(NeighborY) + FMath::Abs(NeighborZ), .5f);
												FLinearColor NeighborAmbientVector = FilteredVolumeLookup<FFloat3Packed>(BrickTextureCoordinate, CurrentLevelData.BrickDataDimensions, (const FFloat3Packed*)InputBrickData.AmbientVector.Data.GetData());
												AmbientVector += NeighborAmbientVector * Weight;

												for (int32 i = 0; i < ARRAY_COUNT(SHCoefficients); i++)
												{
													static_assert(ARRAY_COUNT(SHCoefficients) == 6, "Assuming 2 SHCoefficient vectors per color channel");

													// Weight by ambient before filtering, normalized SH coefficients don't filter properly
													float AmbientCoefficient = NeighborAmbientVector.Component(i / 2);
													SHCoefficients[i] += AmbientCoefficient * Weight * FilteredVolumeLookup<FColor>(BrickTextureCoordinate, CurrentLevelData.BrickDataDimensions, (const FColor*)InputBrickData.SHCoefficients[i].Data.GetData());
												}
												TotalWeight += Weight;
											}
										}
									}
								}
							}
						}

						if (TotalWeight > 0.0f)
						{
							// Store filtered output to temporary brick data to avoid order dependent results between voxels
							// This still produces order dependent filtering between neighboring bricks
							FilteredBrickDataValid[LinearVoxelIndex] = true;

							const float InvTotalWeight = 1.0f / TotalWeight;

							const FLinearColor FilteredAmbientColor = AmbientVector * InvTotalWeight;
							FilteredBrickData[LinearVoxelIndex].AmbientVector = ConvertFromLinearColor<FFloat3Packed>(FilteredAmbientColor);

							for (int32 i = 0; i < ARRAY_COUNT(SHCoefficients); i++)
							{
								static_assert(ARRAY_COUNT(SHCoefficients) == 6, "Assuming 2 SHCoefficient vectors per color channel");

								float AmbientCoefficient = FMath::Max(FilteredAmbientColor.Component(i / 2), KINDA_SMALL_NUMBER);

								FilteredBrickData[LinearVoxelIndex].SHCoefficients[i] = ConvertFromLinearColor<FColor>(SHCoefficients[i] * InvTotalWeight / AmbientCoefficient);
							}
						}
					}
				}
			}
		}

		for (int32 Z = 0; Z < BrickSize; Z++)
		{
			for (int32 Y = 0; Y < BrickSize; Y++)
			{
				for (int32 X = 0; X < BrickSize; X++)
				{
					FIntVector VoxelCoordinate(X, Y, Z);
					const int32 LinearVoxelIndex = ComputeLinearVoxelIndex(VoxelCoordinate, FIntVector(BrickSize, BrickSize, BrickSize));

					// Write filtered voxel data back to the original
					if (FilteredBrickDataValid[LinearVoxelIndex])
					{
						const int32 LinearDestCellIndex = ComputeLinearVoxelIndex(VoxelCoordinate + BrickLayoutPosition, CurrentLevelData.BrickDataDimensions);

						FFloat3Packed* DestAmbientVector = (FFloat3Packed*)&CurrentLevelData.BrickData.AmbientVector.Data[LinearDestCellIndex * sizeof(FFloat3Packed)];
						*DestAmbientVector = FilteredBrickData[LinearVoxelIndex].AmbientVector;

						for (int32 i = 0; i < ARRAY_COUNT(FilteredBrickData[LinearVoxelIndex].SHCoefficients); i++)
						{
							FColor* DestCoefficients = (FColor*)&CurrentLevelData.BrickData.SHCoefficients[i].Data[LinearDestCellIndex * sizeof(FColor)];
							*DestCoefficients = FilteredBrickData[LinearVoxelIndex].SHCoefficients[i];
						}
					}
				}
			}
		}
	}
}

void StitchDetailBricksWithLowDensityNeighbors(
	const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth,
	int32 BrickStartAllocation,
	int32 CurrentDepth,
	FIntVector BrickLayoutDimensions,
	const Lightmass::FVolumetricLightmapSettings& VolumetricLightmapSettings,
	FPrecomputedVolumetricLightmapData& CurrentLevelData)
{
	int32 BrickSize = VolumetricLightmapSettings.BrickSize;
	int32 PaddedBrickSize = BrickSize + 1;
	const int32 BrickSizeLog2 = FMath::FloorLog2(BrickSize);
	const float InvBrickSize = 1.0f / BrickSize;

	// Stitch all higher density bricks with neighboring lower density bricks
	for (int32 BrickIndex = 0; BrickIndex < BricksAtCurrentDepth.Num(); BrickIndex++)
	{
		const FImportedVolumetricLightmapBrick& Brick = *BricksAtCurrentDepth[BrickIndex];
		const FIntVector BrickLayoutPosition = ComputeBrickLayoutPosition(BrickStartAllocation + BrickIndex, BrickLayoutDimensions) * PaddedBrickSize;
		const int32 DetailCellsPerCurrentLevelBrick = 1 << ((VolumetricLightmapSettings.MaxRefinementLevels - Brick.TreeDepth) * BrickSizeLog2);
		const int32 NumBottomLevelBricks = DetailCellsPerCurrentLevelBrick / BrickSize;
		const FVector IndirectionTexturePosition = FVector(Brick.IndirectionTexturePosition);

		int32 X, Y, Z = 0;

		// Iterate over unique data on the edge of the brick which needs to match padding on lower resolution bricks
		for (X = 0, Z = 0; Z < BrickSize; Z++)
		{
			for (Y = 0; Y < BrickSize; Y++)
			{
				FVector IndirectionDataSourceCoordinate = IndirectionTexturePosition + FVector(X, Y, Z) * InvBrickSize * NumBottomLevelBricks;

				for (int32 StitchDirection = 1; StitchDirection < 8; StitchDirection++)
				{
					FVector StitchSourceCoordinate = IndirectionDataSourceCoordinate;

					if ((StitchDirection & 1) && X == 0)
					{
						StitchSourceCoordinate.X -= GPointFilteringThreshold * 2;
					}

					if ((StitchDirection & 2) && Y == 0)
					{
						StitchSourceCoordinate.Y -= GPointFilteringThreshold * 2;
					}

					if ((StitchDirection & 4) && Z == 0)
					{
						StitchSourceCoordinate.Z -= GPointFilteringThreshold * 2;
					}

					if (StitchSourceCoordinate != IndirectionDataSourceCoordinate)
					{
						bool bStitched = CopyFromBrickmapTexel(
							StitchSourceCoordinate,
							FIntVector(X, Y, Z),
							// Restrict copies to only read from bricks that are lower effective resolution (higher NumBottomLevelBricks)
							NumBottomLevelBricks,
							BrickSize,
							BrickLayoutPosition,
							CurrentLevelData,
							CurrentLevelData.BrickData);

						if (bStitched)
						{
							break;
						}
					}
				}
			}
		}

		for (Z = 0, Y = 0; Y < BrickSize; Y++)
		{
			for (X = 1; X < BrickSize; X++)
			{
				FVector IndirectionDataSourceCoordinate = IndirectionTexturePosition + FVector(X, Y, Z) * InvBrickSize * NumBottomLevelBricks;
									
				for (int32 StitchDirection = 1; StitchDirection < 8; StitchDirection++)
				{
					FVector StitchSourceCoordinate = IndirectionDataSourceCoordinate;

					if ((StitchDirection & 1) && X == 0)
					{
						StitchSourceCoordinate.X -= GPointFilteringThreshold * 2;
					}

					if ((StitchDirection & 2) && Y == 0)
					{
						StitchSourceCoordinate.Y -= GPointFilteringThreshold * 2;
					}

					if ((StitchDirection & 4) && Z == 0)
					{
						StitchSourceCoordinate.Z -= GPointFilteringThreshold * 2;
					}

					if (StitchSourceCoordinate != IndirectionDataSourceCoordinate)
					{
						bool bStitched = CopyFromBrickmapTexel(
							StitchSourceCoordinate,
							FIntVector(X, Y, Z),
							// Restrict copies to only read from bricks that are lower effective resolution (higher NumBottomLevelBricks)
							NumBottomLevelBricks,
							BrickSize,
							BrickLayoutPosition,
							CurrentLevelData,
							CurrentLevelData.BrickData);

						if (bStitched)
						{
							break;
						}
					}
				}
			}
		}

		for (Y = 0, Z = 1; Z < BrickSize; Z++)
		{
			for (X = 1; X < BrickSize; X++)
			{
				FVector IndirectionDataSourceCoordinate = IndirectionTexturePosition + FVector(X, Y, Z) * InvBrickSize * NumBottomLevelBricks;
								
				for (int32 StitchDirection = 1; StitchDirection < 8; StitchDirection++)
				{
					FVector StitchSourceCoordinate = IndirectionDataSourceCoordinate;

					if ((StitchDirection & 1) && X == 0)
					{
						StitchSourceCoordinate.X -= GPointFilteringThreshold * 2;
					}

					if ((StitchDirection & 2) && Y == 0)
					{
						StitchSourceCoordinate.Y -= GPointFilteringThreshold * 2;
					}

					if ((StitchDirection & 4) && Z == 0)
					{
						StitchSourceCoordinate.Z -= GPointFilteringThreshold * 2;
					}

					if (StitchSourceCoordinate != IndirectionDataSourceCoordinate)
					{
						bool bStitched = CopyFromBrickmapTexel(
							StitchSourceCoordinate,
							FIntVector(X, Y, Z),
							// Restrict copies to only read from bricks that are lower effective resolution (higher NumBottomLevelBricks)
							NumBottomLevelBricks,
							BrickSize,
							BrickLayoutPosition,
							CurrentLevelData,
							CurrentLevelData.BrickData);

						if (bStitched)
						{
							break;
						}
					}
				}
			}
		}
	}
}

void CopyPaddingFromUniqueData(
	const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth,
	int32 BrickStartAllocation,
	int32 CurrentDepth,
	FIntVector BrickLayoutDimensions,
	const Lightmass::FVolumetricLightmapSettings& VolumetricLightmapSettings,
	const FPrecomputedVolumetricLightmapData& CurrentLevelData,
	FVolumetricLightmapBrickData& BrickData)
{
	int32 BrickSize = VolumetricLightmapSettings.BrickSize;
	int32 PaddedBrickSize = BrickSize + 1;
	const int32 BrickSizeLog2 = FMath::FloorLog2(BrickSize);
	const float InvBrickSize = 1.0f / BrickSize;

	for (int32 BrickIndex = 0; BrickIndex < BricksAtCurrentDepth.Num(); BrickIndex++)
	{
		const FImportedVolumetricLightmapBrick& Brick = *BricksAtCurrentDepth[BrickIndex];
		const FIntVector BrickLayoutPosition = ComputeBrickLayoutPosition(BrickStartAllocation + BrickIndex, BrickLayoutDimensions) * PaddedBrickSize;
		const int32 DetailCellsPerCurrentLevelBrick = 1 << ((VolumetricLightmapSettings.MaxRefinementLevels - Brick.TreeDepth) * BrickSizeLog2);
		const int32 NumBottomLevelBricks = DetailCellsPerCurrentLevelBrick / BrickSize;
		const FVector IndirectionTexturePosition = FVector(Brick.IndirectionTexturePosition);

		int32 X, Y, Z = 0;

		// Iterate over padding voxels
		for (X = PaddedBrickSize - 1, Z = 0; Z < PaddedBrickSize; Z++)
		{
			for (Y = 0; Y < PaddedBrickSize; Y++)
			{
				const FVector IndirectionDataSourceCoordinate = IndirectionTexturePosition + FVector(X, Y, Z) * InvBrickSize * NumBottomLevelBricks;

				// Overwrite padding with unique data from this same coordinate in the indirection texture
				CopyFromBrickmapTexel(
					IndirectionDataSourceCoordinate,
					FIntVector(X, Y, Z),
					0,
					BrickSize,
					BrickLayoutPosition,
					CurrentLevelData,
					BrickData);
			}
		}

		for (Z = PaddedBrickSize - 1, Y = 0; Y < PaddedBrickSize; Y++)
		{
			for (X = 0; X < PaddedBrickSize; X++)
			{
				const FVector IndirectionDataSourceCoordinate = IndirectionTexturePosition + FVector(X, Y, Z) * InvBrickSize * NumBottomLevelBricks;

				CopyFromBrickmapTexel(
					IndirectionDataSourceCoordinate,
					FIntVector(X, Y, Z),
					0,
					BrickSize,
					BrickLayoutPosition,
					CurrentLevelData,
					BrickData);
			}
		}

		for (Y = PaddedBrickSize - 1, Z = 0; Z < PaddedBrickSize; Z++)
		{
			for (X = 0; X < PaddedBrickSize; X++)
			{
				const FVector IndirectionDataSourceCoordinate = IndirectionTexturePosition + FVector(X, Y, Z) * InvBrickSize * NumBottomLevelBricks;

				CopyFromBrickmapTexel(
					IndirectionDataSourceCoordinate,
					FIntVector(X, Y, Z),
					0,
					BrickSize,
					BrickLayoutPosition,
					CurrentLevelData,
					BrickData);
			}
		}
	}
}

FVector GetLookupPositionAwayFromBorder(int32 PaddedBrickSize, FIntVector LocalCellCoordinate)
{
	FVector LookupCoordinate(LocalCellCoordinate);

	if (LocalCellCoordinate.X == PaddedBrickSize - 1)
	{
		LookupCoordinate.X -= 1;
	}

	if (LocalCellCoordinate.Y == PaddedBrickSize - 1)
	{
		LookupCoordinate.Y -= 1;
	}

	if (LocalCellCoordinate.Z == PaddedBrickSize - 1)
	{
		LookupCoordinate.Z -= 1;
	}

	return LookupCoordinate;
}

void CopyVolumeBorderFromInterior(
	const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth,
	int32 BrickStartAllocation,
	int32 CurrentDepth,
	FIntVector BrickLayoutDimensions,
	const Lightmass::FVolumetricLightmapSettings& VolumetricLightmapSettings,
	FPrecomputedVolumetricLightmapData& CurrentLevelData,
	FVolumetricLightmapBrickData& BrickData)
{
	int32 BrickSize = VolumetricLightmapSettings.BrickSize;
	int32 PaddedBrickSize = BrickSize + 1;
	const int32 BrickSizeLog2 = FMath::FloorLog2(BrickSize);
	const float InvBrickSize = 1.0f / BrickSize;

	for (int32 BrickIndex = 0; BrickIndex < BricksAtCurrentDepth.Num(); BrickIndex++)
	{
		const FImportedVolumetricLightmapBrick& Brick = *BricksAtCurrentDepth[BrickIndex];
		const FIntVector BrickLayoutPosition = ComputeBrickLayoutPosition(BrickStartAllocation + BrickIndex, BrickLayoutDimensions) * PaddedBrickSize;
		const int32 DetailCellsPerCurrentLevelBrick = 1 << ((VolumetricLightmapSettings.MaxRefinementLevels - Brick.TreeDepth) * BrickSizeLog2);
		const int32 NumBottomLevelBricks = DetailCellsPerCurrentLevelBrick / BrickSize;
		const FVector IndirectionTexturePosition = FVector(Brick.IndirectionTexturePosition);

		// Operate on bricks on the edge of the volume covered by the indirection texture
		if (Brick.IndirectionTexturePosition.X + NumBottomLevelBricks == CurrentLevelData.IndirectionTextureDimensions.X)
		{
			int32 X, Y, Z = 0;

			// Iterate over padding voxels
			for (X = PaddedBrickSize - 1, Z = 0; Z < PaddedBrickSize; Z++)
			{
				for (Y = 0; Y < PaddedBrickSize; Y++)
				{
					const FVector LookupPosition = GetLookupPositionAwayFromBorder(PaddedBrickSize, FIntVector(X, Y, Z));
					const FVector IndirectionDataSourceCoordinate = IndirectionTexturePosition + LookupPosition * InvBrickSize * NumBottomLevelBricks;

					// Overwrite padding on the edge of the volume with neighboring data inside the volume
					CopyFromBrickmapTexel(
						IndirectionDataSourceCoordinate,
						FIntVector(X, Y, Z),
						0,
						BrickSize,
						BrickLayoutPosition,
						CurrentLevelData,
						BrickData);
				}
			}
		}

		if (Brick.IndirectionTexturePosition.Y + NumBottomLevelBricks == CurrentLevelData.IndirectionTextureDimensions.Y)
		{
			int32 X, Y, Z = 0;

			// Iterate over padding voxels
			for (Y = PaddedBrickSize - 1, Z = 0; Z < PaddedBrickSize; Z++)
			{
				for (X = 0; X < PaddedBrickSize; X++)
				{
					const FVector LookupPosition = GetLookupPositionAwayFromBorder(PaddedBrickSize, FIntVector(X, Y, Z));
					const FVector IndirectionDataSourceCoordinate = IndirectionTexturePosition + LookupPosition * InvBrickSize * NumBottomLevelBricks;

					CopyFromBrickmapTexel(
						IndirectionDataSourceCoordinate,
						FIntVector(X, Y, Z),
						0,
						BrickSize,
						BrickLayoutPosition,
						CurrentLevelData,
						BrickData);
				}
			}
		}

		if (Brick.IndirectionTexturePosition.Z + NumBottomLevelBricks == CurrentLevelData.IndirectionTextureDimensions.Z)
		{
			int32 X, Y, Z = 0;

			// Iterate over padding voxels
			for (Z = PaddedBrickSize - 1, Y = 0; Y < PaddedBrickSize; Y++)
			{
				for (X = 0; X < PaddedBrickSize; X++)
				{
					const FVector LookupPosition = GetLookupPositionAwayFromBorder(PaddedBrickSize, FIntVector(X, Y, Z));
					const FVector IndirectionDataSourceCoordinate = IndirectionTexturePosition + LookupPosition * InvBrickSize * NumBottomLevelBricks;

					CopyFromBrickmapTexel(
						IndirectionDataSourceCoordinate,
						FIntVector(X, Y, Z),
						0,
						BrickSize,
						BrickLayoutPosition,
						CurrentLevelData,
						BrickData);
				}
			}
		}
	}
}

int32 TrimBricks(
	TArray<TArray<const FImportedVolumetricLightmapBrick*>>& BricksByDepth,
	const Lightmass::FVolumetricLightmapSettings& VolumetricLightmapSettings,
	int32 VoxelSizeBytes,
	const float MaximumBrickMemoryMb)
{
	int32 NumBricksBeforeTrimming = 0;

	for (int32 CurrentDepth = 0; CurrentDepth < VolumetricLightmapSettings.MaxRefinementLevels; CurrentDepth++)
	{
		const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth = BricksByDepth[CurrentDepth];
		NumBricksBeforeTrimming += BricksAtCurrentDepth.Num();
	}

	const int32 PaddedBrickSize = VolumetricLightmapSettings.BrickSize + 1;
	TArray<const FImportedVolumetricLightmapBrick*>& HighestDensityBricks = BricksByDepth[VolumetricLightmapSettings.MaxRefinementLevels - 1];

	const int32 BrickSizeBytes = VoxelSizeBytes * PaddedBrickSize * PaddedBrickSize * PaddedBrickSize;
	const int32 MaxBrickBytes = MaximumBrickMemoryMb * 1024 * 1024;
	const int32 NumBricksBudgeted = FMath::DivideAndRoundUp(MaxBrickBytes, BrickSizeBytes);
	const int32 NumBricksToRemove = FMath::Clamp<int32>(NumBricksBeforeTrimming - NumBricksBudgeted, 0, HighestDensityBricks.Num());

	if (NumBricksToRemove > 0)
	{
		struct FCompareBricksByVisualImpact
		{
			FORCEINLINE bool operator()(const FImportedVolumetricLightmapBrick& A, const FImportedVolumetricLightmapBrick& B) const
			{
				// Sort smallest to largest
				return A.AverageClosestGeometryDistance < B.AverageClosestGeometryDistance;
			}
		};

		HighestDensityBricks.Sort(FCompareBricksByVisualImpact());
		HighestDensityBricks.RemoveAt(HighestDensityBricks.Num() - NumBricksToRemove, NumBricksToRemove);
	}

	return NumBricksToRemove;
}

void BuildIndirectionTexture(
	const TArray<TArray<const FImportedVolumetricLightmapBrick*>>& BricksByDepth,
	const Lightmass::FVolumetricLightmapSettings& VolumetricLightmapSettings,
	int32 MaxBricksInLayoutOneDim,
	FIntVector BrickLayoutDimensions,
	int32 IndirectionTextureDataStride,
	FPrecomputedVolumetricLightmapData& CurrentLevelData)
{
	const int32 BrickSizeLog2 = FMath::FloorLog2(VolumetricLightmapSettings.BrickSize);

	int32 BrickStartAllocation = 0;

	for (int32 CurrentDepth = 0; CurrentDepth < VolumetricLightmapSettings.MaxRefinementLevels; CurrentDepth++)
	{
		const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth = BricksByDepth[CurrentDepth];

		for (int32 BrickIndex = 0; BrickIndex < BricksAtCurrentDepth.Num(); BrickIndex++)
		{
			const FImportedVolumetricLightmapBrick& Brick = *BricksAtCurrentDepth[BrickIndex];
			const FIntVector BrickLayoutPosition = ComputeBrickLayoutPosition(BrickStartAllocation + BrickIndex, BrickLayoutDimensions);
			check(BrickLayoutPosition.X < MaxBricksInLayoutOneDim && BrickLayoutPosition.Y < MaxBricksInLayoutOneDim && BrickLayoutPosition.Z < MaxBricksInLayoutOneDim);
			checkSlow(IndirectionTextureDataStride == sizeof(uint8) * 4);

			const int32 DetailCellsPerCurrentLevelBrick = 1 << ((VolumetricLightmapSettings.MaxRefinementLevels - Brick.TreeDepth) * BrickSizeLog2);
			const int32 NumBottomLevelBricks = DetailCellsPerCurrentLevelBrick / VolumetricLightmapSettings.BrickSize;
			check(NumBottomLevelBricks < MaxBricksInLayoutOneDim);

			for (int32 Z = 0; Z < NumBottomLevelBricks; Z++)
			{
				for (int32 Y = 0; Y < NumBottomLevelBricks; Y++)
				{
					for (int32 X = 0; X < NumBottomLevelBricks; X++)
					{
						const FIntVector IndirectionDestDataCoordinate = Brick.IndirectionTexturePosition + FIntVector(X, Y, Z);
						const int32 IndirectionDestDataIndex = ((IndirectionDestDataCoordinate.Z * CurrentLevelData.IndirectionTextureDimensions.Y) + IndirectionDestDataCoordinate.Y) * CurrentLevelData.IndirectionTextureDimensions.X + IndirectionDestDataCoordinate.X;
						uint8* IndirectionVoxelPtr = (uint8*)&CurrentLevelData.IndirectionTexture.Data[IndirectionDestDataIndex * IndirectionTextureDataStride];
						*(IndirectionVoxelPtr + 0) = BrickLayoutPosition.X;
						*(IndirectionVoxelPtr + 1) = BrickLayoutPosition.Y;
						*(IndirectionVoxelPtr + 2) = BrickLayoutPosition.Z;
						*(IndirectionVoxelPtr + 3) = NumBottomLevelBricks;
					}
				}
			}
		}

		BrickStartAllocation += BricksAtCurrentDepth.Num();
	}
}

// For debugging
bool bStitchDetailBricksWithLowDensityNeighbors = true;
bool bCopyPaddingFromUniqueData = true;
bool bCopyVolumeBorderFromInterior = true;

void FLightmassProcessor::ImportVolumetricLightmap()
{
	const double StartTime = FPlatformTime::Seconds();

	Lightmass::FVolumetricLightmapSettings VolumetricLightmapSettings;
	GetLightmassExporter()->SetVolumetricLightmapSettings(VolumetricLightmapSettings);

	const int32 BrickSize = VolumetricLightmapSettings.BrickSize;
	const int32 PaddedBrickSize = VolumetricLightmapSettings.BrickSize + 1;
	const int32 MaxBricksInLayoutOneDim = 1 << 8;
	const int32 MaxBrickTextureLayoutSize = PaddedBrickSize * MaxBricksInLayoutOneDim;

	TArray<FImportedVolumetricLightmapTaskData> TaskDataArray;
	TaskDataArray.Empty(Exporter->VolumetricLightmapTaskGuids.Num());

	bool bGenerateSkyShadowing = false;

	ImportIrradianceTasks(bGenerateSkyShadowing, TaskDataArray);

	checkf(TaskDataArray.Num() == Exporter->VolumetricLightmapTaskGuids.Num(), TEXT("Import Volumetric Lightmap failed: Expected %u tasks, only found %u"), Exporter->VolumetricLightmapTaskGuids.Num(), TaskDataArray.Num());

	TArray<TArray<const FImportedVolumetricLightmapBrick*>> BricksByDepth;
	BricksByDepth.Empty(VolumetricLightmapSettings.MaxRefinementLevels);
	BricksByDepth.AddDefaulted(VolumetricLightmapSettings.MaxRefinementLevels);

	for (int32 TaskDataIndex = 0; TaskDataIndex < TaskDataArray.Num(); TaskDataIndex++)
	{
		const FImportedVolumetricLightmapTaskData& TaskData = TaskDataArray[TaskDataIndex];

		for (int32 BrickIndex = 0; BrickIndex < TaskData.Bricks.Num(); BrickIndex++)
		{
			const FImportedVolumetricLightmapBrick& Brick = TaskData.Bricks[BrickIndex];
			BricksByDepth[Brick.TreeDepth].Add(&Brick);
		}
	}

	ULevel* StorageLevel = System.LightingScenario ? System.LightingScenario : System.GetWorld()->PersistentLevel;
	UMapBuildDataRegistry* Registry = StorageLevel->GetOrCreateMapBuildData();
	FPrecomputedVolumetricLightmapData& CurrentLevelData = Registry->AllocateLevelPrecomputedVolumetricLightmapBuildData(StorageLevel->LevelBuildDataId);

	{
		CurrentLevelData.InitializeOnImport(FBox(VolumetricLightmapSettings.VolumeMin, VolumetricLightmapSettings.VolumeMin + VolumetricLightmapSettings.VolumeSize), BrickSize);

		CurrentLevelData.BrickData.AmbientVector.Format = PF_FloatR11G11B10;
		CurrentLevelData.BrickData.SkyBentNormal.Format = PF_B8G8R8A8;
		CurrentLevelData.BrickData.DirectionalLightShadowing.Format = PF_G8;

		for (int32 i = 0; i < ARRAY_COUNT(CurrentLevelData.BrickData.SHCoefficients); i++)
		{
			CurrentLevelData.BrickData.SHCoefficients[i].Format = PF_B8G8R8A8;
		}
	}

	const float MaximumBrickMemoryMb = System.GetWorld()->GetWorldSettings()->LightmassSettings.VolumetricLightmapMaximumBrickMemoryMb;
	const int32 NumBottomLevelBricksTrimmed = TrimBricks(BricksByDepth, VolumetricLightmapSettings, CurrentLevelData.BrickData.GetMinimumVoxelSize(), MaximumBrickMemoryMb);

	int32 BrickTextureLinearAllocator = 0;

	for (int32 CurrentDepth = 0; CurrentDepth < VolumetricLightmapSettings.MaxRefinementLevels; CurrentDepth++)
	{
		const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth = BricksByDepth[CurrentDepth];
		BrickTextureLinearAllocator += BricksAtCurrentDepth.Num();
	}

	FIntVector BrickLayoutDimensions;
	BrickLayoutDimensions.X = FMath::Min(BrickTextureLinearAllocator, MaxBricksInLayoutOneDim);
	BrickTextureLinearAllocator = FMath::DivideAndRoundUp(BrickTextureLinearAllocator, BrickLayoutDimensions.X);
	BrickLayoutDimensions.Y = FMath::Min(BrickTextureLinearAllocator, MaxBricksInLayoutOneDim);
	BrickTextureLinearAllocator = FMath::DivideAndRoundUp(BrickTextureLinearAllocator, BrickLayoutDimensions.Y);
	BrickLayoutDimensions.Z = FMath::Min(BrickTextureLinearAllocator, MaxBricksInLayoutOneDim);

	const int32 BrickSizeLog2 = FMath::FloorLog2(BrickSize);
	const int32 DetailCellsPerTopLevelBrick = 1 << (VolumetricLightmapSettings.MaxRefinementLevels * BrickSizeLog2);
	const int32 IndirectionCellsPerTopLevelCell = DetailCellsPerTopLevelBrick / BrickSize;

	int32 IndirectionTextureDataStride;
	{
		CurrentLevelData.IndirectionTextureDimensions = VolumetricLightmapSettings.TopLevelGridSize * IndirectionCellsPerTopLevelCell;
		CurrentLevelData.IndirectionTexture.Format = PF_R8G8B8A8_UINT;
		IndirectionTextureDataStride = GPixelFormats[CurrentLevelData.IndirectionTexture.Format].BlockBytes;

		const int32 TotalIndirectionTextureSize = CurrentLevelData.IndirectionTextureDimensions.X * CurrentLevelData.IndirectionTextureDimensions.Y * CurrentLevelData.IndirectionTextureDimensions.Z;
		CurrentLevelData.IndirectionTexture.Resize(TotalIndirectionTextureSize * IndirectionTextureDataStride);
	}

	BuildIndirectionTexture(BricksByDepth, VolumetricLightmapSettings, MaxBricksInLayoutOneDim, BrickLayoutDimensions, IndirectionTextureDataStride, CurrentLevelData);

	TArray<Lightmass::FIrradianceVoxelImportProcessingData> VoxelImportProcessingData;

	{
		CurrentLevelData.BrickDataDimensions = BrickLayoutDimensions * PaddedBrickSize;
		const int32 TotalBrickDataSize = CurrentLevelData.BrickDataDimensions.X * CurrentLevelData.BrickDataDimensions.Y * CurrentLevelData.BrickDataDimensions.Z;

		CurrentLevelData.BrickData.AmbientVector.Resize(TotalBrickDataSize * GPixelFormats[CurrentLevelData.BrickData.AmbientVector.Format].BlockBytes);

		if (bGenerateSkyShadowing)
		{
			CurrentLevelData.BrickData.SkyBentNormal.Resize(TotalBrickDataSize * GPixelFormats[CurrentLevelData.BrickData.SkyBentNormal.Format].BlockBytes);
		}

		CurrentLevelData.BrickData.DirectionalLightShadowing.Resize(TotalBrickDataSize * GPixelFormats[CurrentLevelData.BrickData.DirectionalLightShadowing.Format].BlockBytes);

		for (int32 i = 0; i < ARRAY_COUNT(CurrentLevelData.BrickData.SHCoefficients); i++)
		{
			const int32 Stride = GPixelFormats[CurrentLevelData.BrickData.SHCoefficients[i].Format].BlockBytes;
			CurrentLevelData.BrickData.SHCoefficients[i].Resize(TotalBrickDataSize * Stride);
		}

		VoxelImportProcessingData.Empty(TotalBrickDataSize);
		VoxelImportProcessingData.AddZeroed(TotalBrickDataSize);
	}

	int32 BrickStartAllocation = 0;

	for (int32 CurrentDepth = 0; CurrentDepth < VolumetricLightmapSettings.MaxRefinementLevels; CurrentDepth++)
	{
		const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth = BricksByDepth[CurrentDepth];

		for (int32 BrickIndex = 0; BrickIndex < BricksAtCurrentDepth.Num(); BrickIndex++)
		{
			const FImportedVolumetricLightmapBrick& Brick = *BricksAtCurrentDepth[BrickIndex];
			const FIntVector BrickLayoutPosition = ComputeBrickLayoutPosition(BrickStartAllocation + BrickIndex, BrickLayoutDimensions) * PaddedBrickSize;

			CopyBrickToAtlasVolumeTexture(
				GPixelFormats[CurrentLevelData.BrickData.AmbientVector.Format].BlockBytes,
				CurrentLevelData.BrickDataDimensions,
				BrickLayoutPosition,
				FIntVector(BrickSize),
				(const uint8*)Brick.AmbientVector.GetData(),
				CurrentLevelData.BrickData.AmbientVector.Data.GetData());

			for (int32 i = 0; i < ARRAY_COUNT(Brick.SHCoefficients); i++)
			{
				CopyBrickToAtlasVolumeTexture(
					GPixelFormats[CurrentLevelData.BrickData.SHCoefficients[i].Format].BlockBytes,
					CurrentLevelData.BrickDataDimensions,
					BrickLayoutPosition,
					FIntVector(BrickSize),
					(const uint8*)Brick.SHCoefficients[i].GetData(),
					CurrentLevelData.BrickData.SHCoefficients[i].Data.GetData());
			}

			if (bGenerateSkyShadowing)
			{
				CopyBrickToAtlasVolumeTexture(
					GPixelFormats[CurrentLevelData.BrickData.SkyBentNormal.Format].BlockBytes,
					CurrentLevelData.BrickDataDimensions,
					BrickLayoutPosition,
					FIntVector(BrickSize),
					(const uint8*)Brick.SkyBentNormal.GetData(),
					CurrentLevelData.BrickData.SkyBentNormal.Data.GetData());
			}

			CopyBrickToAtlasVolumeTexture(
				GPixelFormats[CurrentLevelData.BrickData.DirectionalLightShadowing.Format].BlockBytes,
				CurrentLevelData.BrickDataDimensions,
				BrickLayoutPosition,
				FIntVector(BrickSize),
				(const uint8*)Brick.DirectionalLightShadowing.GetData(),
				CurrentLevelData.BrickData.DirectionalLightShadowing.Data.GetData());

			CopyBrickToAtlasVolumeTexture(
				VoxelImportProcessingData.GetTypeSize(),
				CurrentLevelData.BrickDataDimensions,
				BrickLayoutPosition,
				FIntVector(BrickSize),
				(const uint8*)Brick.TaskVoxelImportProcessingData.GetData(),
				(uint8*)VoxelImportProcessingData.GetData());
		}

		BrickStartAllocation += BricksAtCurrentDepth.Num();
	}

	const float InvBrickSize = 1.0f / BrickSize;
	const FVector DetailCellSize = VolumetricLightmapSettings.VolumeSize / FVector(VolumetricLightmapSettings.TopLevelGridSize * DetailCellsPerTopLevelBrick);

	if (bOverwriteVoxelsInsideGeometryWithNeighbors || bFilterWithNeighbors)
	{
		BrickStartAllocation = 0;

		for (int32 CurrentDepth = 0; CurrentDepth < VolumetricLightmapSettings.MaxRefinementLevels - 1; CurrentDepth++)
		{
			BrickStartAllocation += BricksByDepth[CurrentDepth].Num();
		}

		TArray<const FImportedVolumetricLightmapBrick*>& HighestDensityBricks = BricksByDepth[VolumetricLightmapSettings.MaxRefinementLevels - 1];

		// Reads from unique data of any density bricks, writes to unique data
		// This is doing a filter in-place which causes extra blurring
		FilterWithNeighbors(
			HighestDensityBricks,
			BrickStartAllocation,
			VoxelImportProcessingData,
			DetailCellSize,
			VolumetricLightmapSettings.MaxRefinementLevels - 1,
			BrickLayoutDimensions,
			VolumetricLightmapSettings,
			CurrentLevelData.BrickData,
			CurrentLevelData);
	}

	BrickStartAllocation = 0;

	for (int32 CurrentDepth = 0; CurrentDepth < VolumetricLightmapSettings.MaxRefinementLevels; CurrentDepth++)
	{
		const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth = BricksByDepth[CurrentDepth];

		if (bStitchDetailBricksWithLowDensityNeighbors && CurrentDepth > 0)
		{
			// Reads from both unique and padding data of lower density bricks, writes to unique data
			StitchDetailBricksWithLowDensityNeighbors(
				BricksAtCurrentDepth,
				BrickStartAllocation,
				CurrentDepth,
				BrickLayoutDimensions,
				VolumetricLightmapSettings,
				CurrentLevelData);
		}

		if (bCopyPaddingFromUniqueData)
		{
			// Compute padding for all the bricks
			// Reads from unique data, writes to padding data of bricks
			// Padding must be computed after all operations that might modify the unique data
			CopyPaddingFromUniqueData(
				BricksAtCurrentDepth,
				BrickStartAllocation,
				CurrentDepth,
				BrickLayoutDimensions,
				VolumetricLightmapSettings,
				CurrentLevelData,
				CurrentLevelData.BrickData);
		}

		if (bCopyVolumeBorderFromInterior)
		{
			// The volume border padding had no unique data to copy from, replicate the neighboring interior value
			CopyVolumeBorderFromInterior(
				BricksAtCurrentDepth,
				BrickStartAllocation,
				CurrentDepth,
				BrickLayoutDimensions,
				VolumetricLightmapSettings,
				CurrentLevelData,
				CurrentLevelData.BrickData);
		}

		BrickStartAllocation += BricksAtCurrentDepth.Num();
	}

	CurrentLevelData.FinalizeImport();

	float ImportTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogVolumetricLightmapImport, Log,  TEXT("Imported Volumetric Lightmap in %.3fs"), ImportTime);
	UE_LOG(LogVolumetricLightmapImport, Log,  TEXT("     Indirection Texture %ux%ux%u = %.1fMb"), 
		CurrentLevelData.IndirectionTextureDimensions.X,
		CurrentLevelData.IndirectionTextureDimensions.Y,
		CurrentLevelData.IndirectionTextureDimensions.Z,
		CurrentLevelData.IndirectionTexture.Data.Num() / 1024.0f / 1024.0f);

	int32 BrickDataSize = CurrentLevelData.BrickData.AmbientVector.Data.Num() + CurrentLevelData.BrickData.SkyBentNormal.Data.Num() + CurrentLevelData.BrickData.DirectionalLightShadowing.Data.Num();

	for (int32 i = 0; i < ARRAY_COUNT(CurrentLevelData.BrickData.SHCoefficients); i++)
	{
		BrickDataSize += CurrentLevelData.BrickData.SHCoefficients[i].Data.Num();
	}

	int32 TotalNumBricks = 0;

	for (int32 CurrentDepth = 0; CurrentDepth < VolumetricLightmapSettings.MaxRefinementLevels; CurrentDepth++)
	{
		const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth = BricksByDepth[CurrentDepth];
		TotalNumBricks += BricksAtCurrentDepth.Num();
	}

	const int32 ActualBrickSizeBytes = BrickDataSize / TotalNumBricks;

	FString TrimmedString;
	
	if (NumBottomLevelBricksTrimmed > 0)
	{
		TrimmedString = FString::Printf(TEXT(" (trimmed %.1fMb due to %.1fMb MaximumBrickMemoryMb)"),
			NumBottomLevelBricksTrimmed * ActualBrickSizeBytes / 1024.0f / 1024.0f,
			MaximumBrickMemoryMb);
	}

	UE_LOG(LogVolumetricLightmapImport, Log,  TEXT("     BrickData %ux%ux%u = %.1fMb%s"), 
		CurrentLevelData.BrickDataDimensions.X,
		CurrentLevelData.BrickDataDimensions.Y,
		CurrentLevelData.BrickDataDimensions.Z,
		BrickDataSize / 1024.0f / 1024.0f,
		*TrimmedString);

	UE_LOG(LogVolumetricLightmapImport, Log, TEXT("     Bricks at Level"));

	const float TotalVolume = VolumetricLightmapSettings.VolumeSize.X * VolumetricLightmapSettings.VolumeSize.Y * VolumetricLightmapSettings.VolumeSize.Z;

	for (int32 CurrentDepth = 0; CurrentDepth < VolumetricLightmapSettings.MaxRefinementLevels; CurrentDepth++)
	{
		const int32 DetailCellsPerCurrentLevelBrick = 1 << ((VolumetricLightmapSettings.MaxRefinementLevels - CurrentDepth) * BrickSizeLog2);
		const FVector CurrentDepthBrickSize = DetailCellSize * DetailCellsPerCurrentLevelBrick;
		const TArray<const FImportedVolumetricLightmapBrick*>& BricksAtCurrentDepth = BricksByDepth[CurrentDepth];
		const float CurrentDepthBrickVolume = CurrentDepthBrickSize.X * CurrentDepthBrickSize.Y * CurrentDepthBrickSize.Z;

		UE_LOG(LogVolumetricLightmapImport, Log, TEXT("         %u: %.1f%% covering %.1f%% of volume"), 
			CurrentDepth, 
			100.0f * BricksAtCurrentDepth.Num() / TotalNumBricks,
			100.0f * BricksAtCurrentDepth.Num() * CurrentDepthBrickVolume / TotalVolume);
	}
}

#undef LOCTEXT_NAMESPACE
