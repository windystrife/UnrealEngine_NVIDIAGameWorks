// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsSettings.h: Declares the PhysicsSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Templates/Casts.h"
#include "Engine/DeveloperSettings.h"
#include "PhysicsEngine/PhysicsSettingsEnums.h"
#include "PhysicsEngine/BodySetupEnums.h"
#include "PhysicsSettings.generated.h"

/**
 * Structure that represents the name of physical surfaces.
 */
USTRUCT()
struct FPhysicalSurfaceName
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TEnumAsByte<enum EPhysicalSurface> Type;

	UPROPERTY()
	FName Name;

	FPhysicalSurfaceName()
		: Type(SurfaceType_Max)
	{}
	FPhysicalSurfaceName(EPhysicalSurface InType, const FName& InName)
		: Type(InType)
		, Name(InName)
	{}
};

UENUM()
namespace ESettingsDOF
{
	enum Type
	{
		/** Allows for full 3D movement and rotation. */
		Full3D,
		/** Allows 2D movement along the Y-Z plane. */
		YZPlane,
		/** Allows 2D movement along the X-Z plane. */
		XZPlane,
		/** Allows 2D movement along the X-Y plane. */
		XYPlane,
	};
}

UENUM()
namespace ESettingsLockedAxis
{
	enum Type
	{
		/** No axis is locked. */
		None,
		/** Lock movement along the x-axis. */
		X,
		/** Lock movement along the y-axis. */
		Y,
		/** Lock movement along the z-axis. */
		Z,
		/** Used for backwards compatibility. Indicates that we've updated into the new struct. */
		Invalid
	};
}


/**
 * Default physics settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Physics"))
class ENGINE_API UPhysicsSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	/** Default gravity. */
	UPROPERTY(config, EditAnywhere, Category = Constants)
	float DefaultGravityZ;

	/** Default terminal velocity for Physics Volumes. */
	UPROPERTY(config, EditAnywhere, Category = Constants)
	float DefaultTerminalVelocity;
	
	/** Default fluid friction for Physics Volumes. */
	UPROPERTY(config, EditAnywhere, Category = Constants)
	float DefaultFluidFriction;
	
	/** Amount of memory to reserve for PhysX simulate(), this is per pxscene and will be rounded up to the next 16K boundary */
	UPROPERTY(config, EditAnywhere, Category = Constants, meta = (ClampMin = "0", UIMin = "0"))
	int32 SimulateScratchMemorySize;

	/** Threshold for ragdoll bodies above which they will be added to an aggregate before being added to the scene */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "1", UIMin = "1", ClampMax = "127", UIMax = "127"), Category = Constants)
	int32 RagdollAggregateThreshold;

	/** Triangles from triangle meshes (BSP) with an area less than or equal to this value will be removed from physics collision data. Set to less than 0 to disable. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "-1.0", UIMin = "-1.0", ClampMax = "10.0", UIMax = "10.0"), Category = Constants)
	float TriangleMeshTriangleMinAreaThreshold;

	/** Enables the use of an async scene */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category=Simulation)
	bool bEnableAsyncScene;

	/** Enables shape sharing between sync and async scene for static rigid actors */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Simulation, meta = (editcondition="bEnableAsyncScene"))
	bool bEnableShapeSharing;

	/** Enables persistent contact manifolds. This will generate fewer contact points, but with more accuracy. Reduces stability of stacking, but can help energy conservation.*/
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Simulation)
	bool bEnablePCM;

	/** Enables stabilization of contacts for slow moving bodies. This will help improve the stability of stacking.*/
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Simulation)
	bool bEnableStabilization;

	/** Whether to warn when physics locks are used incorrectly. Turning this off is not recommended and should only be used by very advanced users. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Simulation)
	bool bWarnMissingLocks;

	/** Can 2D physics be used (Box2D)? */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	bool bEnable2DPhysics;

	UPROPERTY(config)
	TEnumAsByte<ESettingsLockedAxis::Type> LockedAxis_DEPRECATED;

	/** Useful for constraining all objects in the world, for example if you are making a 2D game using 3D environments.*/
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	TEnumAsByte<ESettingsDOF::Type> DefaultDegreesOfFreedom;
	
	/** Minimum relative velocity required for an object to bounce. A typical value for simulation stability is about 0.2 * gravity*/
	UPROPERTY(config, EditAnywhere, Category = Simulation, meta = (ClampMin = "0", UIMin = "0"))
	float BounceThresholdVelocity;

	/** Friction combine mode, controls how friction is computed for multiple materials. */
	UPROPERTY(config, EditAnywhere, Category=Simulation)
	TEnumAsByte<EFrictionCombineMode::Type> FrictionCombineMode;

	/** Restitution combine mode, controls how restitution is computed for multiple materials. */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	TEnumAsByte<EFrictionCombineMode::Type> RestitutionCombineMode;

	/** Max angular velocity that a simulated object can achieve.*/
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	float MaxAngularVelocity;

	/** Max velocity which may be used to depenetrate simulated physics objects. 0 means no maximum. */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	float MaxDepenetrationVelocity;

	/** Contact offset multiplier. When creating a physics shape we look at its bounding volume and multiply its minimum value by this multiplier. A bigger number will generate contact points earlier which results in higher stability at the cost of performance. */
	UPROPERTY(config, EditAnywhere, Category = Simulation, meta = (ClampMin = "0.001", UIMin = "0.001"))
	float ContactOffsetMultiplier;

	/** Min Contact offset. */
	UPROPERTY(config, EditAnywhere, Category = Simulation, meta = (ClampMin = "0.0001", UIMin = "0.0001"))
	float MinContactOffset;

	/** Max Contact offset. */
	UPROPERTY(config, EditAnywhere, Category = Simulation, meta = (ClampMin = "0.001", UIMin = "0.001"))
	float MaxContactOffset;

	/**
	*  If true, simulate physics for this component on a dedicated server.
	*  This should be set if simulating physics and replicating with a dedicated server.
	*/
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	bool bSimulateSkeletalMeshOnDedicatedServer;

	/**
	*  Determines the default physics shape complexity. */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	TEnumAsByte<ECollisionTraceFlag> DefaultShapeComplexity;
	
	/**
	*  If true, static meshes will use per poly collision as complex collision by default. If false the default behavior is the same as UseSimpleAsComplex. */
	UPROPERTY(config)
	bool bDefaultHasComplexCollision_DEPRECATED;

	/**
	*  If true, the internal physx face to UE face mapping will not be generated. This is a memory optimization available if you do not rely on face indices returned by scene queries. */
	UPROPERTY(config, EditAnywhere, Category = Optimization)
	bool bSuppressFaceRemapTable;

	/** If true, store extra information to allow FindCollisionUV to derive UV info from a line trace hit result, using the FindCollisionUV utility */
	UPROPERTY(config, EditAnywhere, Category = Optimization, meta = (DisplayName = "Support UV From Hit Results", ConfigRestartRequired = true))
	bool bSupportUVFromHitResults;

	/**
	* If true, physx will not update unreal with any bodies that have moved during the simulation. This should only be used if you have no physx simulation or you are manually updating the unreal data via polling physx.  */
	UPROPERTY(config, EditAnywhere, Category = Optimization)
	bool bDisableActiveActors;

	/**
	*  If true CCD will be ignored. This is an optimization when CCD is never used which removes the need for physx to check it internally. */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	bool bDisableCCD;

	/** If set to true, the scene will use enhanced determinism at the cost of a bit more resources. See eENABLE_ENHANCED_DETERMINISM to learn about the specifics */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	bool bEnableEnhancedDeterminism;

	/** Max Physics Delta Time to be clamped. */
	UPROPERTY(config, EditAnywhere, meta=(ClampMin="0.0013", UIMin = "0.0013", ClampMax="1.0", UIMax="1.0"), Category=Framerate)
	float MaxPhysicsDeltaTime;

	/** Whether to substep the physics simulation. This feature is still experimental. Certain functionality might not work correctly*/
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	bool bSubstepping;

	/** Whether to substep the async physics simulation. This feature is still experimental. Certain functionality might not work correctly*/
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	bool bSubsteppingAsync;

	/** Max delta time (in seconds) for an individual simulation substep. */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "0.0013", UIMin = "0.0013", ClampMax = "1.0", UIMax = "1.0", editcondition = "bSubStepping"), Category=Framerate)
	float MaxSubstepDeltaTime;

	/** Max number of substeps for physics simulation. */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "1", UIMin = "1", ClampMax = "16", UIMax = "16", editcondition = "bSubstepping"), Category=Framerate)
	int32 MaxSubsteps;

	/** Physics delta time smoothing factor for sync scene. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Framerate)
	float SyncSceneSmoothingFactor;

	/** Physics delta time smoothing factor for async scene. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Framerate)
	float AsyncSceneSmoothingFactor;

	/** Physics delta time initial average. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0.0013", UIMin = "0.0013", ClampMax = "1.0", UIMax = "1.0"), Category = Framerate)
	float InitialAverageFrameRate;

	/** The number of frames it takes to rebuild the PhysX scene query AABB tree. The bigger the number, the smaller fetchResults takes per frame, but the more the tree deteriorates until a new tree is built */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "4", UIMin = "4"), Category = Framerate)
	int PhysXTreeRebuildRate;

	// PhysicalMaterial Surface Types
	UPROPERTY(config, EditAnywhere, Category=PhysicalSurfaces)
	TArray<FPhysicalSurfaceName> PhysicalSurfaces;

public:

	static UPhysicsSettings* Get() { return CastChecked<UPhysicsSettings>(UPhysicsSettings::StaticClass()->GetDefaultObject()); }

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual bool CanEditChange( const UProperty* Property ) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Load Material Type data from INI file **/
	/** this changes displayname meta data. That means we won't need it outside of editor*/
	void LoadSurfaceType();

#endif // WITH_EDITOR
};
