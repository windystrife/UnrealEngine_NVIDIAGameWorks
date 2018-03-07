// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "RenderResource.h"
#include "SubsurfaceProfile.generated.h"

// struct with all the settings we want in USubsurfaceProfile, separate to make it easer to pass this data around in the engine.
USTRUCT(BlueprintType)
struct FSubsurfaceProfileStruct
{
	GENERATED_USTRUCT_BODY()
	
	/** in world/unreal units (cm) */
	UPROPERTY(Category = "SubsurfaceProfileStruct", EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1", UIMax = "50.0", ClampMax = "1000.0"))
	float ScatterRadius;

	/**
	* Specifies the how much of the diffuse light gets into the material,
	* can be seen as a per-channel mix factor between the original image,
	* and the SSS-filtered image (called "strength" in SeparableSSS, default there: 0.48, 0.41, 0.28)
	*/
	UPROPERTY(Category = "SubsurfaceProfileStruct", EditAnywhere, BlueprintReadOnly, meta = (HideAlphaChannel))
	FLinearColor SubsurfaceColor;

	/**
	* defines the per-channel falloff of the gradients
	* produced by the subsurface scattering events, can be used to fine tune the color of the gradients
	* (called "falloff" in SeparableSSS, default there: 1, 0.37, 0.3)
	*/
	UPROPERTY(Category = "SubsurfaceProfileStruct", EditAnywhere, BlueprintReadOnly, meta = (HideAlphaChannel))
	FLinearColor FalloffColor;

	// constructor
	FSubsurfaceProfileStruct()
	{
		// defaults from SeparableSSS.h and the demo
		ScatterRadius = 1.2f;
		SubsurfaceColor = FLinearColor(0.48f, 0.41f, 0.28f);
		FalloffColor = FLinearColor(1.0f, 0.37f, 0.3f);
	}

	void Invalidate()
	{
		// nicer for debugging / VisualizeSSS
		ScatterRadius = 0.0f;
		SubsurfaceColor = FLinearColor(0, 0, 0);
		FalloffColor = FLinearColor(0, 0, 0);
	}
};

/**
 * Subsurface Scattering profile asset, can be specified at the material. Only for "Subsurface Profile" materials, is use during Screenspace Subsurface Scattering
 * Don't change at runtime. All properties in here are per material - texture like variations need to come from properties that are in the GBuffer.
 */
UCLASS(autoexpandcategories = SubsurfaceProfile, MinimalAPI)
class USubsurfaceProfile : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category = USubsurfaceProfile, EditAnywhere, meta = (ShowOnlyInnerProperties))
	struct FSubsurfaceProfileStruct Settings;

	//~ Begin UObject Interface
	virtual void BeginDestroy();
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
	//~ End UObject Interface
};

// render thread
class FSubsurfaceProfileTexture : public FRenderResource
{
public:
	// constructor
	FSubsurfaceProfileTexture();

	// destructor
	~FSubsurfaceProfileTexture();

	// convenience, can be optimized 
	// @param Profile must not be 0, game thread pointer, do not dereference, only for comparison
	int32 AddOrUpdateProfile(const FSubsurfaceProfileStruct Settings, const USubsurfaceProfile* Profile)
	{
		check(Profile);

		int32 AllocationId = FindAllocationId(Profile); 
		
		if (AllocationId != -1)
		{
			UpdateProfile(AllocationId, Settings);
		}
		else
		{
			AllocationId = AddProfile(Settings, Profile);
		}

		return AllocationId;
	}

	// O(n) n is a small number
	// @param InProfile must not be 0, game thread pointer, do not dereference, only for comparison
	// @return AllocationId -1: no allocation, should be deallocated with DeallocateSubsurfaceProfile()
	int32 AddProfile(const FSubsurfaceProfileStruct Settings, const USubsurfaceProfile* InProfile);

	// O(n) to find the element, n is the SSProfile count and usually quite small
	void RemoveProfile(const USubsurfaceProfile* InProfile);

	// @param InProfile must not be 0, game thread pointer, do not dereference, only for comparison
	void UpdateProfile(const FSubsurfaceProfileStruct Settings, const USubsurfaceProfile* Profile) { int32 AllocationId = FindAllocationId(Profile); UpdateProfile(AllocationId, Settings); }

	// @param InProfile must not be 0, game thread pointer, do not dereference, only for comparison
	void UpdateProfile(int32 AllocationId, const FSubsurfaceProfileStruct Settings);

	// @return can be 0 if there is no SubsurfaceProfile
	const struct IPooledRenderTarget* GetTexture(FRHICommandListImmediate& RHICmdList);


	//~ Begin FRenderResource Interface.
	/**
	* Release textures when device is lost/destroyed.
	*/
	virtual void ReleaseDynamicRHI() override;

	// for debugging, can be removed
	void Dump();

	// for debugging / VisualizeSSS
	ENGINE_API bool GetEntryString(uint32 Index, FString& Out) const;

	// @param InProfile must not be 0, game thread pointer, do not dereference, only for comparison
	// @return -1 if not found
	int32 FindAllocationId(const USubsurfaceProfile* InProfile) const;

private:

	struct FSubsurfaceProfileEntry
	{
		// @param InProfile game thread pointer, do not dereference, only for comparison, 0 if the entry can be reused or it's [0] which is used as default
		FSubsurfaceProfileEntry(FSubsurfaceProfileStruct InSubsurfaceProfileStruct, const USubsurfaceProfile* InProfile)
			: Settings(InSubsurfaceProfileStruct)
			, Profile(InProfile)
		{
		}

		FSubsurfaceProfileStruct Settings;

		// 0 if the entry can be reused or it's [0] which is used as default
		// game thread pointer, do not dereference, only for comparison
		const USubsurfaceProfile* Profile;
	};

	//
	TArray<FSubsurfaceProfileEntry> SubsurfaceProfileEntries;

	// Could be optimized but should not happen too often (during level load or editor operations).
	void CreateTexture(FRHICommandListImmediate& RHICmdList);

};

// If you change this you need to recompile the SSS shaders.
// Required if we use a texture format with limited size but want to express a larger radius
static const int32 SUBSURFACE_RADIUS_SCALE = 1024;

 // The kernels range from -3 to 3
static const int32 SUBSURFACE_KERNEL_SIZE = 3;

// lives on the render thread
extern ENGINE_API TGlobalResource<FSubsurfaceProfileTexture> GSubsurfaceProfileTextureObject;
