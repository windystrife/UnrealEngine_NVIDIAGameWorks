// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FlexAssetCloth.generated.h"

/* A Flex cloth asset is a specialized Flex asset that creates one particle per mesh vertex and contains parameter 
to configure cloth behavior. */
UCLASS(config = Engine, editinlinenew, meta = (DisplayName = "Flex Cloth Asset"))
class ENGINE_API UFlexAssetCloth : public UFlexAsset
{
public:

	GENERATED_UCLASS_BODY()

	/** How much the cloth resists stretching */
	UPROPERTY(EditAnywhere, Category = Flex)
	float StretchStiffness;

	/** How much the cloth resists bending */
	UPROPERTY(EditAnywhere, Category = Flex)
	float BendStiffness;

	/** How strong tethers resist stretching */
	UPROPERTY(EditAnywhere, Category = Flex)
	float TetherStiffness;

	/** How much tethers have to stretch past their rest-length before becoming enabled, 0.1=10% elongation */
	UPROPERTY(EditAnywhere, Category = Flex)
	float TetherGive;

	/** Can be enabled for closed meshes, a volume conserving constraint will be added to the simulation */
	UPROPERTY(EditAnywhere, Category = Flex)
	bool EnableInflatable;

	/** The inflatable pressure, a value of 1.0 corresponds to the rest volume, 0.5 corressponds to being deflated by half, and values > 1.0 correspond to over-inflation */
	UPROPERTY(EditAnywhere, Category = Flex, meta = (editcondition = "EnableInflatable"))
	float OverPressure;

	/** Wether the mesh can be torn, tether stiffness must be equal to 0.0 for tearing to be enabled */
	UPROPERTY(EditAnywhere, Category = Flex, meta = (editcondition = "TetherStiffness=0.0"))
	bool TearingEnabled;

	/** The maximum edge strain before a tearing even occurs, a value of 2.0 means the edge can be stretched to twice its rest length before breaking  */
	UPROPERTY(EditAnywhere, Category = Flex, meta = (editcondition = "TearingEnabled"))
	float TearingMaxStrain;

	/** The maximum number of edges to break in a single simulation step */
	UPROPERTY(EditAnywhere, Category = Flex, meta = (editcondition = "TearingEnabled"))
	uint32 TearingMaxBreakRate;

	/** When vertices are torn their vertex color alpha channel will be set to this value, this can be used to mix in a torn cloth texture mask for example */
	UPROPERTY(EditAnywhere, Category = Flex, meta = (editcondition = "TearingEnabled"))
	float TearingVertexAlpha;

	/** The stiffness coefficient for the inflable, this will automatically be calculated */
	UPROPERTY()
	float InflatableStiffness;

	/* The rest volume of the inflatable, this will automatically be calculated */
	UPROPERTY()
	float InflatableVolume;

	/** The rigid body stiffness */
	UPROPERTY(EditAnywhere, Category = Flex)
	float RigidStiffness;

	/** Store the rigid body center of mass, not editable */
	UPROPERTY()
	FVector RigidCenter;

	virtual void ReImport(const UStaticMesh* Parent) override;
	virtual const NvFlexExtAsset* GetFlexAsset() override;
};
