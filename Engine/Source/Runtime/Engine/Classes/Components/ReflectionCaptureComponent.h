// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Misc/CoreStats.h"
#include "Templates/RefCounting.h"
#include "Components/SceneComponent.h"
#include "RenderCommandFence.h"
#include "ReflectionCaptureComponent.generated.h"

class FReflectionCaptureProxy;
class UBillboardComponent;

/*
* Refcounted class to pass around uncompressed cubemap data and track memory, for use with FReflectionCaptureFullHDR::GetUncompressedData
*/
class FReflectionCaptureUncompressedData : public FRefCountedObject
{
public:
	FReflectionCaptureUncompressedData() :
		TrackedBufferSize(0)
	{}

	FReflectionCaptureUncompressedData( uint32 InSizeBytes ) :
		TrackedBufferSize(0)
	{
		CubemapDataArray.Empty(InSizeBytes);
		CubemapDataArray.AddUninitialized(InSizeBytes);
		UpdateMemoryTracking();
	}

	~FReflectionCaptureUncompressedData()
	{
		DEC_MEMORY_STAT_BY(STAT_ReflectionCaptureMemory, CubemapDataArray.Num() );
	}

	uint8* GetData( int Offset = 0 )
	{
		return &CubemapDataArray[Offset];
	}

	const uint32 Size() const 
	{ 
		return CubemapDataArray.Num();
	}

	TArray<uint8>& GetArray() 
	{
		return CubemapDataArray;
	}

	void UpdateMemoryTracking()
	{
		INC_MEMORY_STAT_BY( STAT_ReflectionCaptureMemory, CubemapDataArray.Num() - TrackedBufferSize);
		TrackedBufferSize = CubemapDataArray.Num();
	}
private:
	int32 TrackedBufferSize;
	TArray<uint8> CubemapDataArray;
};


class FReflectionCaptureFullHDR
{
public:
	/** 
	 * The compressed full HDR capture data, with all mips and faces laid out linearly. 
	 * This is read from the rendering thread directly after load, so the game thread must not write to it again.
	 * This is kept compressed because it must persist even after creating the texture for rendering,
	 * Because it is used with a texture array so must support multiple uploads.
	 */
	TArray<uint8> CompressedCapturedData;
	int32 CubemapSize;

	/**
	* Generated at cook time. Avoids decompression overhead in GetUncompressedData
	*/
	TRefCountPtr<FReflectionCaptureUncompressedData> StoredUncompressedData;

	/** Destructor. */
	~FReflectionCaptureFullHDR();

	/** Initializes the compressed data from an uncompressed source. */
	ENGINE_API void InitializeFromUncompressedData(const TArray<uint8>& UncompressedData, int32 CubmapSize);

	/** Decompresses the capture data. */
	ENGINE_API TRefCountPtr<FReflectionCaptureUncompressedData> GetUncompressedData() const;

	ENGINE_API TRefCountPtr<FReflectionCaptureUncompressedData> GetCapturedDataForSM4Load()
	{
		CapturedDataForSM4Load = GetUncompressedData();
		return CapturedDataForSM4Load;
	}

	ENGINE_API bool HasValidData() const { return StoredUncompressedData != nullptr || CompressedCapturedData.Num() > 0; }

private:
	/** 
	 * This is used to pass the uncompressed capture data to the RT on load for SM4. 
	 * The game thread must not modify it again after sending read commands to the RT.
	 */
	TRefCountPtr<FReflectionCaptureUncompressedData> CapturedDataForSM4Load;
};

struct FReflectionCaptureEncodedHDRDerivedData : FRefCountedObject
{
	FReflectionCaptureEncodedHDRDerivedData()
	{
		CapturedData = new FReflectionCaptureUncompressedData;
	}
	/** 
	 * The uncompressed encoded HDR capture data, with all mips and faces laid out linearly. 
	 * This is read and written from the rendering thread directly after load, so the game thread must not access it again.
	 */
	TRefCountPtr<FReflectionCaptureUncompressedData> CapturedData;

	/** Destructor. */
	virtual ~FReflectionCaptureEncodedHDRDerivedData();

	FORCEINLINE int32 CalculateCubemapDimension() const
	{
		// top mip size of the encoded data is given by the eq:
		// Data / (6 cubemaps * sizeof(FColor)) = (1/4 + 1/4^2 + 1/4^3 ...) - fractional pixel of the last mip
		// Data / (6 cubemaps * sizeof(FColor)) = (4*topMip - lastMip)/3
		// when lastMip = 1 simplifies to the following:
		// see https://en.wikipedia.org/wiki/1/4_%2B_1/16_%2B_1/64_%2B_1/256_%2B_%E2%8B%AF for maths
		return (int32)sqrt((float)(2 * sizeof(FColor) + CapturedData->Size()) / (float)(8 * sizeof(FColor)));
	}

	/** Generates encoded HDR data from full HDR data and saves it in the DDC, or loads an already generated version from the DDC. */
	static TRefCountPtr<FReflectionCaptureEncodedHDRDerivedData> GenerateEncodedHDRData(const FReflectionCaptureFullHDR& FullHDRData, const FGuid& StateId, float Brightness);

private:

	/** Constructs a key string for the DDC that uniquely identifies a FReflectionCaptureEncodedHDRDerivedData. */
	static FString GetDDCKeyString(const FGuid& StateId, int32 CubemapDimension);

	/** Encodes the full HDR data of FullHDRData. */
	void GenerateFromDerivedDataSource(const FReflectionCaptureFullHDR& FullHDRData, float Brightness);
};

UENUM()
enum class EReflectionSourceType : uint8
{
	/** Construct the reflection source from the captured scene*/
	CapturedScene,
	/** Construct the reflection source from the specified cubemap. */
	SpecifiedCubemap,
};

// -> will be exported to EngineDecalClasses.h
UCLASS(abstract, hidecategories=(Collision, Object, Physics, SceneComponent, Activation, "Components|Activation", Mobility), MinimalAPI)
class UReflectionCaptureComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UBillboardComponent* CaptureOffsetComponent;

	/** Indicates where to get the reflection source from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=ReflectionCapture)
	EReflectionSourceType ReflectionSourceType;

	/** Cubemap to use for reflection if ReflectionSourceType is set to RS_SpecifiedCubemap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=ReflectionCapture)
	class UTextureCube* Cubemap;

	/** Angle to rotate the source cubemap when SourceType is set to SLS_SpecifiedCubemap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=ReflectionCapture, meta = (UIMin = "0", UIMax = "360"))
	float SourceCubemapAngle;

	/** A brightness control to scale the captured scene's reflection intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=ReflectionCapture, meta=(UIMin = ".5", UIMax = "4"))
	float Brightness;
	
	/** World space offset to apply before capturing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReflectionCapture, AdvancedDisplay)
	FVector CaptureOffset;

	/** The rendering thread's mirror of this reflection capture. */
	FReflectionCaptureProxy* SceneProxy;

	/** Callback to create the rendering thread mirror. */
	ENGINE_API FReflectionCaptureProxy* CreateSceneProxy();

	/** Called to update the preview shapes when something they are dependent on has changed. */
	virtual void UpdatePreviewShape();

	/** Indicates that the capture needs to recapture the scene, adds it to the recapture queue. */
	ENGINE_API void SetCaptureIsDirty();

	/** 
	 * Reads reflection capture contents back from the scene and saves the results to the DDC.
	 * Note: this requires a valid scene and RHI and therefore can't be done while cooking.
	 */
	void ReadbackFromGPU(UWorld* WorldToUpdate);

	/** Marks this component has having been recaptured. */
	void SetCaptureCompleted() { bCaptureDirty = false; }

	/** Gets the radius that bounds the shape's influence, used for culling. */
	virtual float GetInfluenceBoundingRadius() const PURE_VIRTUAL(UReflectionCaptureComponent::GetInfluenceBoundingRadius,return 0;);

	/** Called each tick to recapture and queued reflection captures. */
	ENGINE_API static void UpdateReflectionCaptureContents(UWorld* WorldToUpdate);

	ENGINE_API const FReflectionCaptureFullHDR* GetFullHDRData() const
	{
		return FullHDRData;
	}

	inline float GetAverageBrightness() const 
	{ 
		return AverageBrightness; 
	}

	inline float* GetAverageBrightnessPtr() { return &AverageBrightness; }
	inline const float* GetAverageBrightnessPtr() const { return &AverageBrightness; }

	/** Issues a renderthread command to release the data, and NULLs the pointer on the gamethread */
	ENGINE_API void ReleaseHDRData();

	ENGINE_API static int32 GetReflectionCaptureSize_GameThread();
	ENGINE_API static int32 GetReflectionCaptureSize_RenderThread();

	//~ Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	//~ End UActorComponent Interface

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;	
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* Property) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	virtual void PreFeatureLevelChange(ERHIFeatureLevel::Type PendingFeatureLevel) override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;
	//~ End UObject Interface

private:

	/** Whether the reflection capture needs to re-capture the scene. */
	bool bCaptureDirty;

	/** Whether DerivedData is up to date. */
	bool bDerivedDataDirty;

	/** Whether or not this component was serialized from cooked data. */
	bool bLoadedCookedData;

	/** List of formats loaded from cooked data. Only used when duplicating this object after loading it from cooked data. */
	TArray<FName> LoadedFormats;

	UPROPERTY()
	FGuid StateId;

	/** Average brightness of the captured data, read back to the CPU after the capture. */
	float AverageBrightness;

	/**
	 * The full HDR capture data to use for rendering.
	 * This will be loaded from inlined data.
	 * Can be NULL, which indicates there is no up-to-date cached derived data
	 * The rendering thread reads directly from the contents of this object to avoid an extra data copy, so it must be deleted in a thread safe way.
	 */
	FReflectionCaptureFullHDR* FullHDRData;

	/** Only used in SM4, since cubemap texture arrays are not available. */
	class FReflectionTextureCubeResource* SM4FullHDRCubemapTexture;

	/**
	 * The encoded HDR capture data to use for rendering.
	 * If loading cooked, this will be loaded from inlined data.
	 * If loading uncooked, this will be generated from FullHDRData or loaded from the DDC.
	 * The rendering thread reads directly from the contents of this object to avoid an extra data copy, so it must be deleted in a thread safe way.
	 */
	TRefCountPtr<FReflectionCaptureEncodedHDRDerivedData> EncodedHDRDerivedData;

	/** Cubemap texture resource used for rendering with the encoded HDR values. */
	class FReflectionTextureCubeResource* EncodedHDRCubemapTexture;

	/** Fence used to track progress of releasing resources on the rendering thread. */
	FRenderCommandFence ReleaseResourcesFence;

	/** 
	 * List of reflection captures that need to be recaptured.
	 * These have to be queued because we can only render the scene to update captures at certain points, after the level has loaded.
	 * This queue should be in the UWorld or the FSceneInterface, but those are not available yet in PostLoad.
	 */
	static TArray<UReflectionCaptureComponent*> ReflectionCapturesToUpdate;

	/** 
	 * List of reflection captures that need to be recaptured because they were dirty on load.
	 * These have to be queued because we can only render the scene to update captures at certain points, after the level has loaded.
	 * This queue should be in the UWorld or the FSceneInterface, but those are not available yet in PostLoad.
	 */
	static TArray<UReflectionCaptureComponent*> ReflectionCapturesToUpdateForLoad;
	static FCriticalSection ReflectionCapturesToUpdateForLoadLock;

	void UpdateDerivedData(FReflectionCaptureFullHDR* NewDerivedData);
	void SerializeSourceData(FArchive& Ar);

	friend class FReflectionCaptureProxy;
};

