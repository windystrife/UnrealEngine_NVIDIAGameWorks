// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleBeamTrailVertexFactory.h: Shared Particle Beam and Trail vertex 
										factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "ParticleVertexFactory.h"

class FMaterial;

/**
 * Uniform buffer for particle beam/trail vertex factories.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FParticleBeamTrailUniformParameters, ENGINE_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, CameraRight )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, CameraUp )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, ScreenAlignment )
END_UNIFORM_BUFFER_STRUCT( FParticleBeamTrailUniformParameters )
typedef TUniformBufferRef<FParticleBeamTrailUniformParameters> FParticleBeamTrailUniformBufferRef;

/**
 * Beam/Trail particle vertex factory.
 */
class ENGINE_API FParticleBeamTrailVertexFactory : public FParticleVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FParticleBeamTrailVertexFactory);

public:

	/** Default constructor. */
	FParticleBeamTrailVertexFactory( EParticleVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel )
		: FParticleVertexFactoryBase(InType, InFeatureLevel)
		, IndexBuffer(nullptr)
		, FirstIndex(0)
		, OutTriangleCount(0)
		, bUsesDynamicParameter(true)
	{}

	FParticleBeamTrailVertexFactory()
		: FParticleVertexFactoryBase(PVFT_MAX, ERHIFeatureLevel::Num)
		, IndexBuffer(nullptr)
		, FirstIndex(0)
		, OutTriangleCount(0)
		, bUsesDynamicParameter(true)
	{}

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	// FRenderResource interface.
	virtual void InitRHI() override;

	/**
	 * Set the uniform buffer for this vertex factory.
	 */
	FORCEINLINE void SetBeamTrailUniformBuffer( FParticleBeamTrailUniformBufferRef InSpriteUniformBuffer )
	{
		BeamTrailUniformBuffer = InSpriteUniformBuffer;
	}

	/**
	 * Retrieve the uniform buffer for this vertex factory.
	 */
	FORCEINLINE FParticleBeamTrailUniformBufferRef GetBeamTrailUniformBuffer()
	{
		return BeamTrailUniformBuffer;
	}	
	
	/**
	 * Set the source vertex buffer.
	 */
	void SetVertexBuffer(const FVertexBuffer* InBuffer, uint32 StreamOffset, uint32 Stride);

	/**
	 * Set the source vertex buffer that contains particle dynamic parameter data.
	 */
	void SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride);
	inline void SetUsesDynamicParameter(bool bInUsesDynamicParameter)
	{
		bUsesDynamicParameter = bInUsesDynamicParameter;
	}

	/**
	 * Construct shader parameters for this type of vertex factory.
	 */
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	FIndexBuffer*& GetIndexBuffer()
	{
		return IndexBuffer;
	}

	uint32& GetFirstIndex()
	{
		return FirstIndex;
	}

	int32& GetOutTriangleCount()
	{
		return OutTriangleCount;
	}

private:

	/** Uniform buffer with beam/trail parameters. */
	FParticleBeamTrailUniformBufferRef BeamTrailUniformBuffer;

	/** Used to hold the index buffer allocation information when we call GDME more than once per frame. */
	FIndexBuffer* IndexBuffer;
	uint32 FirstIndex;
	int32 OutTriangleCount;
	bool bUsesDynamicParameter;
};