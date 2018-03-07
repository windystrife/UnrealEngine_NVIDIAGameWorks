// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshParticleVertexFactory.h: Mesh particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "Components.h"
#include "SceneManagement.h"
#include "ParticleVertexFactory.h"
//@todo - parallelrendering - remove once FOneFrameResource no longer needs to be referenced in header

class FMaterial;
class FVertexBuffer;
struct FDynamicReadBuffer;
struct FShaderCompilerEnvironment;

/**
 * Uniform buffer for mesh particle vertex factories.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FMeshParticleUniformParameters, ENGINE_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, SubImageSize )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, TexCoordWeightA )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, TexCoordWeightB )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, PrevTransformAvailable )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( float, DeltaSeconds)
	END_UNIFORM_BUFFER_STRUCT(FMeshParticleUniformParameters)
typedef TUniformBufferRef<FMeshParticleUniformParameters> FMeshParticleUniformBufferRef;

class FMeshParticleInstanceVertices;


/**
 * Vertex factory for rendering instanced mesh particles with out dynamic parameter support.
 */
class ENGINE_API FMeshParticleVertexFactory : public FParticleVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FMeshParticleVertexFactory);
public:

	struct FDataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		TArray<FVertexStreamComponent,TFixedAllocator<MAX_TEXCOORDS> > TextureCoordinates;

		/** The stream to read the vertex  color from. */
		FVertexStreamComponent VertexColorComponent;

		/** The stream to read the vertex  color from. */
		FVertexStreamComponent ParticleColorComponent;

		/** The stream to read the mesh transform from. */
		FVertexStreamComponent TransformComponent[3];

		/** The stream to read the particle velocity from */
		FVertexStreamComponent VelocityComponent;

		/** The stream to read SubUV parameters from.. */
		FVertexStreamComponent SubUVs;

		/** The stream to read SubUV lerp and the particle relative time from */
		FVertexStreamComponent SubUVLerpAndRelTime;

		/** Flag to mark as initialized */
		bool bInitialized;

		FDataType()
		: bInitialized(false)
		{
		}
	};

	class FBatchParametersCPU : public FOneFrameResource
	{
	public:
		const struct FMeshParticleInstanceVertex* InstanceBuffer;
		const struct FMeshParticleInstanceVertexDynamicParameter* DynamicParameterBuffer;
		const struct FMeshParticleInstanceVertexPrevTransform* PrevTransformBuffer;
	};

	/** Default constructor. */
	FMeshParticleVertexFactory(EParticleVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel, int32 InDynamicVertexStride, int32 InDynamicParameterVertexStride)
		: FParticleVertexFactoryBase(InType, InFeatureLevel)
		, DynamicVertexStride(InDynamicVertexStride)
		, DynamicParameterVertexStride(InDynamicParameterVertexStride)
		, InstanceVerticesCPU(nullptr)
	{}

	FMeshParticleVertexFactory()
		: FParticleVertexFactoryBase(PVFT_MAX, ERHIFeatureLevel::Num)
		, DynamicVertexStride(-1)
		, DynamicParameterVertexStride(-1)
		, InstanceVerticesCPU(nullptr)
	{}

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);


	/**
	 * Modify compile environment to enable instancing
	 * @param OutEnvironment - shader compile environment to modify
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FParticleVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		// Set a define so we can tell in MaterialTemplate.usf when we are compiling a mesh particle vertex factory
		OutEnvironment.SetDefine(TEXT("PARTICLE_MESH_FACTORY"),TEXT("1"));
		OutEnvironment.SetDefine(TEXT("PARTICLE_MESH_INSTANCED"),TEXT("1"));
	}
	
	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FDataType& InData);

	/**
	 * Set the uniform buffer for this vertex factory.
	 */
	FORCEINLINE void SetUniformBuffer( const FMeshParticleUniformBufferRef& InMeshParticleUniformBuffer )
	{
		MeshParticleUniformBuffer = InMeshParticleUniformBuffer;
	}

	/**
	 * Retrieve the uniform buffer for this vertex factory.
	 */
	FORCEINLINE FUniformBufferRHIParamRef GetUniformBuffer()
	{
		return MeshParticleUniformBuffer;
	}
	
	/**
	 * Update the data strides (MUST HAPPEN BEFORE InitRHI is called)
	 */
	void SetStrides(int32 InDynamicVertexStride, int32 InDynamicParameterVertexStride)
	{
		ensure(!IsInitialized());
		DynamicVertexStride = InDynamicVertexStride;
		DynamicParameterVertexStride = InDynamicParameterVertexStride;
	}
	
	 /**
	 * Set the source vertex buffer that contains particle instance data.
	 */
	void SetInstanceBuffer(const FVertexBuffer* InstanceBuffer, uint32 StreamOffset, uint32 Stride);

	/**
	 * Set the source vertex buffer that contains particle dynamic parameter data.
	 */
	void SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride);

	uint8* LockPreviousTransformBuffer(uint32 ParticleCount);
	void UnlockPreviousTransformBuffer();
	FShaderResourceViewRHIParamRef GetPreviousTransformBufferSRV() const;

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FMeshParticleVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;

	static bool SupportsTessellationShaders() { return true; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	FMeshParticleInstanceVertices*& GetInstanceVerticesCPU()
	{
		return InstanceVerticesCPU;
	}

protected:
	FDataType Data;
	
	/** Stride information for instanced mesh particles */
	int32 DynamicVertexStride;
	int32 DynamicParameterVertexStride;
	
	/** Uniform buffer with mesh particle parameters. */
	FUniformBufferRHIParamRef MeshParticleUniformBuffer;

	FDynamicReadBuffer PrevTransformBuffer;

	/** Used to remember this in the case that we reuse the same vertex factory for multiple renders . */
	FMeshParticleInstanceVertices* InstanceVerticesCPU;
};


class ENGINE_API FMeshParticleVertexFactoryEmulatedInstancing : public FMeshParticleVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FMeshParticleVertexFactoryEmulatedInstancing);

public:
	FMeshParticleVertexFactoryEmulatedInstancing(EParticleVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel, int32 InDynamicVertexStride, int32 InDynamicParameterVertexStride)
		: FMeshParticleVertexFactory(InType, InFeatureLevel, InDynamicVertexStride, InDynamicParameterVertexStride)
	{}

	FMeshParticleVertexFactoryEmulatedInstancing()
		: FMeshParticleVertexFactory()
	{}

	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return (Platform == SP_OPENGL_ES2_ANDROID) // Android platforms that might not support hardware instancing
			&& FMeshParticleVertexFactory::ShouldCache(Platform, Material, ShaderType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshParticleVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("PARTICLE_MESH_INSTANCED"),TEXT("0"));
	}
};

inline FMeshParticleVertexFactory* ConstructMeshParticleVertexFactory()
{
	if (GRHISupportsInstancing)
	{
		return new FMeshParticleVertexFactory();
	}
	else
	{
		return new FMeshParticleVertexFactoryEmulatedInstancing();
	}
}

inline FMeshParticleVertexFactory* ConstructMeshParticleVertexFactory(EParticleVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel, int32 InDynamicVertexStride, int32 InDynamicParameterVertexStride)
{
	if (GRHISupportsInstancing)
	{
		return new FMeshParticleVertexFactory(InType, InFeatureLevel, InDynamicVertexStride, InDynamicParameterVertexStride);
	}
	else
	{
		return new FMeshParticleVertexFactoryEmulatedInstancing(InType, InFeatureLevel, InDynamicVertexStride, InDynamicParameterVertexStride);
	}
}
