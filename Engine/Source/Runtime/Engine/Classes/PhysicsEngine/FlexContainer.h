// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FlexContainer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFlex, Log, All);

// users, e.g.: emitters, mesh components, should implement this interface to 
// specify their collision bounds, and receive synchronization updates
struct IFlexContainerClient
{
	virtual bool IsEnabled() = 0;
	virtual FBoxSphereBounds GetBounds() = 0;
	virtual void Synchronize() {}
};

/** The Flex asset type */
UENUM()
namespace EFlexSolverRelaxationMode
{
	enum Type
	{
		/** Local relaxtion mode will average constraint deltas per-particle based on the number of constraints, this mode will always converge, but may converge slowly */
		Local,
		/** Global relaxtion mode will apply a global scaling to each constraint delta, if the scale factor is too high then the simulation may fail to converge, or even diverge, but will often converge much faster */
		Global,
	};
}

UCLASS(hidecategories=Object, MinimalAPI, BlueprintType)
class UFlexContainer : public UObject
{
	GENERATED_UCLASS_BODY()

	/** The radius of particles in this container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Flex, meta=(UIMin = "0"))
	float Radius;

	/** The maximum number of particles in this solver. */
	UPROPERTY(EditAnywhere, Category=Flex, meta=(UIMin = "0"))
	int32 MaxParticles;

	/** Enable debug drawing for this container */
	UPROPERTY(EditAnywhere, Category=Flex)
	bool DebugDraw;

	/** Number of solver iterations to perform per-substep */
	UPROPERTY(EditAnywhere, Category=Simulation, meta=(UIMin = "1"))
    int32 NumIterations;

	/** Number of sub-steps to take, each sub-step will perform NumIterations constraint iterations. Increasing sub-steps is generally more expensive than taking more solver iterations, but can be more effective at increasing stability. */
	UPROPERTY(EditAnywhere, Category=Simulation, meta=(UIMin = "1"))
    int32 NumSubsteps;

	/** Controls the minimum frame-rate that Flex will attempt to sub-step, any time-steps from the game are clamped to this minimum.
	 *  Setting this lower will result in more sub-steps being taken so it should be set as high as possible, (although the simulation 
	 *  will appear to run slower than real-time if the game cannot maintain this frame rate).
	 */
	UPROPERTY(EditAnywhere, Category=Simulation)
	int32 MinFrameRate;

	/**
	 * If true then each sub-step uses a fixed timestep = 1/(NumSubsteps*60) seconds and will take multiple sub-steps if necessary. 
	 *  If this value is false then each substep will use the variable game's dt/NumSubsteps and will take NumSubsteps steps. 
	 *  It is highly recommended to leave FixedTimeStep enabled for improved behaviour and stability. 
	 */
	UPROPERTY(EditAnywhere, Category=Simulation)
    bool FixedTimeStep;

	/** Physics delta time smoothing factor. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Simulation, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float TimeStepSmoothingFactor;

	/** Constant acceleration applied to all particles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Simulation)
    FVector Gravity;

	/** Particles with a velocity magnitude < this threshold will be considered fixed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Simulation)
	float SleepThreshold;
    
	/** Particle velocity will be clamped to this value at the end of each step */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Simulation)
	float MaxVelocity;                 
   
	/** Clamp the maximum bound for this container to prevent crashes if flex particles move too far away*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Simulation)
	float MaxContainerBound;  

	/** The mode used for constraint relaxation */
	UPROPERTY(EditAnywhere, Category=Simulation)
	TEnumAsByte<EFlexSolverRelaxationMode::Type> RelaxationMode;

	/** Control the convergence rate of the parallel solver, for global relaxation values < 1.0 should be used, e.g: (0.25), high values will converge faster but may cause divergence */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Simulation)
	float RelaxationFactor;	

	/** Viscous damping applied to all particles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Simulation, meta = (UIMin = "0"))
	float Damping;

	/** If true then particles will collide with complex collision shapes */
	UPROPERTY(EditAnywhere, Category=Collision)
    bool ComplexCollision;

	/** Enum indicating what type of object this should be considered as when it moves */
	UPROPERTY(EditAnywhere, Category=Collision)
	TEnumAsByte<enum ECollisionChannel> ObjectType;

	/** Custom Channels for Responses*/
	UPROPERTY(EditAnywhere, Category=Collision)
	struct FCollisionResponseContainer	ResponseToChannels;
	
	/** Distance particles maintain against shapes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Collision)
    float CollisionDistance;

	/** Increases the radius used during neighbor finding, this is useful if particles are expected to move significantly during a single step to ensure contacts aren't missed on subsequent iterations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Collision, AdvancedDisplay)
	float CollisionMarginParticles;

	/** Increases the radius used during contact finding against kinematic shapes, this is useful if particles are expected to move significantly during a single step to ensure contacts aren't missed on subsequent iterations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Collision, AdvancedDisplay)
	float CollisionMarginShapes;

    /** Use the merged bounds of all Flex actors to query for collision components, this can be more efficient than querying each actor's bounds if actors are typically in close proximity*/
    UPROPERTY(EditAnywhere, Category = Collision, AdvancedDisplay)
    bool bUseMergedBounds;

	/** Coefficient of friction used when colliding against shapes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Collision)
    float ShapeFriction;

	/** Multiplier for friction of particles against other particles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Collision)
	float ParticleFriction;
	
	/** Coefficient of restitution used when colliding against shapes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Collision)
    float Restitution;                 

	/** Control how strongly particles stick to surfaces they hit, affects both fluid and non-fluid particles, default 0.0, range [0.0, +inf] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Collision)
	float Adhesion;
    
	/** Artificially decrease the mass of particles based on height from a fixed reference point, this makes stacks and piles converge faster */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Collision, AdvancedDisplay)
	float ShockPropagation;
    
	/** Damp particle velocity based on how many particle contacts it has */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Collision)
	float Dissipation;
	
    /** Constant acceleration applied to particles that belong to dynamic triangles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Cloth)
    FVector Wind;

	/** Drag force applied to particles belonging to dynamic triangles, proportional to velocity^2*area in the negative velocity direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Cloth)
    float Drag;

	/** Lift force applied to particles belonging to dynamic triangles, proportional to velocity^2*area in the direction perpendicular to velocity and (if possible), parallel to the plane normal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Cloth)
    float Lift;

	/** If true, particles with phase 0 are considered fluid particles and interact using the position based fluids method */
    UPROPERTY(EditAnywhere, Category=Fluid)
    bool Fluid;                        
	
	/** Controls the distance fluid particles are spaced at the rest density, the absolute distance is given by this value*radius, must be in the range (0, 1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Fluid, meta=(editcondition="Fluid"))
	float RestDistance;                
       
	/** Control how strongly particles hold each other together, default: 0.025, range [0.0, +inf] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Fluid, meta=(editcondition="Fluid"))
	float Cohesion;
    
	/** Controls how strongly particles attempt to minimize surface area, default: 0.0, range: [0.0, +inf]  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Fluid, meta=(editcondition="Fluid"))
	float SurfaceTension;
    
	/** Smoothes particle velocities using XSPH viscosity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Fluid, meta=(editcondition="Fluid"))
	float Viscosity;
    
	/** Increases vorticity by applying rotational forces to particles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Fluid, meta=(editcondition="Fluid"))
	float VorticityConfinement;
    
	/** Add pressure from solid surfaces to particles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Fluid, meta=(editcondition="Fluid"))
	float SolidPressure;
	
	/** Anisotropy scale for ellipsoid surface generation, default 0.0 disables anisotropy computation.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Fluid, meta = (editcondition = "Fluid"))
	float AnisotropyScale;

	/** Anisotropy minimum scale, this is specified as a fraction of the particle radius, the scale of the particle will be clamped to this minimum in each direction.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Fluid, meta = (editcondition = "Fluid"))
	float AnisotropyMin;

	/** Anisotropy maximum scale, this is specified as a fraction of the particle radius, the scale of the particle will be clamped to this minimum in each direction.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Fluid, meta = (editcondition = "Fluid"))
	float AnisotropyMax;

	/** Scales smoothing of particle positions for surface rendering, default 0.0 disables smoothing.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Fluid, meta = (editcondition = "Fluid"))
	float PositionSmoothing;

	/** Drag force applied to boundary fluid particles */
	UPROPERTY()
	float FreeSurfaceDrag;

	/** Particles belonging to rigid shapes that move with a position delta magnitude > threshold will be permanently deformed in the rest pose */
    UPROPERTY()
    float PlasticThreshold;            
	
	/** Controls the rate at which particles in the rest pose are deformed for particles passing the deformation threshold */
	UPROPERTY()
    float PlasticCreep;
};

