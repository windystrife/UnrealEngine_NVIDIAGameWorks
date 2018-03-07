// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Physics/IPhysXCooking.h"
#include "Physics/IPhysXCookingModule.h"

namespace physx
{
	class PxCooking;
	class PxTriangleMesh;
	class PxConvexMesh;
	class PxHeightField;
	class PxFoundation;
	struct PxCookingParams;
}

/**
* FPhysXCooking. Cooks physics data.
**/
class PHYSXCOOKING_API FPhysXCooking : public IPhysXCooking
{	
public:

	FPhysXCooking();
	virtual physx::PxCooking* GetCooking() const override;
	virtual bool AllowParallelBuild() const override;
	virtual uint16 GetVersion(FName Format) const override;
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override;
	
	virtual EPhysXCookingResult CookConvex(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcBuffer, TArray<uint8>& OutBuffer) const override;
	EPhysXCookingResult CreateConvex(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcBuffer, physx::PxConvexMesh*& OutConvexMesh) const override;

	virtual bool CookTriMesh(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcVertices, const TArray<FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, TArray<uint8>& OutBuffer) const override;
	virtual bool CreateTriMesh(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcVertices, const TArray<FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, physx::PxTriangleMesh*& OutTriangleMesh) const override;

	virtual bool CookHeightField(FName Format, FIntPoint HFSize, const void* Samples, uint32 SamplesStride, TArray<uint8>& OutBuffer) const override;
	virtual bool CreateHeightField(FName Format, FIntPoint HFSize, const void* Samples, uint32 SamplesStride, physx::PxHeightField*& OutHeightField) const override;
	
	virtual bool SerializeActors(FName Format, const TArray<FBodyInstance*>& Bodies, const TArray<UBodySetup*>& BodySetups, const TArray<UPhysicalMaterial*>& PhysicalMaterials, TArray<uint8>& OutBuffer) const override;

private:

	template <bool bUseBuffer>
	EPhysXCookingResult CookConvexImp(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcBuffer, TArray<uint8>& OutBuffer, physx::PxConvexMesh*& OutConvexMesh) const;

	template <bool bUseBuffer>
	bool CookHeightFieldImp(FName Format, FIntPoint HFSize, const void* Samples, uint32 SamplesStride, TArray<uint8>& OutBuffer, physx::PxHeightField*& OutHeightField) const;

	template <bool bUseBuffer>
	bool CookTriMeshImp(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcVertices, const TArray<FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, TArray<uint8>& OutBuffer, physx::PxTriangleMesh*& OutTriangleMesh) const;

private:
	enum
	{
		/** Version for PhysX format, this becomes part of the DDC key. */
		UE_PHYSX_PC_VER = 0,
	};

private:
	physx::PxCooking* PhysXCooking;

};


/**
* Module for PhysX cooking
*/

class PHYSXCOOKING_API FPhysXPlatformModule : public IPhysXCookingModule
{
public:
	FPhysXPlatformModule();
	
	virtual ~FPhysXPlatformModule();
	
	virtual IPhysXCooking* GetPhysXCooking();
	
	virtual physx::PxCooking* CreatePhysXCooker(uint32 version, physx::PxFoundation& foundation, const physx::PxCookingParams& params);
	
	virtual void Terminate() override;
	
private:

	/**
	*	Load the required modules for PhysX
	*/
	void InitPhysXCooking();
	
	void ShutdownPhysXCooking();

	uint32 PhysXCookerTLS;
};