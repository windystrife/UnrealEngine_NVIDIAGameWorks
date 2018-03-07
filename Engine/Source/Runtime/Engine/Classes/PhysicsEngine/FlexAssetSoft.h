// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FlexAssetSoft.generated.h"


class FFlexSoftSkinningIndicesVertexBuffer : public FVertexBuffer 
{
public:

	TArray<int16> Vertices;

	void Init(const TArray<int32>& ClusterIndices);
	virtual void InitRHI() override;
};

class FFlexSoftSkinningWeightsVertexBuffer : public FVertexBuffer 
{
public:
	
	TArray<float> Vertices;

	void Init(const TArray<float>& Weights);
	virtual void InitRHI() override;
};

/* A Flex Soft asset is a specialized Flex asset that creates particles on a regular grid within a mesh and contains parameter
to configure rigid behavior. */
UCLASS(config = Engine, editinlinenew, meta = (DisplayName = "Flex Soft Asset"))
class ENGINE_API UFlexAssetSoft : public UFlexAsset
{
public:

	GENERATED_UCLASS_BODY()

	/** The spacing to use when creating particles, should be approximately the radius on the container for this asset */
	UPROPERTY(EditAnywhere, Category = Sampling)
	float ParticleSpacing;

	/** Control the resolution the mesh is voxelized at in order to generate interior
	 *  sampling, if the mesh is not closed then this should be set to zero and surface  
	 *  sampling should be used instead */
	UPROPERTY(EditAnywhere, Category = Sampling)
	float VolumeSampling;

	/** Controls how many samples are taken of the mesh surface, this is useful to ensure
	 *  fine features of the mesh are represented by particles, or if the mesh is not closed */
	UPROPERTY(EditAnywhere, Category = Sampling)
	float SurfaceSampling;

	/** The spacing for shape-matching clusters, should be at least the particle spacing */
	UPROPERTY(EditAnywhere, Category = Clusters)
	float ClusterSpacing;

	/** Controls the overall size of the clusters, this controls how much overlap 
	 *  the clusters have which affects how smooth the final deformation is, if parts 
	 *  of the body are detaching then it means the clusters are not overlapping sufficiently 
	 *  to form a fully connected set of clusters */
	UPROPERTY(EditAnywhere, Category = Clusters)
	float ClusterRadius;

	/** Controls the stiffness of the resulting clusters */
	UPROPERTY(EditAnywhere, Category = Clusters)
	float ClusterStiffness;

	/** Any particles below this distance will have additional distance constraints created between them */
	UPROPERTY(EditAnywhere, Category = Links)
	float LinkRadius;

	/** The stiffness of distance links */
	UPROPERTY(EditAnywhere, Category = Links)
	float LinkStiffness;

	/** Skinning weights for the mesh vertices will be generated with the falloff inversely with distance to cluster according to this parameter */
	UPROPERTY(EditAnywhere, Category = Skinning)
	float SkinningFalloff;

	/** Any clusters greater than this distance from a particle won't contribute to the skinning */
	UPROPERTY(EditAnywhere, Category = Skinning)
	float SkinningMaxDistance;

	virtual void ReImport(const UStaticMesh* Parent) override;
	virtual const NvFlexExtAsset* GetFlexAsset() override;

	virtual void PostLoad() override;
	virtual void BeginDestroy() override;

public:

	FFlexSoftSkinningIndicesVertexBuffer IndicesVertexBuffer;
	FFlexSoftSkinningWeightsVertexBuffer WeightsVertexBuffer;
};
