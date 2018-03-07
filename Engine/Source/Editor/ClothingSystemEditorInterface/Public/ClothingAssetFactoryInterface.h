// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "ObjectMacros.h"
#include "Features/IModularFeature.h"

#include "ClothingAssetFactoryInterface.generated.h"

namespace nvidia
{
	namespace apex
	{
		class ClothingAsset;
	}
}

class USkeletalMesh;
class UClothingAssetBase;
struct FSkeletalMeshClothBuildParams;

// Clothing asset factories should inherit this interface/uobject to provide functionality
// to build clothing assets from .apx files imported to the engine
UCLASS(Abstract)
class CLOTHINGSYSTEMEDITORINTERFACE_API UClothingAssetFactoryBase : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Given a target mesh and parameters describing the build operation, create a clothing asset for
	 * use on the mesh
	 * @param TargetMesh - The mesh to target
	 * @param Params - Extra operation params (LOD/Section index etc...)
	 */
	virtual UClothingAssetBase* CreateFromSkeletalMesh(USkeletalMesh* TargetMesh, FSkeletalMeshClothBuildParams& Params)
	PURE_VIRTUAL(UClothingAssetFactoryBase::CreateFromSkeletalMesh, return nullptr;);

	/** 
	 * Given a target mesh and valid parameters, import a simulation mesh as a LOD for the clothing
	 * specified by the build parameters, returning the modified clothing object
	 * @param TargetMesh The owner mesh
	 * @param Params Build parameters for the operation (target clothing object, source data)
	 */
	virtual UClothingAssetBase* ImportLodToClothing(USkeletalMesh* TargetMesh, FSkeletalMeshClothBuildParams& Params)
	PURE_VIRTUAL(UClothingAssetFactoryBase::ImportLodToClothing, return nullptr;);

	/**
	 * Should return whether or not the factory can handle the incoming file (check validity etc.)
	 */
	virtual bool CanImport(const FString& Filename)
	PURE_VIRTUAL(UClothingAssetFactoryBase::CanImport, return false;);

	/**
	 * Given an APEX asset instantiated from the filename checked with CanImport, the factory
	 * is expected to build a valid clothing asset (or nullptr if it cannot).
	 */
	virtual UClothingAssetBase* CreateFromApexAsset(nvidia::apex::ClothingAsset* InApexAsset, USkeletalMesh* TargetMesh, FName InName = NAME_None)
	PURE_VIRTUAL(UClothingAssetFactoryBase::CreateFromApexAsset, return nullptr;);

	/**
	 * Import an asset from the specified file
	 * @param Filename - file to import
	 * @param TargetMesh - the mesh to import the clothing asset to
	 * @param InName - optional name for the clothing asset object (will take the filename otherwise)
	 */
	virtual UClothingAssetBase* Import(const FString& Filename, USkeletalMesh* TargetMesh, FName InName = NAME_None)
	PURE_VIRTUAL(UClothingAssetFactoryBase::Import, return nullptr;);

	/**
	 * Reimport an asset from the specified file
	 * @param Filename - file to import
	 * @param TargetMesh - the mesh to import the clothing asset to
	 * @param OriginalAsset - the original clothing asset to replace
	 */
	virtual UClothingAssetBase* Reimport(const FString& Filename, USkeletalMesh* TargetMesh, UClothingAssetBase* OriginalAsset)
	PURE_VIRTUAL(UClothingAssetFactoryBase::Reimport, return nullptr;);
};

// An interface for a class that will provide a clothing asset factory, this should be
// registered as a feature under its FeatureName to be picked up by the engine
class CLOTHINGSYSTEMEDITORINTERFACE_API IClothingAssetFactoryProvider : public IModularFeature
{
public:
	static const FName FeatureName;

	// Called by the engine to retrieve a valid factory from a provider
	// This can be the default object for the factory class or a full instance
	virtual UClothingAssetFactoryBase* GetFactory() = 0;
};
