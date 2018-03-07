// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "LandscapeToolInterface.h"
#include "LandscapeEdMode.h"
#include "LandscapeEditorObject.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdModeTools.h"

namespace
{
	FORCEINLINE FVector4 GetWorldPos(const FMatrix& LocalToWorld, FVector2D LocalXY, uint16 Height, FVector2D XYOffset)
	{
		return LocalToWorld.TransformPosition(FVector(LocalXY.X + XYOffset.X, LocalXY.Y + XYOffset.Y, ((float)Height - 32768.0f) * LANDSCAPE_ZSCALE));
	}

	FORCEINLINE FVector4 GetWorldPos(const FMatrix& LocalToWorld, FVector2D LocalXY, FVector XYOffsetVector)
	{
		return LocalToWorld.TransformPosition(FVector(LocalXY.X + XYOffsetVector.X, LocalXY.Y + XYOffsetVector.Y, XYOffsetVector.Z));
	}

	const int32 XOffsets[4] = { 0, 1, 0, 1 };
	const int32 YOffsets[4] = { 0, 0, 1, 1 };

	float GetHeight(int32 X, int32 Y, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY, const FVector& XYOffset, const TArray<FVector>& XYOffsetVectorData)
	{
		float Height[4];
		for (int32 Idx = 0; Idx < 4; ++Idx)
		{
			int32 XX = FMath::Clamp(FMath::FloorToInt(X + XYOffset.X + XOffsets[Idx]), MinX, MaxX);
			int32 YY = FMath::Clamp(FMath::FloorToInt(Y + XYOffset.Y + YOffsets[Idx]), MinY, MaxY);
			Height[Idx] = XYOffsetVectorData[XX - MinX + (YY - MinY) * (MaxX - MinX + 1)].Z;
		}
		float FracX = FMath::Fractional(X + XYOffset.X);
		float FracY = FMath::Fractional(Y + XYOffset.Y);
		return FMath::Lerp(FMath::Lerp(Height[0], Height[1], FracX),
			FMath::Lerp(Height[2], Height[3], FracX),
			FracY);
	}
};


//
// FLandscapeToolRetopologize
//

class FLandscapeToolStrokeRetopologize : public FLandscapeToolStrokeBase
{
public:
	FLandscapeToolStrokeRetopologize(FEdModeLandscape* InEdMode, FEditorViewportClient* InViewportClient, const FLandscapeToolTarget& InTarget)
		: FLandscapeToolStrokeBase(InEdMode, InViewportClient, InTarget)
		, Cache(InTarget)
	{
	}

	void Apply(FEditorViewportClient* ViewportClient, FLandscapeBrush* Brush, const ULandscapeEditorObject* UISettings, const TArray<FLandscapeToolInteractorPosition>& InteractorPositions)
	{
		if (!LandscapeInfo)
		{
			return;
		}

		// Get list of verts to update
		// TODO - only retrieve bounds as we don't need the data
		// or use the brush data?
		FLandscapeBrushData BrushInfo = Brush->ApplyBrush(InteractorPositions);
		if (!BrushInfo)
		{
			return;
		}

		int32 X1, Y1, X2, Y2;
		BrushInfo.GetInclusiveBounds(X1, Y1, X2, Y2);

		//LandscapeInfo->Modify();
		//LandscapeInfo->LandscapeProxy->Modify();

		FVector DrawScale3D = LandscapeInfo->DrawScale;
		// Tablet pressure
		float Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.0f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		/*
			X1 -= 1;
			Y1 -= 1;
			X2 += 1;
			Y2 += 1;
		*/
		{
			int32 ValidX1, ValidX2, ValidY1, ValidY2;
			ValidX1 = ValidY1 = INT_MAX;
			ValidX2 = ValidY2 = INT_MIN;
			int32 ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
			int32 ComponentSizeQuads = LandscapeInfo->ComponentSizeQuads;
			ALandscape::CalcComponentIndicesOverlap(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

			for (int32 ComponentIndexY = ComponentIndexY1; ComponentIndexY <= ComponentIndexY2; ComponentIndexY++)
			{
				for (int32 ComponentIndexX = ComponentIndexX1; ComponentIndexX <= ComponentIndexX2; ComponentIndexX++)
				{
					ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(FIntPoint(ComponentIndexX, ComponentIndexY));

					if (Component)
					{
						// Update valid region
						ValidX1 = FMath::Min<int32>(Component->GetSectionBase().X, ValidX1);
						ValidX2 = FMath::Max<int32>(Component->GetSectionBase().X + ComponentSizeQuads, ValidX2);
						ValidY1 = FMath::Min<int32>(Component->GetSectionBase().Y, ValidY1);
						ValidY2 = FMath::Max<int32>(Component->GetSectionBase().Y + ComponentSizeQuads, ValidY2);
					}
				}
			}

			X1 = FMath::Max<int32>(X1, ValidX1);
			X2 = FMath::Min<int32>(X2, ValidX2);
			Y1 = FMath::Max<int32>(Y1, ValidY1);
			Y2 = FMath::Min<int32>(Y2, ValidY2);
		}

		if (X1 > X2 || Y1 > Y2) // No valid region...
		{
			return;
		}

		const float AreaResolution = LANDSCAPE_XYOFFSET_SCALE; //1.0f/256.0f;

		Cache.CacheData(X1, Y1, X2, Y2);

		TArray<FVector> XYOffsetVectorData;
		Cache.GetCachedData(X1, Y1, X2, Y2, XYOffsetVectorData);
		TArray<FVector> NewXYOffset;
		NewXYOffset = XYOffsetVectorData;

		// Retopologize algorithm...
		{
			// Calculate surface world space area without missing area...
			float TotalArea = 0.0f;
			int32 QuadNum = 0;
			const int32 MaxIterNum = 300;

			TArray<int32> QuadX, QuadY, MinX, MaxX, MinY, MaxY;
			QuadX.AddZeroed(X2 - X1);
			QuadY.AddZeroed(Y2 - Y1);

			MinX.Empty(Y2 - Y1 + 1);
			MaxX.Empty(Y2 - Y1 + 1);
			MinY.Empty(X2 - X1 + 1);
			MaxY.Empty(X2 - X1 + 1);
			for (int32 X = X1; X <= X2; ++X)
			{
				MinY.Add(INT_MAX);
				MaxY.Add(INT_MIN);
			}
			for (int32 Y = Y1; Y <= Y2; ++Y)
			{
				MinX.Add(INT_MAX);
				MaxX.Add(INT_MIN);
			}

			// Calculate Average...
			TArray<ULandscapeComponent*> ComponentArray; // Ptr to component
			ComponentArray.AddZeroed((X2 - X1 + 1)*(Y2 - Y1 + 1));
			int32 ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
			int32 ComponentSizeQuads = LandscapeInfo->ComponentSizeQuads;
			ALandscape::CalcComponentIndicesOverlap(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

			for (int32 ComponentIndexY = ComponentIndexY1; ComponentIndexY <= ComponentIndexY2; ComponentIndexY++)
			{
				for (int32 ComponentIndexX = ComponentIndexX1; ComponentIndexX <= ComponentIndexX2; ComponentIndexX++)
				{
					ULandscapeComponent* Comp = LandscapeInfo->XYtoComponentMap.FindRef(FIntPoint(ComponentIndexX, ComponentIndexY));

					if (Comp)
					{
						const FMatrix LocalToWorld = Comp->GetRenderMatrix();

						// Find coordinates of box that lies inside component
						int32 ComponentX1 = FMath::Clamp<int32>(X1 - ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
						int32 ComponentY1 = FMath::Clamp<int32>(Y1 - ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
						int32 ComponentX2 = FMath::Clamp<int32>(X2 - ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
						int32 ComponentY2 = FMath::Clamp<int32>(Y2 - ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

						// World space area calculation
						for (int32 Y = ComponentY1; Y <= ComponentY2; ++Y)
						{
							for (int32 X = ComponentX1; X <= ComponentX2; ++X)
							{
								if (X < ComponentX2 && Y < ComponentY2)
								{
									// Need to read XY Offset value from XYOffsetTexture before this
									FVector P[4];
									for (int32 Idx = 0; Idx < 4; ++Idx)
									{
										int32 XX = X + XOffsets[Idx];
										int32 YY = Y + YOffsets[Idx];
										P[Idx] = FVector(GetWorldPos(LocalToWorld, FVector2D(XX, YY),
											XYOffsetVectorData[(ComponentIndexX*ComponentSizeQuads + XX - X1) + (ComponentIndexY*ComponentSizeQuads + YY - Y1)*(X2 - X1 + 1)]
											));
									}

									TotalArea += (((P[3] - P[0]) ^ (P[1] - P[0])).Size() + ((P[3] - P[0]) ^ (P[2] - P[0])).Size()) * 0.5f;
									QuadNum++;
									QuadX[ComponentIndexX*ComponentSizeQuads + X - X1]++;
									QuadY[ComponentIndexY*ComponentSizeQuads + Y - Y1]++;

									// Mark valid quad position
									ComponentArray[(ComponentIndexX*ComponentSizeQuads) + X - X1 + (ComponentIndexY*ComponentSizeQuads + Y - Y1)*(X2 - X1 + 1)] = Comp;
								}

								MinX[ComponentIndexY*ComponentSizeQuads + Y - Y1] = FMath::Min<int32>(MinX[ComponentIndexY*ComponentSizeQuads + Y - Y1], ComponentIndexX*ComponentSizeQuads + X);
								MaxX[ComponentIndexY*ComponentSizeQuads + Y - Y1] = FMath::Max<int32>(MaxX[ComponentIndexY*ComponentSizeQuads + Y - Y1], ComponentIndexX*ComponentSizeQuads + X);
								MinY[ComponentIndexX*ComponentSizeQuads + X - X1] = FMath::Min<int32>(MinY[ComponentIndexX*ComponentSizeQuads + X - X1], ComponentIndexY*ComponentSizeQuads + Y);
								MaxY[ComponentIndexX*ComponentSizeQuads + X - X1] = FMath::Max<int32>(MaxY[ComponentIndexX*ComponentSizeQuads + X - X1], ComponentIndexY*ComponentSizeQuads + Y);
							}
						}
					}
				}
			}

			const float HeightErrorThreshold = DrawScale3D.X * 0.5f;
			// Made XYOffset and new Z value allocation...
			float AreaErrorThreshold = FMath::Square(AreaResolution);
			float RemainArea = TotalArea;
			int32 RemainQuads = QuadNum;

			for (int32 Y = Y1; Y < Y2 - 1; ++Y)
			{
				// Like rasterization style
				// Search for Y offset
				if (MinX[Y - Y1] > MaxX[Y - Y1])
				{
					continue;
				}

				float AverageArea = RemainArea / RemainQuads;
				float AreaBaseError = AverageArea * 0.5f;
				float TotalLineArea = 0.0f;
				float TargetLineArea = AverageArea * QuadY[Y - Y1];
				float YOffset = Y + 1, PreYOffset = Y + 1; // Need to be bigger than previous Y
				float StepSize = FPlatformMath::Sqrt(2) * 0.25f;
				float LineAreaDiff = FLT_MAX; //Abs(TotalLineArea - TargetLineArea);
				int32 IterNum = 0;
				float TotalHeightError = 0.0f;

				while (NewXYOffset[(Y - Y1) * (X2 - X1 + 1)].Y + Y > XYOffsetVectorData[(FPlatformMath::FloorToInt(YOffset) - Y1) * (X2 - X1 + 1)].Y + FPlatformMath::FloorToInt(YOffset))
				{
					YOffset = YOffset + 1.0f;
					if (YOffset >= Y2)
					{
						YOffset = Y2;
						break;
					}
				}
				PreYOffset = YOffset;

				while (FMath::Abs(TotalLineArea - TargetLineArea) > AreaErrorThreshold)
				{
					IterNum++;
					TotalLineArea = 0.0f;
					TotalHeightError = 0.0f;
					//for (int32 X = X1; X < X2; ++X)
					for (int32 X = MinX[Y - Y1]; X < MaxX[Y - Y1]; ++X)
					{
						ULandscapeComponent* Comp = ComponentArray[X - X1 + (Y - Y1)*(X2 - X1 + 1)];
						if (Comp != NULL) // valid
						{
							const FMatrix LocalToWorld = Comp->GetRenderMatrix();
							FVector P[4];
							for (int32 Idx = 0; Idx < 2; ++Idx)
							{
								int32 XX = FMath::Clamp<int32>(X + XOffsets[Idx], X1, X2);
								P[Idx] = FVector(GetWorldPos(LocalToWorld, FVector2D(XX - Comp->GetSectionBase().X, Y - Comp->GetSectionBase().Y), NewXYOffset[XX - X1 + (Y - Y1) * (X2 - X1 + 1)]));
							}

							int32 YY0 = FMath::Clamp<int32>(FMath::FloorToInt(YOffset - 1), Y1, Y2);
							int32 YY1 = FMath::Clamp<int32>(FMath::FloorToInt(YOffset), Y1, Y2);
							int32 YY2 = FMath::Clamp<int32>(FMath::FloorToInt(1 + YOffset), Y1, Y2);
							// Search for valid YOffset...
							for (int32 Idx = 2; Idx < 4; ++Idx)
							{
								int32 XX = FMath::Clamp<int32>(X + XOffsets[Idx], X1, X2);
								FVector P1(GetWorldPos(LocalToWorld, FVector2D(XX - Comp->GetSectionBase().X, YY1 - Comp->GetSectionBase().Y), XYOffsetVectorData[XX - X1 + (YY1 - Y1) * (X2 - X1 + 1)]));
								FVector P2(GetWorldPos(LocalToWorld, FVector2D(XX - Comp->GetSectionBase().X, YY2 - Comp->GetSectionBase().Y), XYOffsetVectorData[XX - X1 + (YY2 - Y1) * (X2 - X1 + 1)]));
								P[Idx] = FMath::Lerp(P1, P2, FMath::Fractional(YOffset));
								if (Idx == 2)
								{
									FVector P0(GetWorldPos(LocalToWorld, FVector2D(XX - Comp->GetSectionBase().X, YY0 - Comp->GetSectionBase().Y), XYOffsetVectorData[XX - X1 + (YY0 - Y1) * (X2 - X1 + 1)]));
									TotalHeightError += FMath::Abs(((P[2] - P0) ^ (P2 - P[2])).Size() - ((P1 - P0) ^ (P2 - P1)).Size());
								}
							}

							//TotalHeightError += Abs( XYOffsetVectorData( X - X1 + (YY2 - Y1) * (X2-X1+1) ).Z - XYOffsetVectorData( X - X1 + (YY1 - Y1) * (X2-X1+1) ).Z * appFractional(YOffset) );  
							TotalLineArea += (((P[3] - P[0]) ^ (P[1] - P[0])).Size() + ((P[3] - P[0]) ^ (P[2] - P[0])).Size()) * 0.5f;
						}
					}

					if (TotalLineArea < AreaErrorThreshold || IterNum > MaxIterNum)
					{
						break;
					}

					if (MaxX[Y - Y1] - MinX[Y - Y1] > 0)
					{
						TotalHeightError /= (MaxX[Y - Y1] - MinX[Y - Y1]);
					}

					float NewLineAreaDiff = FMath::Abs(TotalLineArea - TargetLineArea);
					if (NewLineAreaDiff > LineAreaDiff || TotalHeightError > HeightErrorThreshold)
					{
						// backtracking
						YOffset = PreYOffset;
						StepSize *= 0.5f;
					}
					else
					{
						PreYOffset = YOffset;
						LineAreaDiff = FMath::Abs(TotalLineArea - TargetLineArea);
						if (TotalLineArea - TargetLineArea > 0)
						{
							YOffset -= StepSize;
						}
						else
						{
							YOffset += StepSize;
						}
						// clamp
						if (YOffset < Y1)
						{
							YOffset = Y1;
							break;
						}
						if (YOffset >= Y2)
						{
							YOffset = Y2;
							break;
						}
					}

					if (StepSize < AreaResolution)
					{
						break;
					}
				}

				// Set Y Offset
				if (TotalLineArea >= AreaErrorThreshold)
				{
					RemainArea -= TotalLineArea;
					RemainQuads -= QuadY[Y - Y1];
					for (int32 X = MinX[Y - Y1]; X < MaxX[Y - Y1]; ++X)
					{
						int32 YY1 = FMath::Clamp<int32>(FMath::FloorToInt(YOffset), Y1, Y2);
						int32 YY2 = FMath::Clamp<int32>(FMath::FloorToInt(1 + YOffset), Y1, Y2);
						FVector P1 = XYOffsetVectorData[X - X1 + (YY1 - Y1) * (X2 - X1 + 1)];
						//P1.X = X+P1.X;
						P1.Y = YY1 + P1.Y;
						FVector P2 = XYOffsetVectorData[X - X1 + (YY2 - Y1) * (X2 - X1 + 1)];
						//P2.X = X+P2.X;
						P2.Y = YY2 + P2.Y;
						FVector& XYOffset = NewXYOffset[X - X1 + (Y + 1 - Y1) * (X2 - X1 + 1)];
						XYOffset = FMath::Lerp(P1, P2, FMath::Fractional(YOffset));
						//XYOffset.X -= X;
						XYOffset.Y -= Y + 1;
					}
				}
			}

			// X ...
			TArray<FVector> NewYOffsets = NewXYOffset;
			RemainArea = TotalArea;
			RemainQuads = QuadNum;

			for (int32 X = X1; X < X2 - 1; ++X)
			{
				// Like rasterization style
				// Search for X offset
				if (MinY[X - X1] > MaxY[X - X1])
				{
					continue;
				}

				float AverageArea = RemainArea / RemainQuads;
				float AreaBaseError = AverageArea * 0.5f;
				float TotalLineArea = 0.0f;
				float TargetLineArea = AverageArea * QuadX[X - X1];
				float XOffset = X + 1, PreXOffset = X + 1; // Need to be bigger than previous Y
				float StepSize = FMath::Sqrt(2) * 0.25f;
				float LineAreaDiff = FLT_MAX; // Abs(TotalLineArea - TargetLineArea);
				int32 IterNum = 0;
				float TotalHeightError = 0.0f;

				while (NewXYOffset[X - X1].X + X > NewYOffsets[FMath::FloorToInt(XOffset) - X1].X + FMath::FloorToFloat(XOffset))
				{
					XOffset = XOffset + 1.0f;
					if (XOffset >= X2)
					{
						XOffset = X2;
						break;
					}
				}
				PreXOffset = XOffset;

				while (FMath::Abs(TotalLineArea - TargetLineArea) > AreaErrorThreshold)
				{
					TotalLineArea = 0.0f;
					IterNum++;
					for (int32 Y = MinY[X - X1]; Y < MaxY[X - X1]; ++Y)
					{
						ULandscapeComponent* Comp = ComponentArray[X - X1 + (Y - Y1)*(X2 - X1 + 1)];
						if (Comp != NULL) // valid
						{
							const FMatrix LocalToWorld = Comp->GetRenderMatrix();
							FVector P[4];
							for (int32 Idx = 0; Idx < 4; Idx += 2)
							{
								int32 YY = FMath::Clamp<int32>(Y + YOffsets[Idx], Y1, Y2);
								P[Idx] = FVector(GetWorldPos(LocalToWorld, FVector2D(X - Comp->GetSectionBase().X, YY - Comp->GetSectionBase().Y), NewXYOffset[X - X1 + (YY - Y1) * (X2 - X1 + 1)]));
							}

							int32 XX0 = FMath::Clamp<int32>(FMath::FloorToInt(XOffset - 1), X1, X2);
							int32 XX1 = FMath::Clamp<int32>(FMath::FloorToInt(XOffset), X1, X2);
							int32 XX2 = FMath::Clamp<int32>(FMath::FloorToInt(1 + XOffset), X1, X2);

							// Search for valid YOffset...
							for (int32 Idx = 1; Idx < 4; Idx += 2)
							{
								int32 YY = FMath::Clamp<int32>(Y + YOffsets[Idx], Y1, Y2);
								FVector P1(GetWorldPos(LocalToWorld, FVector2D(XX1 - Comp->GetSectionBase().X, YY - Comp->GetSectionBase().Y), NewYOffsets[XX1 - X1 + (YY - Y1) * (X2 - X1 + 1)]));
								FVector P2(GetWorldPos(LocalToWorld, FVector2D(XX2 - Comp->GetSectionBase().X, YY - Comp->GetSectionBase().Y), NewYOffsets[XX2 - X1 + (YY - Y1) * (X2 - X1 + 1)]));
								P[Idx] = FMath::Lerp(P1, P2, FMath::Fractional(XOffset));
								if (Idx == 1)
								{
									FVector P0(GetWorldPos(LocalToWorld, FVector2D(XX0 - Comp->GetSectionBase().X, YY - Comp->GetSectionBase().Y), NewYOffsets[XX0 - X1 + (YY - Y1) * (X2 - X1 + 1)]));
									TotalHeightError += FMath::Abs(((P[1] - P0) ^ (P2 - P[1])).Size() - ((P1 - P0) ^ (P2 - P1)).Size());
								}
							}

							//TotalHeightError += Abs( NewYOffsets( XX1 - X1 + (Y - Y1) * (X2-X1+1) ).Z - NewYOffsets( XX2 - X1 + (Y - Y1) * (X2-X1+1) ).Z * appFractional(XOffset) );  
							TotalLineArea += (((P[3] - P[0]) ^ (P[1] - P[0])).Size() + ((P[3] - P[0]) ^ (P[2] - P[0])).Size()) * 0.5f;
						}
					}

					if (TotalLineArea < AreaErrorThreshold || IterNum > MaxIterNum)
					{
						break;
					}

					if (MaxY[X - X1] - MinY[X - X1] > 0)
					{
						TotalHeightError /= (MaxY[X - X1] - MinY[X - X1]);
					}

					float NewLineAreaDiff = FMath::Abs(TotalLineArea - TargetLineArea);
					if (NewLineAreaDiff > LineAreaDiff || TotalHeightError > HeightErrorThreshold)
					{
						// backtracking
						XOffset = PreXOffset;
						StepSize *= 0.5f;
					}
					else
					{
						PreXOffset = XOffset;
						LineAreaDiff = FMath::Abs(TotalLineArea - TargetLineArea);
						if (TotalLineArea - TargetLineArea > 0)
						{
							XOffset -= StepSize;
						}
						else
						{
							XOffset += StepSize;
						}
						// clamp
						if (XOffset <= X1)
						{
							XOffset = X1;
							break;
						}
						else if (XOffset >= X2)
						{
							XOffset = X2;
							break;
						}
					}

					if (StepSize < AreaResolution)
					{
						break;
					}
				}

				// Set X Offset
				if (TotalLineArea >= AreaErrorThreshold)
				{
					RemainArea -= TotalLineArea;
					RemainQuads -= QuadX[X - X1];

					for (int32 Y = MinY[X - X1]; Y < MaxY[X - X1]; ++Y)
					{
						int32 XX1 = FMath::Clamp<int32>(FMath::FloorToInt(XOffset), X1, X2);
						int32 XX2 = FMath::Clamp<int32>(FMath::FloorToInt(1 + XOffset), X1, X2);
						FVector P1 = NewYOffsets[XX1 - X1 + (Y - Y1) * (X2 - X1 + 1)];
						P1.X = XX1 + P1.X;
						FVector P2 = NewYOffsets[XX2 - X1 + (Y - Y1) * (X2 - X1 + 1)];
						P2.X = XX2 + P2.X;
						FVector& XYOffset = NewXYOffset[X + 1 - X1 + (Y - Y1) * (X2 - X1 + 1)];
						XYOffset = FMath::Lerp(P1, P2, FMath::Fractional(XOffset));
						XYOffset.X -= X + 1;
					}
				}
			}

		}


		// Same as Gizmo fall off...
		float W = X2 - X1 + 1;
		float H = Y2 - Y1 + 1;
		float FalloffRadius = W * 0.5f * UISettings->BrushFalloff;
		float SquareRadius = W * 0.5f - FalloffRadius;
		for (int32 Y = 0; Y <= Y2 - Y1; ++Y)
		{
			for (int32 X = 0; X <= X2 - X1; ++X)
			{
				int32 Index = X + Y * (X2 - X1 + 1);
				FVector2D TransformedLocal(FMath::Abs(X - W * 0.5f), FMath::Abs(Y - H * 0.5f) * (W / H));
				float Cos = FMath::Abs(TransformedLocal.X) / TransformedLocal.Size();
				float Sin = FMath::Abs(TransformedLocal.Y) / TransformedLocal.Size();
				float RatioX = FalloffRadius > 0.0f ? 1.0f - FMath::Clamp((FMath::Abs(TransformedLocal.X) - Cos*SquareRadius) / FalloffRadius, 0.0f, 1.0f) : 1.0f;
				float RatioY = FalloffRadius > 0.0f ? 1.0f - FMath::Clamp((FMath::Abs(TransformedLocal.Y) - Sin*SquareRadius) / FalloffRadius, 0.0f, 1.0f) : 1.0f;
				float Ratio = TransformedLocal.SizeSquared() > FMath::Square(SquareRadius) ? RatioX * RatioY : 1.0f; //TransformedLocal.X / LW * TransformedLocal.Y / LW;
				float PaintAmount = Ratio*Ratio*(3 - 2 * Ratio);

				XYOffsetVectorData[Index] = FMath::Lerp(XYOffsetVectorData[Index], NewXYOffset[Index], PaintAmount);
				//XYOffsetVectorData(Index) = NewXYOffset(Index);
			}
		}

		// Apply to XYOffset Texture map and Height map
		Cache.SetCachedData(X1, Y1, X2, Y2, XYOffsetVectorData);
		Cache.Flush();
	}

protected:
	FLandscapeXYOffsetCache<false> Cache;
};

class FLandscapeToolRetopologize : public FLandscapeToolBase<FLandscapeToolStrokeRetopologize>
{
public:
	FLandscapeToolRetopologize(FEdModeLandscape* InEdMode)
		: FLandscapeToolBase<FLandscapeToolStrokeRetopologize>(InEdMode)
	{
	}

	virtual const TCHAR* GetToolName() override { return TEXT("Retopologize"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Retopologize", "Retopologize"); }

	virtual ELandscapeToolTargetTypeMask::Type GetSupportedTargetTypes() override
	{
		// technically not entirely accurate, also modifies the XYOffset map
		return ELandscapeToolTargetTypeMask::Heightmap;
	}
};

void FEdModeLandscape::InitializeTool_Retopologize()
{
	auto Tool_Retopologize = MakeUnique<FLandscapeToolRetopologize>(this);
	Tool_Retopologize->ValidBrushes.Add("BrushSet_Circle");
	Tool_Retopologize->ValidBrushes.Add("BrushSet_Alpha");
	Tool_Retopologize->ValidBrushes.Add("BrushSet_Pattern");
	LandscapeTools.Add(MoveTemp(Tool_Retopologize));
}
