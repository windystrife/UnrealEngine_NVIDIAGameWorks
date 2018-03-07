// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FXSystemPrivate.h: Internal effects system interface.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "FXSystem.h"
#include "VectorField.h"

class FCanvas;
class FGlobalDistanceFieldParameterData;
class FParticleSimulationGPU;
class FParticleSimulationResources;
class UVectorFieldComponent;
struct FGPUSpriteEmitterInfo;
struct FParticleEmitterInstance;

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/

/** An individual particle simulation taking place on the GPU. */
class FParticleSimulationGPU;
/** Resources used for particle simulation. */
class FParticleSimulationResources;

namespace EParticleSimulatePhase
{
	enum Type
	{
		/** The main simulation pass is for standard particles. */
		Main,
		CollisionDistanceField,
		/** The collision pass is used by these that collide against the scene depth buffer. */
		CollisionDepthBuffer,

		/**********************************************************************/

		/** The first simulation phase that is run each frame. */
		First = Main,
		/** The final simulation phase that is run each frame. */
		Last = CollisionDepthBuffer
	};
};

enum EParticleCollisionShaderMode
{
	PCM_None,
	PCM_DepthBuffer,
	PCM_DistanceField
};

/** Helper function to determine whether the given particle collision shader mode is supported on the given shader platform */
inline bool IsParticleCollisionModeSupported(EShaderPlatform InPlatform, EParticleCollisionShaderMode InCollisionShaderMode)
{
	switch (InCollisionShaderMode)
	{
	case PCM_None:
		return IsFeatureLevelSupported(InPlatform, ERHIFeatureLevel::ES2);
	case PCM_DepthBuffer:
		return IsFeatureLevelSupported(InPlatform, ERHIFeatureLevel::SM4) && !IsSimpleForwardShadingEnabled(InPlatform);
	case PCM_DistanceField:
		return IsFeatureLevelSupported(InPlatform, ERHIFeatureLevel::SM5);
	}
	check(0);
	return IsFeatureLevelSupported(InPlatform, ERHIFeatureLevel::SM4);
}

inline EParticleSimulatePhase::Type GetLastParticleSimulationPhase(EShaderPlatform InPlatform)
{
	return (IsParticleCollisionModeSupported(InPlatform, PCM_DepthBuffer) ? EParticleSimulatePhase::Last : EParticleSimulatePhase::Main);
}

/*-----------------------------------------------------------------------------
Injecting particles in to the GPU for simulation.
-----------------------------------------------------------------------------*/

/**
* Data passed to the GPU to inject a new particle in to the simulation.
*/
struct FNewParticle
{
	/** The initial position of the particle. */
	FVector Position;
	/** The relative time of the particle. */
	float RelativeTime;
	/** The initial velocity of the particle. */
	FVector Velocity;
	/** The time scale for the particle. */
	float TimeScale;
	/** Initial size of the particle. */
	FVector2D Size;
	/** Initial rotation of the particle. */
	float Rotation;
	/** Relative rotation rate of the particle. */
	float RelativeRotationRate;
	/** Coefficient of drag. */
	float DragCoefficient;
	/** Per-particle vector field scale. */
	float VectorFieldScale;
	/** Resilience for collision. */
	union
	{
		float Resilience;
		int32 AllocatedTileIndex;
	} ResilienceAndTileIndex;
	/** Random selection of orbit attributes. */
	float RandomOrbit;
	/** The offset at which to inject the new particle. */
	FVector2D Offset;
};

/*-----------------------------------------------------------------------------
	FX system declaration.
-----------------------------------------------------------------------------*/

/**
 * FX system.
 */
class FFXSystem : public FFXSystemInterface
{
public:

	/** Default constructoer. */
	FFXSystem(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform);

	/** Destructor. */
	virtual ~FFXSystem();

	// Begin FFXSystemInterface.
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void Suspend() override;
	virtual void Resume() override;
#endif // #if WITH_EDITOR
	virtual void DrawDebug(FCanvas* Canvas) override;
	virtual void AddVectorField(UVectorFieldComponent* VectorFieldComponent) override;
	virtual void RemoveVectorField(UVectorFieldComponent* VectorFieldComponent) override;
	virtual void UpdateVectorField(UVectorFieldComponent* VectorFieldComponent) override;
	virtual FParticleEmitterInstance* CreateGPUSpriteEmitterInstance(FGPUSpriteEmitterInfo& EmitterInfo) override;
	virtual void PreInitViews() override;
	virtual bool UsesGlobalDistanceField() const override;
	virtual void PreRender(FRHICommandListImmediate& RHICmdList, const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData) override;
	virtual void PostRenderOpaque(FRHICommandListImmediate& RHICmdList, const FUniformBufferRHIParamRef ViewUniformBuffer, FTexture2DRHIParamRef SceneDepthTexture, FTexture2DRHIParamRef GBufferATexture) override;
	// End FFXSystemInterface.

	/*--------------------------------------------------------------------------
		Internal interface for GPU simulation.
	--------------------------------------------------------------------------*/
	/**
	 * Retrieve feature level that this FXSystem was created for
	 */
	ERHIFeatureLevel::Type GetFeatureLevel() const { return FeatureLevel; }

	/**
	 * Retrieve shaderplatform that this FXSystem was created for
	 */
	EShaderPlatform GetShaderPlatform() const { return ShaderPlatform; }

	/**
	 * Add a new GPU simulation to the system.
	 * @param Simulation The GPU simulation to add.
	 */
	void AddGPUSimulation(FParticleSimulationGPU* Simulation);

	/**
	 * Remove an existing GPU simulation to the system.
	 * @param Simulation The GPU simulation to remove.
	 */
	void RemoveGPUSimulation(FParticleSimulationGPU* Simulation);

	/**
	 * Retrieve GPU particle rendering resources.
	 */
	FParticleSimulationResources* GetParticleSimulationResources()
	{
		return ParticleSimulationResources;
	}

	/**
	 * Prepares a GPU simulation to be sorted for a particular view.
	 * @param Simulation The simulation to be sorted.
	 * @param ViewOrigin The origin of the view from which to sort.
	 * @returns an offset in to the sorted buffer from which the simulation may render.
	 */
	int32 AddSortedGPUSimulation(FParticleSimulationGPU* Simulation, const FVector& ViewOrigin);

	void PrepareGPUSimulation(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SceneDepthTexture = nullptr);
	void FinalizeGPUSimulation(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SceneDepthTexture = nullptr);

private:

	/*--------------------------------------------------------------------------
		Private interface for GPU simulations.
	--------------------------------------------------------------------------*/

	/**
	 * Initializes GPU simulation for this system.
	 */
	void InitGPUSimulation();

	/**
	 * Destroys any resources allocated for GPU simulation for this system.
	 */
	void DestroyGPUSimulation();

	/**
	 * Initializes GPU resources.
	 */
	void InitGPUResources();

	/**
	 * Releases GPU resources.
	 */
	void ReleaseGPUResources();

	/**
	 * Prepares GPU particles for simulation and rendering in the next frame.
	 */
	void AdvanceGPUParticleFrame();

	/**
	 * Sorts all GPU particles that have called AddSortedGPUSimulation since the
	 * last reset.
	 */
	void SortGPUParticles(FRHICommandListImmediate& RHICmdList);

	bool UsesGlobalDistanceFieldInternal() const;

	/**
	* Updates resources used in a multi-GPU context
	*/
	void UpdateMultiGPUResources(FRHICommandListImmediate& RHICmdList);

	/**
	 * Update particles simulated on the GPU.
	 * @param Phase				Which emitters are being simulated.
	 * @param CollisionView		View to be used for collision checks.
	 * @param SceneDepthTexture Depth texture to use for collision checks.
	 * @param GBufferATexture	GBuffer texture containing the world normal.
	 */
	void SimulateGPUParticles(
		FRHICommandListImmediate& RHICmdList,
		EParticleSimulatePhase::Type Phase,
		const FUniformBufferRHIParamRef ViewUniformBuffer,
		const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
		FTexture2DRHIParamRef SceneDepthTexture,
		FTexture2DRHIParamRef GBufferATexture
		);

	/**
	 * Visualizes the current state of GPU particles.
	 * @param Canvas The canvas on which to draw the visualization.
	 */
	void VisualizeGPUParticles(FCanvas* Canvas);

private:

	/*-------------------------------------------------------------------------
		GPU simulation state.
	-------------------------------------------------------------------------*/

	/** List of all vector field instances. */
	FVectorFieldInstanceList VectorFields;
	/** List of all active GPU simulations. */
	TSparseArray<FParticleSimulationGPU*> GPUSimulations;
	/** Particle render resources. */
	FParticleSimulationResources* ParticleSimulationResources;
	/** Feature level of this effects system */
	ERHIFeatureLevel::Type FeatureLevel;
	/** Shader platform that will be rendering this effects system */
	EShaderPlatform ShaderPlatform;

	/** Previous frame new particles for multi-gpu simulation*/
	TArray<FNewParticle> LastFrameNewParticles;

#if WITH_EDITOR
	/** true if the system has been suspended. */
	bool bSuspended;
#endif // #if WITH_EDITOR
};
