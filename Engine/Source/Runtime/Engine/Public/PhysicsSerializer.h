// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EngineDefines.h"
#include "Serialization/BulkData.h"

class UBodySetup;
class UPhysicalMaterial;
struct FBodyInstance;

/**
 * Helper class used to serialize a collection of BodyInstances. This allows the physics engine to serialize whatever expensive computations it needs offline.
 * A DDC entry will be created per instance and there is some overhead associated with serialization so it's wise to use this for a group of BodyInstances.
 */

struct FBodyInstance;

namespace physx
{
	class PxRigidActor;
	class PxCollection;
	class PxSerializationRegistry;
}

#include "PhysicsSerializer.generated.h"

UCLASS(DefaultToInstanced, hidecategories=Object, MinimalAPI)
class UPhysicsSerializer : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	 /** 
	  * Performs a binary serialization of the given bodies. This is a physics engine optimization so that various computations can be done offline.
	  * The serialization stores all physics actors and shapes, as well as references to physical materials and geometry
	  * @param Bodies The bodies containing the needed physics actors to serialize
	  * @param BodySetups The various body setups used by Bodies (could be 0). This is needed for keeping geometry out of the serialized data
	  * @param PhysicalMaterials The physical materials used by Bodies (could be 0). This is needed for keeping physical materials out of the serialized data
	  */
	void SerializePhysics(const TArray<FBodyInstance*>& Bodies, const TArray<UBodySetup*>& BodySetups, const TArray<UPhysicalMaterial*>& PhysicalMaterials);

	/** 
	 * Uses the serialized binary data to recreate physics engine specific data structures. This is an optimization so that FBodyInstance::InitBody doesn't have to do certain work at runtime.
	 * Assumes that the binary data has already been serialized either by SerializePhysics or from cooked data.
	 * @param BodySetups The various body setups used by Bodies (could be 0). This is needed for deserialization and pointer fixup
	 * @param PhysicalMaterials The physical materials used by Bodies (could be 0).This is needed for deserialization and pointer fixup
	 */
	void CreatePhysicsData(const TArray<UBodySetup*>& BodySetups, const TArray<UPhysicalMaterial*>& PhysicalMaterials);

	virtual void BeginDestroy();

#if WITH_PHYSX
	physx::PxRigidActor* GetRigidActor(uint64 ObjectId) const;

	static uint64 GetSerializationId();
#endif

	virtual void Serialize(FArchive& Ar) override;

private:
	/** Helper function to find all physical materials that Bodies depend on. During serialization these will be referenced*/
	void GatherNeededMaterials(const TArray<FBodyInstance*>& Bodies);

	/** Helper function to find all geometry that BodySetups depend on. During serialization these will be referenced*/
	void GatherNeededGeometry(const TArray<UBodySetup*>& BodySetups);

private:
	/** Cooked physics data for each format */
	FFormatContainer BinaryFormatData;

	/** Binary data of the physics actors.*/
	FByteBulkData* GetBinaryData(FName Format, const TArray<FBodyInstance*>& Bodies, const TArray<class UBodySetup*>& BodySetups, const TArray<class UPhysicalMaterial*>& PhysicalMaterials);

#if WITH_PHYSX
	TMap <uint64, physx::PxRigidActor*> ActorsMap;
#endif
};
