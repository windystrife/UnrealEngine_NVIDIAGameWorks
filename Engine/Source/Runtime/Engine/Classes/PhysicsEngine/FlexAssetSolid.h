// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FlexAssetSolid.generated.h"

/* A Flex solid asset is a specialized Flex asset that creates particles on a regular grid within a mesh and contains parameter
to configure rigid behavior. */
UCLASS(config = Engine, editinlinenew, meta = (DisplayName = "Flex Solid Asset"))
class ENGINE_API UFlexAssetSolid : public UFlexAsset
{
public:

	GENERATED_UCLASS_BODY()

	/** The spacing to use when sampling solid sha	pes with particles, this should be close to radius of the container this asset will be spawned in */
	UPROPERTY(EditAnywhere, Category = Flex)
	float SamplingDistance;

	/** The rigid body stiffness */
	UPROPERTY(EditAnywhere, Category = Flex)
	float Stiffness;

	/** Store the rigid body center of mass, not editable */
	UPROPERTY()
	FVector RigidCenter;

	virtual void PostLoad() override;
	virtual void ReImport(const UStaticMesh* Parent) override;
	virtual const NvFlexExtAsset* GetFlexAsset() override;
};
