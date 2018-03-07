// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "EngineDefines.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Serialization/BulkData.h"
#include "PhysicsEngine/BodySetupEnums.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Async/TaskGraphInterfaces.h"
#include "BodySetup.generated.h"


class ITargetPlatform;
class UPhysicalMaterial;
class UPrimitiveComponent;
struct FShapeData;
enum class EPhysXMeshCookFlags : uint8;

DECLARE_DELEGATE(FOnAsyncPhysicsCookFinished);

namespace physx
{
	class PxTriangleMesh;
	class PxRigidActor;
	class PxTransform;
	class PxSphereGeometry;
	class PxBoxGeometry;
	class PxCapsuleGeometry;
	class PxConvexMeshGeometry;
	class PxConvexMesh;
	class PxTriangleMesh;
	class PxTriangleMeshGeometry;
}

enum class EPhysXMeshCookFlags : uint8;

DECLARE_CYCLE_STAT_EXTERN(TEXT("PhysX Cooking"), STAT_PhysXCooking, STATGROUP_Physics, );


/** UV information for BodySetup, only created if UPhysicsSettings::bSupportUVFromHitResults */
struct FBodySetupUVInfo
{
	/** Index buffer, required to go from face index to UVs */
	TArray<int32> IndexBuffer;
	/** Vertex positions, used to determine barycentric co-ords */
	TArray<FVector> VertPositions;
	/** UV channels for each vertex */
	TArray< TArray<FVector2D> > VertUVs;

	friend FArchive& operator<<(FArchive& Ar, FBodySetupUVInfo& UVInfo)
	{
		Ar << UVInfo.IndexBuffer;
		Ar << UVInfo.VertPositions;
		Ar << UVInfo.VertUVs;

		return Ar;
	}

	/** Get resource size of UV info */
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	SIZE_T GetResourceSize() const;
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;
	SIZE_T GetResourceSizeBytes() const;

	void FillFromTriMesh(const FTriMeshCollisionData& TriMeshCollisionData);
};

/** Helper struct to indicate which geometry needs to be cooked */
struct ENGINE_API FCookBodySetupInfo
{
	FCookBodySetupInfo();

	/** Trimesh data for cooking */
	FTriMeshCollisionData TriangleMeshDesc;

	/** Trimesh cook flags */
	EPhysXMeshCookFlags TriMeshCookFlags;

	/** Convex cook flags */
	EPhysXMeshCookFlags ConvexCookFlags;

	/** Vertices of NonMirroredConvex hulls */
	TArray<TArray<FVector>> NonMirroredConvexVertices;

	/** Vertices of NonMirroredConvex hulls */
	TArray<TArray<FVector>> MirroredConvexVertices;

	/** Debug name helpful for runtime cooking warnings */
	FString OuterDebugName;

	/** Whether to cook the regular convex hulls */
	bool bCookNonMirroredConvex;

	/** Whether to cook the mirror convex hulls */
	bool bCookMirroredConvex;

	/** Whether the convex being cooked comes from a deformable mesh */
	bool bConvexDeformableMesh;

	/** Whether to cook trimesh collision*/
	bool bCookTriMesh;

	/** Whether to support UV from hit results */
	bool bSupportUVFromHitResults;

	/** Error generating cook info for trimesh*/
	bool bTriMeshError;
};

struct FPhysXCookHelper;

/**
 * BodySetup contains all collision information that is associated with a single asset.
 * A single BodySetup instance is shared among many BodyInstances so that geometry data is not duplicated.
 * Assets typically implement a GetBodySetup function that is used during physics state creation.
 * 
 * @see GetBodySetup
 * @see FBodyInstance
 */

UCLASS(collapseCategories, MinimalAPI)
class UBodySetup : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Simplified collision representation of this  */
	UPROPERTY(EditAnywhere, Category = BodySetup, meta=(DisplayName = "Primitives", NoResetToDefault))
	struct FKAggregateGeom AggGeom;

	/** Used in the PhysicsAsset case. Associates this Body with Bone in a skeletal mesh. */
	UPROPERTY(Category=BodySetup, VisibleAnywhere)
	FName BoneName;

	/** 
	 *	If simulated it will use physics, if kinematic it will not be affected by physics, but can interact with physically simulated bodies. Default will inherit from OwnerComponent's behavior.
	 */
	UPROPERTY(EditAnywhere, Category=Physics)
	TEnumAsByte<EPhysicsType> PhysicsType;

	/** 
	 *	If true (and bEnableFullAnimWeightBodies in SkelMeshComp is true), the physics of this bone will always be blended into the skeletal mesh, regardless of what PhysicsWeight of the SkelMeshComp is. 
	 *	This is useful for bones that should always be physics, even when blending physics in and out for hit reactions (eg cloth or pony-tails).
	 */
	UPROPERTY()
	uint32 bAlwaysFullAnimWeight_DEPRECATED:1;

	/** 
	 *	Should this BodySetup be considered for the bounding box of the PhysicsAsset (and hence SkeletalMeshComponent).
	 *	There is a speed improvement from having less BodySetups processed each frame when updating the bounds.
	 */
	UPROPERTY(EditAnywhere, Category=BodySetup)
	uint32 bConsiderForBounds:1;

	/** 
	 *	If true, the physics of this mesh (only affects static meshes) will always contain ALL elements from the mesh - not just the ones enabled for collision. 
	 *	This is useful for forcing high detail collisions using the entire render mesh.
	 */
	UPROPERTY(Transient)
	uint32 bMeshCollideAll:1;

	/**
	*	If true, the physics triangle mesh will use double sided faces when doing scene queries.
	*	This is useful for planes and single sided meshes that need traces to work on both sides.
	*/
	UPROPERTY(EditAnywhere, Category=Physics)
	uint32 bDoubleSidedGeometry : 1;

	/**	Should we generate data necessary to support collision on normal (non-mirrored) versions of this body. */
	UPROPERTY()
	uint32 bGenerateNonMirroredCollision:1;

	/** Whether the cooked data is shared by multiple body setups. This is needed for per poly collision case where we don't want to duplicate cooked data, but still need multiple body setups for in place geometry changes */
	UPROPERTY()
	uint32 bSharedCookedData : 1;

	/** 
	 *	Should we generate data necessary to support collision on mirrored versions of this mesh. 
	 *	This halves the collision data size for this mesh, but disables collision on mirrored instances of the body.
	 */
	UPROPERTY()
	uint32 bGenerateMirroredCollision:1;

	/** Physical material to use for simple collision on this body. Encodes information about density, friction etc. */
	UPROPERTY(EditAnywhere, Category=Physics, meta=(DisplayName="Simple Collision Physical Material"))
	class UPhysicalMaterial* PhysMaterial;

	/** Collision Type for this body. This eventually changes response to collision to others **/
	UPROPERTY(EditAnywhere, Category=Collision)
	TEnumAsByte<enum EBodyCollisionResponse::Type> CollisionReponse;

	/** Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate **/
	UPROPERTY(EditAnywhere, Category=Collision, meta=(DisplayName = "Collision Complexity"))
	TEnumAsByte<enum ECollisionTraceFlag> CollisionTraceFlag;

	ENGINE_API TEnumAsByte<enum ECollisionTraceFlag> GetCollisionTraceFlag() const;

	/** Default properties of the body instance, copied into objects on instantiation, was URB_BodyInstance */
	UPROPERTY(EditAnywhere, Category=Collision, meta=(FullyExpand = "true"))
	struct FBodyInstance DefaultInstance;

	/** Custom walkable slope setting for this body. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Physics)
	struct FWalkableSlopeOverride WalkableSlopeOverride;

	UPROPERTY()
	float BuildScale_DEPRECATED;

	/** Build scale for this body setup (static mesh settings define this value) */
	UPROPERTY()
	FVector BuildScale3D;

	/** GUID used to uniquely identify this setup so it can be found in the DDC */
	FGuid BodySetupGuid;

	/** Cooked physics data for each format */
	FFormatContainer CookedFormatData;

private:
#if WITH_EDITOR
	/** Cooked physics data with runtime only optimizations. This allows us to remove editor only data (like face index remap) assuming the project doesn't use it at runtime. At runtime we load this into CookedFormatData */
	FFormatContainer CookedFormatDataRuntimeOnlyOptimization;
#endif

#if WITH_PHYSX
	/** Get cook flags for 'runtime only' cooked physics data */
	EPhysXMeshCookFlags GetRuntimeOnlyCookOptimizationFlags() const;
#endif 

public:

	/** Cooked physics data override. This is needed in cases where some other body setup has the cooked data and you don't want to own it or copy it. See per poly skeletal mesh */
	FFormatContainer* CookedFormatDataOverride;

#if WITH_PHYSX
	/** Physics triangle mesh, created from cooked data in CreatePhysicsMeshes */
	TArray<physx::PxTriangleMesh*> TriMeshes;
#endif

	/** Additional UV info, if available. Used for determining UV for a line trace impact. */
	FBodySetupUVInfo UVInfo;

	/** Flag used to know if we have created the physics convex and tri meshes from the cooked data yet */
	bool bCreatedPhysicsMeshes;

	/** Indicates whether this setup has any cooked collision data. */
	bool bHasCookedCollisionData;

	/** Indicates that we will never use convex or trimesh shapes. This is an optimization to skip checking for binary data. */
	bool bNeverNeedsCookedCollisionData;

public:
	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~ End UObject Interface.

	//
	//~ Begin UBodySetup Interface.
	//
	ENGINE_API void CopyBodyPropertiesFrom(const UBodySetup* FromSetup);

	/** Add collision shapes from another body setup to this one */
	ENGINE_API void AddCollisionFrom(class UBodySetup* FromSetup);
	ENGINE_API void AddCollisionFrom(const FKAggregateGeom& FromAggGeom);
	
	/** Create Physics meshes (ConvexMeshes, TriMesh & TriMeshNegX) from cooked data */
	/** Release Physics meshes (ConvexMeshes, TriMesh & TriMeshNegX). Must be called before the BodySetup is destroyed */
	ENGINE_API virtual void CreatePhysicsMeshes();

	/** Create Physics meshes (ConvexMeshes, TriMesh & TriMeshNegX) from cooked data async (useful for runtime cooking as it can go wide off the game thread) */
	/** Release Physics meshes (ConvexMeshes, TriMesh & TriMeshNegX). Must be called before the BodySetup is destroyed */
	/** NOTE: You cannot use the body setup until this operation is done. You must create the physics state (call CreatePhysicsState, or InitBody, etc..) , this does not automatically update the BodyInstance state for you */
	ENGINE_API void CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished OnAsyncPhysicsCookFinished);

private:
	/** Finalize game thread data before calling back user's delegate */
	void FinishCreatePhysicsMeshesAsync(FPhysXCookHelper* AsyncPhysicsCookHelper, FOnAsyncPhysicsCookFinished OnAsyncPhysicsCookFinished);
	
	/**
	* Given a format name returns its cooked data.
	*
	* @param Format Physics format name.
	* @param bRuntimeOnlyOptimizedVersion whether we want the data that has runtime only optimizations. At runtime this flag is ignored and we use the runtime only optimized data regardless.
	* @return Cooked data or NULL of the data was not found.
	*/
	FByteBulkData* GetCookedData(FName Format, bool bRuntimeOnlyOptimizedVersion = false);

public:

	/** Finish creating the physics meshes and update the body setup data with cooked data */
	ENGINE_API void FinishCreatingPhysicsMeshes(const TArray<physx::PxConvexMesh*>& ConvexMeshes, const TArray<physx::PxConvexMesh*>& ConvexMeshesNegX, const TArray<physx::PxTriangleMesh*>& TriMeshes);

	/** Returns the volume of this element */
	ENGINE_API virtual float GetVolume(const FVector& Scale) const;

	/** Release Physics meshes (ConvexMeshes, TriMesh & TriMeshNegX) */
	ENGINE_API void ClearPhysicsMeshes();

	/** Calculates the mass. You can pass in the component where additional information is pulled from ( Scale, PhysMaterialOverride ) */
	ENGINE_API virtual float CalculateMass(const UPrimitiveComponent* Component = nullptr) const;

	/** Returns the physics material used for this body. If none, specified, returns the default engine material. */
	ENGINE_API class UPhysicalMaterial* GetPhysMaterial() const;

	/** Clear all simple collision */
	ENGINE_API void RemoveSimpleCollision();

	/** 
	 * Rescales simple collision geometry.  Note you must recreate physics meshes after this 
	 *
	 * @param BuildScale	The scale to apply to the geometry
	 */
	ENGINE_API void RescaleSimpleCollision( FVector BuildScale );

	/** Invalidate physics data */
	ENGINE_API virtual void	InvalidatePhysicsData();	

	/**
	 * Converts a UModel to a set of convex hulls for simplified collision.  Any convex elements already in
	 * this BodySetup will be destroyed.  WARNING: the input model can have no single polygon or
	 * set of coplanar polygons which merge to more than FPoly::MAX_VERTICES vertices.
	 *
	 * @param		InModel					The input BSP.
	 * @param		bRemoveExisting			If true, clears any pre-existing collision
	 * @return								true on success, false on failure because of vertex count overflow.
	 */
	ENGINE_API void CreateFromModel(class UModel* InModel, bool bRemoveExisting);

	/**
	 * Converts the skinned data of a skeletal mesh into a tri mesh collision. This is used for per poly scene queries and is quite expensive.
	 * In 99% of cases you should be fine using a physics asset created for the skeletal mesh
	 * @param	InSkeletalMeshComponent		The skeletal mesh component we'll be grabbing the skinning information from
	 */
	ENGINE_API void UpdateTriMeshVertices(const TArray<FVector> & NewPositions);

	/**	
	 * Finds the shortest distance between the body setup and a world position. Input and output are given in world space
	 * @param	WorldPosition	The point we are trying to get close to
	 * @param	BodyToWorldTM	The transform to convert BodySetup into world space
	 * @return					The distance between WorldPosition and the body setup. 0 indicates WorldPosition is inside one of the shapes.
	 *
	 * NOTE: This function ignores convex and trimesh data
	 */
	ENGINE_API float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const;

	/** 
	 * Finds the closest point in the body setup. Input and outputs are given in world space.
	 * @param	WorldPosition			The point we are trying to get close to
	 * @param	BodyToWorldTM			The transform to convert BodySetup into world space
	 * @param	ClosestWorldPosition	The closest point on the body setup to WorldPosition
	 * @param	FeatureNormal			The normal of the feature associated with ClosestWorldPosition
	 * @return							The distance between WorldPosition and the body setup. 0 indicates WorldPosition is inside one of the shapes.
	 *
	 * NOTE: This function ignores convex and trimesh data
	 */
	ENGINE_API float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& FeatureNormal) const;

	/**
	* Generates the information needed for cooking geometry.
	* @param	OutCookInfo				Info needed during cooking
	* @param	InCookFlags				Any flags desired for TriMesh cooking
	*/
	ENGINE_API void GetCookInfo(FCookBodySetupInfo& OutCookInfo, EPhysXMeshCookFlags InCookFlags) const;

	/** 
	 *	Given a location in body space, and face index, find the UV of the desired UV channel.
	 *	Note this ONLY works if 'Support UV From Hit Results' is enabled in Physics Settings.
	 */
	bool CalcUVAtLocation(const FVector& BodySpaceLocation, int32 FaceIndex, int32 UVChannel, FVector2D& UV) const;


#if WITH_EDITOR
	ENGINE_API virtual void BeginCacheForCookedPlatformData(  const ITargetPlatform* TargetPlatform ) override;
	ENGINE_API virtual void ClearCachedCookedPlatformData(  const ITargetPlatform* TargetPlatform ) override;

	/*
	* Copy all UPROPERTY settings except the collision geometry.
	* This function is use when we restore the original data after a re-import of a static mesh.
	* All UProperty should be copy here except the collision geometry (i.e. AggGeom)
	*/
	ENGINE_API virtual void CopyBodySetupProperty(const UBodySetup* Other);
#endif // WITH_EDITOR

#if WITH_PHYSX
	/** 
	 *   Add the shapes defined by this body setup to the supplied PxRigidBody. 
	 */
	ENGINE_API void AddShapesToRigidActor_AssumesLocked(FBodyInstance* OwningInstance, physx::PxRigidActor* PDestActor, EPhysicsSceneType SceneType, FVector& Scale3D, physx::PxMaterial* SimpleMaterial, TArray<UPhysicalMaterial*>& ComplexMaterials, FShapeData& ShapeData, const FTransform& RelativeTM = FTransform::Identity, TArray<physx::PxShape*>* NewShapes = NULL, bool bShapeSharing = false);
#endif // WITH_PHYSX

	friend struct FIterateBodySetupHelper;

};

/** Helper struct for iterating over shapes in a body setup.*/
struct ENGINE_API FBodySetupShapeIterator
{
	FBodySetupShapeIterator(const UBodySetup& InBodySetup, FVector& InScale3D, const FTransform& InRelativeTM);

	/** Iterates over the elements array and creates the needed geometry and local pose. Note that this memory is on the stack so it's illegal to use it by reference outside the lambda */
	template <typename ElemType, typename GeomType>
	void ForEachShape(const TArray<ElemType>& Elements, TFunctionRef<void(const ElemType& Elem, const GeomType& Geom, const physx::PxTransform& LocalPose, float ContactOffset)> VisitorFunc) const;

	/** Helper function to determine contact offset params */
	static void GetContactOffsetParams(float& InOutContactOffsetFactor, float& InOutMinContactOffset, float& InOutMaxContactOffset);

private:

	template <typename ElemType, typename GeomType> bool PopulatePhysXGeometryAndTransform(const ElemType& Elem, GeomType& Geom, physx::PxTransform& OutTM) const;
	template <typename GeomType> float ComputeContactOffset(const GeomType& Geom) const;
	template <typename ElemType> FString GetDebugName() const;

private:
	const UBodySetup& BodySetup;
	FVector& Scale3D;
	const FTransform& RelativeTM;

	float MinScaleAbs;
	float MinScale;
	FVector ShapeScale3DAbs;
	FVector ShapeScale3D;

	float ContactOffsetFactor;
	float MinContactOffset;
	float MaxContactOffset;
};

/// @cond DOXYGEN_WARNINGS

//Explicit export of template instantiation 
extern template ENGINE_API void FBodySetupShapeIterator::ForEachShape(const TArray<FKSphereElem>&, TFunctionRef<void(const FKSphereElem&, const physx::PxSphereGeometry&, const physx::PxTransform& , float )>) const;
extern template ENGINE_API void FBodySetupShapeIterator::ForEachShape(const TArray<FKBoxElem>&, TFunctionRef<void(const FKBoxElem&, const physx::PxBoxGeometry&, const physx::PxTransform&, float)>) const;
extern template ENGINE_API void FBodySetupShapeIterator::ForEachShape(const TArray<FKSphylElem>&, TFunctionRef<void(const FKSphylElem&, const physx::PxCapsuleGeometry&, const physx::PxTransform&, float)>) const;
extern template ENGINE_API void FBodySetupShapeIterator::ForEachShape(const TArray<FKConvexElem>&, TFunctionRef<void(const FKConvexElem&, const physx::PxConvexMeshGeometry&, const physx::PxTransform&, float)>) const;
extern template ENGINE_API void FBodySetupShapeIterator::ForEachShape(const TArray<physx::PxTriangleMesh*>&,TFunctionRef<void (physx::PxTriangleMesh* const &, const physx::PxTriangleMeshGeometry&, const physx::PxTransform&,float)>) const;

/// @endcond


struct ENGINE_API FAsyncPhysicsCookHelper
{
	FAsyncPhysicsCookHelper(class IPhysXCookingModule* InPhysXCookingModule, const FCookBodySetupInfo& InCookInfo);

	void CreatePhysicsMeshesAsync_Concurrent(FSimpleDelegateGraphTask::FDelegate FinishDelegate);

	void CreatePhysicsMeshes_Concurrent();

	void CreateConvexElements(const TArray<TArray<FVector>>& Elements, TArray<physx::PxConvexMesh*>& OutConvexMeshes, bool bFlipped);

	FCookBodySetupInfo CookInfo;
	IPhysXCookingModule* PhysXCookingModule;

	//output
	TArray<physx::PxConvexMesh*> OutNonMirroredConvexMeshes;
	TArray<physx::PxConvexMesh*> OutMirroredConvexMeshes;
	TArray<physx::PxTriangleMesh*> OutTriangleMeshes;
	FBodySetupUVInfo OutUVInfo;
};
