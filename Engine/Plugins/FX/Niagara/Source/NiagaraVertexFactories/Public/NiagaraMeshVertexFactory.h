// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.h: Particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "NiagaraVertexFactory.h"
#include "NiagaraDataSet.h"
#include "SceneView.h"
#include "Components.h"
#include "SceneManagement.h"


class FMaterial;
class FVertexBuffer;
struct FDynamicReadBuffer;
struct FShaderCompilerEnvironment;

// Per-particle data sent to the GPU.
struct FNiagaraMeshInstanceVertex
{
	/** The color of the particle. */
	FLinearColor Color;

	/** The instance to world transform of the particle. Translation vector is packed into W components. */
	FVector4 Transform[3];

	/** The velocity of the particle, XYZ: direction, W: speed. */
	FVector4 Velocity;

	/** The sub-image texture offsets for the particle. */
	int16 SubUVParams[4];

	/** The sub-image lerp value for the particle. */
	float SubUVLerp;

	/** The relative time of the particle. */
	float RelativeTime;
};

struct FNiagaraMeshInstanceVertexDynamicParameter
{
	/** The dynamic parameter of the particle. */
	float DynamicValue[4];
};

struct FNiagaraMeshInstanceVertexPrevTransform
{
	FVector4 PrevTransform0;
	FVector4 PrevTransform1;
	FVector4 PrevTransform2;
};




/**
* Uniform buffer for mesh particle vertex factories.
*/
BEGIN_UNIFORM_BUFFER_STRUCT(FNiagaraMeshUniformParameters, NIAGARAVERTEXFACTORIES_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, SubImageSize)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, TexCoordWeightA)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, TexCoordWeightB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, PrevTransformAvailable)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, DeltaSeconds)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, PositionDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, VelocityDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, ColorDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, TransformDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, ScaleDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, SizeDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, MaterialParamDataOffset)

END_UNIFORM_BUFFER_STRUCT(FNiagaraMeshUniformParameters)
typedef TUniformBufferRef<FNiagaraMeshUniformParameters> FNiagaraMeshUniformBufferRef;

class FNiagaraMeshInstanceVertices;


/**
* Vertex factory for rendering instanced mesh particles with out dynamic parameter support.
*/
class NIAGARAVERTEXFACTORIES_API FNiagaraMeshVertexFactory : public FNiagaraVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FNiagaraMeshVertexFactory);
public:

	// TODO get rid of the unnecessary components here when streams are no longer necessary
	struct FDataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		TArray<FVertexStreamComponent, TFixedAllocator<MAX_TEXCOORDS> > TextureCoordinates;

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
		const struct FNiagaraMeshInstanceVertex* InstanceBuffer;
		const struct FNiagaraMeshInstanceVertexDynamicParameter* DynamicParameterBuffer;
		const struct FNiagaraMeshInstanceVertexPrevTransform* PrevTransformBuffer;
	};

	/** Default constructor. */
	FNiagaraMeshVertexFactory(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel, int32 InDynamicVertexStride, int32 InDynamicParameterVertexStride)
		: FNiagaraVertexFactoryBase(InType, InFeatureLevel)
		, DynamicVertexStride(InDynamicVertexStride)
		, DynamicParameterVertexStride(InDynamicParameterVertexStride)
		, InstanceVerticesCPU(nullptr)
	{}

	FNiagaraMeshVertexFactory()
		: FNiagaraVertexFactoryBase(NVFT_MAX, ERHIFeatureLevel::Num)
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
		FNiagaraVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		// Set a define so we can tell in MaterialTemplate.usf when we are compiling a mesh particle vertex factory
		OutEnvironment.SetDefine(TEXT("NIAGARA_MESH_FACTORY"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("NIAGARA_MESH_INSTANCED"), TEXT("1"));
	}

	void SetParticleData(const FNiagaraDataSet *InDataSet)
	{
		DataSet = InDataSet;
	}

	inline FShaderResourceViewRHIParamRef GetFloatDataSRV() const
	{
#if PLATFORM_PS4
		return DataSet->PrevData().GetGPUBufferFloat()->SRV;
#else
		check(IsInRenderingThread());
		return DataSet->PrevDataRender().GetGPUBufferFloat()->SRV;
#endif
	}
	inline FShaderResourceViewRHIParamRef GetIntDataSRV() const
	{
#if PLATFORM_PS4
		return DataSet->PrevData().GetGPUBufferInt()->SRV;
#else
		check(IsInRenderingThread());
		return DataSet->PrevDataRender().GetGPUBufferInt()->SRV;
#endif
	}

	uint32 GetComponentBufferSize()
	{
#if PLATFORM_PS4
		return DataSet->PrevData().GetFloatStride() / sizeof(float);
#else
		check(IsInRenderingThread());
		return DataSet->PrevDataRender().GetFloatStride() / sizeof(float);
#endif
	}

	/**
	* An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	*/
	void SetData(const FDataType& InData);

	/**
	* Set the uniform buffer for this vertex factory.
	*/
	FORCEINLINE void SetUniformBuffer(const FNiagaraMeshUniformBufferRef& InMeshParticleUniformBuffer)
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
	void Copy(const FNiagaraMeshVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;

	static bool SupportsTessellationShaders() { return true; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	FNiagaraMeshInstanceVertices*& GetInstanceVerticesCPU()
	{
		return InstanceVerticesCPU;
	}

protected:
	FDataType Data;
	const FNiagaraDataSet *DataSet;
	/** Stride information for instanced mesh particles */
	int32 DynamicVertexStride;
	int32 DynamicParameterVertexStride;

	/** Uniform buffer with mesh particle parameters. */
	FUniformBufferRHIParamRef MeshParticleUniformBuffer;

	FDynamicReadBuffer PrevTransformBuffer;

	/** Used to remember this in the case that we reuse the same vertex factory for multiple renders . */
	FNiagaraMeshInstanceVertices* InstanceVerticesCPU;
};


class NIAGARAVERTEXFACTORIES_API FNiagaraMeshVertexFactoryEmulatedInstancing : public FNiagaraMeshVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FMeshParticleVertexFactoryEmulatedInstancing);

public:
	FNiagaraMeshVertexFactoryEmulatedInstancing(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel, int32 InDynamicVertexStride, int32 InDynamicParameterVertexStride)
		: FNiagaraMeshVertexFactory(InType, InFeatureLevel, InDynamicVertexStride, InDynamicParameterVertexStride)
	{}

	FNiagaraMeshVertexFactoryEmulatedInstancing()
		: FNiagaraMeshVertexFactory()
	{}

	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return (Platform == SP_OPENGL_ES2_ANDROID || Platform == SP_OPENGL_ES2_WEBGL) // Those are only platforms that might not support hardware instancing
			&& FNiagaraMeshVertexFactory::ShouldCache(Platform, Material, ShaderType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNiagaraMeshVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("PARTICLE_MESH_INSTANCED"), TEXT("0"));
	}
};

inline FNiagaraMeshVertexFactory* ConstructNiagaraMeshVertexFactory()
{
	if (GRHISupportsInstancing)
	{
		return new FNiagaraMeshVertexFactory();
	}
	else
	{
		return new FNiagaraMeshVertexFactoryEmulatedInstancing();
	}
}

inline FNiagaraMeshVertexFactory* ConstructNiagaraMeshVertexFactory(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel, int32 InDynamicVertexStride, int32 InDynamicParameterVertexStride)
{
	if (GRHISupportsInstancing)
	{
		return new FNiagaraMeshVertexFactory(InType, InFeatureLevel, InDynamicVertexStride, InDynamicParameterVertexStride);
	}
	else
	{
		return new FNiagaraMeshVertexFactoryEmulatedInstancing(InType, InFeatureLevel, InDynamicVertexStride, InDynamicParameterVertexStride);
	}
}
