// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/Archive.h"

struct FGridSize2D
{
	uint32 Width;
	uint32 Height;

	explicit FGridSize2D(uint32 InWidth = 0, uint32 InHeight = 0)
		: Width(InWidth), Height(InHeight)
	{
	}
};

/**	No virtuals on purpose */
template<typename CellType, int InvalidCellValue = 0>
struct TSimpleCellGrid
{
	typedef CellType FCellType;

	// DEPRECATED, use GridCellSize instead of this
	uint32 CellSize;

	float GridCellSize;
	FBox WorldBounds;
	FVector Origin;
	FVector BoundsSize;
	FGridSize2D GridSize;

protected:
	TArray<FCellType> Cells;

public:
	TSimpleCellGrid()
		: CellSize(0)
		, GridCellSize(0.0f)
		, WorldBounds(EForceInit::ForceInitToZero)
		, Origin(FLT_MAX)
		, BoundsSize(0)
	{
	}

	bool Init(float InCellSize, const FBox& Bounds)
	{
		if (InCellSize <= 0.0f || !Bounds.IsValid)
		{
			return false;
		}
			
		GridCellSize = InCellSize;
		CellSize = FMath::TruncToInt(InCellSize);

		const FVector RealBoundsSize = Bounds.GetSize();
		GridSize = FGridSize2D(FMath::CeilToInt(RealBoundsSize.X / InCellSize), FMath::CeilToInt(RealBoundsSize.Y / InCellSize));
		BoundsSize = FVector(GridSize.Width * InCellSize, GridSize.Height * InCellSize, RealBoundsSize.Z);
		Origin = FVector(Bounds.Min.X, Bounds.Min.Y, (Bounds.Min.Z + Bounds.Max.Z) * 0.5f);
		UpdateWorldBounds();

		Cells.AddDefaulted(GridSize.Width * GridSize.Height);

		return true;
	}

	void UpdateWorldBounds()
	{
		WorldBounds = FBox(Origin - FVector(0, 0, BoundsSize.Z / 2), Origin + FVector(BoundsSize.X, BoundsSize.Y, BoundsSize.Z / 2));
	}

	FORCEINLINE bool IsValid() const
	{
		return Cells.Num() && GridCellSize > 0;
	}

	FORCEINLINE bool IsValidIndex(const int32 CellIndex) const
	{
		return Cells.IsValidIndex(CellIndex);
	}

	FORCEINLINE bool IsValidCoord(int32 LocationX, int32 LocationY) const
	{
		return (LocationX >= 0) && (LocationX < (int32)GridSize.Width) && (LocationY >= 0) && (LocationY < (int32)GridSize.Height);
	}

	FORCEINLINE bool IsValidCoord(const FIntVector& CellCoords) const
	{
		return IsValidCoord(CellCoords.X, CellCoords.Y);
	}

	FORCEINLINE uint32 GetAllocatedSize() const
	{
		return Cells.GetAllocatedSize();
	}

	/** Convert world location to (X,Y) coords on grid, result can be outside grid */
	FORCEINLINE FIntVector GetCellCoordsUnsafe(const FVector& WorldLocation) const
	{
		return FIntVector(
			FMath::TruncToInt((WorldLocation.X - Origin.X) / GridCellSize),
			FMath::TruncToInt((WorldLocation.Y - Origin.Y) / GridCellSize),
			0);
	}

	/** Convert world location to (X,Y) coords on grid, result is clamped to grid */
	FIntVector GetCellCoords(const FVector& WorldLocation) const
	{
		const FIntVector UnsafeCoords = GetCellCoordsUnsafe(WorldLocation);
		return FIntVector(FMath::Clamp(UnsafeCoords.X, 0, (int32)GridSize.Width - 1), FMath::Clamp(UnsafeCoords.Y, 0, (int32)GridSize.Height - 1), 0);
	}

	/** Convert cell index to coord X on grid, result can be invalid */
	FORCEINLINE int32 GetCellCoordX(int32 CellIndex) const
	{
		return CellIndex / GridSize.Height;
	}

	/** Convert cell index to coord Y on grid, result can be invalid */
	FORCEINLINE int32 GetCellCoordY(int32 CellIndex) const
	{
		return CellIndex % GridSize.Height;
	}

	/** Convert cell index to (X,Y) coords on grid */
	FORCEINLINE FIntVector GetCellCoords(int32 CellIndex) const
	{
		return FIntVector(GetCellCoordX(CellIndex), GetCellCoordY(CellIndex), 0);
	}

	/** Convert world location to cell index, result can be invalid */
	int32 GetCellIndexUnsafe(const FVector& WorldLocation) const
	{
		const FIntVector CellCoords = GetCellCoordsUnsafe(WorldLocation);
		return GetCellIndexUnsafe(CellCoords.X, CellCoords.Y);
	}

	/** Convert (X,Y) coords on grid to cell index, result can be invalid */
	FORCEINLINE int32 GetCellIndexUnsafe(const FIntVector& CellCoords) const
	{
		return GetCellIndexUnsafe(CellCoords.X, CellCoords.Y);
	}

	/** Convert (X,Y) coords on grid to cell index, result can be invalid */
	FORCEINLINE int32 GetCellIndexUnsafe(int32 LocationX, int32 LocationY) const
	{
		return (LocationX * GridSize.Height) + LocationY;
	}

	/** Convert (X,Y) coords on grid to cell index, returns -1 for position outside grid */
	FORCEINLINE int32 GetCellIndex(int32 LocationX, int32 LocationY) const
	{
		return IsValidCoord(LocationX, LocationY) ? GetCellIndexUnsafe(LocationX, LocationY) : INDEX_NONE;
	}

	/** Convert world location to cell index, returns -1 for position outside grid */
	int32 GetCellIndex(const FVector& WorldLocation) const
	{
		const FIntVector CellCoords = GetCellCoordsUnsafe(WorldLocation);
		return GetCellIndex(CellCoords.X, CellCoords.Y);
	}
	
	FORCEINLINE FBox GetWorldCellBox(int32 CellIndex) const
	{
		return GetWorldCellBox(GetCellCoordX(CellIndex), GetCellCoordY(CellIndex));
	}

	FORCEINLINE FBox GetWorldCellBox(int32 LocationX, int32 LocationY) const
	{
		return FBox(
			Origin + FVector(LocationX * GridCellSize, LocationY * GridCellSize, -BoundsSize.Z * 0.5f), 
			Origin + FVector((LocationX + 1) * GridCellSize, (LocationY + 1) * GridCellSize, BoundsSize.Z * 0.5f)
			);
	}

	FORCEINLINE FVector GetWorldCellCenter(int32 CellIndex) const
	{
		return GetWorldCellCenter(GetCellCoordX(CellIndex), GetCellCoordY(CellIndex));
	}

	FORCEINLINE FVector GetWorldCellCenter(int32 LocationX, int32 LocationY) const
	{
		return Origin + FVector((LocationX + 0.5f) * GridCellSize, (LocationY + 0.5f) * GridCellSize, 0);
	}

	const FCellType& GetCellAtWorldLocationUnsafe(const FVector& WorldLocation) const
	{
		const int32 CellIndex = GetCellIndexUnsafe(WorldLocation);
		return Cells[CellIndex];
	}

	const FCellType& GetCellAtWorldLocation(const FVector& WorldLocation) const
	{
		static FCellType InvalidCellInstance = FCellType(InvalidCellValue);
		const int32 CellIndex = GetCellIndex(WorldLocation);
		return (CellIndex == INDEX_NONE) ? InvalidCellInstance : Cells[CellIndex];
	}

	FORCEINLINE FCellType& operator[](int32 CellIndex) { return Cells[CellIndex]; }
	FORCEINLINE const FCellType& operator[](int32 CellIndex) const { return Cells[CellIndex]; }

	FORCEINLINE FCellType& GetCellAtIndexUnsafe(int32 CellIndex) { return Cells.GetData()[CellIndex]; }
	FORCEINLINE const FCellType& GetCellAtIndexUnsafe(int32 CellIndex) const { return Cells.GetData()[CellIndex]; }
	
	FORCEINLINE int32 GetCellsCount() const
	{
		return Cells.Num();
	}

	FORCEINLINE int32 Num() const
	{
		return Cells.Num();
	}

	void Serialize(FArchive& Ar)
	{	
		// CellSize must act as version checking: MAX_uint32, float CellSize is used
		uint32 VersionNum = MAX_uint32;
		Ar << VersionNum;

		if (Ar.IsLoading())
		{
			if (VersionNum == MAX_uint32)
			{
				Ar << GridCellSize;
				CellSize = FMath::TruncToInt(GridCellSize);
			}
			else
			{
				CellSize = VersionNum;
				GridCellSize = VersionNum * 1.0f;
			}
		}
		else
		{
			Ar << GridCellSize;
		}

		Ar << Origin;
		Ar << BoundsSize;
		Ar << GridSize.Width << GridSize.Height;
	
		UpdateWorldBounds();

		if (VersionNum == MAX_uint32)
		{
			Ar << Cells;
		}
		else
		{
			int32 DataBytesCount = GetAllocatedSize();
			Ar << DataBytesCount;

			if (DataBytesCount > 0)
			{
				if (Ar.IsLoading())
				{
					Cells.SetNum(GridSize.Width * GridSize.Height);
				}

				Ar.Serialize(Cells.GetData(), DataBytesCount);
			}
		}
	}

	void AllocateMemory()
	{
		Cells.SetNum(GridSize.Width * GridSize.Height);
	}

	void FreeMemory()
	{
		Cells.Empty();
	}

	void Zero()
	{
		Cells.Reset();
		Cells.AddZeroed(GridSize.Width * GridSize.Height);
	}

	void CleanUp()
	{
		Cells.Empty();
		GridCellSize = 0;
		Origin = FVector(FLT_MAX);
	}

	//////////////////////////////////////////////////////////////////////////
	// DEPRECATED SUPPORT

	DEPRECATED(4.14, "This function is now deprecated, please use the one with float CellSize argument")
	void Init(uint32 InCellSize, const FBox& Bounds)
	{
		Init(1.0f * InCellSize, Bounds);
	}

	DEPRECATED(4.14, "This function is now deprecated, please use GetCellCoords instead.")
	FIntVector WorldToCellCoords(const FVector& WorldLocation) const
	{
		const uint32 LocationX = FMath::TruncToInt((WorldLocation.X - Origin.X) / CellSize);
		const uint32 LocationY = FMath::TruncToInt((WorldLocation.Y - Origin.Y) / CellSize);
		return FIntVector(LocationX, LocationY, 0);
	}

	DEPRECATED(4.14, "This function is now deprecated, please use GetCellCoords instead.")
	void WorldToCellCoords(const FVector& WorldLocation, uint32& LocationX, uint32& LocationY) const
	{
		LocationX = FMath::TruncToInt((WorldLocation.X - Origin.X) / CellSize);
		LocationY = FMath::TruncToInt((WorldLocation.Y - Origin.Y) / CellSize);
	}

	DEPRECATED(4.14, "This function is now deprecated, please use GetCellIndex instead.")
	uint32 WorldToCellIndex(const FVector& WorldLocation) const
	{
		const uint32 LocationX = FMath::TruncToInt((WorldLocation.X - Origin.X) / CellSize);
		const uint32 LocationY = FMath::TruncToInt((WorldLocation.Y - Origin.Y) / CellSize);
		return GetCellIndex(LocationX, LocationY);
	}

	DEPRECATED(4.14, "This function is now deprecated, please use GetCellCoords instead.")
	FIntVector CellIndexToCoords(uint32 CellIndex) const
	{
		return FIntVector(CellIndex / GridSize.Height, CellIndex % GridSize.Height, 0);
	}

	DEPRECATED(4.14, "This function is now deprecated, please use GetCellCoords instead.")
	FIntVector CellIndexToCoords(uint32 CellIndex, uint32& LocationX, uint32& LocationY) const
	{
		LocationX = CellIndex / GridSize.Height;
		LocationY = CellIndex % GridSize.Height;
		return FIntVector(LocationX, LocationY, 0);
	}

	DEPRECATED(4.14, "This function is now deprecated, please use GetCellIndex instead.")
	uint32 CellCoordsToCellIndex(const int32 LocationX, const int32 LocationY) const
	{
		return (LocationX * GridSize.Height) + LocationY;
	}

	DEPRECATED(4.14, "This function is now deprecated, please use GetCellAtWorldLocation instead.")
	const FCellType& GetCellAtWorldLocationSafe(const FVector& WorldLocation) const
	{
		return GetCellAtWorldLocation(WorldLocation);
	}

	DEPRECATED(4.14, "This function is now deprecated, please use GetAllocatedSize instead.")
	uint32 GetValuesMemorySize() const
	{
		return GridSize.Height * GridSize.Width * sizeof(FCellType);
	}

	DEPRECATED(4.14, "This function is now deprecated, please use IsValidIndex instead.")
	bool IsValidCellIndex(const int32 CellIndex) const
	{
		return IsValidIndex(CellIndex);
	}
};
