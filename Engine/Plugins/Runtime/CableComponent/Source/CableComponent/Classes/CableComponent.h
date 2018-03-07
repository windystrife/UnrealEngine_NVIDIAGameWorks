// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/MeshComponent.h"
#include "CableComponent.generated.h"

class FPrimitiveSceneProxy;

/** Struct containing information about a point along the cable */
struct FCableParticle
{
	FCableParticle()
	: bFree(true)
	, Position(0,0,0)
	, OldPosition(0,0,0)
	{}

	/** If this point is free (simulating) or fixed to something */
	bool bFree;
	/** Current position of point */
	FVector Position;
	/** Position of point on previous iteration */
	FVector OldPosition;
};

/** Component that allows you to specify custom triangle mesh geometry */
UCLASS(hidecategories=(Object, Physics, Activation, "Components|Activation"), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class CABLECOMPONENT_API UCableComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

public:

	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual void CreateRenderState_Concurrent() override;
	//~ Begin UActorComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;
	virtual bool HasAnySockets() const override;
	virtual bool DoesSocketExist(FName InSocketName) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	//~ Begin USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.


	/**
	 *	Should we fix the start to something, or leave it free.
	 *	If false, component transform is just used for initial location of start of cable
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cable")
	bool bAttachStart;

	/** 
	 *	Should we fix the end to something (using AttachEndTo and EndLocation), or leave it free. 
	 *	If false, AttachEndTo and EndLocation are just used for initial location of end of cable
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cable")
	bool bAttachEnd;

	/** Actor or Component that the defines the end position of the cable */
	UPROPERTY(EditAnywhere, Category="Cable")
	FComponentReference AttachEndTo;

	/** Socket name on the AttachEndTo component to attach to */
	UPROPERTY(EditAnywhere, Category = "Cable")
	FName AttachEndToSocketName;

	/** End location of cable, relative to AttachEndTo (or AttachEndToSocketName) if specified, otherwise relative to cable component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cable", meta=(MakeEditWidget=true))
	FVector EndLocation;

	/** Attaches the end of the cable to a specific Component within an Actor **/
	UFUNCTION(BlueprintCallable, Category = "Cable")
	void SetAttachEndTo(AActor* Actor, FName ComponentProperty, FName SocketName = NAME_None);
	
	/** Gets the Actor that the cable is attached to **/
	UFUNCTION(BlueprintCallable, Category = "Cable")
	AActor* GetAttachedActor() const;

	/** Gets the specific USceneComponent that the cable is attached to **/
	UFUNCTION(BlueprintCallable, Category = "Cable")
	USceneComponent* GetAttachedComponent() const;

	/** Get array of locations of particles (in world space) making up the cable simulation. */
	UFUNCTION(BlueprintCallable, Category = "Cable")
	void GetCableParticleLocations(TArray<FVector>& Locations) const;

	/** Rest length of the cable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cable", meta=(ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
	float CableLength;

	/** How many segments the cable has */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cable", meta=(ClampMin = "1", UIMin = "1", UIMax = "20"))
	int32 NumSegments;

	/** Controls the simulation substep time for the cable */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category="Cable", meta=(ClampMin = "0.005", UIMin = "0.005", UIMax = "0.1"))
	float SubstepTime;

	/** The number of solver iterations controls how 'stiff' the cable is */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cable", meta=(ClampMin = "1", ClampMax = "16"))
	int32 SolverIterations;

	/** Add stiffness constraints to cable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Cable")
	bool bEnableStiffness;

	/** 
	 *	EXPERIMENTAL. Perform sweeps for each cable particle, each substep, to avoid collisions with the world. 
	 *	Uses the Collision Preset on the component to determine what is collided with.
	 *	This greatly increases the cost of the cable simulation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Cable")
	bool bEnableCollision;

	/** If collision is enabled, control how much sliding friction is applied when cable is in contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Cable", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableCollision"))
	float CollisionFriction;

	/** Force vector (world space) applied to all particles in cable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cable Forces")
	FVector CableForce;

	/** Scaling applied to world gravity affecting this cable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cable Forces")
	float CableGravityScale;

	/** How wide the cable geometry is */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cable Rendering", meta=(ClampMin = "0.01", UIMin = "0.01", UIMax = "50.0"))
	float CableWidth;

	/** Number of sides of the cable geometry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cable Rendering", meta=(ClampMin = "1", ClampMax = "16"))
	int32 NumSides;

	/** How many times to repeat the material along the length of the cable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cable Rendering", meta=(UIMin = "0.1", UIMax = "8"))
	float TileMaterial;

private:

	/** Solve the cable spring constraints */
	void SolveConstraints();
	/** Integrate cable point positions */
	void VerletIntegrate(float InSubstepTime, const FVector& Gravity);
	/** Perform collision traces for particles */
	void PerformCableCollision();
	/** Perform a simulation substep */
	void PerformSubstep(float InSubstepTime, const FVector& Gravity);
	/** Get start and end position for the cable */
	void GetEndPositions(FVector& OutStartPosition, FVector& OutEndPosition);
	/** Amount of time 'left over' from last tick */
	float TimeRemainder;
	/** Array of cable particles */
	TArray<FCableParticle>	Particles;


	friend class FCableSceneProxy;
};


