// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingAssetInterface.h"

#include "ClothingSystemRuntimeTypes.h"
#include "GPUSkinPublicDefs.h"
#include "SkeletalMeshTypes.h"

#include "ClothingAsset.generated.h"

class UClothingAsset;
class UPhysicsAsset;
struct FClothPhysicalMeshData;

namespace nvidia
{
	namespace apex
	{
		class ClothingAsset;
	}
}

DECLARE_LOG_CATEGORY_EXTERN(LogClothingAsset, Log, All);

/** The possible targets for a physical mesh mask */
UENUM()
enum class MaskTarget_PhysMesh : uint8
{
	None,
	MaxDistance,
	BackstopDistance,
	BackstopRadius,
};

/** 
 * A mask is simply some storage for a physical mesh parameter painted onto clothing.
 * Used in the editor for users to paint onto and then target to a parameter, which
 * is then later applied to a phys mesh
 */
USTRUCT()
struct CLOTHINGSYSTEMRUNTIME_API FClothParameterMask_PhysMesh
{
	GENERATED_BODY();

	FClothParameterMask_PhysMesh()
		: MaskName(NAME_None)
		, CurrentTarget(MaskTarget_PhysMesh::None)
		, MaxValue(-MAX_flt)
		, MinValue(MAX_flt)
		, bEnabled(false)
	{}

	/** 
	 * Initialize the mask based on the specified mesh data
	 * @param InMeshData the mesh to initialize against
	 */
	void Initialize(const FClothPhysicalMeshData& InMeshData);

	/** 
	 * Copies the specified parameter from a physical mesh
	 * @param InMeshData The mesh to copy from
	 * @param InTarget The target parameter to copy
	 */
	void CopyFromPhysMesh(const FClothPhysicalMeshData& InMeshData, MaskTarget_PhysMesh InTarget);

	/** 
	 * Set a value in the mask
	 * @param InVertexIndex the value/vertex index to set
	 * @param InValue the value to set
	 */
	void SetValue(int32 InVertexIndex, float InValue);

	/** 
	 * Get a value from the mask
	 * @param InVertexIndex the value/vertex index to retrieve
	 */
	float GetValue(int32 InVertexIndex) const;
	
	/** 
	* Read only version of the array holding the mask values
	*/
	const TArray<float>& GetValueArray() const;

	/** Calculates Min/Max values based on values. */
	void CalcRanges();

#if WITH_EDITOR
	/** 
	 * Get a value represented as a preview color for painting
	 * @param InVertexIndex the value/vertex index to retrieve
	 */
	FColor GetValueAsColor(int32 InVertexIndex) const;
#endif

	/** 
	 * Apply the mask to the provided mesh (if compatible)
	 * @param InTargetMesh the targeted mesh
	 */
	void Apply(FClothPhysicalMeshData& InTargetMesh);

	/** Name of the mask, mainly for users to differentiate */
	UPROPERTY()
	FName MaskName;

	/** The currently targeted parameter for the mask */
	UPROPERTY()
	MaskTarget_PhysMesh CurrentTarget;

	/** The maximum value currently in the mask value array */
	UPROPERTY()
	float MaxValue;

	/** The maximum value currently in the mask value array */
	UPROPERTY()
	float MinValue;

	/** The actual values stored in the mask */
	UPROPERTY()
	TArray<float> Values;

	/** Whether this mask is enabled and able to effect final mesh values */
	UPROPERTY()
	bool bEnabled;
};

// Bone data for a vertex
USTRUCT()
struct FClothVertBoneData
{
	GENERATED_BODY()

	FClothVertBoneData()
	{
		FMemory::Memset(BoneIndices, (uint8)INDEX_NONE, sizeof(BoneIndices));
		FMemory::Memset(BoneWeights, 0, sizeof(BoneWeights));
	}

	// Number of influences for this vertex.
	UPROPERTY()
	int32 NumInfluences;

	// Up to MAX_TOTAL_INFLUENCES bone indices that this vert is weighted to
	UPROPERTY()
	uint16 BoneIndices[MAX_TOTAL_INFLUENCES];

	// The weights for each entry in BoneIndices
	UPROPERTY()
	float BoneWeights[MAX_TOTAL_INFLUENCES];
};

/**
 *	Physical mesh data created during asset import or created from a skeletal mesh
 */
USTRUCT()
struct CLOTHINGSYSTEMRUNTIME_API FClothPhysicalMeshData
{
	GENERATED_BODY()

	void Reset(const int32 InNumVerts);

	// Clear out any target properties in this physical mesh
	void ClearParticleParameters();

	// Positions of each simulation vertex
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<FVector> Vertices;

	// Normal at each vertex
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<FVector> Normals;

	// Indices of the simulation mesh triangles
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<uint32> Indices;

	// The distance that each vertex can move away from its reference (skinned) position
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<float> MaxDistances;

	// Distance along the plane of the surface that the particles can travel (separation constraint)
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<float> BackstopDistances;

	// Radius of movement to allow for backstop movement
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<float> BackstopRadiuses;

	// Inverse mass for each vertex in the physical mesh
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<float> InverseMasses;

	// Indices and weights for each vertex, used to skin the mesh to create the reference pose
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<FClothVertBoneData> BoneData;

	// Maximum number of bone weights of any vetex
	UPROPERTY(EditAnywhere, Category = SimMesh)
	int32 MaxBoneWeights;

	// Number of fixed verts in the simulation mesh (fixed verts are just skinned and do not simulate)
	UPROPERTY(EditAnywhere, Category = SimMesh)
	int32 NumFixedVerts;

	// Valid indices to use for self collisions (reduced set of Indices)
	UPROPERTY(EditAnywhere, Category = SimMesh)
	TArray<uint32> SelfCollisionIndices;
};

USTRUCT()
struct CLOTHINGSYSTEMRUNTIME_API FClothLODData
{
	GENERATED_BODY()

	// Raw phys mesh data
	UPROPERTY(EditAnywhere, Category = SimMesh)
	FClothPhysicalMeshData PhysicalMeshData;

	// Collision primitive and covex data for clothing collisions
	UPROPERTY(EditAnywhere, Category = Collision)
	FClothCollisionData CollisionData;

#if WITH_EDITORONLY_DATA
	// Parameter masks defining the physics mesh masked data
	UPROPERTY(EditAnywhere, Category = Masks)
	TArray<FClothParameterMask_PhysMesh> ParameterMasks;

	// Get all available parameter masks for the specified target
	void GetParameterMasksForTarget(const MaskTarget_PhysMesh& InTarget, TArray<FClothParameterMask_PhysMesh*>& OutMasks);

#endif

	// Skinning data for transitioning from a higher detail LOD to this one
	TArray<FMeshToMeshVertData> TransitionUpSkinData;

	// Skinning data for transitioning from a lower detail LOD to this one
	TArray<FMeshToMeshVertData> TransitionDownSkinData;

	bool Serialize(FArchive& Ar)
	{
		UScriptStruct* Struct = FClothLODData::StaticStruct();

		// Serialize normal tagged data
		if (!Ar.IsCountingMemory())
		{
			Struct->SerializeTaggedProperties(Ar, (uint8*)this, Struct, nullptr);
		}

		// Serialize the mesh to mesh data (not a USTRUCT)
		Ar	<< TransitionUpSkinData
			<< TransitionDownSkinData;

		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FClothLODData> : public TStructOpsTypeTraitsBase2<FClothLODData>
{
	enum
	{
		WithSerializer = true,
	};
};

// Container for a constraint setup, these can be horizontal, vertical, shear and bend
USTRUCT()
struct FClothConstraintSetup
{
	GENERATED_BODY()

	FClothConstraintSetup()
		: Stiffness(1.0f)
		, StiffnessMultiplier(1.0f)
		, StretchLimit(1.0f)
		, CompressionLimit(1.0f)
	{}

	// How stiff this constraint is, this affects how closely it will follow the desired position
	UPROPERTY(EditAnywhere, Category=Constraint)
	float Stiffness;

	// A multiplier affecting the above value
	UPROPERTY(EditAnywhere, Category = Constraint)
	float StiffnessMultiplier;

	// The hard limit on how far this constarint can stretch
	UPROPERTY(EditAnywhere, Category = Constraint)
	float StretchLimit;

	// The hard limit on how far this constraint can compress
	UPROPERTY(EditAnywhere, Category = Constraint)
	float CompressionLimit;
};

UENUM()
enum class EClothingWindMethod : uint8
{
	// Use legacy wind mode, where accelerations are modified directly by the simulation
	// with no regard for drag or lift
	Legacy,

	// Use updated wind calculation for NvCloth based solved taking into account
	// drag and lift, this will require those properties to be correctly set in
	// the clothing configuration
	Accurate UMETA(Hidden)
};

/** Holds initial, asset level config for clothing actors. */
USTRUCT()
struct FClothConfig
{
	GENERATED_BODY()

	FClothConfig()
		: WindMethod(EClothingWindMethod::Legacy)
		, SelfCollisionRadius(0.0f)
		, SelfCollisionStiffness(0.0f)
		, SelfCollisionCullScale(1.0f)
		, Damping(0.4f)
		, Friction(0.1f)
		, WindDragCoefficient(0.02f/100.0f)
		, WindLiftCoefficient(0.02f/100.0f)
		, LinearDrag(0.2f)
		, AngularDrag(0.2f)
		, LinearInertiaScale(1.0f)
		, AngularInertiaScale(1.0f)
		, CentrifugalInertiaScale(1.0f)
		, SolverFrequency(120.0f)
		, StiffnessFrequency(100.0f)
		, GravityScale(1.0f)
		, TetherStiffness(1.0f)
		, TetherLimit(1.0f)
		, CollisionThickness(1.0f)
	{}

	bool HasSelfCollision() const;

	// How wind should be processed, Accurate uses drag and lift to make the cloth react differently, legacy applies similar forces to all clothing without drag and lift (similar to APEX)
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	EClothingWindMethod WindMethod;

	// Constraint data for vertical constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FClothConstraintSetup VerticalConstraintConfig;

	// Constraint data for horizontal constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FClothConstraintSetup HorizontalConstraintConfig;

	// Constraint data for bend constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FClothConstraintSetup BendConstraintConfig;

	// Constraint data for shear constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FClothConstraintSetup ShearConstraintConfig;

	// Size of self collision spheres centered on each vert
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float SelfCollisionRadius;

	// Stiffness of the spring force that will resolve self collisions
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float SelfCollisionStiffness;

	/** 
	 * Scale to use for the radius of the culling checks for self collisions.
	 * Any other self collision body within the radius of this check will be culled.
	 * This helps performance with higher resolution meshes by reducing the number
	 * of colliding bodies within the cloth. Reducing this will have a negative
	 * effect on performance!
	 */
	UPROPERTY(EditAnywhere, Category = ClothConfig, meta = (UIMin="0", ClampMin="0"))
	float SelfCollisionCullScale;

	// Damping of particle motion per-axis
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FVector Damping;

	// Friction of the surface when colliding
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float Friction;

	// Drag coefficient for wind calculations, higher values mean wind has more lateral effect on cloth
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float WindDragCoefficient;

	// Lift coefficient for wind calculations, higher values make cloth rise easier in wind
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float WindLiftCoefficient;

	// Drag applied to linear particle movement per-axis
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FVector LinearDrag;

	// Drag applied to angular particle movement, higher values should limit material bending (per-axis)
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	FVector AngularDrag;

	// Scale for linear particle inertia, how much movement should translate to linear motion (per-axis)
	UPROPERTY(EditAnywhere, Category = ClothConfig, meta = (UIMin="0", UIMax="1", ClampMin="0", ClampMax="1"))
	FVector LinearInertiaScale;

	// Scale for angular particle inertia, how much movement should translate to angular motion (per-axis)
	UPROPERTY(EditAnywhere, Category = ClothConfig, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	FVector AngularInertiaScale;

	// Scale for centrifugal particle inertia, how much movement should translate to angular motion (per-axis)
	UPROPERTY(EditAnywhere, Category = ClothConfig, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	FVector CentrifugalInertiaScale;

	// Frequency of the position solver, lower values will lead to stretchier, bouncier cloth
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float SolverFrequency;

	// Frequency for stiffness calculations, lower values will degrade stiffness of constraints
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float StiffnessFrequency;

	// Scale of gravity effect on particles
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float GravityScale;

	// Scale for stiffness of particle tethers between each other
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float TetherStiffness;

	// Scale for the limit of particle tethers (how far they can separate)
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float TetherLimit;

	// 'Thickness' of the simulated cloth, used to adjust collisions
	UPROPERTY(EditAnywhere, Category = ClothConfig)
	float CollisionThickness;
};

namespace ClothingAssetUtils
{
	// Helper struct to hold binding information on a clothing asset, used to enumerate all of the
	// bindings on a skeletal mesh with GetMeshClothingAssetBindings below.
	struct FClothingAssetMeshBinding
	{
		UClothingAsset* Asset;
		int32 LODIndex;
		int32 SectionIndex;
		int32 AssetInternalLodIndex;
	};

	/**
	 * Given a skeletal mesh, find all of the currently bound clothing assets and their binding information
	 * @param InSkelMesh - The skeletal mesh to search
	 * @param OutBindings - The list of bindings to write to
	 */
	void CLOTHINGSYSTEMRUNTIME_API GetMeshClothingAssetBindings(USkeletalMesh* InSkelMesh, TArray<FClothingAssetMeshBinding>& OutBindings);
	
	// Similar to above, but only inspects the specified LOD
	void CLOTHINGSYSTEMRUNTIME_API GetMeshClothingAssetBindings(USkeletalMesh* InSkelMesh, TArray<FClothingAssetMeshBinding>& OutBindings, int32 InLodIndex);
}

/**
 * Custom data wrapper for clothing assets
 * If writing a new clothing asset importer, creating a new derived custom data is how to store importer (and possibly simulation)
 * data that importer will create. This needs to be set to the CustomData member on the asset your factory creates.
 * Testing whether a UClothingAsset was made from a custom plugin can be achieved with
 * if(AssetPtr->CustomData->IsA(UMyCustomData::StaticClass()))
 */
UCLASS(abstract, MinimalAPI)
class UClothingAssetCustomData : public UObject
{
	GENERATED_BODY()

public:
	virtual void BindToSkeletalMesh(USkeletalMesh* InSkelMesh, int32 InMeshLodIndex, int32 InSectionIndex, int32 InAssetLodIndex)
	{}
};

UCLASS(hidecategories = Object, BlueprintType)
class CLOTHINGSYSTEMRUNTIME_API UClothingAsset : public UClothingAssetBase
{
	GENERATED_BODY()

public:

	UClothingAsset(const FObjectInitializer& ObjectInitializer);

	// UClothingAssetBase Interface ////////////////////////////////////////////
	virtual void RefreshBoneMapping(USkeletalMesh* InSkelMesh) override;
#if WITH_EDITOR
	virtual bool BindToSkeletalMesh(USkeletalMesh* InSkelMesh, int32 InMeshLodIndex, int32 InSectionIndex, int32 InAssetLodIndex) override;
	virtual void UnbindFromSkeletalMesh(USkeletalMesh* InSkelMesh) override;
	virtual void UnbindFromSkeletalMesh(USkeletalMesh* InSkelMesh, int32 InMeshLodIndex) override;
	virtual void InvalidateCachedData() override;
	virtual bool IsValidLod(int32 InLodIndex) override;
	virtual int32 GetNumLods() override;
	// End UClothingAssetBase Interface ////////////////////////////////////////

	/**
	*	Builds the LOD transition data
	*	When we transition between LODs we skin the incoming mesh to the outgoing mesh
	*	in exactly the same way the render mesh is skinned to create a smooth swap
	*/
	void BuildLodTransitionData();

	/** 
	 * Applies the painted parameter masks to the final parameters in the physical mesh
	 */
	void ApplyParameterMasks();
#endif

	// UObject Interface //////////////////////////////////////////////////////
	
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	// End UObject Interface //////////////////////////////////////////////////

#if WITH_EDITOR
	void HandlePhysicsAssetChange();
#endif

	/** 
	 * Builds self collision data
	 * This is a subset of actual verts that need to be considered for self collision such that we cover
	 * as much of the mesh as possible in collision bodies so that nothing can fall through
	 */
	void BuildSelfCollisionData();

	// Calculates the prefered root bone for the simulation
	void CalculateReferenceBoneIndex();

	// The physics asset to extract collisions from when building a simulation
	UPROPERTY(EditAnywhere, Category = Config)
	UPhysicsAsset* PhysicsAsset;

	// Configuration of the cloth, contains all the parameters for how the clothing behaves
	UPROPERTY(EditAnywhere, Category = Config)
	FClothConfig ClothConfig;

	// The actual asset data, listed by LOD
	UPROPERTY()
	TArray<FClothLODData> LodData;

	// Tracks which clothing LOD each skel mesh LOD corresponds to (LodMap[SkelLod]=ClothingLod)
	UPROPERTY()
	TArray<int32> LodMap;

	// List of bones this asset uses inside its parent mesh
	UPROPERTY()
	TArray<FName> UsedBoneNames;

	// List of the indices for the bones in UsedBoneNames, used for remapping
	UPROPERTY()
	TArray<int32> UsedBoneIndices;

	// Bone to treat as the root of the simulation space
	UPROPERTY()
	int32 ReferenceBoneIndex;

	/** Custom data applied by the importer depending on where the asset was imported from */
	UPROPERTY()
	UClothingAssetCustomData* CustomData;

private:

};
