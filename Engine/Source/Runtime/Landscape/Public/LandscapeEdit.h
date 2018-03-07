// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LandscapeEdit.h: Classes for the editor to access to Landscape data
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "LandscapeProxy.h"
#include "Engine/Texture2D.h"

#if WITH_EDITOR
	#include "Containers/ArrayView.h"
#endif

class ULandscapeComponent;
class ULandscapeInfo;
class ULandscapeLayerInfoObject;

#define MAX_LANDSCAPE_LOD_DISTANCE_FACTOR 10.f

#if WITH_EDITOR

struct FLandscapeTextureDataInfo
{
	struct FMipInfo
	{
		void* MipData;
		TArray<FUpdateTextureRegion2D> MipUpdateRegions;
	};

	FLandscapeTextureDataInfo(UTexture2D* InTexture);
	virtual ~FLandscapeTextureDataInfo();

	// returns true if we need to block on the render thread before unlocking the mip data
	bool UpdateTextureData();

	int32 NumMips() { return MipInfo.Num(); }

	void AddMipUpdateRegion(int32 MipNum, int32 InX1, int32 InY1, int32 InX2, int32 InY2)
	{
		check( MipNum < MipInfo.Num() );
		new(MipInfo[MipNum].MipUpdateRegions) FUpdateTextureRegion2D(InX1, InY1, InX1, InY1, 1+InX2-InX1, 1+InY2-InY1);
	}

	void* GetMipData(int32 MipNum)
	{
		check( MipNum < MipInfo.Num() );
		if( !MipInfo[MipNum].MipData )
		{
			MipInfo[MipNum].MipData = Texture->Source.LockMip(MipNum);
		}
		return MipInfo[MipNum].MipData;
	}

	int32 GetMipSizeX(int32 MipNum)
	{
		return FMath::Max(Texture->Source.GetSizeX() >> MipNum, 1);
	}

	int32 GetMipSizeY(int32 MipNum)
	{
		return FMath::Max(Texture->Source.GetSizeY() >> MipNum, 1);
	}

private:
	UTexture2D* Texture;
	TArray<FMipInfo> MipInfo;
};

struct LANDSCAPE_API FLandscapeTextureDataInterface
{
	// tor
	virtual ~FLandscapeTextureDataInterface();

	// Texture data access
	FLandscapeTextureDataInfo* GetTextureDataInfo(UTexture2D* Texture);

	// Flush texture updates
	void Flush();

	// Texture bulk operations for weightmap reallocation
	void CopyTextureChannel(UTexture2D* Dest, int32 DestChannel, UTexture2D* Src, int32 SrcChannel);
	void ZeroTextureChannel(UTexture2D* Dest, int32 DestChannel);
	void CopyTextureFromHeightmap(UTexture2D* Dest, int32 DestChannel, ULandscapeComponent* Comp, int32 SrcChannel);
	void CopyTextureFromWeightmap(UTexture2D* Dest, int32 DestChannel, ULandscapeComponent* Comp, ULandscapeLayerInfoObject* LayerInfo);

	template<typename TData>
	void SetTextureValueTempl(UTexture2D* Dest, TData Value);
	void ZeroTexture(UTexture2D* Dest);
	void SetTextureValue(UTexture2D* Dest, FColor Value);

	template<typename TData>
	bool EqualTextureValueTempl(UTexture2D* Src, TData Value);
	bool EqualTextureValue(UTexture2D* Src, FColor Value);

private:
	TMap<UTexture2D*, FLandscapeTextureDataInfo*> TextureDataMap;
};


struct LANDSCAPE_API FLandscapeEditDataInterface : public FLandscapeTextureDataInterface
{
	// tor
	FLandscapeEditDataInterface(ULandscapeInfo* InLandscape);

	// Misc
	bool GetComponentsInRegion(int32 X1, int32 Y1, int32 X2, int32 Y2, TSet<ULandscapeComponent*>* OutComponents = NULL);

	//
	// Heightmap access
	//
	void SetHeightData(int32 X1, int32 Y1, int32 X2, int32 Y2, const uint16* Data, int32 Stride, bool CalcNormals, const uint16* NormalData = NULL, bool CreateComponents=false);

	// Helper accessor
	FORCEINLINE uint16 GetHeightMapData(const ULandscapeComponent* Component, int32 TexU, int32 TexV, FColor* TextureData = NULL);
	// Generic
	template<typename TStoreData>
	void GetHeightDataTempl(int32& X1, int32& Y1, int32& X2, int32& Y2, TStoreData& StoreData);
	// Without data interpolation, able to get normal data
	template<typename TStoreData>
	void GetHeightDataTemplFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TStoreData& StoreData, TStoreData* NormalData = NULL);
	// Implementation for fixed array
	void GetHeightData(int32& X1, int32& Y1, int32& X2, int32& Y2, uint16* Data, int32 Stride);
	void GetHeightDataFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, uint16* Data, int32 Stride, uint16* NormalData = NULL);
	// Implementation for sparse array
	void GetHeightData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, uint16>& SparseData);
	void GetHeightDataFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TMap<FIntPoint, uint16>& SparseData, TMap<FIntPoint, uint16>* NormalData = NULL);

	// Recaclulate normals for the entire landscape.
	void RecalculateNormals();

	//
	// Weightmap access
	//
	// Helper accessor
	FORCEINLINE uint8 GetWeightMapData(const ULandscapeComponent* Component, ULandscapeLayerInfoObject* LayerInfo, int32 TexU, int32 TexV, uint8 Offset = 0, UTexture2D* Texture = NULL, uint8* TextureData = NULL);
	template<typename TStoreData>
	void GetWeightDataTempl(ULandscapeLayerInfoObject* LayerInfo, int32& X1, int32& Y1, int32& X2, int32& Y2, TStoreData& StoreData);
	// Without data interpolation
	template<typename TStoreData>
	void GetWeightDataTemplFast(ULandscapeLayerInfoObject* LayerInfo, const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TStoreData& StoreData);
	// Implementation for fixed array
	void GetWeightData(ULandscapeLayerInfoObject* LayerInfo, int32& X1, int32& Y1, int32& X2, int32& Y2, uint8* Data, int32 Stride);
	//void GetWeightData(FName LayerName, int32& X1, int32& Y1, int32& X2, int32& Y2, TArray<uint8>* Data, int32 Stride);
	void GetWeightDataFast(ULandscapeLayerInfoObject* LayerInfo, const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, uint8* Data, int32 Stride);
	void GetWeightDataFast(ULandscapeLayerInfoObject* LayerInfo, const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TArray<uint8>* Data, int32 Stride);
	// Implementation for sparse array
	void GetWeightData(ULandscapeLayerInfoObject* LayerInfo, int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, uint8>& SparseData);
	//void GetWeightData(FName LayerName, int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<uint64, TArray<uint8>>& SparseData);
	void GetWeightDataFast(ULandscapeLayerInfoObject* LayerInfo, const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TMap<FIntPoint, uint8>& SparseData);
	void GetWeightDataFast(ULandscapeLayerInfoObject* LayerInfo, const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TMap<FIntPoint, TArray<uint8>>& SparseData);
	// Updates weightmap for LayerInfo, optionally adjusting all other weightmaps.
	void SetAlphaData(ULandscapeLayerInfoObject* LayerInfo, const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const uint8* Data, int32 Stride, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None, bool bWeightAdjust = true, bool bTotalWeightAdjust = false);
	// Updates weightmaps for all layers. Data points to packed data for all layers in the landscape info
	void SetAlphaData(const TSet<ULandscapeLayerInfoObject*>& DirtyLayerInfos, const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const uint8* Data, int32 Stride, ELandscapeLayerPaintingRestriction PaintingRestriction = ELandscapeLayerPaintingRestriction::None);
	// Delete a layer and re-normalize other layers
	void DeleteLayer(ULandscapeLayerInfoObject* LayerInfo);
	// Fill a layer and re-normalize other layers
	void FillLayer(ULandscapeLayerInfoObject* LayerInfo);
	// Fill all empty layers and re-normalize layers
	void FillEmptyLayers(ULandscapeLayerInfoObject* LayerInfo);
	// Replace/merge a layer
	void ReplaceLayer(ULandscapeLayerInfoObject* FromLayerInfo, ULandscapeLayerInfoObject* ToLayerInfo);

	// Without data interpolation, Select Data 
	template<typename TStoreData>
	void GetSelectDataTempl(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TStoreData& StoreData);
	void GetSelectData(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, uint8* Data, int32 Stride);
	void GetSelectData(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TMap<FIntPoint, uint8>& SparseData);
	void SetSelectData(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const uint8* Data, int32 Stride);

	//
	// XYOffsetmap access
	//
	template<typename T>
	void SetXYOffsetDataTempl(int32 X1, int32 Y1, int32 X2, int32 Y2, const T* Data, int32 Stride);
	void SetXYOffsetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const FVector2D* Data, int32 Stride);
	void SetXYOffsetData(int32 X1, int32 Y1, int32 X2, int32 Y2, const FVector* Data, int32 Stride);
	// Helper accessor
	FORCEINLINE FVector2D GetXYOffsetmapData(const ULandscapeComponent* Component, int32 TexU, int32 TexV, FColor* TextureData = NULL);

	template<typename TStoreData>
	void GetXYOffsetDataTempl(int32& X1, int32& Y1, int32& X2, int32& Y2, TStoreData& StoreData);
	void GetXYOffsetData(int32& X1, int32& Y1, int32& X2, int32& Y2, FVector2D* Data, int32 Stride);
	void GetXYOffsetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, FVector2D>& SparseData);
	void GetXYOffsetData(int32& X1, int32& Y1, int32& X2, int32& Y2, FVector* Data, int32 Stride);
	void GetXYOffsetData(int32& X1, int32& Y1, int32& X2, int32& Y2, TMap<FIntPoint, FVector>& SparseData);
	// Without data interpolation
	template<typename TStoreData>
	void GetXYOffsetDataTemplFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TStoreData& StoreData);
	// Without data interpolation, able to get normal data
	void GetXYOffsetDataFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, FVector2D* Data, int32 Stride);
	void GetXYOffsetDataFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TMap<FIntPoint, FVector2D>& SparseData);
	void GetXYOffsetDataFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, FVector* Data, int32 Stride);
	void GetXYOffsetDataFast(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, TMap<FIntPoint, FVector>& SparseData);

	template<typename T>
	static void ShrinkData(TArray<T>& Data, int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY, int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY);

private:
	int32 ComponentSizeQuads;
	int32 SubsectionSizeQuads;
	int32 ComponentNumSubsections;
	FVector DrawScale;

	ULandscapeInfo* LandscapeInfo;

	// Only for Missing Data interpolation... only internal usage
	template<typename TData, typename TStoreData, typename FType>
	FORCEINLINE void CalcMissingValues(const int32& X1, const int32& X2, const int32& Y1, const int32& Y2,
		const int32& ComponentIndexX1, const int32& ComponentIndexX2, const int32& ComponentIndexY1, const int32& ComponentIndexY2,
		const int32& ComponentSizeX, const int32& ComponentSizeY, TData* CornerValues,
		TArray<bool>& NoBorderY1, TArray<bool>& NoBorderY2, TArray<bool>& ComponentDataExist, TStoreData& StoreData);

	// test if layer is whitelisted for a given texel
	inline bool IsWhitelisted(const ULandscapeLayerInfoObject* LayerInfo,
	                          int32 ComponentIndexX, int32 SubIndexX, int32 SubX,
	                          int32 ComponentIndexY, int32 SubIndexY, int32 SubY);

	// counts the total influence of each weight-blended layer on this component
	inline TMap<const ULandscapeLayerInfoObject*, uint32> CountWeightBlendedLayerInfluence(int32 ComponentIndexX, int32 ComponentIndexY, TOptional<TArrayView<const uint8* const>> LayerDataPtrs);

	// chooses a replacement layer to use when erasing from 100% influence on a texel
	const ULandscapeLayerInfoObject* ChooseReplacementLayer(const ULandscapeLayerInfoObject* LayerInfo, int32 ComponentIndexX, int32 SubIndexX, int32 SubX, int32 ComponentIndexY, int32 SubIndexY, int32 SubY, TMap<FIntPoint, TMap<const ULandscapeLayerInfoObject*, uint32>>& LayerInfluenceCache, TArrayView<const uint8* const> LayerDataPtrs);
};

template<typename T>
void FLandscapeEditDataInterface::ShrinkData(TArray<T>& Data, int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY, int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY)
{
	checkSlow(OldMinX <= OldMaxX && OldMinY <= OldMaxY);
	checkSlow(NewMinX >= OldMinX && NewMaxX <= OldMaxX);
	checkSlow(NewMinY >= OldMinY && NewMaxY <= OldMaxY);

	if (NewMinX != OldMinX || NewMinY != OldMinY ||
		NewMaxX != OldMaxX || NewMaxY != OldMaxY)
	{
		// if only the MaxY changes we don't need to do the moving, only the truncate
		if (NewMinX != OldMinX || NewMinY != OldMinY || NewMaxX != OldMaxX)
		{
			for (int32 DestY = 0, SrcY = NewMinY - OldMinY; DestY <= NewMaxY - NewMinY; DestY++, SrcY++)
			{
//				UE_LOG(LogLandscape, Warning, TEXT("Dest: %d, %d = %d Src: %d, %d = %d Width = %d"), 0, DestY, DestY * (1 + NewMaxX - NewMinX), NewMinX - OldMinX, SrcY, SrcY * (1 + OldMaxX - OldMinX) + NewMinX - OldMinX, (1 + NewMaxX - NewMinX));
				T* DestData = &Data[DestY * (1 + NewMaxX - NewMinX)];
				const T* SrcData = &Data[SrcY * (1 + OldMaxX - OldMinX) + NewMinX - OldMinX];
				FMemory::Memmove(DestData, SrcData, (1 + NewMaxX - NewMinX) * sizeof(T));
			}
		}

		const int32 NewSize = (1 + NewMaxY - NewMinY) * (1 + NewMaxX - NewMinX);
		Data.RemoveAt(NewSize, Data.Num() - NewSize);
	}
}

#endif

