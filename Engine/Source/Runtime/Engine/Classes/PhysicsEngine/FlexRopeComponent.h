// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FlexContainer.h"
#include "FlexAsset.h"

#include "FlexRopeComponent.generated.h"

/** Container instance */
struct FFlexContainerInstance;
struct NvFlexExtAsset;
struct NvFlexExtInstance;
class FFlexRopeSceneProxy;

/** Component that allows you to specify custom triangle mesh geometry */
UCLASS(hidecategories=(Object, Physics, Collision, Activation, "Components|Activation"), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UFlexRopeComponent : public UMeshComponent, public IFlexContainerClient
{
	GENERATED_UCLASS_BODY()

public:

	struct FlexParticleAttachment
	{
		TWeakObjectPtr<UPrimitiveComponent> Primitive;
		int32 ShapeIndex;
		int32 ParticleIndex;
		float OldMass;
		FVector LocalPos;
	};

#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Begin UActorComponent interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void SendRenderDynamicData_Concurrent() override;
	// Begin UActorComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	// Begin USceneComponent interface.

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// End UPrimitiveComponent interface.

	// Begin UMeshComponent interface.
	virtual int32 GetNumMaterials() const override;
	// End UMeshComponent interface.

	// Begin IFlexContainerClient Interface
	virtual bool IsEnabled() { return AssetInstance != NULL; }
	virtual FBoxSphereBounds GetBounds() { return Bounds; }
	virtual void Synchronize();
	// End IFlexContainerClient Interface

	void GetEndPositions(FVector& OutStartPosition, FVector& OutEndPosition);
	void CreateRopeGeometry();
	void UpdateSceneProxy(FFlexRopeSceneProxy* Proxy);

	/** The Flex container to use for simulation */
	UPROPERTY(EditAnywhere, Category = "Flex")
	UFlexContainer* ContainerTemplate;
	
	/** The particle phase identifier controlling particle collision */
	UPROPERTY(EditAnywhere, Category = "Flex")
	FFlexPhase Phase;

	/** How strongly the rope resists stretching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flex", meta = (ClampMin = "0.00", UIMin = "0.00", ClampMax = "1.0", UIMax = "1.0"))
	float StretchStiffness;

	/** How strongly the rope resists bending */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flex", meta = (ClampMin = "0.00", UIMin = "0.00", ClampMax = "1.0", UIMax = "1.0"))
	float BendStiffness;

	/** If non-zero this will generate "long range constarints" that reduce stretching, note this should only be used when the top of the rope is fixed, e.g.: inside a collision shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flex", meta = (ClampMin = "0.00", UIMin = "0.00", ClampMax = "1.0", UIMax = "1.0"))
	float TetherStiffness;

	/** Particles embedded in shapes at level start up will be permanent attached to them */
	UPROPERTY(EditAnywhere, Category = "Flex")
	bool AttachToRigids;

	/** End location of FlexRope, relative to AttachEndTo if specified, otherwise relative to FlexRope component. */
	UPROPERTY(EditAnywhere, Category="Rope")
	FVector EndLocation;

	/** Rest length of the FlexRope */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax="2000.0", UIMax = "1000.0"))
	float Length;

	/** How wide the FlexRope geometry is */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope", meta = (ClampMin = "0.01", UIMin = "0.01", ClampMax = "100.0", UIMax = "50.0"))
	float Width;

	/** If set, the number of segments is computed based on length and radius */
	UPROPERTY(EditAnywhere, Category = "Rope")
	bool AutoComputeSegments;

	/** How many segments the FlexRope has */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope", meta = (ClampMin = "1", UIMin = "1", ClampMax = "2000", UIMax = "40"), meta = (EditCondition = "!AutoComputeSegments"))
	int32 NumSegments;

	/** Number of sides of the FlexRope geometry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rope Rendering", meta=(ClampMin = "1", ClampMax = "16"))
	int32 NumSides;

	/** How many times to repeat the material along the length of the FlexRope */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rope Rendering", meta=(UIMin = "0.1", UIMax = "8"))
	float TileMaterial;

private:

	/* Attachments to rigid bodies */
	TArray<FlexParticleAttachment> Attachments;

	/** Container instance */
	FFlexContainerInstance* ContainerInstance;
	
	/** Particle / Constraint definition */
	NvFlexExtAsset* Asset;
	NvFlexExtInstance* AssetInstance;

	/** Array of Flex particle indices */
	TArray<FVector4> Particles;	
	TArray<int> SpringIndices;
	TArray<float> SpringLengths;
	TArray<float> SpringCoefficients;

	friend class FFlexRopeSceneProxy;
};


