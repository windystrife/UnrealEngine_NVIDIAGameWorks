// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/TextureStreamingTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "SortedMap.h"
#include "MeshComponent.generated.h"

/**
 * MeshComponent is an abstract base for any component that is an instance of a renderable collection of triangles.
 *
 * @see UStaticMeshComponent
 * @see USkeletalMeshComponent
 */
UCLASS(abstract)
class ENGINE_API UMeshComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	/** Per-Component material overrides.  These must NOT be set directly or a race condition can occur between GC and the rendering thread. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Rendering, Meta=(ToolTip="Material overrides."))
	TArray<class UMaterialInterface*> OverrideMaterials;
	
	UFUNCTION(BlueprintCallable, Category="Components|Mesh")
	virtual TArray<class UMaterialInterface*> GetMaterials() const;

	UFUNCTION(BlueprintCallable, Category = "Components|Mesh")
	virtual int32 GetMaterialIndex(FName MaterialSlotName) const;

	UFUNCTION(BlueprintCallable, Category = "Components|Mesh")
	virtual TArray<FName> GetMaterialSlotNames() const;

	UFUNCTION(BlueprintCallable, Category = "Components|Mesh")
	virtual bool IsMaterialSlotNameValid(FName MaterialSlotName) const;

	/** Returns override Materials count */
	virtual int32 GetNumOverrideMaterials() const;

#if WITH_EDITOR
	/*
	 * Make sure the Override array is using only the space it should use.
	 * 1. The override array cannot be bigger then the number of mesh material.
	 * 2. The override array must not end with a nullptr UMaterialInterface.
	 */
	void CleanUpOverrideMaterials();
	/** 
	 * This empties all override materials and used by editor when replacing preview mesh 
	 */
	void EmptyOverrideMaterials();

	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	//~ Begin UPrimitiveComponent Interface
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	virtual void SetMaterialByName(FName MaterialSlotName, class UMaterialInterface* Material) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;	
	//~ End UPrimitiveComponent Interface

	/** Accesses the scene relevance information for the materials applied to the mesh. Valid from game thread only. */
	FMaterialRelevance GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const;

	/**
	 *	Tell the streaming system whether or not all mip levels of all textures used by this component should be loaded and remain loaded.
	 *	@param bForceMiplevelsToBeResident		Whether textures should be forced to be resident or not.
	 */
	virtual void SetTextureForceResidentFlag( bool bForceMiplevelsToBeResident );

	/**
	 *	Tell the streaming system to start loading all textures with all mip-levels.
	 *	@param Seconds							Number of seconds to force all mip-levels to be resident
	 *	@param bPrioritizeCharacterTextures		Whether character textures should be prioritized for a while by the streaming system
	 *	@param CinematicTextureGroups			Bitfield indicating which texture groups that use extra high-resolution mips
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	virtual void PrestreamTextures( float Seconds, bool bPrioritizeCharacterTextures, int32 CinematicTextureGroups = 0 );

	/** Get the material info for texture stremaing. Return whether the data is valid or not. */
	virtual bool GetMaterialStreamingData(int32 MaterialIndex, FPrimitiveMaterialInfo& MaterialData) const { return false; }

	/** Generate streaming data for all materials. */
	void GetStreamingTextureInfoInner(FStreamingTextureLevelContext& LevelContext, const TArray<FStreamingTextureBuildInfo>* PreBuiltData, float ComponentScaling, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * Output to the log which materials and textures are used by this component.
	 * @param Indent	Number of tabs to put before the log.
	 */
	virtual void LogMaterialsAndTextures(FOutputDevice& Ar, int32 Indent) const;
#endif

public:
	/** Material parameter setting and caching */

	/** Set all occurrences of Scalar Material Parameters with ParameterName in the set of materials of the SkeletalMesh to ParameterValue */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetScalarParameterValueOnMaterials(const FName ParameterName, const float ParameterValue);

	/** Set all occurrences of Vector Material Parameters with ParameterName in the set of materials of the SkeletalMesh to ParameterValue */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetVectorParameterValueOnMaterials(const FName ParameterName, const FVector ParameterValue);

	/**  
	 * Returns default value for the parameter input. 
	 *
	 * NOTE: This is not reliable when cooking, as initializing the default value 
	 *       requires a render resource that only exists if the owning world is rendering.
	 */
	float GetScalarParameterDefaultValue(const FName ParameterName)
	{
		FMaterialParameterCache* ParameterCache = MaterialParameterCache.Find(ParameterName);
		return (ParameterCache ? ParameterCache->ScalarParameterDefaultValue : 0.f);
	}
protected:
	/** Retrieves all the (scalar/vector-)parameters from within the used materials on the SkeletalMesh, and stores material index vs parameter names */
	void CacheMaterialParameterNameIndices();

	/** Mark cache parameters map as dirty, cache will be rebuild once SetScalar/SetVector functions are called */
	void MarkCachedMaterialParameterNameIndicesDirty();
	
	/** Struct containing information about a given parameter name */
	struct FMaterialParameterCache
	{
		/** Material indices for the retrieved scalar material parameter names */
		TArray<int32> ScalarParameterMaterialIndices;
		/** Material indices for the retrieved vector material parameter names */
		TArray<int32> VectorParameterMaterialIndices;
		/** Material default parameter for the scalar parameter
		 * We only cache the last one as we can't trace back from [name, index] 
		 * This data is used for animation system to set default back to it*/
		float ScalarParameterDefaultValue = 0.f;
	};

	TSortedMap<FName, FMaterialParameterCache> MaterialParameterCache;

	/** Flag whether or not the cached material parameter indices map is dirty (defaults to true, and is set from SetMaterial/Set(Skeletal)Mesh */
	uint32 bCachedMaterialParameterIndicesAreDirty : 1;
};
