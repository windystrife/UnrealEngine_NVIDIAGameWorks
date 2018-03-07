// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SkeletalMeshTypes.h"
#include "Engine/EngineTypes.h"
#include "PhysicsAssetUtils.generated.h"

class UBodySetup;
class UPhysicsAsset;
class UPhysicsConstraintTemplate;
class USkeletalMesh;
class USkeletalMeshComponent;

UENUM()
enum EPhysAssetFitGeomType
{
	EFG_Box					UMETA(DisplayName="Box"),
	EFG_Sphyl				UMETA(DisplayName="Capsule"),
	EFG_Sphere				UMETA(DisplayName="Sphere"),
	EFG_SingleConvexHull	UMETA(DisplayName="Single Convex Hull"),
	EFG_MultiConvexHull		UMETA(DisplayName="Multi Convex Hull")
};

UENUM()
enum EPhysAssetFitVertWeight
{
	EVW_AnyWeight			UMETA(DisplayName="Any Weight"),
	EVW_DominantWeight		UMETA(DisplayName="Dominant Weight"),
};

/** Parameters for PhysicsAsset creation */
USTRUCT()
struct FPhysAssetCreateParams
{
	GENERATED_BODY()

	FPhysAssetCreateParams()
	{
		MinBoneSize = 20.0f;
		MinWeldSize = KINDA_SMALL_NUMBER;
		GeomType = EFG_Sphyl;
		VertWeight = EVW_DominantWeight;
		bAutoOrientToBone = true;
		bCreateJoints = true;
		bWalkPastSmall = true;
		bBodyForAll = false;
		AngularConstraintMode = ACM_Limited;
		HullAccuracy = 0.5f;
		MaxHullVerts = 16;
	}

	/** Bones that are shorter than this value will be ignored for body creation */
	UPROPERTY(EditAnywhere, Category = "Body Creation")
	float MinBoneSize;

	/** Bones that are smaller than this value will be merged together for body creation */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Body Creation")
	float MinWeldSize;

	/** The geometry type that should be used when creating bodies */
	UPROPERTY(EditAnywhere, Category = "Body Creation", meta=(DisplayName="Primitive Type"))
	TEnumAsByte<EPhysAssetFitGeomType> GeomType;

	/** How vertices are mapped bones when approximating them with bodies */
	UPROPERTY(EditAnywhere, Category = "Body Creation", meta=(DisplayName="Vertex Weighting Type"))
	TEnumAsByte<EPhysAssetFitVertWeight> VertWeight;

	/** Whether to automatically orient the created bodies to their corresponding bones */
	UPROPERTY(EditAnywhere, Category = "Body Creation")
	bool bAutoOrientToBone;

	/** Whether to create constraints between adjacent created bodies */
	UPROPERTY(EditAnywhere, Category = "Constraint Creation")
	bool bCreateJoints;

	/** Whether to skip small bones entirely (rather than merge them with adjacent bones) */
	UPROPERTY(EditAnywhere, Category = "Body Creation", meta=(DisplayName="Walk Past Small Bones"))
	bool bWalkPastSmall;

	/** Forces creation of a body for each bone */
	UPROPERTY(EditAnywhere, Category = "Body Creation", meta=(DisplayName="Create Body for All Bones"))
	bool bBodyForAll;

	/** The type of angular constraint to create between bodies */
	UPROPERTY(EditAnywhere, Category = "Constraint Creation", meta=(EditCondition="bCreateJoints"))
	TEnumAsByte<EAngularConstraintMotion> AngularConstraintMode;

	/** When creating convex hulls, the target accuracy of the created hull */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Body Creation")
	float HullAccuracy;

	/** When creating convex hulls, the maximum verts that should be created */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Body Creation")
	int32 MaxHullVerts;
};

class UPhysicsAsset;
class UPhysicsConstraintTemplate;
struct FBoneVertInfo;

/** Collection of functions to create and setup PhysicsAssets */
namespace FPhysicsAssetUtils
{
	/**
	 * Given a USkeletalMesh, construct a new PhysicsAsset automatically, using the vertices weighted to each bone to calculate approximate collision geometry.
	 * Ball-and-socket joints will be created for every joint by default.
	 *
	 * @param	PhysicsAsset		The PhysicsAsset instance to setup
	 * @param	SkelMesh			The Skeletal Mesh to create the physics asset from
	 * @param	Params				Additional creation parameters
	 * @param	OutErrorMessage		Additional error information
	 * @param	bSetToMesh			Whether or not to apply the physics asset to SkelMesh immediately
	 */
	UNREALED_API bool CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, USkeletalMesh* SkelMesh, const FPhysAssetCreateParams& Params, FText& OutErrorMessage, bool bSetToMesh = true);

	/** Replaces any collision already in the BodySetup with an auto-generated one using the parameters provided.
	 * 
	 * @warning Certain physics geometry types, such as multi-convex hull, must recreate internal caches every time this function is called.
	 * If you find you're calling this function repeatedly for different bone indices on the same mesh,
	 * CreateFromSkeletalMesh or CreateCollisionFromBones will provide better performance.
	 *
	 * @param	bs					BodySetup to create the collision for
	 * @param	skelMesh			The SkeletalMesh we create collision for
	 * @param	BoneIndex			Index of the bone the collision is created for
	 * @param	Params				Additional parameters to control the creation 
	 * @param	Info				The vertices to create the collision for
	 * @return  Returns true if successfully created collision from bone
	 */
	UNREALED_API bool CreateCollisionFromBone( UBodySetup* bs, USkeletalMesh* skelMesh, int32 BoneIndex, const FPhysAssetCreateParams& Params, const FBoneVertInfo& Info );

	/** Replaces any collision already in the BodySetup with an auto-generated one using the parameters provided.
	 * 
	 * @param	bs					BodySetup to create the collision for
	 * @param	skelMesh			The SkeletalMesh we create collision for
	 * @param	BoneIndices			Indices of the bones the collisions are created for
	 * @param	Params				Additional parameters to control the creation 
	 * @param	Info				The vertices to create the collision for
	 * @return  Returns true if successfully created collision from all specified bones
	 */
	UNREALED_API bool CreateCollisionFromBones( UBodySetup* bs, USkeletalMesh* skelMesh, const TArray<int32>& BoneIndices, FPhysAssetCreateParams& Params, const FBoneVertInfo& Info );

	/**
	 * Does a few things:
	 * - add any collision primitives from body2 into body1 (adjusting the tm of each).
	 * - reconnect any constraints between 'add body' to 'base body', destroying any between them.
	 * - update collision disable table for any pairs including 'add body'
	 */
	UNREALED_API void WeldBodies(UPhysicsAsset* PhysAsset, int32 BaseBodyIndex, int32 AddBodyIndex, USkeletalMeshComponent* SkelComp);

	/**
	 * Creates a new constraint
	 *
	 * @param	PhysAsset			The PhysicsAsset to create the constraint for
	 * @param	InConstraintName	Name of the constraint
	 * @param	InConstraintSetup	Optional constraint setup
	 * @return  Returns the index of the newly created constraint.
	 **/
	UNREALED_API int32 CreateNewConstraint(UPhysicsAsset* PhysAsset, FName InConstraintName, UPhysicsConstraintTemplate* InConstraintSetup = NULL);

	/**
	 * Destroys the specified constraint
	 *
	 * @param	PhysAsset			The PhysicsAsset for which the constraint should be destroyed
	 * @param	ConstraintIndex		The index of the constraint to destroy
	 */
	UNREALED_API void DestroyConstraint(UPhysicsAsset* PhysAsset, int32 ConstraintIndex);

	/**
	 * Create a new BodySetup and default BodyInstance if there is not one for this body already.
	 *
	 * @param	PhysAsset			The PhysicsAsset to create the body for
	 * @param	InBodyName			Name of the new body
	 * @return	The Index of the newly created body.
	 */
	UNREALED_API int32 CreateNewBody(UPhysicsAsset* PhysAsset, FName InBodyName);
	
	/** 
	 * Destroys the specified body
	 *
	 * @param	PhysAsset			The PhysicsAsset for which the body should be destroyed
	 * @param	BodyIndex			Index of the body to destroy
	 */
	UNREALED_API void DestroyBody(UPhysicsAsset* PhysAsset, int32 BodyIndex);
};