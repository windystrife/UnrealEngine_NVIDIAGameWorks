// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "LandscapeProxy.h"
#include "LandscapeToolInterface.h"
#include "LandscapeEdMode.h"
#include "EditorViewportClient.h"
#include "LandscapeEdit.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "InstancedFoliageActor.h"
#include "VREditorInteractor.h"
#include "AI/Navigation/NavigationSystem.h"

// VR Editor

//
//	FNoiseParameter - Perlin noise
//
struct FNoiseParameter
{
	float	Base,
		NoiseScale,
		NoiseAmount;

	// Constructors.

	FNoiseParameter()
	{
	}
	FNoiseParameter(float InBase, float InScale, float InAmount) :
		Base(InBase),
		NoiseScale(InScale),
		NoiseAmount(InAmount)
	{
	}

	// Sample
	float Sample(int32 X, int32 Y) const
	{
		float	Noise = 0.0f;
		X = FMath::Abs(X);
		Y = FMath::Abs(Y);

		if (NoiseScale > DELTA)
		{
			for (uint32 Octave = 0; Octave < 4; Octave++)
			{
				float	OctaveShift = 1 << Octave;
				float	OctaveScale = OctaveShift / NoiseScale;
				Noise += PerlinNoise2D(X * OctaveScale, Y * OctaveScale) / OctaveShift;
			}
		}

		return Base + Noise * NoiseAmount;
	}

	// TestGreater - Returns 1 if TestValue is greater than the parameter.
	bool TestGreater(int32 X, int32 Y, float TestValue) const
	{
		float	ParameterValue = Base;

		if (NoiseScale > DELTA)
		{
			for (uint32 Octave = 0; Octave < 4; Octave++)
			{
				float	OctaveShift = 1 << Octave;
				float	OctaveAmplitude = NoiseAmount / OctaveShift;

				// Attempt to avoid calculating noise if the test value is outside of the noise amplitude.

				if (TestValue > ParameterValue + OctaveAmplitude)
					return 1;
				else if (TestValue < ParameterValue - OctaveAmplitude)
					return 0;
				else
				{
					float	OctaveScale = OctaveShift / NoiseScale;
					ParameterValue += PerlinNoise2D(X * OctaveScale, Y * OctaveScale) * OctaveAmplitude;
				}
			}
		}

		return TestValue >= ParameterValue;
	}

	// TestLess
	bool TestLess(int32 X, int32 Y, float TestValue) const { return !TestGreater(X, Y, TestValue); }

private:
	static const int32 Permutations[256];

	bool operator==(const FNoiseParameter& SrcNoise)
	{
		if ((Base == SrcNoise.Base) &&
			(NoiseScale == SrcNoise.NoiseScale) &&
			(NoiseAmount == SrcNoise.NoiseAmount))
		{
			return true;
		}

		return false;
	}

	void operator=(const FNoiseParameter& SrcNoise)
	{
		Base = SrcNoise.Base;
		NoiseScale = SrcNoise.NoiseScale;
		NoiseAmount = SrcNoise.NoiseAmount;
	}


	float Fade(float T) const
	{
		return T * T * T * (T * (T * 6 - 15) + 10);
	}


	float Grad(int32 Hash, float X, float Y) const
	{
		int32		H = Hash & 15;
		float	U = H < 8 || H == 12 || H == 13 ? X : Y,
			V = H < 4 || H == 12 || H == 13 ? Y : 0;
		return ((H & 1) == 0 ? U : -U) + ((H & 2) == 0 ? V : -V);
	}

	float PerlinNoise2D(float X, float Y) const
	{
		int32		TruncX = FMath::TruncToInt(X),
			TruncY = FMath::TruncToInt(Y),
			IntX = TruncX & 255,
			IntY = TruncY & 255;
		float	FracX = X - TruncX,
			FracY = Y - TruncY;

		float	U = Fade(FracX),
			V = Fade(FracY);

		int32	A = Permutations[IntX] + IntY,
			AA = Permutations[A & 255],
			AB = Permutations[(A + 1) & 255],
			B = Permutations[(IntX + 1) & 255] + IntY,
			BA = Permutations[B & 255],
			BB = Permutations[(B + 1) & 255];

		return	FMath::Lerp(FMath::Lerp(Grad(Permutations[AA], FracX, FracY),
			Grad(Permutations[BA], FracX - 1, FracY), U),
			FMath::Lerp(Grad(Permutations[AB], FracX, FracY - 1),
			Grad(Permutations[BB], FracX - 1, FracY - 1), U), V);
	}
};



#if WITH_KISSFFT
#include "tools/kiss_fftnd.h"
#endif

template<typename DataType>
inline void LowPassFilter(int32 X1, int32 Y1, int32 X2, int32 Y2, FLandscapeBrushData& BrushInfo, TArray<DataType>& Data, const float DetailScale, const float ApplyRatio = 1.0f)
{
#if WITH_KISSFFT
	// Low-pass filter
	int32 FFTWidth = X2 - X1 - 1;
	int32 FFTHeight = Y2 - Y1 - 1;

	if (FFTWidth <= 1 && FFTHeight <= 1)
	{
		// nothing to do
		return;
	}

	const int32 NDims = 2;
	const int32 Dims[NDims] = { FFTHeight, FFTWidth };
	kiss_fftnd_cfg stf = kiss_fftnd_alloc(Dims, NDims, 0, NULL, NULL),
		sti = kiss_fftnd_alloc(Dims, NDims, 1, NULL, NULL);

	kiss_fft_cpx *buf = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * Dims[0] * Dims[1]);
	kiss_fft_cpx *out = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * Dims[0] * Dims[1]);

	for (int32 Y = Y1 + 1; Y <= Y2 - 1; Y++)
	{
		auto* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
		auto* bufScanline = buf + (Y - (Y1 + 1)) * Dims[1] + (0 - (X1 + 1));

		for (int32 X = X1 + 1; X <= X2 - 1; X++)
		{
			bufScanline[X].r = DataScanline[X];
			bufScanline[X].i = 0;
		}
	}

	// Forward FFT
	kiss_fftnd(stf, buf, out);

	int32 CenterPos[2] = { Dims[0] >> 1, Dims[1] >> 1 };
	for (int32 Y = 0; Y < Dims[0]; Y++)
	{
		float DistFromCenter = 0.0f;
		for (int32 X = 0; X < Dims[1]; X++)
		{
			if (Y < CenterPos[0])
			{
				if (X < CenterPos[1])
				{
					// 1
					DistFromCenter = X*X + Y*Y;
				}
				else
				{
					// 2
					DistFromCenter = (X - Dims[1])*(X - Dims[1]) + Y*Y;
				}
			}
			else
			{
				if (X < CenterPos[1])
				{
					// 3
					DistFromCenter = X*X + (Y - Dims[0])*(Y - Dims[0]);
				}
				else
				{
					// 4
					DistFromCenter = (X - Dims[1])*(X - Dims[1]) + (Y - Dims[0])*(Y - Dims[0]);
				}
			}
			// High frequency removal
			float Ratio = 1.0f - DetailScale;
			float Dist = FMath::Min<float>((Dims[0] * Ratio)*(Dims[0] * Ratio), (Dims[1] * Ratio)*(Dims[1] * Ratio));
			float Filter = 1.0 / (1.0 + DistFromCenter / Dist);
			CA_SUPPRESS(6385);
			out[X + Y*Dims[1]].r *= Filter;
			out[X + Y*Dims[1]].i *= Filter;
		}
	}

	// Inverse FFT
	kiss_fftnd(sti, out, buf);

	float Scale = Dims[0] * Dims[1];
	const int32 BrushX1 = FMath::Max<int32>(BrushInfo.GetBounds().Min.X, X1 + 1);
	const int32 BrushY1 = FMath::Max<int32>(BrushInfo.GetBounds().Min.Y, Y1 + 1);
	const int32 BrushX2 = FMath::Min<int32>(BrushInfo.GetBounds().Max.X, X2);
	const int32 BrushY2 = FMath::Min<int32>(BrushInfo.GetBounds().Max.Y, Y2);
	for (int32 Y = BrushY1; Y < BrushY2; Y++)
	{
		const float* BrushScanline = BrushInfo.GetDataPtr(FIntPoint(0, Y));
		auto* DataScanline = Data.GetData() + (Y - Y1) * (X2 - X1 + 1) + (0 - X1);
		auto* bufScanline = buf + (Y - (Y1 + 1)) * Dims[1] + (0 - (X1 + 1));

		for (int32 X = BrushX1; X < BrushX2; X++)
		{
			const float BrushValue = BrushScanline[X];

			if (BrushValue > 0.0f)
			{
				DataScanline[X] = FMath::Lerp((float)DataScanline[X], bufScanline[X].r / Scale, BrushValue * ApplyRatio);
			}
		}
	}

	// Free FFT allocation
	KISS_FFT_FREE(stf);
	KISS_FFT_FREE(sti);
	KISS_FFT_FREE(buf);
	KISS_FFT_FREE(out);
#endif
}



//
// TLandscapeEditCache
//
template<class Accessor, typename AccessorType>
struct TLandscapeEditCache
{
public:
	typedef AccessorType DataType;
	Accessor DataAccess;

	TLandscapeEditCache(const FLandscapeToolTarget& InTarget)
		: DataAccess(InTarget)
		, Valid(false)
	{
	}

	// X2/Y2 Coordinates are "inclusive" max values
	void CacheData(int32 X1, int32 Y1, int32 X2, int32 Y2)
	{
		if (!Valid)
		{
			if (Accessor::bUseInterp)
			{
				ValidX1 = CachedX1 = X1;
				ValidY1 = CachedY1 = Y1;
				ValidX2 = CachedX2 = X2;
				ValidY2 = CachedY2 = Y2;

				DataAccess.GetData(ValidX1, ValidY1, ValidX2, ValidY2, CachedData);
				if (!ensureMsgf(ValidX1 <= ValidX2 && ValidY1 <= ValidY2, TEXT("Invalid cache area: X(%d-%d), Y(%d-%d) from region X(%d-%d), Y(%d-%d)"), ValidX1, ValidX2, ValidY1, ValidY2, X1, X2, Y1, Y2))
				{
					Valid = false;
					return;
				}
			}
			else
			{
				CachedX1 = X1;
				CachedY1 = Y1;
				CachedX2 = X2;
				CachedY2 = Y2;

				DataAccess.GetDataFast(CachedX1, CachedY1, CachedX2, CachedY2, CachedData);
			}

			OriginalData = CachedData;

			Valid = true;
		}
		else
		{
			// Extend the cache area if needed
			if (X1 < CachedX1)
			{
				if (Accessor::bUseInterp)
				{
					int32 x1 = X1;
					int32 x2 = ValidX1;
					int32 y1 = FMath::Min<int32>(Y1, CachedY1);
					int32 y2 = FMath::Max<int32>(Y2, CachedY2);

					DataAccess.GetData(x1, y1, x2, y2, CachedData);
					ValidX1 = FMath::Min<int32>(x1, ValidX1);
				}
				else
				{
					DataAccess.GetDataFast(X1, CachedY1, CachedX1 - 1, CachedY2, CachedData);
				}

				CacheOriginalData(X1, CachedY1, CachedX1 - 1, CachedY2);
				CachedX1 = X1;
			}

			if (X2 > CachedX2)
			{
				if (Accessor::bUseInterp)
				{
					int32 x1 = ValidX2;
					int32 x2 = X2;
					int32 y1 = FMath::Min<int32>(Y1, CachedY1);
					int32 y2 = FMath::Max<int32>(Y2, CachedY2);

					DataAccess.GetData(x1, y1, x2, y2, CachedData);
					ValidX2 = FMath::Max<int32>(x2, ValidX2);
				}
				else
				{
					DataAccess.GetDataFast(CachedX2 + 1, CachedY1, X2, CachedY2, CachedData);
				}
				CacheOriginalData(CachedX2 + 1, CachedY1, X2, CachedY2);
				CachedX2 = X2;
			}

			if (Y1 < CachedY1)
			{
				if (Accessor::bUseInterp)
				{
					int32 x1 = CachedX1;
					int32 x2 = CachedX2;
					int32 y1 = Y1;
					int32 y2 = ValidY1;

					DataAccess.GetData(x1, y1, x2, y2, CachedData);
					ValidY1 = FMath::Min<int32>(y1, ValidY1);
				}
				else
				{
					DataAccess.GetDataFast(CachedX1, Y1, CachedX2, CachedY1 - 1, CachedData);
				}
				CacheOriginalData(CachedX1, Y1, CachedX2, CachedY1 - 1);
				CachedY1 = Y1;
			}

			if (Y2 > CachedY2)
			{
				if (Accessor::bUseInterp)
				{
					int32 x1 = CachedX1;
					int32 x2 = CachedX2;
					int32 y1 = ValidY2;
					int32 y2 = Y2;

					DataAccess.GetData(x1, y1, x2, y2, CachedData);
					ValidY2 = FMath::Max<int32>(y2, ValidY2);
				}
				else
				{
					DataAccess.GetDataFast(CachedX1, CachedY2 + 1, CachedX2, Y2, CachedData);
				}

				CacheOriginalData(CachedX1, CachedY2 + 1, CachedX2, Y2);
				CachedY2 = Y2;
			}
		}
	}

	AccessorType* GetValueRef(int32 LandscapeX, int32 LandscapeY)
	{
		return CachedData.Find(FIntPoint(LandscapeX, LandscapeY));
	}

	float GetValue(float LandscapeX, float LandscapeY)
	{
		int32 X = FMath::FloorToInt(LandscapeX);
		int32 Y = FMath::FloorToInt(LandscapeY);
		AccessorType* P00 = CachedData.Find(FIntPoint(X, Y));
		AccessorType* P10 = CachedData.Find(FIntPoint(X + 1, Y));
		AccessorType* P01 = CachedData.Find(FIntPoint(X, Y + 1));
		AccessorType* P11 = CachedData.Find(FIntPoint(X + 1, Y + 1));

		// Search for nearest value if missing data
		float V00 = P00 ? *P00 : (P10 ? *P10 : (P01 ? *P01 : (P11 ? *P11 : 0.0f)));
		float V10 = P10 ? *P10 : (P00 ? *P00 : (P11 ? *P11 : (P01 ? *P01 : 0.0f)));
		float V01 = P01 ? *P01 : (P00 ? *P00 : (P11 ? *P11 : (P10 ? *P10 : 0.0f)));
		float V11 = P11 ? *P11 : (P10 ? *P10 : (P01 ? *P01 : (P00 ? *P00 : 0.0f)));

		return FMath::Lerp(
			FMath::Lerp(V00, V10, LandscapeX - X),
			FMath::Lerp(V01, V11, LandscapeX - X),
			LandscapeY - Y);
	}

	FVector GetNormal(int32 X, int32 Y)
	{
		AccessorType* P00 = CachedData.Find(FIntPoint(X, Y));
		AccessorType* P10 = CachedData.Find(FIntPoint(X + 1, Y));
		AccessorType* P01 = CachedData.Find(FIntPoint(X, Y + 1));
		AccessorType* P11 = CachedData.Find(FIntPoint(X + 1, Y + 1));

		// Search for nearest value if missing data
		float V00 = P00 ? *P00 : (P10 ? *P10 : (P01 ? *P01 : (P11 ? *P11 : 0.0f)));
		float V10 = P10 ? *P10 : (P00 ? *P00 : (P11 ? *P11 : (P01 ? *P01 : 0.0f)));
		float V01 = P01 ? *P01 : (P00 ? *P00 : (P11 ? *P11 : (P10 ? *P10 : 0.0f)));
		float V11 = P11 ? *P11 : (P10 ? *P10 : (P01 ? *P01 : (P00 ? *P00 : 0.0f)));

		FVector Vert00 = FVector(0.0f, 0.0f, V00);
		FVector Vert01 = FVector(0.0f, 1.0f, V01);
		FVector Vert10 = FVector(1.0f, 0.0f, V10);
		FVector Vert11 = FVector(1.0f, 1.0f, V11);

		FVector FaceNormal1 = ((Vert00 - Vert10) ^ (Vert10 - Vert11)).GetSafeNormal();
		FVector FaceNormal2 = ((Vert11 - Vert01) ^ (Vert01 - Vert00)).GetSafeNormal();
		return (FaceNormal1 + FaceNormal2).GetSafeNormal();
	}

	void SetValue(int32 LandscapeX, int32 LandscapeY, AccessorType Value)
	{
		CachedData.Add(FIntPoint(LandscapeX, LandscapeY), Forward<AccessorType>(Value));
	}

	bool IsZeroValue(const FVector& Value)
	{
		return (FMath::IsNearlyZero(Value.X) && FMath::IsNearlyZero(Value.Y));
	}

	bool IsZeroValue(const FVector2D& Value)
	{
		return (FMath::IsNearlyZero(Value.X) && FMath::IsNearlyZero(Value.Y));
	}

	bool IsZeroValue(const uint16& Value)
	{
		return Value == 0;
	}

	bool IsZeroValue(const uint8& Value)
	{
		return Value == 0;
	}

	// X2/Y2 Coordinates are "inclusive" max values
	bool GetCachedData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<AccessorType>& OutData)
	{
		const int32 XSize = (1 + X2 - X1);
		const int32 YSize = (1 + Y2 - Y1);
		const int32 NumSamples = XSize * YSize;
		OutData.Empty(NumSamples);
		OutData.AddUninitialized(NumSamples);
		bool bHasNonZero = false;

		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			const int32 YOffset = (Y - Y1) * XSize;
			for (int32 X = X1; X <= X2; X++)
			{
				const int32 XYOffset = YOffset + (X - X1);
				AccessorType* Ptr = GetValueRef(X, Y);
				if (Ptr)
				{
					OutData[XYOffset] = *Ptr;
					if (!IsZeroValue(*Ptr))
					{
						bHasNonZero = true;
					}
				}
				else
				{
					OutData[XYOffset] = (AccessorType)0;
				}
			}
		}

		return bHasNonZero;
	}

	// X2/Y2 Coordinates are "inclusive" max values
	void SetCachedData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<AccessorType>& Data, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None)
	{
		checkSlow(Data.Num() == (1 + Y2 - Y1) * (1 + X2 - X1));

		// Update cache
		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			for (int32 X = X1; X <= X2; X++)
			{
				SetValue(X, Y, Data[(X - X1) + (Y - Y1)*(1 + X2 - X1)]);
			}
		}

		// Update real data
		DataAccess.SetData(X1, Y1, X2, Y2, Data.GetData(), PaintingRestriction);
	}

	// Get the original data before we made any changes with the SetCachedData interface.
	// X2/Y2 Coordinates are "inclusive" max values
	void GetOriginalData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<AccessorType>& OutOriginalData)
	{
		int32 NumSamples = (1 + X2 - X1)*(1 + Y2 - Y1);
		OutOriginalData.Empty(NumSamples);
		OutOriginalData.AddUninitialized(NumSamples);

		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			for (int32 X = X1; X <= X2; X++)
			{
				AccessorType* Ptr = OriginalData.Find(FIntPoint(X, Y));
				if (Ptr)
				{
					OutOriginalData[(X - X1) + (Y - Y1)*(1 + X2 - X1)] = *Ptr;
				}
			}
		}
	}

	void Flush()
	{
		DataAccess.Flush();
	}

private:
	// X2/Y2 Coordinates are "inclusive" max values
	void CacheOriginalData(int32 X1, int32 Y1, int32 X2, int32 Y2)
	{
		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			for (int32 X = X1; X <= X2; X++)
			{
				FIntPoint Key = FIntPoint(X, Y);
				AccessorType* Ptr = CachedData.Find(Key);
				if (Ptr)
				{
					check(OriginalData.Find(Key) == NULL);
					OriginalData.Add(Key, *Ptr);
				}
			}
		}
	}

	TMap<FIntPoint, AccessorType> CachedData;
	TMap<FIntPoint, AccessorType> OriginalData;

	bool Valid;

	int32 CachedX1;
	int32 CachedY1;
	int32 CachedX2;
	int32 CachedY2;

	// To store valid region....
	int32 ValidX1, ValidX2, ValidY1, ValidY2;
};

//
// FHeightmapAccessor
//
template<bool bInUseInterp>
struct FHeightmapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FHeightmapAccessor(ULandscapeInfo* InLandscapeInfo)
	{
		LandscapeInfo = InLandscapeInfo;
		LandscapeEdit = new FLandscapeEditDataInterface(InLandscapeInfo);
	}

	FHeightmapAccessor(const FLandscapeToolTarget& InTarget)
		: FHeightmapAccessor(InTarget.LandscapeInfo.Get())
	{
	}

	// accessors
	void GetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, uint16>& Data)
	{
		LandscapeEdit->GetHeightData(X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(int32 X1, int32 Y1, int32 X2, int32 Y2, TMap<FIntPoint, uint16>& Data)
	{
		LandscapeEdit->GetHeightDataFast(X1, Y1, X2, Y2, Data);
	}

	void SetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const uint16* Data, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None)
	{
		TSet<ULandscapeComponent*> Components;
		if (LandscapeInfo && LandscapeEdit->GetComponentsInRegion(X1, Y1, X2, Y2, &Components))
		{
			// Update data
			ChangedComponents.Append(Components);

			for (ULandscapeComponent* Component : Components)
			{
				Component->InvalidateLightingCache();
			}

			// Flush dynamic foliage (grass)
			ALandscapeProxy::InvalidateGeneratedComponentData(Components);

			// Notify foliage to move any attached instances
			bool bUpdateFoliage = false;
			for (ULandscapeComponent* Component : Components)
			{
				ULandscapeHeightfieldCollisionComponent* CollisionComponent = Component->CollisionComponent.Get();
				if (CollisionComponent && AInstancedFoliageActor::HasFoliageAttached(CollisionComponent))
				{
					bUpdateFoliage = true;
					break;
				}
			}

			if (bUpdateFoliage)
			{
				// Calculate landscape local-space bounding box of old data, to look for foliage instances.
				TArray<ULandscapeHeightfieldCollisionComponent*> CollisionComponents;
				CollisionComponents.Empty(Components.Num());
				TArray<FBox> PreUpdateLocalBoxes;
				PreUpdateLocalBoxes.Empty(Components.Num());

				for (ULandscapeComponent* Component : Components)
				{
					ULandscapeHeightfieldCollisionComponent* CollisionComponent = Component->CollisionComponent.Get();
					if (CollisionComponent)
					{
						CollisionComponents.Add(CollisionComponent);
						PreUpdateLocalBoxes.Add(FBox(FVector((float)X1, (float)Y1, Component->CachedLocalBox.Min.Z), FVector((float)X2, (float)Y2, Component->CachedLocalBox.Max.Z)));
					}
				}

				// Update landscape.
				LandscapeEdit->SetHeightData(X1, Y1, X2, Y2, Data, 0, true);

				// Snap foliage for each component.
				for (int32 Index = 0; Index < CollisionComponents.Num(); ++Index)
				{
					ULandscapeHeightfieldCollisionComponent* CollisionComponent = CollisionComponents[Index];
					CollisionComponent->SnapFoliageInstances(PreUpdateLocalBoxes[Index].TransformBy(LandscapeInfo->GetLandscapeProxy()->LandscapeActorToWorld().ToMatrixWithScale()).ExpandBy(1.0f));
				}
			}
			else
			{
				// No foliage, just update landscape.
				LandscapeEdit->SetHeightData(X1, Y1, X2, Y2, Data, 0, true);
			}
		}
	}

	void Flush()
	{
		LandscapeEdit->Flush();
	}

	virtual ~FHeightmapAccessor()
	{
		delete LandscapeEdit;
		LandscapeEdit = NULL;

		// Update the bounds and navmesh for the components we edited
		for (TSet<ULandscapeComponent*>::TConstIterator It(ChangedComponents); It; ++It)
		{
			(*It)->UpdateCachedBounds();
			(*It)->UpdateComponentToWorld();

			// Recreate collision for modified components to update the physical materials
			ULandscapeHeightfieldCollisionComponent* CollisionComponent = (*It)->CollisionComponent.Get();
			if (CollisionComponent)
			{
				CollisionComponent->RecreateCollision();
				UNavigationSystem::UpdateComponentInNavOctree(*CollisionComponent);
			}
		}
	}

private:
	ULandscapeInfo* LandscapeInfo;
	FLandscapeEditDataInterface* LandscapeEdit;
	TSet<ULandscapeComponent*> ChangedComponents;
};

struct FLandscapeHeightCache : public TLandscapeEditCache<FHeightmapAccessor<true>, uint16>
{
	static uint16 ClampValue(int32 Value) { return FMath::Clamp(Value, 0, LandscapeDataAccess::MaxValue); }

	FLandscapeHeightCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FHeightmapAccessor<true>, uint16>(InTarget)
	{
	}
};

//
// FXYOffsetmapAccessor
//
template<bool bInUseInterp>
struct FXYOffsetmapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FXYOffsetmapAccessor(ULandscapeInfo* InLandscapeInfo)
	{
		LandscapeInfo = InLandscapeInfo;
		LandscapeEdit = new FLandscapeEditDataInterface(InLandscapeInfo);
	}

	FXYOffsetmapAccessor(const FLandscapeToolTarget& InTarget)
		: FXYOffsetmapAccessor(InTarget.LandscapeInfo.Get())
	{
	}

	// accessors
	void GetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, FVector>& Data)
	{
		LandscapeEdit->GetXYOffsetData(X1, Y1, X2, Y2, Data);

		TMap<FIntPoint, uint16> NewHeights;
		LandscapeEdit->GetHeightData(X1, Y1, X2, Y2, NewHeights);
		for (int32 Y = Y1; Y <= Y2; ++Y)
		{
			for (int32 X = X1; X <= X2; ++X)
			{
				FVector* Value = Data.Find(FIntPoint(X, Y));
				if (Value)
				{
					Value->Z = ((float)NewHeights.FindRef(FIntPoint(X, Y)) - 32768.0f) * LANDSCAPE_ZSCALE;
				}
			}
		}
	}

	void GetDataFast(int32 X1, int32 Y1, int32 X2, int32 Y2, TMap<FIntPoint, FVector>& Data)
	{
		LandscapeEdit->GetXYOffsetDataFast(X1, Y1, X2, Y2, Data);

		TMap<FIntPoint, uint16> NewHeights;
		LandscapeEdit->GetHeightData(X1, Y1, X2, Y2, NewHeights);
		for (int32 Y = Y1; Y <= Y2; ++Y)
		{
			for (int32 X = X1; X <= X2; ++X)
			{
				FVector* Value = Data.Find(FIntPoint(X, Y));
				if (Value)
				{
					Value->Z = ((float)NewHeights.FindRef(FIntPoint(X, Y)) - 32768.0f) * LANDSCAPE_ZSCALE;
				}
			}
		}
	}

	void SetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const FVector* Data, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None)
	{
		TSet<ULandscapeComponent*> Components;
		if (LandscapeInfo && LandscapeEdit->GetComponentsInRegion(X1, Y1, X2, Y2, &Components))
		{
			// Update data
			ChangedComponents.Append(Components);

			// Convert Height to uint16
			TArray<uint16> NewHeights;
			NewHeights.AddZeroed((Y2 - Y1 + 1) * (X2 - X1 + 1));
			for (int32 Y = Y1; Y <= Y2; ++Y)
			{
				for (int32 X = X1; X <= X2; ++X)
				{
					NewHeights[X - X1 + (Y - Y1) * (X2 - X1 + 1)] = FMath::Clamp<uint16>(Data[(X - X1 + (Y - Y1) * (X2 - X1 + 1))].Z * LANDSCAPE_INV_ZSCALE + 32768.0f, 0, 65535);
				}
			}

			// Flush dynamic foliage (grass)
			ALandscapeProxy::InvalidateGeneratedComponentData(Components);

			// Notify foliage to move any attached instances
			bool bUpdateFoliage = false;
			for (ULandscapeComponent* Component : Components)
			{
				ULandscapeHeightfieldCollisionComponent* CollisionComponent = Component->CollisionComponent.Get();
				if (CollisionComponent && AInstancedFoliageActor::HasFoliageAttached(CollisionComponent))
				{
					bUpdateFoliage = true;
					break;
				}
			}

			if (bUpdateFoliage)
			{
				// Calculate landscape local-space bounding box of old data, to look for foliage instances.
				TArray<ULandscapeHeightfieldCollisionComponent*> CollisionComponents;
				CollisionComponents.Empty(Components.Num());
				TArray<FBox> PreUpdateLocalBoxes;
				PreUpdateLocalBoxes.Empty(Components.Num());

				for (ULandscapeComponent* Component : Components)
				{
					CollisionComponents.Add(Component->CollisionComponent.Get());
					PreUpdateLocalBoxes.Add(FBox(FVector((float)X1, (float)Y1, Component->CachedLocalBox.Min.Z), FVector((float)X2, (float)Y2, Component->CachedLocalBox.Max.Z)));
				}

				// Update landscape.
				LandscapeEdit->SetXYOffsetData(X1, Y1, X2, Y2, Data, 0); // XY Offset always need to be update before the height update
				LandscapeEdit->SetHeightData(X1, Y1, X2, Y2, NewHeights.GetData(), 0, true);

				// Snap foliage for each component.
				for (int32 Index = 0; Index < CollisionComponents.Num(); ++Index)
				{
					ULandscapeHeightfieldCollisionComponent* CollisionComponent = CollisionComponents[Index];
					CollisionComponent->SnapFoliageInstances(PreUpdateLocalBoxes[Index].TransformBy(LandscapeInfo->GetLandscapeProxy()->LandscapeActorToWorld().ToMatrixWithScale()).ExpandBy(1.0f));
				}
			}
			else
			{
				// No foliage, just update landscape.
				LandscapeEdit->SetXYOffsetData(X1, Y1, X2, Y2, Data, 0); // XY Offset always need to be update before the height update
				LandscapeEdit->SetHeightData(X1, Y1, X2, Y2, NewHeights.GetData(), 0, true);
			}
		}
	}

	void Flush()
	{
		LandscapeEdit->Flush();
	}

	virtual ~FXYOffsetmapAccessor()
	{
		delete LandscapeEdit;
		LandscapeEdit = NULL;

		// Update the bounds for the components we edited
		for (TSet<ULandscapeComponent*>::TConstIterator It(ChangedComponents); It; ++It)
		{
			(*It)->UpdateCachedBounds();
			(*It)->UpdateComponentToWorld();
		}
	}

private:
	ULandscapeInfo* LandscapeInfo;
	FLandscapeEditDataInterface* LandscapeEdit;
	TSet<ULandscapeComponent*> ChangedComponents;
};

template<bool bInUseInterp>
struct FLandscapeXYOffsetCache : public TLandscapeEditCache<FXYOffsetmapAccessor<bInUseInterp>, FVector>
{
	FLandscapeXYOffsetCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FXYOffsetmapAccessor<bInUseInterp>, FVector>(InTarget)
	{
	}
};

//
// FAlphamapAccessor
//
template<bool bInUseInterp, bool bInUseTotalNormalize>
struct FAlphamapAccessor
{
	enum { bUseInterp = bInUseInterp };
	enum { bUseTotalNormalize = bInUseTotalNormalize };
	FAlphamapAccessor(ULandscapeInfo* InLandscapeInfo, ULandscapeLayerInfoObject* InLayerInfo)
		: LandscapeInfo(InLandscapeInfo)
		, LandscapeEdit(InLandscapeInfo)
		, LayerInfo(InLayerInfo)
		, bBlendWeight(true)
	{
		// should be no Layer change during FAlphamapAccessor lifetime...
		if (InLandscapeInfo && InLayerInfo)
		{
			if (LayerInfo == ALandscapeProxy::VisibilityLayer)
			{
				bBlendWeight = false;
			}
			else
			{
				bBlendWeight = !LayerInfo->bNoWeightBlend;
			}
		}
	}

	FAlphamapAccessor(const FLandscapeToolTarget& InTarget)
		: FAlphamapAccessor(InTarget.LandscapeInfo.Get(), InTarget.LayerInfo.Get())
	{
	}

	~FAlphamapAccessor()
	{
		// Recreate collision for modified components to update the physical materials
		for (ULandscapeComponent* Component : ModifiedComponents)
		{
			ULandscapeHeightfieldCollisionComponent* CollisionComponent = Component->CollisionComponent.Get();
			if (CollisionComponent)
			{
				CollisionComponent->RecreateCollision();

				// We need to trigger navigation mesh build, in case user have painted holes on a landscape
				if (LayerInfo == ALandscapeProxy::VisibilityLayer)
				{
					UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(Component);
					if (NavSys)
					{
						NavSys->UpdateComponentInNavOctree(*CollisionComponent);
					}
				}
			}
		}
	}

	void GetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, uint8>& Data)
	{
		LandscapeEdit.GetWeightData(LayerInfo, X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(int32 X1, int32 Y1, int32 X2, int32 Y2, TMap<FIntPoint, uint8>& Data)
	{
		LandscapeEdit.GetWeightDataFast(LayerInfo, X1, Y1, X2, Y2, Data);
	}

	void SetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const uint8* Data, ELandscapeLayerPaintingRestriction PaintingRestriction)
	{
		TSet<ULandscapeComponent*> Components;
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2, &Components))
		{
			// Flush dynamic foliage (grass)
			ALandscapeProxy::InvalidateGeneratedComponentData(Components);

			LandscapeEdit.SetAlphaData(LayerInfo, X1, Y1, X2, Y2, Data, 0, PaintingRestriction, bBlendWeight, bUseTotalNormalize);
			//LayerInfo->IsReferencedFromLoadedData = true;
			ModifiedComponents.Append(Components);
		}
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

private:
	ULandscapeInfo* LandscapeInfo;
	FLandscapeEditDataInterface LandscapeEdit;
	TSet<ULandscapeComponent*> ModifiedComponents;
	ULandscapeLayerInfoObject* LayerInfo;
	bool bBlendWeight;
};

struct FLandscapeAlphaCache : public TLandscapeEditCache<FAlphamapAccessor<true, false>, uint8>
{
	static uint8 ClampValue(int32 Value) { return FMath::Clamp(Value, 0, 255); }

	FLandscapeAlphaCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FAlphamapAccessor<true, false>, uint8>(InTarget)
	{
	}
};

struct FVisibilityAccessor : public FAlphamapAccessor<false, false>
{
	FVisibilityAccessor(const FLandscapeToolTarget& InTarget)
		: FAlphamapAccessor<false, false>(InTarget.LandscapeInfo.Get(), ALandscapeProxy::VisibilityLayer)
	{
	}
};

struct FLandscapeVisCache : public TLandscapeEditCache<FAlphamapAccessor<false, false>, uint8>
{
	static uint8 ClampValue(int32 Value) { return FMath::Clamp(Value, 0, 255); }

	FLandscapeVisCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FAlphamapAccessor<false, false>, uint8>(InTarget)
	{
	}
};

//
// FFullWeightmapAccessor
//
template<bool bInUseInterp>
struct FFullWeightmapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FFullWeightmapAccessor(ULandscapeInfo* InLandscapeInfo)
		: LandscapeInfo(InLandscapeInfo)
		, LandscapeEdit(InLandscapeInfo)
	{
	}

	FFullWeightmapAccessor(const FLandscapeToolTarget& InTarget)
		: FFullWeightmapAccessor(InTarget.LandscapeInfo.Get())
	{
	}

	~FFullWeightmapAccessor()
	{
		// Recreate collision for modified components to update the physical materials
		for (ULandscapeComponent* Component : ModifiedComponents)
		{
			ULandscapeHeightfieldCollisionComponent* CollisionComponent = Component->CollisionComponent.Get();
			if (CollisionComponent)
			{
				CollisionComponent->RecreateCollision();

				// We need to trigger navigation mesh build, in case user have painted holes on a landscape
				if (LandscapeInfo->GetLayerInfoIndex(ALandscapeProxy::VisibilityLayer) != INDEX_NONE)
				{
					UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(Component);
					if (NavSys)
					{
						NavSys->UpdateComponentInNavOctree(*CollisionComponent);
					}
				}
			}
		}
	}

	void GetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, TArray<uint8>>& Data)
	{
		// Do not Support for interpolation....
		check(false && TEXT("Do not support interpolation for FullWeightmapAccessor for now"));
	}

	void GetDataFast(int32 X1, int32 Y1, int32 X2, int32 Y2, TMap<FIntPoint, TArray<uint8>>& Data)
	{
		DirtyLayerInfos.Empty();
		LandscapeEdit.GetWeightDataFast(NULL, X1, Y1, X2, Y2, Data);
	}

	void SetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const uint8* Data, ELandscapeLayerPaintingRestriction PaintingRestriction)
	{
		TSet<ULandscapeComponent*> Components;
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2, &Components))
		{
			// Flush dynamic foliage (grass)
			ALandscapeProxy::InvalidateGeneratedComponentData(Components);

			LandscapeEdit.SetAlphaData(DirtyLayerInfos, X1, Y1, X2, Y2, Data, 0, PaintingRestriction);
			ModifiedComponents.Append(Components);
		}
		DirtyLayerInfos.Empty();
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

	TSet<ULandscapeLayerInfoObject*> DirtyLayerInfos;

private:
	ULandscapeInfo* LandscapeInfo;
	FLandscapeEditDataInterface LandscapeEdit;
	TSet<ULandscapeComponent*> ModifiedComponents;
};

struct FLandscapeFullWeightCache : public TLandscapeEditCache<FFullWeightmapAccessor<false>, TArray<uint8>>
{
	FLandscapeFullWeightCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FFullWeightmapAccessor<false>, TArray<uint8>>(InTarget)
	{
	}

	// Only for all weight case... the accessor type should be TArray<uint8>
	void GetCachedData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<uint8>& OutData, int32 ArraySize)
	{
		if (ArraySize == 0)
		{
			OutData.Empty();
			return;
		}

		const int32 XSize = (1 + X2 - X1);
		const int32 YSize = (1 + Y2 - Y1);
		const int32 Stride = XSize * ArraySize;
		int32 NumSamples = XSize * YSize * ArraySize;
		OutData.Empty(NumSamples);
		OutData.AddUninitialized(NumSamples);

		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			const int32 YOffset = (Y - Y1) * Stride;
			for (int32 X = X1; X <= X2; X++)
			{
				const int32 XYOffset = YOffset + (X - X1) * ArraySize;
				TArray<uint8>* Ptr = GetValueRef(X, Y);
				if (Ptr)
				{
					for (int32 Z = 0; Z < ArraySize; Z++)
					{
						OutData[XYOffset + Z] = (*Ptr)[Z];
					}
				}
				else
				{
					FMemory::Memzero((void*)&OutData[XYOffset], (SIZE_T)ArraySize);
				}
			}
		}
	}

	// Only for all weight case... the accessor type should be TArray<uint8>
	void SetCachedData(int32 X1, int32 Y1, int32 X2, int32 Y2, TArray<uint8>& Data, int32 ArraySize, ELandscapeLayerPaintingRestriction PaintingRestriction)
	{
		// Update cache
		for (int32 Y = Y1; Y <= Y2; Y++)
		{
			for (int32 X = X1; X <= X2; X++)
			{
				TArray<uint8> Value;
				Value.Empty(ArraySize);
				Value.AddUninitialized(ArraySize);
				for (int32 Z = 0; Z < ArraySize; Z++)
				{
					Value[Z] = Data[((X - X1) + (Y - Y1)*(1 + X2 - X1)) * ArraySize + Z];
				}
				SetValue(X, Y, MoveTemp(Value));
			}
		}

		// Update real data
		DataAccess.SetData(X1, Y1, X2, Y2, Data.GetData(), PaintingRestriction);
	}

	void AddDirtyLayer(ULandscapeLayerInfoObject* LayerInfo)
	{
		DataAccess.DirtyLayerInfos.Add(LayerInfo);
	}
};

// 
// FDatamapAccessor
//
template<bool bInUseInterp>
struct FDatamapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FDatamapAccessor(ULandscapeInfo* InLandscapeInfo)
		: LandscapeEdit(InLandscapeInfo)
	{
	}

	FDatamapAccessor(const FLandscapeToolTarget& InTarget)
		: FDatamapAccessor(InTarget.LandscapeInfo.Get())
	{
	}

	void GetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, uint8>& Data)
	{
		LandscapeEdit.GetSelectData(X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TMap<FIntPoint, uint8>& Data)
	{
		LandscapeEdit.GetSelectData(X1, Y1, X2, Y2, Data);
	}

	void SetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const uint8* Data, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None)
	{
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2))
		{
			LandscapeEdit.SetSelectData(X1, Y1, X2, Y2, Data, 0);
		}
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

private:
	FLandscapeEditDataInterface LandscapeEdit;
};

struct FLandscapeDataCache : public TLandscapeEditCache<FDatamapAccessor<false>, uint8>
{
	static uint8 ClampValue(int32 Value) { return FMath::Clamp(Value, 0, 255); }

	FLandscapeDataCache(const FLandscapeToolTarget& InTarget)
		: TLandscapeEditCache<FDatamapAccessor<false>, uint8>(InTarget)
	{
	}
};


//
// Tool targets
//
struct FHeightmapToolTarget
{
	typedef FLandscapeHeightCache CacheClass;
	static const ELandscapeToolTargetType::Type TargetType = ELandscapeToolTargetType::Heightmap;

	static float StrengthMultiplier(ULandscapeInfo* LandscapeInfo, float BrushRadius)
	{
		if (LandscapeInfo)
		{
			// Adjust strength based on brush size and drawscale, so strength 1 = one hemisphere
			return BrushRadius * LANDSCAPE_INV_ZSCALE / (LandscapeInfo->DrawScale.Z);
		}
		return 5.0f * LANDSCAPE_INV_ZSCALE;
	}

	static FMatrix ToWorldMatrix(ULandscapeInfo* LandscapeInfo)
	{
		FMatrix Result = FTranslationMatrix(FVector(0, 0, -32768.0f));
		Result *= FScaleMatrix(FVector(1.0f, 1.0f, LANDSCAPE_ZSCALE) * LandscapeInfo->DrawScale);
		return Result;
	}

	static FMatrix FromWorldMatrix(ULandscapeInfo* LandscapeInfo)
	{
		FMatrix Result = FScaleMatrix(FVector(1.0f, 1.0f, LANDSCAPE_INV_ZSCALE) / (LandscapeInfo->DrawScale));
		Result *= FTranslationMatrix(FVector(0, 0, 32768.0f));
		return Result;
	}
};


struct FWeightmapToolTarget
{
	typedef FLandscapeAlphaCache CacheClass;
	static const ELandscapeToolTargetType::Type TargetType = ELandscapeToolTargetType::Weightmap;

	static float StrengthMultiplier(ULandscapeInfo* LandscapeInfo, float BrushRadius)
	{
		return 255.0f;
	}

	static FMatrix ToWorldMatrix(ULandscapeInfo* LandscapeInfo) { return FMatrix::Identity; }
	static FMatrix FromWorldMatrix(ULandscapeInfo* LandscapeInfo) { return FMatrix::Identity; }
};

/**
 * FLandscapeToolStrokeBase - base class for tool strokes (used by FLandscapeToolBase)
 */

class FLandscapeToolStrokeBase : protected FGCObject
{
public:
	// Whether to call Apply() every frame even if the mouse hasn't moved
	enum { UseContinuousApply = false };

	// This is also the expected signature of derived class constructor used by FLandscapeToolBase
	FLandscapeToolStrokeBase(FEdModeLandscape* InEdMode, FEditorViewportClient* InViewportClient, const FLandscapeToolTarget& InTarget)
		: EdMode(InEdMode)
		, Target(InTarget)
		, LandscapeInfo(InTarget.LandscapeInfo.Get())
	{
	}

	// Signature of Apply() method for derived strokes
	// void Apply(FEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolMousePosition>& MousePositions);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(LandscapeInfo);
	}

protected:
	FEdModeLandscape* EdMode;
	const FLandscapeToolTarget& Target;
	ULandscapeInfo* LandscapeInfo;
};


/**
 * FLandscapeToolBase - base class for painting tools
 *		ToolTarget - the target for the tool (weight or heightmap)
 *		StrokeClass - the class that implements the behavior for a mouse stroke applying the tool.
 */
template<class TStrokeClass>
class FLandscapeToolBase : public FLandscapeTool
{
public:
	FLandscapeToolBase(FEdModeLandscape* InEdMode)
		: LastInteractorPosition(FVector2D::ZeroVector)
		, TimeSinceLastInteractorMove(0.0f)
		, EdMode(InEdMode)
		, bCanToolBeActivated(true)
		, ToolStroke()
		, bExternalModifierPressed(false)
	{
	}

	virtual bool BeginTool(FEditorViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, const FVector& InHitLocation) override
	{
		if (!ensure(InteractorPositions.Num() == 0))
		{
			InteractorPositions.Empty(1);
		}

		if( !IsToolActive() )
		{
			ToolStroke.Emplace( EdMode, ViewportClient, InTarget );
			EdMode->CurrentBrush->BeginStroke( InHitLocation.X, InHitLocation.Y, this );
		}

		// Save the mouse position
		LastInteractorPosition = FVector2D(InHitLocation);
		InteractorPositions.Emplace(LastInteractorPosition, ViewportClient ? IsModifierPressed(ViewportClient) : false); // Copy tool sometimes activates without a specific viewport via ctrl+c hotkey
		TimeSinceLastInteractorMove = 0.0f;

		ToolStroke->Apply(ViewportClient, EdMode->CurrentBrush, EdMode->UISettings, InteractorPositions);

		InteractorPositions.Empty(1);
		return true;
	}

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override
	{
		if (IsToolActive())
		{
			if (InteractorPositions.Num() > 0)
			{
				ToolStroke->Apply(ViewportClient, EdMode->CurrentBrush, EdMode->UISettings, InteractorPositions);
				ViewportClient->Invalidate(false, false);
				InteractorPositions.Empty(1);
			}
			else if (TStrokeClass::UseContinuousApply && TimeSinceLastInteractorMove >= 0.25f)
			{
				InteractorPositions.Emplace(LastInteractorPosition, IsModifierPressed(ViewportClient));
				ToolStroke->Apply(ViewportClient, EdMode->CurrentBrush, EdMode->UISettings, InteractorPositions);
				ViewportClient->Invalidate(false, false);
				InteractorPositions.Empty(1);
			}
			TimeSinceLastInteractorMove += DeltaTime;

			// Prevent landscape from baking textures while tool stroke is active
			EdMode->CurrentToolTarget.LandscapeInfo->PostponeTextureBaking();
		}
	}

	virtual void EndTool(FEditorViewportClient* ViewportClient) override
	{
		if (IsToolActive() && InteractorPositions.Num())
		{
			ToolStroke->Apply(ViewportClient, EdMode->CurrentBrush, EdMode->UISettings, InteractorPositions);
			InteractorPositions.Empty(1);
		}

		ToolStroke.Reset();
		EdMode->CurrentBrush->EndStroke();
		EdMode->UpdateLayerUsageInformation(&EdMode->CurrentToolTarget.LayerInfo);
		bExternalModifierPressed = false;
	}

	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override
	{
		if (ViewportClient != nullptr && Viewport != nullptr)
		{
			FVector HitLocation;
			if (EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitLocation))
			{
				if (EdMode->CurrentBrush)
				{
					// Inform the brush of the current location, to update the cursor
					EdMode->CurrentBrush->MouseMove(HitLocation.X, HitLocation.Y);
				}

				if (IsToolActive())
				{
					// Save the interactor position
					if (InteractorPositions.Num() == 0 || LastInteractorPosition != FVector2D(HitLocation))
					{
						LastInteractorPosition = FVector2D(HitLocation);
						InteractorPositions.Emplace(LastInteractorPosition, IsModifierPressed(ViewportClient));
					}
					TimeSinceLastInteractorMove = 0.0f;
				}
			}
		}
		else
		{
			const FVector2D NewPosition(x, y);
			if (InteractorPositions.Num() == 0 || LastInteractorPosition != FVector2D(NewPosition))
			{
				LastInteractorPosition = FVector2D(NewPosition);
				InteractorPositions.Emplace(LastInteractorPosition, IsModifierPressed());
			}
			TimeSinceLastInteractorMove = 0.0f;
		}

		return true;
	}

	virtual bool IsToolActive() const override { return ToolStroke.IsSet();  }

	virtual void SetCanToolBeActivated(bool Value) { bCanToolBeActivated = Value; }
	virtual bool CanToolBeActivated() const {	return bCanToolBeActivated; }

	virtual void SetExternalModifierPressed(const bool bPressed) override
	{
		bExternalModifierPressed = bPressed;
	}

protected:
	TArray<FLandscapeToolInteractorPosition> InteractorPositions;
	FVector2D LastInteractorPosition;
	float TimeSinceLastInteractorMove;
	FEdModeLandscape* EdMode;
	bool bCanToolBeActivated;
	TOptional<TStrokeClass> ToolStroke;

	/** Whether a modifier was pressed in another system (VREditor). */
	bool bExternalModifierPressed;

	bool IsModifierPressed(const class FEditorViewportClient* ViewportClient = nullptr)
	{
		return bExternalModifierPressed || (ViewportClient != nullptr && IsShiftDown(ViewportClient->Viewport));
	}
};

struct FToolFlattenCustomData
{
	FToolFlattenCustomData()
		: ActiveEyeDropperMode(false)
		, EyeDropperModeHeight(0.0f)
	{}

	bool ActiveEyeDropperMode;
	float EyeDropperModeHeight;
};
