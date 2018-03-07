// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ALandscapeProxy;
class ULandscapeLayerInfoObject;

namespace LandscapeEditorUtils
{
	template<typename T>
	void ExpandData(T* OutData, const T* InData,
		int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
		int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY)
	{
		const int32 OldWidth = OldMaxX - OldMinX + 1;
		const int32 OldHeight = OldMaxY - OldMinY + 1;
		const int32 NewWidth = NewMaxX - NewMinX + 1;
		const int32 NewHeight = NewMaxY - NewMinY + 1;
		const int32 OffsetX = NewMinX - OldMinX;
		const int32 OffsetY = NewMinY - OldMinY;

		for (int32 Y = 0; Y < NewHeight; ++Y)
		{
			const int32 OldY = FMath::Clamp<int32>(Y + OffsetY, 0, OldHeight - 1);

			// Pad anything to the left
			const T PadLeft = InData[OldY * OldWidth + 0];
			for (int32 X = 0; X < -OffsetX; ++X)
			{
				OutData[Y * NewWidth + X] = PadLeft;
			}

			// Copy one row of the old data
			{
				const int32 X = FMath::Max(0, -OffsetX);
				const int32 OldX = FMath::Clamp<int32>(X + OffsetX, 0, OldWidth - 1);
				FMemory::Memcpy(&OutData[Y * NewWidth + X], &InData[OldY * OldWidth + OldX], FMath::Min<int32>(OldWidth, NewWidth) * sizeof(T));
			}

			const T PadRight = InData[OldY * OldWidth + OldWidth - 1];
			for (int32 X = -OffsetX + OldWidth; X < NewWidth; ++X)
			{
				OutData[Y * NewWidth + X] = PadRight;
			}
		}
	}

	template<typename T>
	TArray<T> ExpandData(const TArray<T>& Data,
		int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
		int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY)
	{
		const int32 NewWidth = NewMaxX - NewMinX + 1;
		const int32 NewHeight = NewMaxY - NewMinY + 1;

		TArray<T> Result;
		Result.Empty(NewWidth * NewHeight);
		Result.AddUninitialized(NewWidth * NewHeight);

		ExpandData(Result.GetData(), Data.GetData(),
			OldMinX, OldMinY, OldMaxX, OldMaxY,
			NewMinX, NewMinY, NewMaxX, NewMaxY);

		return Result;
	}

	template<typename T>
	TArray<T> ResampleData(const TArray<T>& Data, int32 OldWidth, int32 OldHeight, int32 NewWidth, int32 NewHeight)
	{
		TArray<T> Result;
		Result.Empty(NewWidth * NewHeight);
		Result.AddUninitialized(NewWidth * NewHeight);

		const float XScale = (float)(OldWidth - 1) / (NewWidth - 1);
		const float YScale = (float)(OldHeight - 1) / (NewHeight - 1);
		for (int32 Y = 0; Y < NewHeight; ++Y)
		{
			for (int32 X = 0; X < NewWidth; ++X)
			{
				const float OldY = Y * YScale;
				const float OldX = X * XScale;
				const int32 X0 = FMath::FloorToInt(OldX);
				const int32 X1 = FMath::Min(FMath::FloorToInt(OldX) + 1, OldWidth - 1);
				const int32 Y0 = FMath::FloorToInt(OldY);
				const int32 Y1 = FMath::Min(FMath::FloorToInt(OldY) + 1, OldHeight - 1);
				const T& Original00 = Data[Y0 * OldWidth + X0];
				const T& Original10 = Data[Y0 * OldWidth + X1];
				const T& Original01 = Data[Y1 * OldWidth + X0];
				const T& Original11 = Data[Y1 * OldWidth + X1];
				Result[Y * NewWidth + X] = FMath::BiLerp(Original00, Original10, Original01, Original11, FMath::Fractional(OldX), FMath::Fractional(OldY));
			}
		}

		return Result;
	}

	bool LANDSCAPEEDITOR_API SetHeightmapData(ALandscapeProxy* Landscape, const TArray<uint16>& Data);

	bool LANDSCAPEEDITOR_API SetWeightmapData(ALandscapeProxy* Landscape, ULandscapeLayerInfoObject* LayerObject, const TArray<uint8>& Data);
}
