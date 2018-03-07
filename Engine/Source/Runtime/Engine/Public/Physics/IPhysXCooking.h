// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"

// The result of a cooking operation
enum class EPhysXCookingResult : uint8
{
	// Cooking failed
	Failed,

	// Cooking succeeded with no issues
	Succeeded,

	// Cooking the exact source data failed, but succeeded after retrying with inflation enabled
	SucceededWithInflation,
};

enum class EPhysXMeshCookFlags : uint8
{
	Default = 0x0,

	// Don't perform mesh cleaning, so resulting mesh has same vertex order as input mesh
	DeformableMesh = 0x1,

	// Prioritize cooking speed over runtime speed
	FastCook = 0x2,

	// Do not create remap table for this mesh
	SuppressFaceRemapTable = 0x4
};
ENUM_CLASS_FLAGS(EPhysXMeshCookFlags);

namespace physx
{
	class PxCooking;
	class PxConvexMesh;
	class PxTriangleMesh;
	class PxHeightField;
}

/**
 * IPhysXCooking, PhysX cooking and serialization abstraction
**/
class IPhysXCooking
{
public:

	/**
	 * Checks whether parallel PhysX cooking is allowed.
	 *
	 * Note: This method is not currently used yet.
	 *
	 * @return true if this PhysX format can cook in parallel, false otherwise.
	 */
	virtual bool AllowParallelBuild( ) const
	{
		return false;
	}

	/**
	 * Cooks the source convex data for the platform and stores the cooked data internally.
	 *
	 * @param Format The desired format
	 * @param CookFlags Flags used to provide options for this cook
	 * @param SrcBuffer The source buffer
	 * @param OutBuffer The resulting cooked data
	 * @return An enum indicating full success, partial success, or failure (@see EPhysXCookingResult)
	 */
	virtual EPhysXCookingResult CookConvex( FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcBuffer, TArray<uint8>& OutBuffer ) const = 0;

	/**
	* Cooks the source convex data for the platform and returns the PhysX geometry directly (meant for runtime when you just need the geometry directly without serializing out)
	*
	* @param Format The desired format
	* @param CookFlags Flags used to provide options for this cook
	* @param SrcBuffer The source buffer
	* @param OutBuffer The resulting cooked data
	* @return The cooked convex mesh geometry
	*/
	virtual EPhysXCookingResult CreateConvex(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcBuffer, physx::PxConvexMesh*& OutBuffer) const = 0;

	/**
	 * Cooks the source Tri-Mesh data for the platform and stores the cooked data internally.
	 *
	 * @param Format The desired format.
	 * @param CookFlags Flags used to provide options for this cook
	 * @param SrcBuffer The source buffer.
	 * @param OutBuffer The resulting cooked data.
	 * @return true on success, false otherwise.
	 */
	virtual bool CookTriMesh( FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcVertices, const TArray<struct FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, TArray<uint8>& OutBuffer) const = 0;
	
	/**
	* Cooks the source Tri-Mesh data for the platform and returns the PhysX geometry directly (meant for runtime when you just need the geometry directly without serializing out)
	*
	* @param Format The desired format.
	* @param CookFlags Flags used to provide options for this cook
	* @param SrcBuffer The source buffer.
	* @param OutBuffer The resulting cooked data.
	* @return true on success, false otherwise.
	*/
	virtual bool CreateTriMesh(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcVertices, const TArray<struct FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, physx::PxTriangleMesh*& OutTriangleMesh) const = 0;
	/**
	 * Cooks the source height field data for the platform and stores the cooked data internally.
	 *
	 * @param Format The desired format
	 * @param HFSize Size of height field [NumColumns, NumRows]
	 * @param SrcBuffer The source buffer
	 * @param OutBuffer The resulting cooked data
	 * @return true on success, false otherwise.
	 */
	virtual bool CookHeightField( FName Format, FIntPoint HFSize, const void* Samples, uint32 SamplesStride, TArray<uint8>& OutBuffer ) const = 0;

	/**
	* Cooks the source height field data for the platform and returns the PhysX geometry directly (meant for runtime when you just need the geometry directly without serializing out)
	*
	* @param Format The desired format
	* @param HFSize Size of height field [NumColumns, NumRows]
	* @param SrcBuffer The source buffer
	* @param OutBuffer The resulting cooked data
	* @return true on success, false otherwise.
	*/
	virtual bool CreateHeightField(FName Format, FIntPoint HFSize, const void* Samples, uint32 SamplesStride, physx::PxHeightField*& OutHeightField) const = 0;

	/**
	 * Serializes the BodyInstance
	 *
	 * @param Format The desired format
	 * @param Bodies The bodies containing the needed physx actors to serialize
	 * @param BodySetups The various body setups used by Bodies (could be just 1). This is needed for keeping geometry out of the serialized data
	 * @param PhysicalMaterials The physical materials used by Bodies (could be just 1). This is needed for keeping physical materials out of the serialized data
	 * @param OutBuffer The resulting cooked data
	 * @return true on success, false otherwise.
	 */
	virtual bool SerializeActors( FName Format, const TArray<struct FBodyInstance*>& Bodies, const TArray<class UBodySetup*>& BodySetups, const TArray<class UPhysicalMaterial*>& PhysicalMaterials, TArray<uint8>& OutBuffer ) const = 0;

	/**
	 * Gets the list of supported formats.
	 *
	 * @param OutFormats Will hold the list of formats.
	 */
	virtual void GetSupportedFormats( TArray<FName>& OutFormats ) const = 0;

	/**
	 * Gets the current version of the specified PhysX format.
	 *
	 * @param Format The format to get the version for.
	 * @return Version number.
	 */
	virtual uint16 GetVersion( FName Format ) const = 0;

	/** Get the actual physx cooker object */
	virtual physx::PxCooking* GetCooking() const = 0;


public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IPhysXCooking( ) { }
};
