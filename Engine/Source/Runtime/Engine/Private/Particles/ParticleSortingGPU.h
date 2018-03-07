// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleSortingGPU.h: Interface for sorting GPU particles.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"

struct FGPUSortBuffers;

/**
 * Buffers in GPU memory used to sort particles.
 */
class FParticleSortBuffers : public FRenderResource
{
public:

	/** Initialization constructor. */
	explicit FParticleSortBuffers(int32 InBufferSize)
		: BufferSize(InBufferSize)
	{
	}

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override;

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override;

	/**
	 * Retrieve the UAV for writing particle sort keys.
	 */
	FUnorderedAccessViewRHIParamRef GetKeyBufferUAV()
	{
		return KeyBufferUAVs[0];
	}

	/**
	 * Retrieve the UAV for writing particle vertices.
	 */
	FUnorderedAccessViewRHIParamRef GetVertexBufferUAV()
	{
		return VertexBufferUAVs[0];
	}

	/**
	 * Retrieve buffers needed to sort on the GPU.
	 */
	FGPUSortBuffers GetSortBuffers();

	/**
	 * Retrieve the sorted vertex buffer at the given index.
	 */
	FVertexBufferRHIParamRef GetSortedVertexBufferRHI(int32 BufferIndex)
	{
		check((BufferIndex & 0xFFFFFFFE) == 0);
		return VertexBuffers[BufferIndex];
	}

	/**
	 * Retrieve the SRV for the sorted vertex buffer at the given index.
	 */
	FShaderResourceViewRHIParamRef GetSortedVertexBufferSRV(int32 BufferIndex)
	{
		check((BufferIndex & 0xFFFFFFFE) == 0);
		return VertexBufferSRVs[BufferIndex];
	}

	/**
	 * Get the size allocated for sorted vertex buffers.
	 */
	int32 GetSize() { return BufferSize; }

private:

	/** Vertex buffer storage for particle sort keys. */
	FVertexBufferRHIRef KeyBuffers[2];
	/** Shader resource view for particle sort keys. */
	FShaderResourceViewRHIRef KeyBufferSRVs[2];
	/** Unordered access view for particle sort keys. */
	FUnorderedAccessViewRHIRef KeyBufferUAVs[2];

	/** Vertex buffer containing sorted particle vertices. */
	FVertexBufferRHIRef VertexBuffers[2];
	/** Shader resource view for reading particle vertices out of the sorting buffer. */
	FShaderResourceViewRHIRef VertexBufferSRVs[2];
	/** Unordered access view for writing particle vertices in to the sorting buffer. */
	FUnorderedAccessViewRHIRef VertexBufferUAVs[2];
	/** Shader resource view for sorting particle vertices. */
	FShaderResourceViewRHIRef VertexBufferSortSRVs[2];
	/** Unordered access view for sorting particle vertices. */
	FUnorderedAccessViewRHIRef VertexBufferSortUAVs[2];

	/** Size allocated for buffers. */
	int32 BufferSize;
};

/**
 * The information required to sort particles belonging to an individual simulation.
 */
struct FParticleSimulationSortInfo
{
	/** Vertex buffer containing indices in to the particle state texture. */
	FShaderResourceViewRHIParamRef VertexBufferSRV;
	/** World space position from which to sort. */
	FVector ViewOrigin;
	/** The number of particles in the simulation. */
	uint32 ParticleCount;
};

/**
 * Sort particles on the GPU.
 * @param ParticleSortBuffers - Buffers to use while sorting GPU particles.
 * @param PositionTextureRHI - Texture containing world space position for all particles.
 * @param SimulationsToSort - A list of simulations that must be sorted.
 * @returns the buffer index in which sorting results are stored.
 */
int32 SortParticlesGPU(
	FRHICommandListImmediate& RHICmdList,
	FParticleSortBuffers& ParticleSortBuffers,
	FTexture2DRHIParamRef PositionTextureRHI,
	const TArray<FParticleSimulationSortInfo>& SimulationsToSort,
	ERHIFeatureLevel::Type FeatureLevel
	);
