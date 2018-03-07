// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/App.h"
#include "InputCoreTypes.h"
#include "Engine/EngineTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "LandscapeToolInterface.h"
#include "LandscapeProxy.h"
#include "LandscapeEdMode.h"
#include "LandscapeEditorObject.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdModeTools.h"


const int32 FNoiseParameter::Permutations[256] =
{
	151, 160, 137, 91, 90, 15,
	131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
	190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
	88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
	77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
	102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
	135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
	5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
	223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
	129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
	251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
	49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
	138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};



// 
// FLandscapeToolPaintBase
//


template<class TToolTarget, class TStrokeClass>
class FLandscapeToolPaintBase : public FLandscapeToolBase<TStrokeClass>
{
public:
	FLandscapeToolPaintBase(FEdModeLandscape* InEdMode)
		: FLandscapeToolBase<TStrokeClass>(InEdMode)
	{
	}

	virtual ELandscapeToolTargetTypeMask::Type GetSupportedTargetTypes() override
	{
		return ELandscapeToolTargetTypeMask::FromType(TToolTarget::TargetType);
	}
};


template<class ToolTarget>
class FLandscapeToolStrokePaintBase : public FLandscapeToolStrokeBase
{
public:
	FLandscapeToolStrokePaintBase(FEdModeLandscape* InEdMode, FEditorViewportClient* InViewportClient, const FLandscapeToolTarget& InTarget)
		: FLandscapeToolStrokeBase(InEdMode, InViewportClient, InTarget)
		, Cache(InTarget)
	{
	}

protected:
	typename ToolTarget::CacheClass Cache;
};



// 
// FLandscapeToolPaint
//
class FLandscapeToolStrokePaint : public FLandscapeToolStrokePaintBase<FWeightmapToolTarget>
{
	typedef FWeightmapToolTarget ToolTarget;

	TMap<FIntPoint, float> TotalInfluenceMap; // amount of time and weight the brush has spent on each vertex.

	bool bIsWhitelistMode;
	bool bAddToWhitelist;
public:
	// Heightmap sculpt tool will continuously sculpt in the same location, weightmap paint tool doesn't
	enum { UseContinuousApply = false };

	FLandscapeToolStrokePaint(FEdModeLandscape* InEdMode, FEditorViewportClient* InViewportClient, const FLandscapeToolTarget& InTarget)
		: FLandscapeToolStrokePaintBase<FWeightmapToolTarget>(InEdMode, InViewportClient, InTarget)
		, bIsWhitelistMode(EdMode->UISettings->PaintingRestriction == ELandscapeLayerPaintingRestriction::UseComponentWhitelist &&
		                   (InViewportClient->Viewport->KeyState(EKeys::Equals) || InViewportClient->Viewport->KeyState(EKeys::Hyphen)))
		, bAddToWhitelist(bIsWhitelistMode && InViewportClient->Viewport->KeyState(EKeys::Equals))
	{
	}

	void Apply(FEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolInteractorPosition>& InteractorPositions)
	{
		// Invert when holding Shift
		//UE_LOG(LogLandscape, Log, TEXT("bInvert = %d"), bInvert);
		bool bInvert = InteractorPositions.Last().bModifierPressed;

		if (bIsWhitelistMode)
		{
			// Get list of components to delete from brush
			// TODO - only retrieve bounds as we don't need the vert data
			FLandscapeBrushData BrushInfo = Brush->ApplyBrush(InteractorPositions);
			if (!BrushInfo)
			{
				return;
			}

			int32 X1, Y1, X2, Y2;
			BrushInfo.GetInclusiveBounds(X1, Y1, X2, Y2);

			// Shrink bounds by 1,1 to avoid GetComponentsInRegion picking up extra components on all sides due to the overlap between components
			TSet<ULandscapeComponent*> SelectedComponents;
			LandscapeInfo->GetComponentsInRegion(X1 + 1, Y1 + 1, X2 - 1, Y2 - 1, SelectedComponents);

			for (ULandscapeComponent* Component : SelectedComponents)
			{
				Component->Modify();
			}

			if (bAddToWhitelist)
			{
				for (ULandscapeComponent* Component : SelectedComponents)
				{
					Component->LayerWhitelist.AddUnique(Target.LayerInfo.Get());
				}
			}
			else
			{
				FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
				for (ULandscapeComponent* Component : SelectedComponents)
				{
					Component->LayerWhitelist.RemoveSingle(Target.LayerInfo.Get());
					Component->DeleteLayer(Target.LayerInfo.Get(), LandscapeEdit);
				}
			}

			return;
		}

		// Get list of verts to update
		FLandscapeBrushData BrushInfo = Brush->ApplyBrush(InteractorPositions);
		if (!BrushInfo)
		{
			return;
		}

		int32 X1, Y1, X2, Y2;
		BrushInfo.GetInclusiveBounds(X1, Y1, X2, Y2);

		// Tablet pressure
		float Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.0f;

		this->Cache.CacheData(X1, Y1, X2, Y2);

		bool bUseWeightTargetValue = UISettings->bUseWeightTargetValue;

		// The data we'll be writing to
		TArray<ToolTarget::CacheClass::DataType> Data;
		this->Cache.GetCachedData(X1, Y1, X2, Y2, Data);

		// The source data we use for editing. 
		TArray<ToolTarget::CacheClass::DataType>* SourceDataArrayPtr = &Data;
		TArray<ToolTarget::CacheClass::DataType> OriginalData;

		if (!bUseWeightTargetValue)
		{
			// When painting weights (and not using target value mode), we use a source value that tends more
			// to the current value as we paint over the same region multiple times.
			// TODO: Make this frame-rate independent
			this->Cache.GetOriginalData(X1, Y1, X2, Y2, OriginalData);
			SourceDataArrayPtr = &OriginalData;

			for (int32 Y = Y1; Y < Y2; Y++)
			{
				auto* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
				auto* OriginalDataScanline = OriginalData.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
				for (int32 X = X1; X < X2; X++)
				{
					float VertexInfluence = TotalInfluenceMap.FindRef(FIntPoint(X, Y));

					auto& CurrentValue = DataScanline[X];
					auto& SourceValue = OriginalDataScanline[X];

					SourceValue = FMath::Lerp(SourceValue, CurrentValue, FMath::Min<float>(VertexInfluence * 0.05f, 1.0f));
				}
			}
		}

		// Adjust strength based on brush size and drawscale, so strength 1 = one hemisphere
		const float AdjustedStrength = ToolTarget::StrengthMultiplier(this->LandscapeInfo, UISettings->BrushRadius);
		FWeightmapToolTarget::CacheClass::DataType DestValue = FWeightmapToolTarget::CacheClass::ClampValue(255.0f * UISettings->WeightTargetValue);

		float PaintStrength = UISettings->ToolStrength * Pressure * AdjustedStrength;

		// TODO: make paint tool framerate independent like the sculpt tool
		// const float DeltaTime = FMath::Min<float>(FApp::GetDeltaTime(), 0.1f); // Under 10 fps slow down paint speed
		// SculptStrength *= DeltaTime * 3.0f; // * 3.0f to partially compensate for impact of DeltaTime on slowing the tools down compared to the old framerate-dependent version

		if (PaintStrength <= 0.0f)
		{
			return;
		}

		if (!bUseWeightTargetValue)
		{
			PaintStrength = FMath::Max(PaintStrength, 1.0f);
		}

		// Apply the brush
		for (int32 Y = BrushInfo.GetBounds().Min.Y; Y < BrushInfo.GetBounds().Max.Y; Y++)
		{
			const float* BrushScanline = BrushInfo.GetDataPtr(FIntPoint(0, Y));
			auto* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
			auto* SourceDataScanline = SourceDataArrayPtr->GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);

			for (int32 X = BrushInfo.GetBounds().Min.X; X < BrushInfo.GetBounds().Max.X; X++)
			{
				const FIntPoint Key = FIntPoint(X, Y);
				const float BrushValue = BrushScanline[X];

				// Update influence map
				float VertexInfluence = TotalInfluenceMap.FindRef(Key);
				TotalInfluenceMap.Add(Key, VertexInfluence + BrushValue);

				float PaintAmount = BrushValue * PaintStrength;
				auto& CurrentValue = DataScanline[X];
				const auto& SourceValue = SourceDataScanline[X];

				if (bUseWeightTargetValue)
				{
					CurrentValue = FMath::Lerp(CurrentValue, DestValue, PaintAmount / AdjustedStrength);
				}
				else
				{
					if (bInvert)
					{
						CurrentValue = ToolTarget::CacheClass::ClampValue(FMath::Min<int32>(SourceValue - FMath::RoundToInt(PaintAmount), CurrentValue));
					}
					else
					{
						CurrentValue = ToolTarget::CacheClass::ClampValue(FMath::Max<int32>(SourceValue + FMath::RoundToInt(PaintAmount), CurrentValue));
					}
				}
			}
		}

		this->Cache.SetCachedData(X1, Y1, X2, Y2, Data, UISettings->PaintingRestriction);
		this->Cache.Flush();
	}
};

class FLandscapeToolPaint : public FLandscapeToolPaintBase<FWeightmapToolTarget, FLandscapeToolStrokePaint>
{
public:
	FLandscapeToolPaint(FEdModeLandscape* InEdMode)
		: FLandscapeToolPaintBase<FWeightmapToolTarget, FLandscapeToolStrokePaint>(InEdMode)
	{
	}

	virtual const TCHAR* GetToolName() override { return TEXT("Paint"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Paint", "Paint"); };

	virtual void EnterTool()
	{
		if (EdMode->UISettings->PaintingRestriction == ELandscapeLayerPaintingRestriction::UseComponentWhitelist)
		{
			EdMode->UISettings->UpdateComponentLayerWhitelist();
		}

		FLandscapeToolPaintBase::EnterTool();
	}
};

//
class FLandscapeToolStrokeSculpt : public FLandscapeToolStrokePaintBase<FHeightmapToolTarget>
{
	typedef FHeightmapToolTarget ToolTarget;

public:
	// Heightmap sculpt tool will continuously sculpt in the same location, weightmap paint tool doesn't
	enum { UseContinuousApply = true };

	FLandscapeToolStrokeSculpt(FEdModeLandscape* InEdMode, FEditorViewportClient* InViewportClient, const FLandscapeToolTarget& InTarget)
		: FLandscapeToolStrokePaintBase<FHeightmapToolTarget>(InEdMode, InViewportClient, InTarget)
	{
	}

	void Apply(FEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolInteractorPosition>& InteractorPositions)
	{
		// Invert when holding Shift
		//UE_LOG(LogLandscape, Log, TEXT("bInvert = %d"), bInvert);
		bool bInvert = InteractorPositions.Last().bModifierPressed;

		// Get list of verts to update
		FLandscapeBrushData BrushInfo = Brush->ApplyBrush(InteractorPositions);
		if (!BrushInfo)
		{
			return;
		}

		int32 X1, Y1, X2, Y2;
		BrushInfo.GetInclusiveBounds(X1, Y1, X2, Y2);

		// Tablet pressure
		float Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.0f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		this->Cache.CacheData(X1, Y1, X2, Y2);

		bool bUseClayBrush = UISettings->bUseClayBrush;

		// The data we'll be writing to
		TArray<ToolTarget::CacheClass::DataType> Data;
		this->Cache.GetCachedData(X1, Y1, X2, Y2, Data);

		// The source data we use for editing. 
		TArray<ToolTarget::CacheClass::DataType>* SourceDataArrayPtr = &Data;

		FMatrix ToWorld = ToolTarget::ToWorldMatrix(this->LandscapeInfo);
		FMatrix FromWorld = ToolTarget::FromWorldMatrix(this->LandscapeInfo);

		// Adjust strength based on brush size and drawscale, so strength 1 = one hemisphere
		const float AdjustedStrength = ToolTarget::StrengthMultiplier(this->LandscapeInfo, UISettings->BrushRadius);

		float SculptStrength = UISettings->ToolStrength * Pressure * AdjustedStrength;
		const float DeltaTime = FMath::Min<float>(FApp::GetDeltaTime(), 0.1f); // Under 10 fps slow down paint speed
		SculptStrength *= DeltaTime * 3.0f; // * 3.0f to partially compensate for impact of DeltaTime on slowing the tools down compared to the old framerate-dependent version

		if (SculptStrength <= 0.0f)
		{
			return;
		}

		if (!bUseClayBrush)
		{
			SculptStrength = FMath::Max(SculptStrength, 1.0f);
		}

		FPlane BrushPlane;
		TArray<FVector> Normals;

		if (bUseClayBrush)
		{
			// Calculate normals for brush verts in data space
			Normals.Empty(SourceDataArrayPtr->Num());
			Normals.AddZeroed(SourceDataArrayPtr->Num());

			for (int32 Y = Y1; Y < Y2; Y++)
			{
				auto* SourceDataScanline_0 = SourceDataArrayPtr->GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
				auto* SourceDataScanline_1 = SourceDataArrayPtr->GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
				auto* NormalsScanline_0 = Normals.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
				auto* NormalsScanline_1 = Normals.GetData() + (Y + 1 - Y1) * (X2 - X1 + 1) + (0 - X1);
				for (int32 X = X1; X < X2; X++)
				{
					FVector Vert00 = ToWorld.TransformPosition(FVector((float)X + 0.0f, (float)Y + 0.0f, SourceDataScanline_0[X + 0]));
					FVector Vert01 = ToWorld.TransformPosition(FVector((float)X + 0.0f, (float)Y + 1.0f, SourceDataScanline_1[X + 0]));
					FVector Vert10 = ToWorld.TransformPosition(FVector((float)X + 1.0f, (float)Y + 0.0f, SourceDataScanline_0[X + 1]));
					FVector Vert11 = ToWorld.TransformPosition(FVector((float)X + 1.0f, (float)Y + 1.0f, SourceDataScanline_1[X + 1]));

					FVector FaceNormal1 = ((Vert00 - Vert10) ^ (Vert10 - Vert11)).GetSafeNormal();
					FVector FaceNormal2 = ((Vert11 - Vert01) ^ (Vert01 - Vert00)).GetSafeNormal();

					// contribute to the vertex normals.
					NormalsScanline_0[X + 1] += FaceNormal1;
					NormalsScanline_1[X + 0] += FaceNormal2;
					NormalsScanline_0[X + 0] += FaceNormal1 + FaceNormal2;
					NormalsScanline_1[X + 1] += FaceNormal1 + FaceNormal2;
				}
			}
			for (int32 Y = Y1; Y <= Y2; Y++)
			{
				auto* NormalsScanline = Normals.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
				for (int32 X = X1; X <= X2; X++)
				{
					NormalsScanline[X] = NormalsScanline[X].GetSafeNormal();
				}
			}

			// Find brush centroid location
			FVector AveragePoint(0.0f, 0.0f, 0.0f);
			FVector AverageNormal(0.0f, 0.0f, 0.0f);
			float TotalWeight = 0.0f;
			for (int32 Y = BrushInfo.GetBounds().Min.Y; Y < BrushInfo.GetBounds().Max.Y; Y++)
			{
				const float* BrushScanline = BrushInfo.GetDataPtr(FIntPoint(0, Y));
				auto* SourceDataScanline = SourceDataArrayPtr->GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
				auto* NormalsScanline = Normals.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);

				for (int32 X = BrushInfo.GetBounds().Min.X; X < BrushInfo.GetBounds().Max.X; X++)
				{
					const float BrushValue = BrushScanline[X];

					if (BrushValue > 0.0f)
					{
						AveragePoint += FVector((float)X * BrushValue, (float)Y * BrushValue, (float)SourceDataScanline[X] * BrushValue);

						FVector SampleNormal = NormalsScanline[X];
						AverageNormal += SampleNormal * BrushValue;

						TotalWeight += BrushValue;
					}
				}
			}

			if (TotalWeight > 0.0f)
			{
				AveragePoint /= TotalWeight;
				AverageNormal = AverageNormal.GetSafeNormal();
			}

			// Convert to world space
			FVector AverageLocation = ToWorld.TransformPosition(AveragePoint);
			FVector StrengthVector = ToWorld.TransformVector(FVector(0, 0, SculptStrength));

			// Brush pushes out in the normal direction
			FVector OffsetVector = AverageNormal * StrengthVector.Z;
			if (bInvert)
			{
				OffsetVector *= -1;
			}

			// World space brush plane
			BrushPlane = FPlane(AverageLocation + OffsetVector, AverageNormal);
		}

		// Apply the brush
		for (int32 Y = BrushInfo.GetBounds().Min.Y; Y < BrushInfo.GetBounds().Max.Y; Y++)
		{
			const float* BrushScanline = BrushInfo.GetDataPtr(FIntPoint(0, Y));
			auto* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
			auto* SourceDataScanline = SourceDataArrayPtr->GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);

			for (int32 X = BrushInfo.GetBounds().Min.X; X < BrushInfo.GetBounds().Max.X; X++)
			{
				const FIntPoint Key = FIntPoint(X, Y);
				const float BrushValue = BrushScanline[X];

				float SculptAmount = BrushValue * SculptStrength;
				auto& CurrentValue = DataScanline[X];
				const auto& SourceValue = SourceDataScanline[X];

				if (bUseClayBrush)
				{
					// Brush application starts from original world location at start of stroke
					FVector WorldLoc = ToWorld.TransformPosition(FVector(X, Y, SourceValue));

					// Calculate new location on the brush plane
					WorldLoc.Z = (BrushPlane.W - BrushPlane.X*WorldLoc.X - BrushPlane.Y*WorldLoc.Y) / BrushPlane.Z;

					// Painted amount lerps based on brush falloff.
					float PaintValue = FMath::Lerp<float>((float)SourceValue, FromWorld.TransformPosition(WorldLoc).Z, BrushValue);

					if (bInvert)
					{
						CurrentValue = ToolTarget::CacheClass::ClampValue(FMath::Min<int32>(FMath::RoundToInt(PaintValue), CurrentValue));
					}
					else
					{
						CurrentValue = ToolTarget::CacheClass::ClampValue(FMath::Max<int32>(FMath::RoundToInt(PaintValue), CurrentValue));
					}
				}
				else
				{
					if (bInvert)
					{
						CurrentValue = ToolTarget::CacheClass::ClampValue(FMath::Min<int32>(SourceValue - FMath::RoundToInt(SculptAmount), CurrentValue));
					}
					else
					{
						CurrentValue = ToolTarget::CacheClass::ClampValue(FMath::Max<int32>(SourceValue + FMath::RoundToInt(SculptAmount), CurrentValue));
					}
				}
			}
		}

		this->Cache.SetCachedData(X1, Y1, X2, Y2, Data);
		this->Cache.Flush();
	}
};

class FLandscapeToolSculpt : public FLandscapeToolPaintBase<FHeightmapToolTarget, FLandscapeToolStrokeSculpt>
{
public:
	FLandscapeToolSculpt(FEdModeLandscape* InEdMode)
		: FLandscapeToolPaintBase<FHeightmapToolTarget, FLandscapeToolStrokeSculpt>(InEdMode)
	{
	}

	virtual const TCHAR* GetToolName() override { return TEXT("Sculpt"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Sculpt", "Sculpt"); };
};

// 
// FLandscapeToolSmooth
//

template<class ToolTarget>
class FLandscapeToolStrokeSmooth : public FLandscapeToolStrokePaintBase<ToolTarget>
{
public:
	FLandscapeToolStrokeSmooth(FEdModeLandscape* InEdMode, FEditorViewportClient* InViewportClient, const FLandscapeToolTarget& InTarget)
		: FLandscapeToolStrokePaintBase<ToolTarget>(InEdMode, InViewportClient, InTarget)
	{
	}

	void Apply(FEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolInteractorPosition>& InteractorPositions)
	{
		if (!this->LandscapeInfo) return;

		// Get list of verts to update
		FLandscapeBrushData BrushInfo = Brush->ApplyBrush(InteractorPositions);
		if (!BrushInfo)
		{
			return;
		}

		int32 X1, Y1, X2, Y2;
		BrushInfo.GetInclusiveBounds(X1, Y1, X2, Y2);

		// Tablet pressure
		float Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.0f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		CA_SUPPRESS(6326);
		if (ToolTarget::TargetType == ELandscapeToolTargetType::Heightmap)
		{
			X1 -= 1;
			Y1 -= 1;
			X2 += 1;
			Y2 += 1;
		}

		this->Cache.CacheData(X1, Y1, X2, Y2);

		TArray<typename ToolTarget::CacheClass::DataType> Data;
		this->Cache.GetCachedData(X1, Y1, X2, Y2, Data);

		const float ToolStrength = FMath::Clamp<float>(UISettings->ToolStrength * Pressure, 0.0f, 1.0f);

		// Apply the brush
		if (UISettings->bDetailSmooth)
		{
			LowPassFilter<typename ToolTarget::CacheClass::DataType>(X1, Y1, X2, Y2, BrushInfo, Data, UISettings->DetailScale, ToolStrength);
		}
		else
		{
			const int32 FilterRadius = UISettings->SmoothFilterKernelSize;

			for (int32 Y = BrushInfo.GetBounds().Min.Y; Y < BrushInfo.GetBounds().Max.Y; Y++)
			{
				const float* BrushScanline = BrushInfo.GetDataPtr(FIntPoint(0, Y));
				auto* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);

				for (int32 X = BrushInfo.GetBounds().Min.X; X < BrushInfo.GetBounds().Max.X; X++)
				{
					const float BrushValue = BrushScanline[X];

					if (BrushValue > 0.0f)
					{
						// needs to be ~12 bits larger than ToolTarget::CacheClass::DataType (for max FilterRadius (31))
						// the editor is 64-bit native so just go the whole hog :)
						int64 FilterValue = 0;
						int32 FilterSamplingNumber = 0;

						const int32 XRadius = FMath::Min3<int32>(FilterRadius, X - BrushInfo.GetBounds().Min.X, BrushInfo.GetBounds().Max.X - X - 1);
						const int32 YRadius = FMath::Min3<int32>(FilterRadius, Y - BrushInfo.GetBounds().Min.Y, BrushInfo.GetBounds().Max.Y - Y - 1);

						const int32 SampleX1 = X - XRadius; checkSlow(SampleX1 >= BrushInfo.GetBounds().Min.X);
						const int32 SampleY1 = Y - YRadius; checkSlow(SampleY1 >= BrushInfo.GetBounds().Min.Y);
						const int32 SampleX2 = X + XRadius; checkSlow(SampleX2 <  BrushInfo.GetBounds().Max.X);
						const int32 SampleY2 = Y + YRadius; checkSlow(SampleY2 <  BrushInfo.GetBounds().Max.Y);
						for (int32 SampleY = SampleY1; SampleY <= SampleY2; SampleY++)
						{
							const float* SampleBrushScanline = BrushInfo.GetDataPtr(FIntPoint(0, SampleY));
							const float* SampleBrushScanline2 = BrushInfo.GetDataPtr(FIntPoint(0, Y + (Y - SampleY)));
							auto* SampleDataScanline = Data.GetData() + (SampleY - Y1) * (X2 - X1 + 1) + (0 - X1);

							for (int32 SampleX = SampleX1; SampleX <= SampleX2; SampleX++)
							{
								// constrain sample to within the brush, symmetrically to prevent flattening bug
								const float SampleBrushValue =
									FMath::Min(
										FMath::Min<float>(SampleBrushScanline [SampleX], SampleBrushScanline [X + (X - SampleX)]),
										FMath::Min<float>(SampleBrushScanline2[SampleX], SampleBrushScanline2[X + (X - SampleX)])
										);
								if (SampleBrushValue > 0.0f)
								{
									FilterValue += SampleDataScanline[SampleX];
									FilterSamplingNumber++;
								}
							}
						}

						FilterValue /= FilterSamplingNumber;

						DataScanline[X] = FMath::Lerp(DataScanline[X], (typename ToolTarget::CacheClass::DataType)FilterValue, BrushValue * ToolStrength);
					}
				}
			}
		}

		this->Cache.SetCachedData(X1, Y1, X2, Y2, Data, UISettings->PaintingRestriction);
		this->Cache.Flush();
	}
};

template<class ToolTarget>
class FLandscapeToolSmooth : public FLandscapeToolPaintBase<ToolTarget, FLandscapeToolStrokeSmooth<ToolTarget>>
{
public:
	FLandscapeToolSmooth(FEdModeLandscape* InEdMode)
		: FLandscapeToolPaintBase<ToolTarget, FLandscapeToolStrokeSmooth<ToolTarget> >(InEdMode)
	{
	}

	virtual const TCHAR* GetToolName() override { return TEXT("Smooth"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Smooth", "Smooth"); };

};

//
// FLandscapeToolFlatten
//
template<class ToolTarget>
class FLandscapeToolStrokeFlatten : public FLandscapeToolStrokePaintBase<ToolTarget>
{
	typename ToolTarget::CacheClass::DataType FlattenHeight;

	FVector FlattenNormal;
	float FlattenPlaneDist;
	bool bInitializedFlattenHeight;
	bool bTargetIsHeightmap;

public:
	FLandscapeToolStrokeFlatten(FEdModeLandscape* InEdMode, FEditorViewportClient* InViewportClient, const FLandscapeToolTarget& InTarget)
		: FLandscapeToolStrokePaintBase<ToolTarget>(InEdMode, InViewportClient, InTarget)
		, bInitializedFlattenHeight(false)
		, bTargetIsHeightmap(InTarget.TargetType == ELandscapeToolTargetType::Heightmap)
	{
		if (InEdMode->UISettings->bUseFlattenTarget && bTargetIsHeightmap)
		{
			FTransform LocalToWorld = InTarget.LandscapeInfo->GetLandscapeProxy()->ActorToWorld();
			float Height = (InEdMode->UISettings->FlattenTarget - LocalToWorld.GetTranslation().Z) / LocalToWorld.GetScale3D().Z;
			FlattenHeight = LandscapeDataAccess::GetTexHeight(Height);
			bInitializedFlattenHeight = true;
		}
	}

	void Apply(FEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolInteractorPosition>& InteractorPositions)
	{
		if (!this->LandscapeInfo) return;

		if (!bInitializedFlattenHeight || (UISettings->bPickValuePerApply && bTargetIsHeightmap))
		{
			bInitializedFlattenHeight = false;
			float FlattenX = InteractorPositions[0].Position.X;
			float FlattenY = InteractorPositions[0].Position.Y;
			int32 FlattenHeightX = FMath::FloorToInt(FlattenX);
			int32 FlattenHeightY = FMath::FloorToInt(FlattenY);

			this->Cache.CacheData(FlattenHeightX, FlattenHeightY, FlattenHeightX + 1, FlattenHeightY + 1);
			float HeightValue = this->Cache.GetValue(FlattenX, FlattenY);
			FlattenHeight = HeightValue;

			if (UISettings->bUseSlopeFlatten && bTargetIsHeightmap)
			{
				FlattenNormal = this->Cache.GetNormal(FlattenHeightX, FlattenHeightY);
				FlattenPlaneDist = -(FlattenNormal | FVector(FlattenX, FlattenY, HeightValue));
			}

			bInitializedFlattenHeight = true;
		}


		// Get list of verts to update
		FLandscapeBrushData BrushInfo = Brush->ApplyBrush(InteractorPositions);
		if (!BrushInfo)
		{
			return;
		}

		int32 X1, Y1, X2, Y2;
		BrushInfo.GetInclusiveBounds(X1, Y1, X2, Y2);

		// Tablet pressure
		float Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.0f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		CA_SUPPRESS(6326);
		if (ToolTarget::TargetType == ELandscapeToolTargetType::Heightmap)
		{
			X1 -= 1;
			Y1 -= 1;
			X2 += 1;
			Y2 += 1;
		}

		this->Cache.CacheData(X1, Y1, X2, Y2);

		TArray<typename ToolTarget::CacheClass::DataType> Data;
		this->Cache.GetCachedData(X1, Y1, X2, Y2, Data);

		// Apply the brush
		for (int32 Y = BrushInfo.GetBounds().Min.Y; Y < BrushInfo.GetBounds().Max.Y; Y++)
		{
			const float* BrushScanline = BrushInfo.GetDataPtr(FIntPoint(0, Y));
			auto* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);

			for (int32 X = BrushInfo.GetBounds().Min.X; X < BrushInfo.GetBounds().Max.X; X++)
			{
				const float BrushValue = BrushScanline[X];

				if (BrushValue > 0.0f)
				{
					float Strength = FMath::Clamp<float>(BrushValue * UISettings->ToolStrength * Pressure, 0.0f, 1.0f);

					if (!(UISettings->bUseSlopeFlatten && bTargetIsHeightmap))
					{
						int32 Delta = DataScanline[X] - FlattenHeight;
						switch (UISettings->FlattenMode)
						{
						case ELandscapeToolFlattenMode::Raise:
							if (Delta < 0)
							{
								DataScanline[X] = FMath::CeilToInt(FMath::Lerp((float)DataScanline[X], (float)FlattenHeight, Strength));
							}
							break;
						case ELandscapeToolFlattenMode::Lower:
							if (Delta > 0)
							{
								DataScanline[X] = FMath::FloorToInt(FMath::Lerp((float)DataScanline[X], (float)FlattenHeight, Strength));
							}
							break;
						default:
						case ELandscapeToolFlattenMode::Both:
							if (Delta > 0)
							{
								DataScanline[X] = FMath::FloorToInt(FMath::Lerp((float)DataScanline[X], (float)FlattenHeight, Strength));
							}
							else
							{
								DataScanline[X] = FMath::CeilToInt(FMath::Lerp((float)DataScanline[X], (float)FlattenHeight, Strength));
							}
							break;
						}
					}
					else
					{
						typename ToolTarget::CacheClass::DataType DestValue = -(FlattenNormal.X * X + FlattenNormal.Y * Y + FlattenPlaneDist) / FlattenNormal.Z;
						//float PlaneDist = FlattenNormal | FVector(X, Y, HeightData(HeightDataIndex)) + FlattenPlaneDist;
						float PlaneDist = DataScanline[X] - DestValue;
						DestValue = DataScanline[X] - PlaneDist * Strength;
						switch (UISettings->FlattenMode)
						{
						case ELandscapeToolFlattenMode::Raise:
							if (PlaneDist < 0)
							{
								DataScanline[X] = FMath::CeilToInt(FMath::Lerp((float)DataScanline[X], (float)DestValue, Strength));
							}
							break;
						case ELandscapeToolFlattenMode::Lower:
							if (PlaneDist > 0)
							{
								DataScanline[X] = FMath::FloorToInt(FMath::Lerp((float)DataScanline[X], (float)DestValue, Strength));
							}
							break;
						default:
						case ELandscapeToolFlattenMode::Both:
							if (PlaneDist > 0)
							{
								DataScanline[X] = FMath::FloorToInt(FMath::Lerp((float)DataScanline[X], (float)DestValue, Strength));
							}
							else
							{
								DataScanline[X] = FMath::CeilToInt(FMath::Lerp((float)DataScanline[X], (float)DestValue, Strength));
							}
							break;
						}
					}
				}
			}
		}

		this->Cache.SetCachedData(X1, Y1, X2, Y2, Data, UISettings->PaintingRestriction);
		this->Cache.Flush();
	}
};

template<class ToolTarget>
class FLandscapeToolFlatten : public FLandscapeToolPaintBase < ToolTarget, FLandscapeToolStrokeFlatten<ToolTarget> >
{
protected:
	UStaticMesh* PlaneMesh;
	UStaticMeshComponent* MeshComponent;
	bool CanToolBeActivatedNextTick;
	bool CanToolBeActivatedValue;
	float EyeDropperFlattenTargetValue;

public:
	FLandscapeToolFlatten(FEdModeLandscape* InEdMode)
		: FLandscapeToolPaintBase<ToolTarget, FLandscapeToolStrokeFlatten<ToolTarget>>(InEdMode)
		, PlaneMesh(LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorLandscapeResources/FlattenPlaneMesh.FlattenPlaneMesh")))
		, MeshComponent(NULL)
		, CanToolBeActivatedNextTick(false)
		, CanToolBeActivatedValue(false)
		, EyeDropperFlattenTargetValue(0.0f)
	{
		check(PlaneMesh);
	}

	virtual bool GetCursor(EMouseCursor::Type& OutCursor) const override
	{ 
		if (this->EdMode->UISettings->bFlattenEyeDropperModeActivated)
		{
			OutCursor = EMouseCursor::EyeDropper;
			return true;
		}
		
		return false; 
	}

	virtual void SetCanToolBeActivated(bool Value) override
	{ 
		CanToolBeActivatedNextTick = true;
		CanToolBeActivatedValue = Value;
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(PlaneMesh);
		Collector.AddReferencedObject(MeshComponent);
	}

	virtual const TCHAR* GetToolName() override { return TEXT("Flatten"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Flatten", "Flatten"); };

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override
	{
		if (CanToolBeActivatedNextTick)
		{
			this->bCanToolBeActivated = CanToolBeActivatedValue;
			CanToolBeActivatedNextTick = false;
		}

		FLandscapeToolPaintBase<ToolTarget, FLandscapeToolStrokeFlatten<ToolTarget>>::Tick(ViewportClient, DeltaTime);

		bool bShowGrid = this->EdMode->UISettings->bUseFlattenTarget && this->EdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap && this->EdMode->UISettings->bShowFlattenTargetPreview;
		MeshComponent->SetVisibility(bShowGrid);
	}

	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override
	{
		bool bResult = FLandscapeToolPaintBase<ToolTarget, FLandscapeToolStrokeFlatten<ToolTarget>>::MouseMove(ViewportClient, Viewport, x, y);

		if (ViewportClient->IsLevelEditorClient() && MeshComponent != NULL)
		{
			FVector MousePosition;
			this->EdMode->LandscapeMouseTrace((FEditorViewportClient*)ViewportClient, x, y, MousePosition);

			const FTransform LocalToWorld = this->EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy()->ActorToWorld();
			FVector Origin;
			Origin.X = FMath::RoundToFloat(MousePosition.X);
			Origin.Y = FMath::RoundToFloat(MousePosition.Y);
			Origin.Z = (FMath::RoundToFloat((this->EdMode->UISettings->FlattenTarget - LocalToWorld.GetTranslation().Z) / LocalToWorld.GetScale3D().Z * LANDSCAPE_INV_ZSCALE) - 0.1f) * LANDSCAPE_ZSCALE;
			MeshComponent->SetRelativeLocation(Origin, false);

			// Clamp the value to the height map
			uint16 TexHeight = LandscapeDataAccess::GetTexHeight(MousePosition.Z);
			float Height = LandscapeDataAccess::GetLocalHeight(TexHeight);

			// Convert the height back to world space
			this->EdMode->UISettings->FlattenEyeDropperModeDesiredTarget = (Height * LocalToWorld.GetScale3D().Z) + LocalToWorld.GetTranslation().Z;
		}

		return bResult;
	}

	virtual void EnterTool() override
	{
		FLandscapeToolPaintBase<ToolTarget, FLandscapeToolStrokeFlatten<ToolTarget>>::EnterTool();

		ALandscapeProxy* LandscapeProxy = this->EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
		MeshComponent = NewObject<UStaticMeshComponent>(LandscapeProxy, NAME_None, RF_Transient);
		MeshComponent->SetStaticMesh(PlaneMesh);
		MeshComponent->AttachToComponent(LandscapeProxy->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		MeshComponent->RegisterComponent();

		bool bShowGrid = this->EdMode->UISettings->bUseFlattenTarget && this->EdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Heightmap && this->EdMode->UISettings->bShowFlattenTargetPreview;
		MeshComponent->SetVisibility(bShowGrid);

		// Try to set a sane initial location for the preview grid
		const FTransform LocalToWorld = this->EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy()->GetRootComponent()->GetComponentToWorld();
		FVector Origin = FVector::ZeroVector;
		Origin.Z = (FMath::RoundToFloat((this->EdMode->UISettings->FlattenTarget - LocalToWorld.GetTranslation().Z) / LocalToWorld.GetScale3D().Z * LANDSCAPE_INV_ZSCALE) - 0.1f) * LANDSCAPE_ZSCALE;
		MeshComponent->SetRelativeLocation(Origin, false);
	}

	virtual void ExitTool() override
	{
		FLandscapeToolPaintBase<ToolTarget, FLandscapeToolStrokeFlatten<ToolTarget>>::ExitTool();

		MeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		MeshComponent->DestroyComponent();
	}
};

// 
// FLandscapeToolNoise
//
template<class ToolTarget>
class FLandscapeToolStrokeNoise : public FLandscapeToolStrokePaintBase<ToolTarget>
{
public:
	FLandscapeToolStrokeNoise(FEdModeLandscape* InEdMode, FEditorViewportClient* InViewportClient, const FLandscapeToolTarget& InTarget)
		: FLandscapeToolStrokePaintBase<ToolTarget>(InEdMode, InViewportClient, InTarget)
	{
	}

	void Apply(FEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolInteractorPosition>& InteractorPositions)
	{
		if (!this->LandscapeInfo) return;

		// Get list of verts to update
		FLandscapeBrushData BrushInfo = Brush->ApplyBrush(InteractorPositions);
		if (!BrushInfo)
		{
			return;
		}

		int32 X1, Y1, X2, Y2;
		BrushInfo.GetInclusiveBounds(X1, Y1, X2, Y2);

		// Tablet pressure
		float Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.0f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		CA_SUPPRESS(6326);
		if (ToolTarget::TargetType == ELandscapeToolTargetType::Heightmap)
		{
			X1 -= 1;
			Y1 -= 1;
			X2 += 1;
			Y2 += 1;
		}

		this->Cache.CacheData(X1, Y1, X2, Y2);
		TArray<typename ToolTarget::CacheClass::DataType> Data;
		this->Cache.GetCachedData(X1, Y1, X2, Y2, Data);

		float BrushSizeAdjust = 1.0f;
		CA_SUPPRESS(6326);
		if (ToolTarget::TargetType != ELandscapeToolTargetType::Weightmap && UISettings->BrushRadius < UISettings->MaximumValueRadius)
		{
			BrushSizeAdjust = UISettings->BrushRadius / UISettings->MaximumValueRadius;
		}

		CA_SUPPRESS(6326);
		bool bUseWeightTargetValue = UISettings->bUseWeightTargetValue && ToolTarget::TargetType == ELandscapeToolTargetType::Weightmap;

		// Apply the brush
		for (int32 Y = BrushInfo.GetBounds().Min.Y; Y < BrushInfo.GetBounds().Max.Y; Y++)
		{
			const float* BrushScanline = BrushInfo.GetDataPtr(FIntPoint(0, Y));
			auto* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);

			for (int32 X = BrushInfo.GetBounds().Min.X; X < BrushInfo.GetBounds().Max.X; X++)
			{
				const float BrushValue = BrushScanline[X];

				if (BrushValue > 0.0f)
				{
					float OriginalValue = DataScanline[X];
					if (bUseWeightTargetValue)
					{
						FNoiseParameter NoiseParam(0, UISettings->NoiseScale, 255.0f / 2.0f);
						float DestValue = NoiseModeConversion(ELandscapeToolNoiseMode::Add, NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y)) * UISettings->WeightTargetValue;
						switch (UISettings->NoiseMode)
						{
						case ELandscapeToolNoiseMode::Add:
							if (OriginalValue >= DestValue)
							{
								continue;
							}
							break;
						case ELandscapeToolNoiseMode::Sub:
							DestValue += (1.0f - UISettings->WeightTargetValue) * NoiseParam.NoiseAmount;
							if (OriginalValue <= DestValue)
							{
								continue;
							}
							break;
						}
						DataScanline[X] = ToolTarget::CacheClass::ClampValue(FMath::RoundToInt(FMath::Lerp(OriginalValue, DestValue, BrushValue * UISettings->ToolStrength * Pressure)));
					}
					else
					{
						float TotalStrength = BrushValue * UISettings->ToolStrength * Pressure * ToolTarget::StrengthMultiplier(this->LandscapeInfo, UISettings->BrushRadius);
						FNoiseParameter NoiseParam(0, UISettings->NoiseScale, TotalStrength * BrushSizeAdjust);
						float PaintAmount = NoiseModeConversion(UISettings->NoiseMode, NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y));
						DataScanline[X] = ToolTarget::CacheClass::ClampValue(OriginalValue + PaintAmount);
					}
				}
			}
		}

		this->Cache.SetCachedData(X1, Y1, X2, Y2, Data, UISettings->PaintingRestriction);
		this->Cache.Flush();
	}
};

template<class ToolTarget>
class FLandscapeToolNoise : public FLandscapeToolPaintBase < ToolTarget, FLandscapeToolStrokeNoise<ToolTarget> >
{
public:
	FLandscapeToolNoise(FEdModeLandscape* InEdMode)
		: FLandscapeToolPaintBase<ToolTarget, FLandscapeToolStrokeNoise<ToolTarget> >(InEdMode)
	{
	}

	virtual const TCHAR* GetToolName() override { return TEXT("Noise"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Noise", "Noise"); };
};


//
// Toolset initialization
//
void FEdModeLandscape::InitializeTool_Paint()
{
	auto Tool_Sculpt = MakeUnique<FLandscapeToolSculpt>(this);
	Tool_Sculpt->ValidBrushes.Add("BrushSet_Circle");
	Tool_Sculpt->ValidBrushes.Add("BrushSet_Alpha");
	Tool_Sculpt->ValidBrushes.Add("BrushSet_Pattern");
	Tool_Sculpt->ValidBrushes.Add("BrushSet_Component");
	LandscapeTools.Add(MoveTemp(Tool_Sculpt));

	auto Tool_Paint = MakeUnique<FLandscapeToolPaint>(this);
	Tool_Paint->ValidBrushes.Add("BrushSet_Circle");
	Tool_Paint->ValidBrushes.Add("BrushSet_Alpha");
	Tool_Paint->ValidBrushes.Add("BrushSet_Pattern");
	Tool_Paint->ValidBrushes.Add("BrushSet_Component");
	LandscapeTools.Add(MoveTemp(Tool_Paint));
}

void FEdModeLandscape::InitializeTool_Smooth()
{
	auto Tool_Smooth_Heightmap = MakeUnique<FLandscapeToolSmooth<FHeightmapToolTarget>>(this);
	Tool_Smooth_Heightmap->ValidBrushes.Add("BrushSet_Circle");
	Tool_Smooth_Heightmap->ValidBrushes.Add("BrushSet_Alpha");
	Tool_Smooth_Heightmap->ValidBrushes.Add("BrushSet_Pattern");
	LandscapeTools.Add(MoveTemp(Tool_Smooth_Heightmap));

	auto Tool_Smooth_Weightmap = MakeUnique<FLandscapeToolSmooth<FWeightmapToolTarget>>(this);
	Tool_Smooth_Weightmap->ValidBrushes.Add("BrushSet_Circle");
	Tool_Smooth_Weightmap->ValidBrushes.Add("BrushSet_Alpha");
	Tool_Smooth_Weightmap->ValidBrushes.Add("BrushSet_Pattern");
	LandscapeTools.Add(MoveTemp(Tool_Smooth_Weightmap));
}

void FEdModeLandscape::InitializeTool_Flatten()
{
	auto Tool_Flatten_Heightmap = MakeUnique<FLandscapeToolFlatten<FHeightmapToolTarget>>(this);
	Tool_Flatten_Heightmap->ValidBrushes.Add("BrushSet_Circle");
	Tool_Flatten_Heightmap->ValidBrushes.Add("BrushSet_Alpha");
	Tool_Flatten_Heightmap->ValidBrushes.Add("BrushSet_Pattern");
	LandscapeTools.Add(MoveTemp(Tool_Flatten_Heightmap));

	auto Tool_Flatten_Weightmap = MakeUnique<FLandscapeToolFlatten<FWeightmapToolTarget>>(this);
	Tool_Flatten_Weightmap->ValidBrushes.Add("BrushSet_Circle");
	Tool_Flatten_Weightmap->ValidBrushes.Add("BrushSet_Alpha");
	Tool_Flatten_Weightmap->ValidBrushes.Add("BrushSet_Pattern");
	LandscapeTools.Add(MoveTemp(Tool_Flatten_Weightmap));
}

void FEdModeLandscape::InitializeTool_Noise()
{
	auto Tool_Noise_Heightmap = MakeUnique<FLandscapeToolNoise<FHeightmapToolTarget>>(this);
	Tool_Noise_Heightmap->ValidBrushes.Add("BrushSet_Circle");
	Tool_Noise_Heightmap->ValidBrushes.Add("BrushSet_Alpha");
	Tool_Noise_Heightmap->ValidBrushes.Add("BrushSet_Pattern");
	LandscapeTools.Add(MoveTemp(Tool_Noise_Heightmap));

	auto Tool_Noise_Weightmap = MakeUnique<FLandscapeToolNoise<FWeightmapToolTarget>>(this);
	Tool_Noise_Weightmap->ValidBrushes.Add("BrushSet_Circle");
	Tool_Noise_Weightmap->ValidBrushes.Add("BrushSet_Alpha");
	Tool_Noise_Weightmap->ValidBrushes.Add("BrushSet_Pattern");
	LandscapeTools.Add(MoveTemp(Tool_Noise_Weightmap));
}
