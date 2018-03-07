// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FlexAsset.generated.h"

// Flex extensions types
struct FlexSolver;
struct FlexExtContainer;
struct NvFlexExtAsset;
struct NvFlexExtInstance;

class UFlexContainer;

/** Defines flags that control how the particle behaves */
USTRUCT()
struct FFlexPhase
{
	GENERATED_USTRUCT_BODY()

	/** If true, then particles will be auto-assigned a new group, by default particles will only collide with particles in different groups */
	UPROPERTY(EditAnywhere, Category = Phase)
	bool AutoAssignGroup;

	/** Manually set the group that the particles will be placed in */
	UPROPERTY(EditAnywhere, Category = Phase, meta = (editcondition = "!AutoAssignGroup"))
	int32 Group;

	/** Control whether particles interact with other particles in the same group */
	UPROPERTY(EditAnywhere, Category = Phase)
	bool SelfCollide;

	/** If true then particles will not collide or interact with any particles they overlap in the rest pose */
	UPROPERTY(EditAnywhere, Category = Phase)
	bool IgnoreRestCollisions;

	/** Control whether the particles will generate fluid density constraints when interacting with other fluid particles, note that fluids must also be enabled on the container */
	UPROPERTY(EditAnywhere, Category = Phase)
	bool Fluid;

	FFlexPhase();
};

/** Defines values that control how the localized inertia is applied */
USTRUCT()
struct FFlexInertialScale
{
	GENERATED_USTRUCT_BODY()

	/** Scale how much of local linear velocity to transmit */
	UPROPERTY(EditAnywhere, Category = LocalInertia)
	float LinearInertialScale;

	/** Scale how much of local angular velocity to transmit */
	UPROPERTY(EditAnywhere, Category = LocalInertia)
	float AngularInertialScale;

	FFlexInertialScale();
};

/* A Flex asset contains the particle and constraint data for a shape, such as cloth, rigid body or inflatable, an asset 
   is added to a container by spawning through a particle system or Flex actor. */
UCLASS(Abstract, config = Engine, editinlinenew)
class ENGINE_API UFlexAsset : public UObject
{
public:

	GENERATED_UCLASS_BODY()

	/** The simulation container to spawn any flex data contained in the static mesh into */
	UPROPERTY(EditAnywhere, Category = Flex, meta = (ToolTip = "If the static mesh has Flex data then it will be spawned into this simulation container."))
	UFlexContainer* ContainerTemplate;

	/** The phase to assign to particles spawned for this mesh */
	UPROPERTY(EditAnywhere, Category = Flex)
	FFlexPhase Phase;

	/** If true then the particles will be attached to any overlapping shapes on spawn*/
	UPROPERTY(EditAnywhere, Category = Flex)
	bool AttachToRigids;

	/** The per-particle mass to use for the particles, for clothing this value be multiplied by 0-1 dependent on the vertex color */
	UPROPERTY(EditAnywhere, Category = Flex)
	float Mass;
	
	// particles created from the mesh
	UPROPERTY()
	TArray<FVector4> Particles;

	// distance constraints
	UPROPERTY()
	TArray<int32> SpringIndices;
	UPROPERTY()
	TArray<float> SpringCoefficients;
	UPROPERTY()
	TArray<float> SpringRestLengths;

	// faces for cloth
	UPROPERTY()
	TArray<int32> Triangles;
	UPROPERTY()
	TArray<int32> VertexToParticleMap;

	// shape constraints
	UPROPERTY()
	TArray<int32> ShapeIndices;
	UPROPERTY()
	TArray<int32> ShapeOffsets;
	UPROPERTY()
	TArray<float> ShapeCoefficients;
	UPROPERTY()
	TArray<FVector> ShapeCenters;

	// mesh skinning information
	UPROPERTY()
	TArray<float> SkinningWeights;
	UPROPERTY()
	TArray<int32> SkinningIndices;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;

	virtual void ReImport(const UStaticMesh* Parent) {}
	virtual const NvFlexExtAsset* GetFlexAsset() { return NULL; }

	//
	NvFlexExtAsset* Asset;

};
