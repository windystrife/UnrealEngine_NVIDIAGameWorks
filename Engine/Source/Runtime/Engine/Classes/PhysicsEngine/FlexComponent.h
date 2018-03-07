// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FlexContainer.h"
#include "FlexAsset.h"
#include "Components/StaticMeshComponent.h"

#include "FlexComponent.generated.h"

struct FFlexContainerInstance;
class FFlexMeshSceneProxy;

struct NvFlexExtInstance;
struct NvFlexExtMovingFrame;
struct NvFlexExtTearingMeshEdit;

UCLASS(Blueprintable, hidecategories = (Object), meta=(BlueprintSpawnableComponent))
class ENGINE_API UFlexComponent : public UStaticMeshComponent, public IFlexContainerClient
{
	GENERATED_UCLASS_BODY()		

public:

	struct FlexParticleAttachment
	{
		TWeakObjectPtr<USceneComponent> Primitive;
		int32 ItemIndex;
		int32 ParticleIndex;
		float OldMass;
		FVector LocalPos;
	};

	/** Override the FlexAsset's container / phase / attachment properties */
	UPROPERTY(EditAnywhere, Category = Flex)
	bool OverrideAsset;

	/** The simulation container to spawn any flex data contained in the static mesh into */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Flex, meta=(editcondition = "OverrideAsset", ToolTip="If the static mesh has Flex data then it will be spawned into this simulation container."))
	UFlexContainer* ContainerTemplate;

	/** The phase to assign to particles spawned for this mesh */
	UPROPERTY(EditAnywhere, Category = Flex, meta = (editcondition = "OverrideAsset"))
	FFlexPhase Phase;

	/** The per-particle mass to use for the particles, for clothing this value be multiplied by 0-1 dependent on the vertex color*/
	UPROPERTY(EditAnywhere, Category = Flex, meta = (editcondition = "OverrideAsset"))
	float Mass;

	/** If true then the particles will be attached to any overlapping shapes on spawn*/
	UPROPERTY(EditAnywhere, Category = Flex, meta = (editcondition = "OverrideAsset"))
	bool AttachToRigids;

	/** Multiply the asset's over pressure amount for inflatable meshes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Flex)
	float InflatablePressureMultiplier;

	/** Multiply the asset's max strain before tearing, this can be used to script breaking by lowering the max strain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Flex)
	float TearingMaxStrainMultiplier;

	/** The number of tearing events that have occured */
	UPROPERTY(BlueprintReadWrite, Category = Flex)
	int32 TearingBreakCount;

	UFUNCTION(BlueprintNativeEvent, Category = Flex)
	void OnTear();
	
	/** Instance of a FlexAsset referencing particles and constraints in a solver */
	NvFlexExtInstance* AssetInstance;

	/* Clone of the cloth asset for tearing meshes */
	NvFlexExtAsset* TearingAsset;

	/* The simulation container the instance belongs to */
	FFlexContainerInstance* ContainerInstance; 

	/* Simulated particle positions  */
	TArray<FVector4> SimPositions;
	/* Simulated particle normals */
	TArray<FVector> SimNormals;

	/* Pre-simulated particle positions  */
	UPROPERTY(NonPIEDuplicateTransient)
	TArray<FVector> PreSimPositions;

	UPROPERTY(NonPIEDuplicateTransient)
	TArray<FVector> PreSimShapeTranslations;

	UPROPERTY(NonPIEDuplicateTransient)
	TArray<FQuat> PreSimShapeRotations;

	/* Pre-simulated transform of the component  */
	UPROPERTY(NonPIEDuplicateTransient)
	FVector PreSimRelativeLocation;

	UPROPERTY(NonPIEDuplicateTransient)
	FRotator PreSimRelativeRotation;

	UPROPERTY(NonPIEDuplicateTransient)
	FTransform PreSimTransform;

	/* transform of the component before keep simulation */
	UPROPERTY(NonPIEDuplicateTransient)
	FVector SavedRelativeLocation;

	UPROPERTY(NonPIEDuplicateTransient)
	FRotator SavedRelativeRotation;

	UPROPERTY(NonPIEDuplicateTransient)
	FTransform SavedTransform;

	/** Whether this component will simulate in the local space of a parent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Flex)
	bool bLocalSpace;

	/** Control local interial force scale */
	UPROPERTY(EditAnywhere, Category = Flex)
	FFlexInertialScale InertialScale;

	/* For local space simulation */
	NvFlexExtMovingFrame* MovingFrame;

	/* Shape transforms */
	TArray<FQuat> ShapeRotations;
	TArray<FVector> ShapeTranslations;

	/* Attachments to rigid bodies */
	TArray<FlexParticleAttachment> Attachments;

	/* Cached local bounds */
	FBoxSphereBounds LocalBounds;

	// sends updated simulation data to the rendering proxy
	void UpdateSceneProxy(FFlexMeshSceneProxy* SceneProxy);

	// Begin UActorComponent Interface
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual void SendRenderTransform_Concurrent() override;
	// End UActorComponent Interface

	// Begin UPrimitiveComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;
	virtual FMatrix GetRenderMatrix() const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual bool CanEditSimulatePhysics() override { return false; }
	// End UPrimitiveComponent Interface

	// Begin IFlexContainerClient Interface
	virtual bool IsEnabled() { return AssetInstance != NULL; }
	virtual FBoxSphereBounds GetBounds() { return Bounds; }
	virtual void Synchronize();
	// End IFlexContainerClient Interface

	virtual void DisableSim();
	virtual void EnableSim();
	
	virtual bool IsTearingCloth();

	// attach particles to a component within a radius)
	virtual void AttachToComponent(USceneComponent* Component, float Radius);

		/// Returns true if the component is in editor world or conversely not in a game world.
		/// Will return true if GetWorld() is null    
	bool IsInEditorWorld() const;

	/**
	* Get the FleX container template
	*/
	UFUNCTION(BlueprintCallable, Category = "Components|Flex")
	virtual UFlexContainer* GetContainerTemplate();

private:
	void UpdateSimPositions();
	void SynchronizeAttachments();
	void ApplyLocalSpace();
};
