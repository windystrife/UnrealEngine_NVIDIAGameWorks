// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "RenderingThread.h"
#include "SceneManagement.h"

class FShadowMap2D;
class UInstancedStaticMeshComponent;
class ULightComponent;
class UMapBuildDataRegistry;
class UShadowMapTexture2D;

struct FSignedDistanceFieldShadowSample
{
	/** Normalized and encoded distance to the nearest shadow transition, in the range [0, 1], where .5 is at the transition. */
	float Distance;
	/** Normalized penumbra size, in the range [0,1]. */
	float PenumbraSize;

	/** True if this sample maps to a valid point on a surface. */
	bool IsMapped;
};

struct FQuantizedSignedDistanceFieldShadowSample
{
	enum { NumFilterableComponents = 2 };
	uint8 Distance;
	uint8 PenumbraSize;
	uint8 Coverage;

	float GetFilterableComponent(int32 Index) const
	{
		if (Index == 0)
		{
			return Distance / 255.0f;
		}
		else
		{
			checkSlow(Index == 1);
			return PenumbraSize / 255.0f;
		}
	}

	void SetFilterableComponent(float InComponent, int32 Index)
	{
		if (Index == 0)
		{
			Distance = (uint8)FMath::Clamp<int32>(FMath::TruncToInt(InComponent * 255.0f), 0, 255);
		}
		else
		{
			checkSlow(Index == 1);
			PenumbraSize = (uint8)FMath::Clamp<int32>(FMath::TruncToInt(InComponent * 255.0f), 0, 255);
		}
	}

	/** Equality operator */
	bool operator==(const FQuantizedSignedDistanceFieldShadowSample& RHS) const
	{
		return Distance == RHS.Distance &&
			PenumbraSize == RHS.PenumbraSize &&
			Coverage == RHS.Coverage;
	}

	FQuantizedSignedDistanceFieldShadowSample() {}
	FQuantizedSignedDistanceFieldShadowSample(const FSignedDistanceFieldShadowSample& InSample)
	{
		Distance = (uint8)FMath::Clamp<int32>(FMath::TruncToInt(InSample.Distance * 255.0f), 0, 255);
		PenumbraSize = (uint8)FMath::Clamp<int32>(FMath::TruncToInt(InSample.PenumbraSize * 255.0f), 0, 255);
		Coverage = InSample.IsMapped ? 255 : 0;
	}
};

/**
 * The raw data which is used to construct a 2D shadowmap.
 */
class FShadowMapData2D
{
public:
	virtual ~FShadowMapData2D() {}

	// Accessors.
	uint32 GetSizeX() const { return SizeX; }
	uint32 GetSizeY() const { return SizeY; }

	// USurface interface
	virtual float GetSurfaceWidth() const { return SizeX; }
	virtual float GetSurfaceHeight() const { return SizeY; }

	enum ShadowMapDataType {
		UNKNOWN,
		SHADOW_FACTOR_DATA,
		SHADOW_FACTOR_DATA_QUANTIZED,
		SHADOW_SIGNED_DISTANCE_FIELD_DATA,
		SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED,
	};
	virtual ShadowMapDataType GetType() const { return UNKNOWN; }

	virtual void Quantize(TArray<FQuantizedSignedDistanceFieldShadowSample>& OutData) const {}

	virtual void Serialize(FArchive* OutShadowMap) {}

protected:

	FShadowMapData2D(uint32 InSizeX, uint32 InSizeY) :
		SizeX(InSizeX),
		SizeY(InSizeY)
	{
	}

	/** The width of the shadow-map. */
	uint32 SizeX;

	/** The height of the shadow-map. */
	uint32 SizeY;
};

/**
 * A 2D signed distance field map, which consists of FSignedDistanceFieldShadowSample's.
 */
class FShadowSignedDistanceFieldData2D : public FShadowMapData2D
{
public:

	FShadowSignedDistanceFieldData2D(uint32 InSizeX, uint32 InSizeY)
		: FShadowMapData2D(InSizeX, InSizeY)
	{
		Data.Empty(SizeX * SizeY);
		Data.AddZeroed(SizeX * SizeY);
	}

	const FSignedDistanceFieldShadowSample& operator()(uint32 X, uint32 Y) const { return Data[SizeX * Y + X]; }
	FSignedDistanceFieldShadowSample& operator()(uint32 X, uint32 Y) { return Data[SizeX * Y + X]; }

	virtual ShadowMapDataType GetType() const override { return SHADOW_SIGNED_DISTANCE_FIELD_DATA; }
	virtual void Quantize(TArray<FQuantizedSignedDistanceFieldShadowSample>& OutData) const override
	{
		OutData.Empty(Data.Num());
		OutData.AddUninitialized(Data.Num());
		for (int32 Index = 0; Index < Data.Num(); Index++)
		{
			OutData[Index] = Data[Index];
		}
	}

	virtual void Serialize(FArchive* OutShadowMap) override
	{
		for (int32 Index = 0; Index < Data.Num(); Index++)
		{
			OutShadowMap->Serialize(&Data[Index], sizeof(FSignedDistanceFieldShadowSample));
		}
	}

private:
	TArray<FSignedDistanceFieldShadowSample> Data;
};

/**
 * A 2D signed distance field map, which consists of FQuantizedSignedDistanceFieldShadowSample's.
 */
class FQuantizedShadowSignedDistanceFieldData2D : public FShadowMapData2D
{
public:

	FQuantizedShadowSignedDistanceFieldData2D(uint32 InSizeX, uint32 InSizeY)
		: FShadowMapData2D(InSizeX, InSizeY)
	{
		Data.Empty(SizeX * SizeY);
		Data.AddZeroed(SizeX * SizeY);
	}

	FQuantizedSignedDistanceFieldShadowSample* GetData() { return Data.GetData(); }

	const FQuantizedSignedDistanceFieldShadowSample& operator()(uint32 X, uint32 Y) const { return Data[SizeX * Y + X]; }
	FQuantizedSignedDistanceFieldShadowSample& operator()(uint32 X, uint32 Y) { return Data[SizeX * Y + X]; }

	virtual ShadowMapDataType GetType() const override { return SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED; }
	virtual void Quantize(TArray<FQuantizedSignedDistanceFieldShadowSample>& OutData) const override
	{
		OutData = Data;
	}

	virtual void Serialize(FArchive* OutShadowMap) override
	{
		for (int32 Index = 0; Index < Data.Num(); Index++)
		{
			OutShadowMap->Serialize(&Data[Index], sizeof(FQuantizedSignedDistanceFieldShadowSample));
		}
	}

private:
	TArray<FQuantizedSignedDistanceFieldShadowSample> Data;
};

class FFourDistanceFieldSamples
{
public:
	FQuantizedSignedDistanceFieldShadowSample Samples[4];
};

/**
 * The abstract base class of 1D and 2D shadow-maps.
 */
class ENGINE_API FShadowMap : private FDeferredCleanupInterface
{
public:
	enum
	{
		SMT_None = 0,
		SMT_2D = 2,
	};

	/** The GUIDs of lights which this shadow-map stores. */
	TArray<FGuid> LightGuids;

	/** Default constructor. */
	FShadowMap() :
		NumRefs(0)
	{
	}

	FShadowMap(TArray<FGuid> InLightGuids)
		: LightGuids(MoveTemp(InLightGuids))
		, NumRefs(0)
	{
	}

	/** Destructor. */
	virtual ~FShadowMap() { check(NumRefs == 0); }

	/**
	 * Checks if a light is stored in this shadow-map.
	 * @param	LightGuid - The GUID of the light to check for.
	 * @return	True if the light is stored in the light-map.
	 */
	bool ContainsLight(const FGuid& LightGuid) const
	{
		return LightGuids.Find(LightGuid) != INDEX_NONE;
	}

	// FShadowMap interface.
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
	virtual void Serialize(FArchive& Ar);
	virtual FShadowMapInteraction GetInteraction() const = 0;

	// Runtime type casting.
	virtual FShadowMap2D* GetShadowMap2D() { return NULL; }
	virtual const FShadowMap2D* GetShadowMap2D() const { return NULL; }

	// Reference counting.
	void AddRef()
	{
		check(IsInGameThread() || IsInAsyncLoadingThread());
		NumRefs++;
	}
	void Release()
	{
		check(IsInGameThread() || IsInAsyncLoadingThread());
		checkSlow(NumRefs > 0);
		if (--NumRefs == 0)
		{
			Cleanup();
		}
	}

protected:

	/**
	 * Called when the light-map is no longer referenced.  Should release the lightmap's resources.
	 */
	virtual void Cleanup();

private:
	int32 NumRefs;

	// FDeferredCleanupInterface
	virtual void FinishCleanup();
};

class ENGINE_API FShadowMap2D : public FShadowMap
{
public:

	/**
	 * Executes all pending shadow-map encoding requests.
	 * @param	InWorld				World in which the textures exist
	 * @param	bLightingSuccessful	Whether the lighting build was successful or not.
	 */
	static void EncodeTextures(UWorld* InWorld, ULevel* LightingScenario, bool bLightingSuccessful, bool bMultithreadedEncode =false );

	/**
	 * Constructs mip maps for a single shadowmap texture.
	 */
	static int32 EncodeSingleTexture(ULevel* LightingScenario, struct FShadowMapPendingTexture& PendingTexture, UShadowMapTexture2D* Texture, TArray< TArray<FFourDistanceFieldSamples>>& MipData);

	static TRefCountPtr<FShadowMap2D> AllocateShadowMap(
		UObject* LightMapOuter,
		const TMap<ULightComponent*, FShadowMapData2D*>& ShadowMapData,
		const FBoxSphereBounds& Bounds,
		ELightMapPaddingType InPaddingType,
		EShadowMapFlags InShadowmapFlags);

	/**
	 * Allocates texture space for the shadow-map and stores the shadow-map's raw data for deferred encoding.
	 * If the shadow-map has no shadows in it, it will return NULL.
	 * @param	Component - The component that owns this shadowmap
	 * @param	InstancedShadowMapData - The raw shadow-map data to fill the texture with.
	 * @param	Bounds - The bounds of the primitive the shadow-map will be rendered on.  Used as a hint to pack shadow-maps on nearby primitives in the same texture.
	 * @param	InPaddingType - the method for padding the shadowmap.
	 * @param	ShadowmapFlags - flags that determine how the shadowmap is stored (e.g. streamed or not)
	 */
	static TRefCountPtr<FShadowMap2D> AllocateInstancedShadowMap(UObject* LightMapOuter, UInstancedStaticMeshComponent* Component, TArray<TMap<ULightComponent*, TUniquePtr<FShadowMapData2D>>>&& InstancedShadowMapData,
		UMapBuildDataRegistry* Registry, FGuid MapBuildDataId, const FBoxSphereBounds& Bounds, ELightMapPaddingType PaddingType, EShadowMapFlags ShadowmapFlags);

	FShadowMap2D();

	FShadowMap2D(const TMap<ULightComponent*, FShadowMapData2D*>& ShadowMapData);
	FShadowMap2D(TArray<FGuid> LightGuids);

	// Accessors.
	UShadowMapTexture2D* GetTexture() const { check(IsValid()); return Texture; }
	const FVector2D& GetCoordinateScale() const { check(IsValid()); return CoordinateScale; }
	const FVector2D& GetCoordinateBias() const { check(IsValid()); return CoordinateBias; }
	bool IsValid() const { return Texture != NULL; }
	bool IsShadowFactorTexture() const { return false; }

	// FShadowMap interface.
	virtual void AddReferencedObjects(FReferenceCollector& Collector);
	virtual void Serialize(FArchive& Ar);
	virtual FShadowMapInteraction GetInteraction() const;

	// Runtime type casting.
	virtual FShadowMap2D* GetShadowMap2D() { return this; }
	virtual const FShadowMap2D* GetShadowMap2D() const { return this; }

#if WITH_EDITOR
	/** Call to enable/disable status update of LightMap encoding */
	static void SetStatusUpdate(bool bInEnable)
	{
		bUpdateStatus = bInEnable;
	}

	static bool GetStatusUpdate()
	{
		return bUpdateStatus;
	}
#endif

protected:

	/** The texture which contains the shadow-map data. */
	UShadowMapTexture2D* Texture;

	/** The scale which is applied to the shadow-map coordinates before sampling the shadow-map textures. */
	FVector2D CoordinateScale;

	/** The bias which is applied to the shadow-map coordinates before sampling the shadow-map textures. */
	FVector2D CoordinateBias;

	/** Tracks which of the 4 channels has valid texture data. */
	bool bChannelValid[4];

	/** Stores the inverse of the penumbra size, normalized.  Stores 1 to interpret the shadowmap as a shadow factor directly, instead of as a distance field. */
	FVector4 InvUniformPenumbraSize;

#if WITH_EDITOR
	/** If true, update the status when encoding light maps */
	static bool bUpdateStatus;
#endif
};



/**
 * Shadowmap reference serializer
 * Intended to be used by TRefCountPtr's serializer, not called directly
 */
extern ENGINE_API FArchive& operator<<(FArchive& Ar, FShadowMap*& R);
