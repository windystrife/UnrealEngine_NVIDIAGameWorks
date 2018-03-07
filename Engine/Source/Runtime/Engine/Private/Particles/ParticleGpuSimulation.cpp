// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	ParticleGpuSimulation.cpp: Implementation of GPU particle simulation.
==============================================================================*/

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "Math/RandomStream.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "HAL/IConsoleManager.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "VertexFactory.h"
#include "RHIStaticStates.h"
#include "GlobalDistanceFieldParameters.h"
#include "StaticBoundShaderState.h"
#include "Materials/Material.h"
#include "ParticleVertexFactory.h"
#include "SceneUtils.h"
#include "SceneManagement.h"
#include "ParticleHelper.h"
#include "ParticleEmitterInstances.h"
#include "Particles/ParticleSystemComponent.h"
#include "VectorField.h"
#include "CanvasTypes.h"
#include "Particles/FXSystemPrivate.h"
#include "Particles/ParticleSortingGPU.h"
#include "Particles/ParticleCurveTexture.h"
#include "ParticleResources.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"
#include "VectorFieldVisualization.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Spawn/ParticleModuleSpawnPerUnit.h"
#include "Particles/TypeData/ParticleModuleTypeDataGpu.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "VectorField/VectorField.h"
#include "CoreDelegates.h"
#include "PipelineStateCache.h"

// NvFlow begin
#define NV_FLOW_WITH_GPU_PARTICLES 1

#if NV_FLOW_WITH_GPU_PARTICLES
#include "GridAccessHooksNvFlow.h"
#endif
// NvFlow end

DECLARE_CYCLE_STAT(TEXT("GPUSpriteEmitterInstance Init"), STAT_GPUSpriteEmitterInstance_Init, STATGROUP_Particles);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Particle Simulation"), Stat_GPU_ParticleSimulation, STATGROUP_GPU);

/*------------------------------------------------------------------------------
	Constants to tune memory and performance for GPU particle simulation.
------------------------------------------------------------------------------*/

/** Enable this define to permit tracking of tile allocations by GPU emitters. */
#define TRACK_TILE_ALLOCATIONS 0

/** The texture size allocated for GPU simulation. */
extern const int32 GParticleSimulationTextureSizeX = 1024;
extern const int32 GParticleSimulationTextureSizeY = 1024;

/** Texture size must be power-of-two. */
static_assert((GParticleSimulationTextureSizeX & (GParticleSimulationTextureSizeX - 1)) == 0, "Particle simulation texture size X is not a power of two.");
static_assert((GParticleSimulationTextureSizeY & (GParticleSimulationTextureSizeY - 1)) == 0, "Particle simulation texture size Y is not a power of two.");

/** The tile size. Texture space is allocated in TileSize x TileSize units. */
const int32 GParticleSimulationTileSize = 4;
const int32 GParticlesPerTile = GParticleSimulationTileSize * GParticleSimulationTileSize;

/** Tile size must be power-of-two and <= each dimension of the simulation texture. */
static_assert((GParticleSimulationTileSize & (GParticleSimulationTileSize - 1)) == 0, "Particle simulation tile size is not a power of two.");
static_assert(GParticleSimulationTileSize <= GParticleSimulationTextureSizeX, "Particle simulation tile size is larger than texture.");
static_assert(GParticleSimulationTileSize <= GParticleSimulationTextureSizeY, "Particle simulation tile size is larger than texture.");

/** How many tiles are in the simulation textures. */
const int32 GParticleSimulationTileCountX = GParticleSimulationTextureSizeX / GParticleSimulationTileSize;
const int32 GParticleSimulationTileCountY = GParticleSimulationTextureSizeY / GParticleSimulationTileSize;
const int32 GParticleSimulationTileCount = GParticleSimulationTileCountX * GParticleSimulationTileCountY;

/** GPU particle rendering code assumes that the number of particles per instanced draw is <= 16. */
static_assert(MAX_PARTICLES_PER_INSTANCE <= 16, "Max particles per instance is greater than 16.");
/** Also, it must be a power of 2. */
static_assert((MAX_PARTICLES_PER_INSTANCE & (MAX_PARTICLES_PER_INSTANCE - 1)) == 0, "Max particles per instance is not a power of two.");

/** Particle tiles are aligned to the same number as when rendering. */
enum { TILES_PER_INSTANCE = 8 };
/** The number of tiles per instance must be <= MAX_PARTICLES_PER_INSTANCE. */
static_assert(TILES_PER_INSTANCE <= MAX_PARTICLES_PER_INSTANCE, "Tiles per instance is greater than max particles per instance.");
/** Also, it must be a power of 2. */
static_assert((TILES_PER_INSTANCE & (TILES_PER_INSTANCE - 1)) == 0, "Tiles per instance is not a power of two.");

/** Maximum number of vector fields that can be evaluated at once. */
enum { MAX_VECTOR_FIELDS = 4 };

// Using a fix step 1/30, allows game targetting 30 fps and 60 fps to have single iteration updates.
static TAutoConsoleVariable<float> CVarGPUParticleFixDeltaSeconds(TEXT("r.GPUParticle.FixDeltaSeconds"), 1.f/30.f,TEXT("GPU particle fix delta seconds."));
static TAutoConsoleVariable<float> CVarGPUParticleFixTolerance(TEXT("r.GPUParticle.FixTolerance"),.1f,TEXT("Delta second tolerance before switching to a fix delta seconds."));
static TAutoConsoleVariable<int32> CVarGPUParticleMaxNumIterations(TEXT("r.GPUParticle.MaxNumIterations"),3,TEXT("Max number of iteration when using a fix delta seconds."));

static TAutoConsoleVariable<int32> CVarSimulateGPUParticles(TEXT("r.GPUParticle.Simulate"), 1, TEXT("Enable or disable GPU particle simulation"));

static TAutoConsoleVariable<int32> CVarGPUParticleAFRReinject(
	TEXT("r.GPUParticle.AFRReinject"),
	1,
	TEXT("Toggle optimization when running in AFR to re-inject particle injections on the next GPU rather than doing a slow GPU->GPU transfer of the texture data\n")	
	TEXT("  0: Reinjection off\n")
	TEXT("  1: Reinjection on"),
	ECVF_ReadOnly);

/*-----------------------------------------------------------------------------
	Allocators used to manage GPU particle resources.
-----------------------------------------------------------------------------*/

/**
 * Stack allocator for managing tile lifetime.
 */
class FParticleTileAllocator
{
public:

	/** Default constructor. */
	FParticleTileAllocator()
		: FreeTileCount(GParticleSimulationTileCount)
	{
		for ( int32 TileIndex = 0; TileIndex < GParticleSimulationTileCount; ++TileIndex )
		{
			FreeTiles[TileIndex] = GParticleSimulationTileCount - TileIndex - 1;
		}
	}

	/**
	 * Allocate a tile.
	 * @returns the index of the allocated tile, INDEX_NONE if no tiles are available.
	 */
	uint32 Allocate()
	{
		FScopeLock Lock(&CriticalSection);
		if ( FreeTileCount > 0 )
		{
			FreeTileCount--;
			return FreeTiles[FreeTileCount];
		}
		return INDEX_NONE;
	}

	/**
	 * Frees a tile so it may be allocated by another emitter.
	 * @param TileIndex - The index of the tile to free.
	 */
	void Free( uint32 TileIndex )
	{
		FScopeLock Lock(&CriticalSection);
		check( TileIndex < GParticleSimulationTileCount );
		check( FreeTileCount < GParticleSimulationTileCount );
		FreeTiles[FreeTileCount] = TileIndex;
		FreeTileCount++;
	}

	/**
	 * Returns the number of free tiles.
	 */
	int32 GetFreeTileCount() const
	{
		FScopeLock Lock(&CriticalSection);
		return FreeTileCount;
	}

private:

	/** List of free tiles. */
	uint32 FreeTiles[GParticleSimulationTileCount];
	/** How many tiles are in the free list. */
	int32 FreeTileCount;

	mutable FCriticalSection CriticalSection;
};

/*-----------------------------------------------------------------------------
	GPU resources required for simulation.
-----------------------------------------------------------------------------*/

/**
 * Per-particle information stored in a vertex buffer for drawing GPU sprites.
 */
struct FParticleIndex
{
	/** The X coordinate of the particle within the texture. */
	FFloat16 X;
	/** The Y coordinate of the particle within the texture. */
	FFloat16 Y;
};

/**
 * Texture resources holding per-particle state required for GPU simulation.
 */
class FParticleStateTextures : public FRenderResource
{
public:

	/** Contains the positions of all simulating particles. */
	FTexture2DRHIRef PositionTextureTargetRHI;
	FTexture2DRHIRef PositionTextureRHI;
	/** Contains the velocity of all simulating particles. */
	FTexture2DRHIRef VelocityTextureTargetRHI;
	FTexture2DRHIRef VelocityTextureRHI;

	bool bTexturesCleared;

	/**
	 * Initialize RHI resources used for particle simulation.
	 */
	virtual void InitRHI() override
	{
		const int32 SizeX = GParticleSimulationTextureSizeX;
		const int32 SizeY = GParticleSimulationTextureSizeY;

		// 32-bit per channel RGBA texture for position.
		check( !IsValidRef( PositionTextureTargetRHI ) );
		check( !IsValidRef( PositionTextureRHI ) );

		FRHIResourceCreateInfo CreateInfo(FClearValueBinding::Transparent);
		RHICreateTargetableShaderResource2D(
			SizeX,
			SizeY,
			PF_A32B32G32R32F,
			/*NumMips=*/ 1,
			TexCreate_None,
			TexCreate_RenderTargetable,
			/*bForceSeparateTargetAndShaderResource=*/ false,
			CreateInfo,
			PositionTextureTargetRHI,
			PositionTextureRHI
			);

		// 16-bit per channel RGBA texture for velocity.
		check( !IsValidRef( VelocityTextureTargetRHI ) );
		check( !IsValidRef( VelocityTextureRHI ) );

		RHICreateTargetableShaderResource2D(
			SizeX,
			SizeY,
			PF_FloatRGBA,
			/*NumMips=*/ 1,
			TexCreate_None,
			TexCreate_RenderTargetable,
			/*bForceSeparateTargetAndShaderResource=*/ false,
			CreateInfo,
			VelocityTextureTargetRHI,
			VelocityTextureRHI
			);

		static FName PositionTextureName(TEXT("ParticleStatePosition"));
		static FName VelocityTextureName(TEXT("ParticleStateVelocity"));
		PositionTextureTargetRHI->SetName(PositionTextureName);
		VelocityTextureTargetRHI->SetName(VelocityTextureName);

		bTexturesCleared = false;
	}

	/**
	 * Releases RHI resources used for particle simulation.
	 */
	virtual void ReleaseRHI() override
	{
		// Release textures.
		PositionTextureTargetRHI.SafeRelease();
		PositionTextureRHI.SafeRelease();
		VelocityTextureTargetRHI.SafeRelease();
		VelocityTextureRHI.SafeRelease();
	}
};

/**
 * A texture holding per-particle attributes.
 */
class FParticleAttributesTexture : public FRenderResource
{
public:

	/** Contains the attributes of all simulating particles. */
	FTexture2DRHIRef TextureTargetRHI;
	FTexture2DRHIRef TextureRHI;

	/**
	 * Initialize RHI resources used for particle simulation.
	 */
	virtual void InitRHI() override
	{
		const int32 SizeX = GParticleSimulationTextureSizeX;
		const int32 SizeY = GParticleSimulationTextureSizeY;

		const uint32 ExtraFlags = CVarGPUParticleAFRReinject.GetValueOnRenderThread() == 1 ? TexCreate_AFRManual : 0;

		FRHIResourceCreateInfo CreateInfo(FClearValueBinding::None);
		RHICreateTargetableShaderResource2D(
			SizeX,
			SizeY,
			PF_B8G8R8A8,
			/*NumMips=*/ 1,
			TexCreate_None,
			TexCreate_RenderTargetable | TexCreate_NoFastClear | ExtraFlags,
			/*bForceSeparateTargetAndShaderResource=*/ false,
			CreateInfo,
			TextureTargetRHI,
			TextureRHI
			);

		static FName AttributesTextureName(TEXT("ParticleAttributes"));	
		TextureTargetRHI->SetName(AttributesTextureName);		
	}

	/**
	 * Releases RHI resources used for particle simulation.
	 */
	virtual void ReleaseRHI() override
	{
		TextureTargetRHI.SafeRelease();
		TextureRHI.SafeRelease();
	}
};

/**
 * Vertex buffer used to hold particle indices.
 */
class FParticleIndicesVertexBuffer : public FVertexBuffer
{
public:

	/** Shader resource view of the vertex buffer. */
	FShaderResourceViewRHIRef VertexBufferSRV;

	/** Release RHI resources. */
	virtual void ReleaseRHI() override
	{
		VertexBufferSRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}
};

/**
 * Resources required for GPU particle simulation.
 */
class FParticleSimulationResources
{
public:

	/** Textures needed for simulation, double buffered. */
	FParticleStateTextures StateTextures[2];
	/** Texture holding render attributes. */
	FParticleAttributesTexture RenderAttributesTexture;
	/** Texture holding simulation attributes. */
	FParticleAttributesTexture SimulationAttributesTexture;
	/** Vertex buffer that points to the current sorted vertex buffer. */
	FParticleIndicesVertexBuffer SortedVertexBuffer;

	/** Frame index used to track double buffered resources on the GPU. */
	int32 FrameIndex;

	/** List of simulations to be sorted. */
	TArray<FParticleSimulationSortInfo> SimulationsToSort;
	/** The total number of sorted particles. */
	int32 SortedParticleCount;

	/** Default constructor. */
	FParticleSimulationResources()
		: FrameIndex(0)
		, SortedParticleCount(0)
	{
	}

	/**
	 * Initialize resources.
	 */
	void Init()
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FInitParticleSimulationResourcesCommand,
			FParticleSimulationResources*, ParticleResources, this,
		{
			ParticleResources->StateTextures[0].InitResource();
			ParticleResources->StateTextures[1].InitResource();
			ParticleResources->RenderAttributesTexture.InitResource();
			ParticleResources->SimulationAttributesTexture.InitResource();
			ParticleResources->SortedVertexBuffer.InitResource();
		});
	}

	/**
	 * Release resources.
	 */
	void Release()
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FReleaseParticleSimulationResourcesCommand,
			FParticleSimulationResources*, ParticleResources, this,
		{
			ParticleResources->StateTextures[0].ReleaseResource();
			ParticleResources->StateTextures[1].ReleaseResource();
			ParticleResources->RenderAttributesTexture.ReleaseResource();
			ParticleResources->SimulationAttributesTexture.ReleaseResource();
			ParticleResources->SortedVertexBuffer.ReleaseResource();
		});
	}

	/**
	 * Destroy resources.
	 */
	void Destroy()
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDestroyParticleSimulationResourcesCommand,
			FParticleSimulationResources*, ParticleResources, this,
		{
			delete ParticleResources;
		});
	}

	/**
	 * Retrieve texture resources with up-to-date particle state.
	 */
	FParticleStateTextures& GetCurrentStateTextures()
	{
		return StateTextures[FrameIndex];
	}

	/**
	 * Retrieve texture resources with previous particle state.
	 */
	FParticleStateTextures& GetPreviousStateTextures()
	{
		return StateTextures[FrameIndex ^ 0x1];
	}

	FParticleStateTextures& GetVisualizeStateTextures()
	{
		const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnRenderThread();
		if (FixDeltaSeconds > 0)
		{
			return GetPreviousStateTextures();
		}
		else
		{
			return GetCurrentStateTextures();
		}
	}
	/**
	 * Allocate a particle tile.
	 */
	uint32 AllocateTile()
	{
		return TileAllocator.Allocate();
	}

	/**
	 * Free a particle tile.
	 */
	void FreeTile( uint32 Tile )
	{
		TileAllocator.Free( Tile );
	}

	/**
	 * Returns the number of free tiles.
	 */
	int32 GetFreeTileCount() const
	{
		return TileAllocator.GetFreeTileCount();
	}

private:

	/** Allocator for managing particle tiles. */
	FParticleTileAllocator TileAllocator;
};

/** The global vertex buffers used for sorting particles on the GPU. */
TGlobalResource<FParticleSortBuffers> GParticleSortBuffers(GParticleSimulationTextureSizeX * GParticleSimulationTextureSizeY);

/*-----------------------------------------------------------------------------
	Vertex factory.
-----------------------------------------------------------------------------*/

/**
 * Uniform buffer for GPU particle sprite emitters.
 */
BEGIN_UNIFORM_BUFFER_STRUCT(FGPUSpriteEmitterUniformParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, ColorCurve)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, ColorScale)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, ColorBias)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, MiscCurve)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, MiscScale)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, MiscBias)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, SizeBySpeed)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, SubImageSize)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, TangentSelector)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector, CameraFacingBlend)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, RemoveHMDRoll)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, RotationRateScale)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, RotationBias)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, CameraMotionBlurAmount)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector2D, PivotOffset)
END_UNIFORM_BUFFER_STRUCT(FGPUSpriteEmitterUniformParameters)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FGPUSpriteEmitterUniformParameters,TEXT("EmitterUniforms"));

typedef TUniformBufferRef<FGPUSpriteEmitterUniformParameters> FGPUSpriteEmitterUniformBufferRef;

/**
 * Uniform buffer to hold dynamic parameters for GPU particle sprite emitters.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FGPUSpriteEmitterDynamicUniformParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector2D, LocalToWorldScale )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, AxisLockRight )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, AxisLockUp )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, DynamicColor)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, MacroUVParameters )
END_UNIFORM_BUFFER_STRUCT( FGPUSpriteEmitterDynamicUniformParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FGPUSpriteEmitterDynamicUniformParameters,TEXT("EmitterDynamicUniforms"));

typedef TUniformBufferRef<FGPUSpriteEmitterDynamicUniformParameters> FGPUSpriteEmitterDynamicUniformBufferRef;

/**
 * Vertex shader parameters for the particle vertex factory.
 */
class FGPUSpriteVertexFactoryShaderParametersVS : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind( const FShaderParameterMap& ParameterMap ) override
	{
		ParticleIndices.Bind(ParameterMap, TEXT("ParticleIndices"));
		ParticleIndicesOffset.Bind(ParameterMap, TEXT("ParticleIndicesOffset"));
		PositionTexture.Bind(ParameterMap, TEXT("PositionTexture"));
		PositionTextureSampler.Bind(ParameterMap, TEXT("PositionTextureSampler"));
		VelocityTexture.Bind(ParameterMap, TEXT("VelocityTexture"));
		VelocityTextureSampler.Bind(ParameterMap, TEXT("VelocityTextureSampler"));
		AttributesTexture.Bind(ParameterMap, TEXT("AttributesTexture"));
		AttributesTextureSampler.Bind(ParameterMap, TEXT("AttributesTextureSampler"));
		CurveTexture.Bind(ParameterMap, TEXT("CurveTexture"));
		CurveTextureSampler.Bind(ParameterMap, TEXT("CurveTextureSampler"));
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << ParticleIndices;
		Ar << ParticleIndicesOffset;
		Ar << PositionTexture;
		Ar << PositionTextureSampler;
		Ar << VelocityTexture;
		Ar << VelocityTextureSampler;
		Ar << AttributesTexture;
		Ar << AttributesTextureSampler;
		Ar << CurveTexture;
		Ar << CurveTextureSampler;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override;

	virtual uint32 GetSize() const override { return sizeof(*this); }

private:

	/** Buffer containing particle indices. */
	FShaderResourceParameter ParticleIndices;
	/** Offset in to the particle indices buffer. */
	FShaderParameter ParticleIndicesOffset;
	/** Texture containing positions for all particles. */
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter PositionTextureSampler;
	/** Texture containing velocities for all particles. */
	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter VelocityTextureSampler;
	/** Texture containint attributes for all particles. */
	FShaderResourceParameter AttributesTexture;
	FShaderResourceParameter AttributesTextureSampler;
	/** Texture containing curves from which attributes are sampled. */
	FShaderResourceParameter CurveTexture;
	FShaderResourceParameter CurveTextureSampler;
};

/**
 * Pixel shader parameters for the particle vertex factory.
 */
class FGPUSpriteVertexFactoryShaderParametersPS : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind( const FShaderParameterMap& ParameterMap ) override {}

	virtual void Serialize(FArchive& Ar) override {}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override;
	virtual uint32 GetSize() const override { return sizeof(*this); }

private:
};

/**
 * GPU Sprite vertex factory vertex declaration.
 */
class FGPUSpriteVertexDeclaration : public FRenderResource
{
public:

	/** The vertex declaration for GPU sprites. */
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;

		/** The stream to read the texture coordinates from. */
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2D), false));

		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** Global GPU sprite vertex declaration. */
TGlobalResource<FGPUSpriteVertexDeclaration> GGPUSpriteVertexDeclaration;

/**
 * Vertex factory for render sprites from GPU simulated particles.
 */
class FGPUSpriteVertexFactory : public FParticleVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FGPUSpriteVertexFactory);

public:

	/** Emitter uniform buffer. */
	FUniformBufferRHIParamRef EmitterUniformBuffer;
	/** Emitter uniform buffer for dynamic parameters. */
	FUniformBufferRHIRef EmitterDynamicUniformBuffer;
	/** Buffer containing particle indices. */
	FParticleIndicesVertexBuffer* ParticleIndicesBuffer;
	/** Offset in to the particle indices buffer. */
	uint32 ParticleIndicesOffset;
	/** Texture containing positions for all particles. */
	FTexture2DRHIParamRef PositionTextureRHI;
	/** Texture containing velocities for all particles. */
	FTexture2DRHIParamRef VelocityTextureRHI;
	/** Texture containint attributes for all particles. */
	FTexture2DRHIParamRef AttributesTextureRHI;


	FGPUSpriteVertexFactory()
		: FParticleVertexFactoryBase(PVFT_MAX, ERHIFeatureLevel::Num)
		, ParticleIndicesBuffer(nullptr)
		, ParticleIndicesOffset(0)
		, PositionTextureRHI(nullptr)
		, VelocityTextureRHI(nullptr)
		, AttributesTextureRHI(nullptr)
	{}

	/**
	 * Constructs render resources for this vertex factory.
	 */
	virtual void InitRHI() override
	{
		FVertexStream Stream;

		// No streams should currently exist.
		check( Streams.Num() == 0 );

		// Stream 0: Global particle texture coordinate buffer.
		Stream.VertexBuffer = &GParticleTexCoordVertexBuffer;
		Stream.Stride = sizeof(FVector2D);
		Stream.Offset = 0;
		Streams.Add( Stream );

		// Set the declaration.
		SetDeclaration( GGPUSpriteVertexDeclaration.VertexDeclarationRHI );
	}

	virtual bool RendersPrimitivesAsCameraFacingSprites() const override { return true; }

	/**
	 * Set the source vertex buffer that contains particle indices.
	 */
	void SetVertexBuffer( FParticleIndicesVertexBuffer* VertexBuffer, uint32 Offset )
	{
		ParticleIndicesBuffer = VertexBuffer;
		ParticleIndicesOffset = Offset;
	}

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return (Material->IsUsedWithParticleSprites() || Material->IsSpecialEngineMaterial()) && SupportsGPUParticles(Platform);
	}

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FParticleVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PARTICLES_PER_INSTANCE"), MAX_PARTICLES_PER_INSTANCE);

		// Set a define so we can tell in MaterialTemplate.usf when we are compiling a sprite vertex factory
		OutEnvironment.SetDefine(TEXT("PARTICLE_SPRITE_FACTORY"),TEXT("1"));

		if (Platform == SP_OPENGL_ES2_ANDROID)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_FeatureLevelES31);
		}
	}

	/**
	 * Construct shader parameters for this type of vertex factory.
	 */
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency)
	{
		if (ShaderFrequency == SF_Vertex)
		{
			return new FGPUSpriteVertexFactoryShaderParametersVS();
		}
		else if (ShaderFrequency == SF_Pixel)
		{
			return new FGPUSpriteVertexFactoryShaderParametersPS();
		}
		return NULL;
	}
};

/**
 * Set vertex factory shader parameters.
 */
void FGPUSpriteVertexFactoryShaderParametersVS::SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const
{
	FGPUSpriteVertexFactory* GPUVF = (FGPUSpriteVertexFactory*)VertexFactory;
	FVertexShaderRHIParamRef VertexShader = Shader->GetVertexShader();
	FSamplerStateRHIParamRef SamplerStatePoint = TStaticSamplerState<SF_Point>::GetRHI();
	FSamplerStateRHIParamRef SamplerStateLinear = TStaticSamplerState<SF_Bilinear>::GetRHI();
	SetUniformBufferParameter(RHICmdList, VertexShader, Shader->GetUniformBufferParameter<FGPUSpriteEmitterUniformParameters>(), GPUVF->EmitterUniformBuffer );
	SetUniformBufferParameter(RHICmdList, VertexShader, Shader->GetUniformBufferParameter<FGPUSpriteEmitterDynamicUniformParameters>(), GPUVF->EmitterDynamicUniformBuffer );
	if (ParticleIndices.IsBound())
	{
		RHICmdList.SetShaderResourceViewParameter(VertexShader, ParticleIndices.GetBaseIndex(), GPUVF->ParticleIndicesBuffer->VertexBufferSRV);
	}
	SetShaderValue(RHICmdList, VertexShader, ParticleIndicesOffset, GPUVF->ParticleIndicesOffset);
	SetTextureParameter(RHICmdList, VertexShader, PositionTexture, PositionTextureSampler, SamplerStatePoint, GPUVF->PositionTextureRHI );
	SetTextureParameter(RHICmdList, VertexShader, VelocityTexture, VelocityTextureSampler, SamplerStatePoint, GPUVF->VelocityTextureRHI );
	SetTextureParameter(RHICmdList, VertexShader, AttributesTexture, AttributesTextureSampler, SamplerStatePoint, GPUVF->AttributesTextureRHI );
	SetTextureParameter(RHICmdList, VertexShader, CurveTexture, CurveTextureSampler, SamplerStateLinear, GParticleCurveTexture.GetCurveTexture() );
}

void FGPUSpriteVertexFactoryShaderParametersPS::SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const
{
	FGPUSpriteVertexFactory* GPUVF = (FGPUSpriteVertexFactory*)VertexFactory;
	FPixelShaderRHIParamRef PixelShader = Shader->GetPixelShader();
	SetUniformBufferParameter(RHICmdList, PixelShader, Shader->GetUniformBufferParameter<FGPUSpriteEmitterDynamicUniformParameters>(), GPUVF->EmitterDynamicUniformBuffer );
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FGPUSpriteVertexFactory,"/Engine/Private/ParticleGPUSpriteVertexFactory.ush",true,false,true,false,false);

/*-----------------------------------------------------------------------------
	Shaders used for simulation.
-----------------------------------------------------------------------------*/

/**
 * Uniform buffer to hold parameters for particle simulation.
 */
BEGIN_UNIFORM_BUFFER_STRUCT(FParticleSimulationParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, AttributeCurve)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, AttributeCurveScale)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, AttributeCurveBias)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, AttributeScale)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, AttributeBias)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, MiscCurve)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, MiscScale)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4, MiscBias)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector, Acceleration)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector, OrbitOffsetBase)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector, OrbitOffsetRange)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector, OrbitFrequencyBase)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector, OrbitFrequencyRange)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector, OrbitPhaseBase)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector, OrbitPhaseRange)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, CollisionRadiusScale)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, CollisionRadiusBias)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, CollisionTimeBias)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, CollisionRandomSpread)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, CollisionRandomDistribution)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, OneMinusFriction)
END_UNIFORM_BUFFER_STRUCT(FParticleSimulationParameters)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FParticleSimulationParameters,TEXT("Simulation"));

typedef TUniformBufferRef<FParticleSimulationParameters> FParticleSimulationBufferRef;

/**
 * Per-frame parameters for particle simulation.
 */
struct FParticlePerFrameSimulationParameters
{
	/** Position (XYZ) and squared radius (W) of the point attractor. */
	FVector4 PointAttractor;
	/** Position offset (XYZ) to add to particles and strength of the attractor (W). */
	FVector4 PositionOffsetAndAttractorStrength;
	/** Amount by which to scale bounds for collision purposes. */
	FVector2D LocalToWorldScale;

	/** Amount of time by which to simulate particles in the fix dt pass. */
	float DeltaSecondsInFix;
	/** Nbr of iterations to use in the fix dt pass. */
	int32  NumIterationsInFix;

	/** Amount of time by which to simulate particles in the variable dt pass. */
	float DeltaSecondsInVar;
	/** Nbr of iterations to use in the variable dt pass. */
	int32 NumIterationsInVar;

	/** Amount of time by which to simulate particles. */
	float DeltaSeconds;

	FParticlePerFrameSimulationParameters()
		: PointAttractor(FVector::ZeroVector,0.0f)
		, PositionOffsetAndAttractorStrength(FVector::ZeroVector,0.0f)
		, LocalToWorldScale(1.0f, 1.0f)
		, DeltaSecondsInFix(0.0f)
		, NumIterationsInFix(0)
		, DeltaSecondsInVar(0.0f)
		, NumIterationsInVar(0)
		, DeltaSeconds(0.0f)

	{
	}

	void ResetDeltaSeconds() 
	{
		DeltaSecondsInFix = 0.0f;
		NumIterationsInFix = 0;
		DeltaSecondsInVar = 0.0f;
		NumIterationsInVar = 0;
		DeltaSeconds = 0.0f;
	}

};

/**
 * Per-frame shader parameters for particle simulation.
 */
struct FParticlePerFrameSimulationShaderParameters
{
	FShaderParameter PointAttractor;
	FShaderParameter PositionOffsetAndAttractorStrength;
	FShaderParameter LocalToWorldScale;
	FShaderParameter DeltaSeconds;
	FShaderParameter NumIterations;

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		PointAttractor.Bind(ParameterMap,TEXT("PointAttractor"));
		PositionOffsetAndAttractorStrength.Bind(ParameterMap,TEXT("PositionOffsetAndAttractorStrength"));
		LocalToWorldScale.Bind(ParameterMap,TEXT("LocalToWorldScale"));
		DeltaSeconds.Bind(ParameterMap,TEXT("DeltaSeconds"));
		NumIterations.Bind(ParameterMap,TEXT("NumIterations"));
	}

	template <typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const FParticlePerFrameSimulationParameters& Parameters, bool bUseFixDT) const
	{
		// The offset must only be applied once in the frame, and be stored in the persistent data (not the interpolated one).
		const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnRenderThread();
		const bool bApplyOffset = FixDeltaSeconds <= 0 || bUseFixDT;
		const FVector4 OnlyAttractorStrength = FVector4(0, 0, 0, Parameters.PositionOffsetAndAttractorStrength.W);

		SetShaderValue(RHICmdList,ShaderRHI,PointAttractor,Parameters.PointAttractor);
		SetShaderValue(RHICmdList,ShaderRHI,PositionOffsetAndAttractorStrength, bApplyOffset ? Parameters.PositionOffsetAndAttractorStrength : OnlyAttractorStrength);
		SetShaderValue(RHICmdList,ShaderRHI,LocalToWorldScale,Parameters.LocalToWorldScale);
		SetShaderValue(RHICmdList,ShaderRHI,DeltaSeconds, bUseFixDT ? Parameters.DeltaSecondsInFix : Parameters.DeltaSecondsInVar);
		SetShaderValue(RHICmdList,ShaderRHI,NumIterations, bUseFixDT ? Parameters.NumIterationsInFix : Parameters.NumIterationsInVar);
	}
};

FArchive& operator<<(FArchive& Ar, FParticlePerFrameSimulationShaderParameters& PerFrameParameters)
{
	Ar << PerFrameParameters.PointAttractor;
	Ar << PerFrameParameters.PositionOffsetAndAttractorStrength;
	Ar << PerFrameParameters.LocalToWorldScale;
	Ar << PerFrameParameters.DeltaSeconds;
	Ar << PerFrameParameters.NumIterations;
	return Ar;
}

/**
 * Uniform buffer to hold parameters for vector fields sampled during particle
 * simulation.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FVectorFieldUniformParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( int32, Count )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY( FMatrix, WorldToVolume, [MAX_VECTOR_FIELDS] )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY( FMatrix, VolumeToWorld, [MAX_VECTOR_FIELDS] )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY( FVector4, IntensityAndTightness, [MAX_VECTOR_FIELDS] )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY( FVector4, VolumeSize, [MAX_VECTOR_FIELDS] )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY( FVector4, TilingAxes, [MAX_VECTOR_FIELDS] )
END_UNIFORM_BUFFER_STRUCT( FVectorFieldUniformParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FVectorFieldUniformParameters,TEXT("VectorFields"));

typedef TUniformBufferRef<FVectorFieldUniformParameters> FVectorFieldUniformBufferRef;

// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES

BEGIN_UNIFORM_BUFFER_STRUCT(FNvFlowGridUniformParameters, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int32, Count)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FIntVector, BlockDim, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FIntVector, BlockDimBits, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector, BlockDimInv, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FIntVector, LinearBlockDim, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FIntVector, LinearBlockOffset, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector, DimInv, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector, VDim, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector, VDimInv, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FIntVector, PoolGridDim, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FIntVector, GridDim, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(int32, IsVTR, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FMatrix, WorldToVolume, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(float, VelocityScale, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(float, GridToParticleAccelRate, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(float, GridToParticleDecelRate, [MAX_NVFLOW_GRIDS])
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(float, GridToParticleThreshold, [MAX_NVFLOW_GRIDS])
END_UNIFORM_BUFFER_STRUCT(FNvFlowGridUniformParameters)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNvFlowGridUniformParameters, TEXT("NvFlowParams"));

typedef TUniformBufferRef<FNvFlowGridUniformParameters> FNvFlowGridUniformBufferRef;

#endif
// NvFlow end

/**
 * Vertex shader for drawing particle tiles on the GPU.
 */
class FParticleTileVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleTileVS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return SupportsGPUParticles(Platform);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine(TEXT("TILES_PER_INSTANCE"), TILES_PER_INSTANCE);
		OutEnvironment.SetDefine(TEXT("TILE_SIZE_X"), (float)GParticleSimulationTileSize / (float)GParticleSimulationTextureSizeX);
		OutEnvironment.SetDefine(TEXT("TILE_SIZE_Y"), (float)GParticleSimulationTileSize / (float)GParticleSimulationTextureSizeY);

		if (Platform == SP_OPENGL_ES2_ANDROID)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_FeatureLevelES31);
		}
	}

	/** Default constructor. */
	FParticleTileVS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleTileVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		TileOffsets.Bind(Initializer.ParameterMap, TEXT("TileOffsets"));
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << TileOffsets;
		return bShaderHasOutdatedParameters;
	}

	/** Set parameters. */
	void SetParameters(FRHICommandList& RHICmdList, FParticleShaderParamRef TileOffsetsRef)
	{
		FVertexShaderRHIParamRef VertexShaderRHI = GetVertexShader();
		if (TileOffsets.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(VertexShaderRHI, TileOffsets.GetBaseIndex(), TileOffsetsRef);
		}
	}

private:

	/** Buffer from which to read tile offsets. */
	FShaderResourceParameter TileOffsets;
};

/**
 * Pixel shader for simulating particles on the GPU.
 */
template <EParticleCollisionShaderMode CollisionMode>
class TParticleSimulationPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TParticleSimulationPS,Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return SupportsGPUParticles(Platform) && IsParticleCollisionModeSupported(Platform, CollisionMode);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PARTICLE_SIMULATION_PIXELSHADER"), 1);
		OutEnvironment.SetDefine(TEXT("MAX_VECTOR_FIELDS"), MAX_VECTOR_FIELDS);
		OutEnvironment.SetDefine(TEXT("DEPTH_BUFFER_COLLISION"), CollisionMode == PCM_DepthBuffer);
		OutEnvironment.SetDefine(TEXT("DISTANCE_FIELD_COLLISION"), CollisionMode == PCM_DistanceField);
		// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
		OutEnvironment.SetDefine(TEXT("NV_FLOW_WITH_GPU_PARTICLES"), 1);
#endif
		// NvFlow end
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);

		if (Platform == SP_OPENGL_ES2_ANDROID)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_FeatureLevelES31);
		}
	}

	/** Default constructor. */
	TParticleSimulationPS()
	{
	}

	/** Initialization constructor. */
	explicit TParticleSimulationPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PositionTexture.Bind(Initializer.ParameterMap, TEXT("PositionTexture"));
		PositionTextureSampler.Bind(Initializer.ParameterMap, TEXT("PositionTextureSampler"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		VelocityTextureSampler.Bind(Initializer.ParameterMap, TEXT("VelocityTextureSampler"));
		AttributesTexture.Bind(Initializer.ParameterMap, TEXT("AttributesTexture"));
		AttributesTextureSampler.Bind(Initializer.ParameterMap, TEXT("AttributesTextureSampler"));
		RenderAttributesTexture.Bind(Initializer.ParameterMap, TEXT("RenderAttributesTexture"));
		RenderAttributesTextureSampler.Bind(Initializer.ParameterMap, TEXT("RenderAttributesTextureSampler"));
		CurveTexture.Bind(Initializer.ParameterMap, TEXT("CurveTexture"));
		CurveTextureSampler.Bind(Initializer.ParameterMap, TEXT("CurveTextureSampler"));
		for (int32 i = 0; i < MAX_VECTOR_FIELDS; ++i)
		{
			VectorFieldTextures[i].Bind(Initializer.ParameterMap, *FString::Printf(TEXT("VectorFieldTextures%d"), i));
			VectorFieldTexturesSamplers[i].Bind(Initializer.ParameterMap, *FString::Printf(TEXT("VectorFieldTexturesSampler%d"), i));
		}
		SceneDepthTextureParameter.Bind(Initializer.ParameterMap,TEXT("SceneDepthTexture"));
		SceneDepthTextureParameterSampler.Bind(Initializer.ParameterMap,TEXT("SceneDepthTextureSampler"));
		GBufferATextureParameter.Bind(Initializer.ParameterMap,TEXT("GBufferATexture"));
		GBufferATextureParameterSampler.Bind(Initializer.ParameterMap,TEXT("GBufferATextureSampler"));
		CollisionDepthBounds.Bind(Initializer.ParameterMap,TEXT("CollisionDepthBounds"));
		PerFrameParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);

		// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
		for (int32 i = 0; i < MAX_NVFLOW_GRIDS; ++i)
		{
			NvFlowExportData[i].Bind(Initializer.ParameterMap, *FString::Printf(TEXT("NvFlowExportData%d"), i));
			NvFlowExportBlockTable[i].Bind(Initializer.ParameterMap, *FString::Printf(TEXT("NvFlowExportBlockTable%d"), i));
			NvFlowExportDataSampler[i].Bind(Initializer.ParameterMap, *FString::Printf(TEXT("NvFlowExportDataSampler%d"), i));
		}
#endif
		// NvFlow end
	}

	/** Serialization. */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << PositionTexture;
		Ar << PositionTextureSampler;
		Ar << VelocityTexture;
		Ar << VelocityTextureSampler;
		Ar << AttributesTexture;
		Ar << AttributesTextureSampler;
		Ar << RenderAttributesTexture;
		Ar << RenderAttributesTextureSampler;
		Ar << CurveTexture;
		Ar << CurveTextureSampler;
		for (int32 i = 0; i < MAX_VECTOR_FIELDS; i++)
		{
			Ar << VectorFieldTextures[i];
			Ar << VectorFieldTexturesSamplers[i];
		}
		Ar << SceneDepthTextureParameter;
		Ar << SceneDepthTextureParameterSampler;
		Ar << GBufferATextureParameter;
		Ar << GBufferATextureParameterSampler;
		Ar << CollisionDepthBounds;
		Ar << PerFrameParameters;
		Ar << GlobalDistanceFieldParameters;
		// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
		for (int32 i = 0; i < MAX_NVFLOW_GRIDS; ++i)
		{
			Ar << NvFlowExportData[i];
			Ar << NvFlowExportBlockTable[i];
			Ar << NvFlowExportDataSampler[i];
		}
#endif
		// NvFlow end
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FParticleStateTextures& TextureResources,
		const FParticleAttributesTexture& InAttributesTexture,
		const FParticleAttributesTexture& InRenderAttributesTexture,
		const FUniformBufferRHIParamRef ViewUniformBuffer,
		const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
		FTexture2DRHIParamRef SceneDepthTexture,
		FTexture2DRHIParamRef GBufferATexture
		)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		FSamplerStateRHIParamRef SamplerStatePoint = TStaticSamplerState<SF_Point>::GetRHI();
		FSamplerStateRHIParamRef SamplerStateLinear = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		SetTextureParameter(RHICmdList, PixelShaderRHI, PositionTexture, PositionTextureSampler, SamplerStatePoint, TextureResources.PositionTextureRHI);
		SetTextureParameter(RHICmdList, PixelShaderRHI, VelocityTexture, VelocityTextureSampler, SamplerStatePoint, TextureResources.VelocityTextureRHI);
		SetTextureParameter(RHICmdList, PixelShaderRHI, AttributesTexture, AttributesTextureSampler, SamplerStatePoint, InAttributesTexture.TextureRHI);
		SetTextureParameter(RHICmdList, PixelShaderRHI, CurveTexture, CurveTextureSampler, SamplerStateLinear, GParticleCurveTexture.GetCurveTexture());

		if (CollisionMode == PCM_DepthBuffer)
		{
			check(ViewUniformBuffer != NULL);
			FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, PixelShaderRHI, ViewUniformBuffer);
			SetTextureParameter(
				RHICmdList, 
				PixelShaderRHI,
				SceneDepthTextureParameter,
				SceneDepthTextureParameterSampler,
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				SceneDepthTexture
				);
			SetTextureParameter(
				RHICmdList, 
				PixelShaderRHI,
				GBufferATextureParameter,
				GBufferATextureParameterSampler,
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				GBufferATexture
				);
			SetTextureParameter(
				RHICmdList, 
				PixelShaderRHI,
				RenderAttributesTexture,
				RenderAttributesTextureSampler,
				SamplerStatePoint,
				InRenderAttributesTexture.TextureRHI
				);
			SetShaderValue(RHICmdList, PixelShaderRHI, CollisionDepthBounds, FXConsoleVariables::GPUCollisionDepthBounds);
		}
		else if (CollisionMode == PCM_DistanceField)
		{
			GlobalDistanceFieldParameters.Set(RHICmdList, PixelShaderRHI, *GlobalDistanceFieldParameterData);

			SetTextureParameter(
				RHICmdList, 
				PixelShaderRHI,
				RenderAttributesTexture,
				RenderAttributesTextureSampler,
				SamplerStatePoint,
				InRenderAttributesTexture.TextureRHI
				);
		}
	}

	/**
	 * Set parameters for the vector fields sampled by this shader.
	 * @param VectorFieldParameters -Parameters needed to sample local vector fields.
	 */
	void SetVectorFieldParameters(FRHICommandList& RHICmdList, const FVectorFieldUniformBufferRef& UniformBuffer, const FTexture3DRHIParamRef VolumeTexturesRHI[])
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		SetUniformBufferParameter(RHICmdList, PixelShaderRHI, GetUniformBufferParameter<FVectorFieldUniformParameters>(), UniformBuffer);
		
		FSamplerStateRHIParamRef SamplerStateLinear = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

		for (int32 i = 0; i < MAX_VECTOR_FIELDS; ++i)
		{
			SetSamplerParameter(RHICmdList, PixelShaderRHI, VectorFieldTexturesSamplers[i], SamplerStateLinear);
			SetTextureParameter(RHICmdList, PixelShaderRHI, VectorFieldTextures[i], VolumeTexturesRHI[i]);
		}
	}

	/**
	 * Set per-instance parameters for this shader.
	 */
	void SetInstanceParameters(FRHICommandList& RHICmdList, FUniformBufferRHIParamRef UniformBuffer, const FParticlePerFrameSimulationParameters& InPerFrameParameters, bool bUseFixDT)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		SetUniformBufferParameter(RHICmdList, PixelShaderRHI, GetUniformBufferParameter<FParticleSimulationParameters>(), UniformBuffer);
		PerFrameParameters.Set(RHICmdList, PixelShaderRHI, InPerFrameParameters, bUseFixDT);
	}

	/**
	 * Unbinds buffers that may need to be bound as UAVs.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		FShaderResourceViewRHIParamRef NullSRV = FShaderResourceViewRHIParamRef();
		for (int32 i = 0; i < MAX_VECTOR_FIELDS; ++i)
		{
			if (VectorFieldTextures[i].IsBound())
			{
				RHICmdList.SetShaderResourceViewParameter(PixelShaderRHI, VectorFieldTextures[i].GetBaseIndex(), NullSRV);
			}
		}
		// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
		for (int32 i = 0; i < MAX_NVFLOW_GRIDS; ++i)
		{
			SetSRVParameter(RHICmdList, PixelShaderRHI, NvFlowExportData[i], NullSRV);
			SetSRVParameter(RHICmdList, PixelShaderRHI, NvFlowExportBlockTable[i], NullSRV);
		}
#endif
		// NvFlow end
	}
	
	// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
	void SetNvFlowGridParameters(FRHICommandList& RHICmdList, FNvFlowGridUniformBufferRef UniformBuffer, const FShaderResourceViewRHIRef DataSRV[], const FShaderResourceViewRHIRef BlockTableSRV[])
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();
		SetUniformBufferParameter(RHICmdList, PixelShaderRHI, GetUniformBufferParameter<FNvFlowGridUniformParameters>(), UniformBuffer);

		FSamplerStateRHIParamRef BorderSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();

		for (int32 i = 0; i < MAX_NVFLOW_GRIDS; ++i)
		{
			SetSRVParameter(RHICmdList, PixelShaderRHI, NvFlowExportData[i], DataSRV[i]);
			SetSRVParameter(RHICmdList, PixelShaderRHI, NvFlowExportBlockTable[i], BlockTableSRV[i]);
			SetSamplerParameter(RHICmdList, PixelShaderRHI, NvFlowExportDataSampler[i], BorderSampler);
		}
	}
#endif
	// NvFlow end

private:

	/** The position texture parameter. */
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter PositionTextureSampler;
	/** The velocity texture parameter. */
	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter VelocityTextureSampler;
	/** The simulation attributes texture parameter. */
	FShaderResourceParameter AttributesTexture;
	FShaderResourceParameter AttributesTextureSampler;
	/** The render attributes texture parameter. */
	FShaderResourceParameter RenderAttributesTexture;
	FShaderResourceParameter RenderAttributesTextureSampler;
	/** The curve texture parameter. */
	FShaderResourceParameter CurveTexture;
	FShaderResourceParameter CurveTextureSampler;
	/** Vector fields. */
	FShaderResourceParameter VectorFieldTextures[MAX_VECTOR_FIELDS];
	FShaderResourceParameter VectorFieldTexturesSamplers[MAX_VECTOR_FIELDS];
	/** The SceneDepthTexture parameter for depth buffer collision. */
	FShaderResourceParameter SceneDepthTextureParameter;
	FShaderResourceParameter SceneDepthTextureParameterSampler;
	/** The GBufferATexture parameter for depth buffer collision. */
	FShaderResourceParameter GBufferATextureParameter;
	FShaderResourceParameter GBufferATextureParameterSampler;
	/** Per frame simulation parameters. */
	FParticlePerFrameSimulationShaderParameters PerFrameParameters;
	/** Collision depth bounds. */
	FShaderParameter CollisionDepthBounds;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;

	// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
	FShaderResourceParameter NvFlowExportData[MAX_NVFLOW_GRIDS];
	FShaderResourceParameter NvFlowExportBlockTable[MAX_NVFLOW_GRIDS];
	FShaderResourceParameter NvFlowExportDataSampler[MAX_NVFLOW_GRIDS];
#endif
	// NvFlow end
};

/**
 * Pixel shader for clearing particle simulation data on the GPU.
 */
class FParticleSimulationClearPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleSimulationClearPS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return SupportsGPUParticles(Platform);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("PARTICLE_CLEAR_PIXELSHADER"), 1 );
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);

		if (Platform == SP_OPENGL_ES2_ANDROID)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_FeatureLevelES31);
		}
	}

	/** Default constructor. */
	FParticleSimulationClearPS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleSimulationClearPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		return bShaderHasOutdatedParameters;
	}
};

/** Implementation for all shaders used for simulation. */
IMPLEMENT_SHADER_TYPE(,FParticleTileVS,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("VertexMain"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>,TParticleSimulationPS<PCM_None>,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("PixelMain"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TParticleSimulationPS<PCM_DepthBuffer>,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("PixelMain"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TParticleSimulationPS<PCM_DistanceField>,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("PixelMain"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FParticleSimulationClearPS,TEXT("/Engine/Private/ParticleSimulationShader.usf"),TEXT("PixelMain"),SF_Pixel);

/**
 * Vertex declaration for drawing particle tiles.
 */
class FParticleTileVertexDeclaration : public FRenderResource
{
public:

	/** The vertex declaration. */
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		// TexCoord.
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2D), /*bUseInstanceIndex=*/ false));
		VertexDeclarationRHI = RHICreateVertexDeclaration( Elements );
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** Global vertex declaration resource for particle sim visualization. */
TGlobalResource<FParticleTileVertexDeclaration> GParticleTileVertexDeclaration;

/**
 * Computes the aligned tile count.
 */
FORCEINLINE int32 ComputeAlignedTileCount(int32 TileCount)
{
	return (TileCount + (TILES_PER_INSTANCE-1)) & (~(TILES_PER_INSTANCE-1));
}

/**
 * Builds a vertex buffer containing the offsets for a set of tiles.
 * @param TileOffsetsRef - The vertex buffer to fill. Must be at least TileCount * sizeof(FVector4) in size.
 * @param Tiles - The tiles which will be drawn.
 * @param TileCount - The number of tiles in the array.
 */
static void BuildTileVertexBuffer( FParticleBufferParamRef TileOffsetsRef, const uint32* Tiles, int32 TileCount )
{
	const int32 AlignedTileCount = ComputeAlignedTileCount(TileCount);
	FVector2D* TileOffset = (FVector2D*)RHILockVertexBuffer( TileOffsetsRef, 0, AlignedTileCount * sizeof(FVector2D), RLM_WriteOnly );
	for ( int32 Index = 0; Index < TileCount; ++Index )
	{
		const uint32 TileIndex = Tiles[Index];
		TileOffset[Index].X = FMath::Fractional( (float)TileIndex / (float)GParticleSimulationTileCountX );
		TileOffset[Index].Y = FMath::Fractional( FMath::TruncToFloat( (float)TileIndex / (float)GParticleSimulationTileCountX ) / (float)GParticleSimulationTileCountY );
	}
	for ( int32 Index = TileCount; Index < AlignedTileCount; ++Index )
	{
		TileOffset[Index].X = 100.0f;
		TileOffset[Index].Y = 100.0f;
	}
	RHIUnlockVertexBuffer( TileOffsetsRef );
}

/**
 * Builds a vertex buffer containing the offsets for a set of tiles.
 * @param TileOffsetsRef - The vertex buffer to fill. Must be at least TileCount * sizeof(FVector4) in size.
 * @param Tiles - The tiles which will be drawn.
 */
static void BuildTileVertexBuffer( FParticleBufferParamRef TileOffsetsRef, const TArray<uint32>& Tiles )
{
	BuildTileVertexBuffer( TileOffsetsRef, Tiles.GetData(), Tiles.Num() );
}

/**
 * Issues a draw call for an aligned set of tiles.
 * @param TileCount - The number of tiles to be drawn.
 */
static void DrawAlignedParticleTiles(FRHICommandList& RHICmdList, int32 TileCount)
{
	check((TileCount & (TILES_PER_INSTANCE-1)) == 0);

	// Stream 0: TexCoord.
	RHICmdList.SetStreamSource(
		0,
		GParticleTexCoordVertexBuffer.VertexBufferRHI,
		/*Offset=*/ 0
		);

	// Draw tiles.
	RHICmdList.DrawIndexedPrimitive(
		GParticleIndexBuffer.IndexBufferRHI,
		PT_TriangleList,
		/*BaseVertexIndex=*/ 0,
		/*MinIndex=*/ 0,
		/*NumVertices=*/ 4,
		/*StartIndex=*/ 0,
		/*NumPrimitives=*/ 2 * TILES_PER_INSTANCE,
		/*NumInstances=*/ TileCount / TILES_PER_INSTANCE
		);
}

/**
 * The data needed to simulate a set of particle tiles on the GPU.
 */
struct FSimulationCommandGPU
{
	/** Buffer containing the offsets of each tile. */
	FParticleShaderParamRef TileOffsetsRef;
	/** Uniform buffer containing simulation parameters. */
	FUniformBufferRHIParamRef UniformBuffer;
	/** Uniform buffer containing per-frame simulation parameters. */
	FParticlePerFrameSimulationParameters PerFrameParameters;
	/** Parameters to sample the local vector field for this simulation. */
	FVectorFieldUniformBufferRef VectorFieldsUniformBuffer;
	/** Vector field volume textures for this simulation. */
	FTexture3DRHIParamRef VectorFieldTexturesRHI[MAX_VECTOR_FIELDS];
	/** The number of tiles to simulate. */
	int32 TileCount;

	// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
	FNvFlowGridUniformBufferRef NvFlowGridUniformBuffer;
	FShaderResourceViewRHIRef NvFlowGridDataSRV[MAX_NVFLOW_GRIDS];
	FShaderResourceViewRHIRef NvFlowGridBlockTableSRV[MAX_NVFLOW_GRIDS];
#endif
	// NvFlow end

	/** Initialization constructor. */
	FSimulationCommandGPU(FParticleShaderParamRef InTileOffsetsRef, FUniformBufferRHIParamRef InUniformBuffer, const FParticlePerFrameSimulationParameters& InPerFrameParameters, FVectorFieldUniformBufferRef& InVectorFieldsUniformBuffer, int32 InTileCount)
		: TileOffsetsRef(InTileOffsetsRef)
		, UniformBuffer(InUniformBuffer)
		, PerFrameParameters(InPerFrameParameters)
		, VectorFieldsUniformBuffer(InVectorFieldsUniformBuffer)
		, TileCount(InTileCount)
	{
		FTexture3DRHIParamRef BlackVolumeTextureRHI = (FTexture3DRHIParamRef)(FTextureRHIParamRef)GBlackVolumeTexture->TextureRHI;
		for (int32 i = 0; i < MAX_VECTOR_FIELDS; ++i)
		{
			VectorFieldTexturesRHI[i] = BlackVolumeTextureRHI;
		}
	}
};

/**
 * Executes each command invoking the simulation pixel shader for each particle.
 * calling with empty SimulationCommands is a waste of performance
 * @param SimulationCommands The list of simulation commands to execute.
 * @param TextureResources	The resources from which the current state can be read.
 * @param AttributeTexture	The texture from which particle simulation attributes can be read.
 * @param CollisionView		The view to use for collision, if any.
 * @param SceneDepthTexture The depth texture to use for collision, if any.
 */
template <EParticleCollisionShaderMode CollisionMode>
void ExecuteSimulationCommands(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	ERHIFeatureLevel::Type FeatureLevel,
	const TArray<FSimulationCommandGPU>& SimulationCommands,
	FParticleSimulationResources* ParticleSimulationResources,
	const FUniformBufferRHIParamRef ViewUniformBuffer,
	const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
	FTexture2DRHIParamRef SceneDepthTexture,
	FTexture2DRHIParamRef GBufferATexture,
	bool bUseFixDT)
{
	if (!CVarSimulateGPUParticles.GetValueOnAnyThread())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, ParticleSimulation);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ParticleSimulation);


	const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnRenderThread();
	const FParticleStateTextures& TextureResources = (FixDeltaSeconds <= 0 || bUseFixDT) ? ParticleSimulationResources->GetPreviousStateTextures() : ParticleSimulationResources->GetCurrentStateTextures();
	const FParticleAttributesTexture& AttributeTexture = ParticleSimulationResources->SimulationAttributesTexture;
	const FParticleAttributesTexture& RenderAttributeTexture = ParticleSimulationResources->RenderAttributesTexture;

	// Grab shaders.
	TShaderMapRef<FParticleTileVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
	TShaderMapRef<TParticleSimulationPS<CollisionMode> > PixelShader(GetGlobalShaderMap(FeatureLevel));

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GParticleTileVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	PixelShader->SetParameters(RHICmdList, TextureResources, AttributeTexture, RenderAttributeTexture, ViewUniformBuffer, GlobalDistanceFieldParameterData, SceneDepthTexture, GBufferATexture);

	// Draw tiles to perform the simulation step.
	const int32 CommandCount = SimulationCommands.Num();
	for (int32 CommandIndex = 0; CommandIndex < CommandCount; ++CommandIndex)
	{
		const FSimulationCommandGPU& Command = SimulationCommands[CommandIndex];
		VertexShader->SetParameters(RHICmdList, Command.TileOffsetsRef);
		PixelShader->SetInstanceParameters(RHICmdList, Command.UniformBuffer, Command.PerFrameParameters, bUseFixDT);
		PixelShader->SetVectorFieldParameters(
			RHICmdList, 
			Command.VectorFieldsUniformBuffer,
			Command.VectorFieldTexturesRHI
			);
		// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
		PixelShader->SetNvFlowGridParameters(RHICmdList, Command.NvFlowGridUniformBuffer, Command.NvFlowGridDataSRV, Command.NvFlowGridBlockTableSRV);
#endif
		// NvFlow end
		DrawAlignedParticleTiles(RHICmdList, Command.TileCount);
	}

	// Unbind input buffers.
	PixelShader->UnbindBuffers(RHICmdList);
}


void ExecuteSimulationCommands(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	ERHIFeatureLevel::Type FeatureLevel,
	const TArray<FSimulationCommandGPU>& SimulationCommands,
	FParticleSimulationResources* ParticleSimulationResources,
	const FUniformBufferRHIParamRef ViewUniformBuffer,
	const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
	FTexture2DRHIParamRef SceneDepthTexture,
	FTexture2DRHIParamRef GBufferATexture,
	EParticleSimulatePhase::Type Phase,
	bool bUseFixDT)
{
	if (Phase == EParticleSimulatePhase::CollisionDepthBuffer && ViewUniformBuffer)
	{
		ExecuteSimulationCommands<PCM_DepthBuffer>(
			RHICmdList,
			GraphicsPSOInit,
			FeatureLevel,
			SimulationCommands,
			ParticleSimulationResources,
			ViewUniformBuffer,
			GlobalDistanceFieldParameterData,
			SceneDepthTexture,
			GBufferATexture,
			bUseFixDT);
	}
	else if (Phase == EParticleSimulatePhase::CollisionDistanceField && GlobalDistanceFieldParameterData)
	{
		ExecuteSimulationCommands<PCM_DistanceField>(
			RHICmdList,
			GraphicsPSOInit,
			FeatureLevel,
			SimulationCommands,
			ParticleSimulationResources,
			ViewUniformBuffer,
			GlobalDistanceFieldParameterData,
			SceneDepthTexture,
			GBufferATexture,
			bUseFixDT);
	}
	else
	{
		ExecuteSimulationCommands<PCM_None>(
			RHICmdList,
			GraphicsPSOInit,
			FeatureLevel,
			SimulationCommands,
			ParticleSimulationResources,
			NULL,
			GlobalDistanceFieldParameterData,
			FTexture2DRHIParamRef(),
			FTexture2DRHIParamRef(),
			bUseFixDT);
	}
}

/**
 * Invokes the clear simulation shader for each particle in each tile.
 * @param Tiles - The list of tiles to clear.
 */
void ClearTiles(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type FeatureLevel, const TArray<uint32>& Tiles)
{
	if (!CVarSimulateGPUParticles.GetValueOnAnyThread())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, ClearTiles);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ParticleSimulation);

	const int32 MaxTilesPerDrawCallUnaligned = GParticleScratchVertexBufferSize / sizeof(FVector2D);
	const int32 MaxTilesPerDrawCall = MaxTilesPerDrawCallUnaligned & (~(TILES_PER_INSTANCE-1));

	FParticleShaderParamRef ShaderParam = GParticleScratchVertexBuffer.GetShaderParam();
	check(ShaderParam);
	FParticleBufferParamRef BufferParam = GParticleScratchVertexBuffer.GetBufferParam();
	check(BufferParam);
	
	int32 TileCount = Tiles.Num();
	int32 FirstTile = 0;

	// Grab shaders.
	TShaderMapRef<FParticleTileVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
	TShaderMapRef<FParticleSimulationClearPS> PixelShader(GetGlobalShaderMap(FeatureLevel));
	
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GParticleTileVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	while (TileCount > 0)
	{
		// Copy new particles in to the vertex buffer.
		const int32 TilesThisDrawCall = FMath::Min<int32>( TileCount, MaxTilesPerDrawCall );
		const uint32* TilesPtr = Tiles.GetData() + FirstTile;
		BuildTileVertexBuffer( BufferParam, TilesPtr, TilesThisDrawCall );
		
		VertexShader->SetParameters(RHICmdList, ShaderParam);
		DrawAlignedParticleTiles(RHICmdList, ComputeAlignedTileCount(TilesThisDrawCall));
		TileCount -= TilesThisDrawCall;
		FirstTile += TilesThisDrawCall;
	}
}

/**
 * Uniform buffer to hold parameters for particle simulation.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FParticleInjectionParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector2D, PixelScale )
END_UNIFORM_BUFFER_STRUCT( FParticleInjectionParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FParticleInjectionParameters,TEXT("ParticleInjection"));

typedef TUniformBufferRef<FParticleInjectionParameters> FParticleInjectionBufferRef;

/**
 * Vertex shader for simulating particles on the GPU.
 */
class FParticleInjectionVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleInjectionVS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return SupportsGPUParticles(Platform);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );

		if (Platform == SP_OPENGL_ES2_ANDROID)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_FeatureLevelES31);
		}
	}

	/** Default constructor. */
	FParticleInjectionVS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleInjectionVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
	}

	/**
	 * Sets parameters for particle injection.
	 */
	void SetParameters(FRHICommandList& RHICmdList)
	{
		FParticleInjectionParameters Parameters;
		Parameters.PixelScale.X = 1.0f / GParticleSimulationTextureSizeX;
		Parameters.PixelScale.Y = 1.0f / GParticleSimulationTextureSizeY;
		FParticleInjectionBufferRef UniformBuffer = FParticleInjectionBufferRef::CreateUniformBufferImmediate( Parameters, UniformBuffer_SingleDraw );
		FVertexShaderRHIParamRef VertexShader = GetVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShader, GetUniformBufferParameter<FParticleInjectionParameters>(), UniformBuffer );
	}
};

/**
 * Pixel shader for simulating particles on the GPU.
 */
template <bool StaticPropertiesOnly>
class TParticleInjectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TParticleInjectionPS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return SupportsGPUParticles(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("STATIC_PROPERTIES_ONLY"), StaticPropertiesOnly);

		
		OutEnvironment.SetRenderTargetOutputFormat(0, StaticPropertiesOnly ? PF_A8R8G8B8 : PF_A32B32G32R32F);

		if (Platform == SP_OPENGL_ES2_ANDROID)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_FeatureLevelES31);
		}

	}

	/** Default constructor. */
	TParticleInjectionPS()
	{
	}

	/** Initialization constructor. */
	explicit TParticleInjectionPS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		return bShaderHasOutdatedParameters;
	}
};

/** Implementation for all shaders used for particle injection. */
IMPLEMENT_SHADER_TYPE(,FParticleInjectionVS,TEXT("/Engine/Private/ParticleInjectionShader.usf"),TEXT("VertexMain"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, TParticleInjectionPS<false>, TEXT("/Engine/Private/ParticleInjectionShader.usf"), TEXT("PixelMain"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TParticleInjectionPS<true>, TEXT("/Engine/Private/ParticleInjectionShader.usf"), TEXT("PixelMain"), SF_Pixel);


/**
 * Vertex declaration for injecting particles.
 */
class FParticleInjectionVertexDeclaration : public FRenderResource
{
public:

	/** The vertex declaration. */
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;

		// Stream 0.
		{
			int32 Offset = 0;
			uint16 Stride = sizeof(FNewParticle);
			// InitialPosition.
			Elements.Add(FVertexElement(0, Offset, VET_Float4, 0, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector4);
			// InitialVelocity.
			Elements.Add(FVertexElement(0, Offset, VET_Float4, 1, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector4);
			// RenderAttributes.
			Elements.Add(FVertexElement(0, Offset, VET_Float4, 2, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector4);
			// SimulationAttributes.
			Elements.Add(FVertexElement(0, Offset, VET_Float4, 3, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector4);
			// ParticleIndex.
			Elements.Add(FVertexElement(0, Offset, VET_Float2, 4, Stride, /*bUseInstanceIndex=*/ true));
			Offset += sizeof(FVector2D);
		}

		// Stream 1.
		{
			int32 Offset = 0;
			// TexCoord.
			Elements.Add(FVertexElement(1, Offset, VET_Float2, 5, sizeof(FVector2D), /*bUseInstanceIndex=*/ false));
			Offset += sizeof(FVector2D);
		}

		VertexDeclarationRHI = RHICreateVertexDeclaration( Elements );
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** The global particle injection vertex declaration. */
TGlobalResource<FParticleInjectionVertexDeclaration> GParticleInjectionVertexDeclaration;

/**
 * Injects new particles in to the GPU simulation.
 * @param NewParticles - A list of particles to inject in to the simulation.
 */
template<bool StaticPropertiesOnly>
void InjectNewParticles(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit,  ERHIFeatureLevel::Type FeatureLevel, const TArray<FNewParticle>& NewParticles)
{
	if (GIsRenderingThreadSuspended || !CVarSimulateGPUParticles.GetValueOnAnyThread())
	{
		return;
	}

	const int32 MaxParticlesPerDrawCall = GParticleScratchVertexBufferSize / sizeof(FNewParticle);
	FVertexBufferRHIParamRef ScratchVertexBufferRHI = GParticleScratchVertexBuffer.VertexBufferRHI;
	int32 ParticleCount = NewParticles.Num();
	int32 FirstParticle = 0;

	
	while ( ParticleCount > 0 )
	{
		// Copy new particles in to the vertex buffer.
		const int32 ParticlesThisDrawCall = FMath::Min<int32>( ParticleCount, MaxParticlesPerDrawCall );
		const void* Src = NewParticles.GetData() + FirstParticle;
		void* Dest = RHILockVertexBuffer( ScratchVertexBufferRHI, 0, ParticlesThisDrawCall * sizeof(FNewParticle), RLM_WriteOnly );
		FMemory::Memcpy( Dest, Src, ParticlesThisDrawCall * sizeof(FNewParticle) );
		RHIUnlockVertexBuffer( ScratchVertexBufferRHI );
		ParticleCount -= ParticlesThisDrawCall;
		FirstParticle += ParticlesThisDrawCall;

		// Grab shaders.
		TShaderMapRef<FParticleInjectionVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
		TShaderMapRef<TParticleInjectionPS<StaticPropertiesOnly> > PixelShader(GetGlobalShaderMap(FeatureLevel));

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GParticleInjectionVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		
		VertexShader->SetParameters(RHICmdList);

		// Stream 0: New particles.
		RHICmdList.SetStreamSource(
			0,
			ScratchVertexBufferRHI,
			/*Offset=*/ 0
			);

		// Stream 1: TexCoord.
		RHICmdList.SetStreamSource(
			1,
			GParticleTexCoordVertexBuffer.VertexBufferRHI,
			/*Offset=*/ 0
			);

		// Inject particles.
		RHICmdList.DrawIndexedPrimitive(
			GParticleIndexBuffer.IndexBufferRHI,
			PT_TriangleList,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ 4,
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ 2,
			/*NumInstances=*/ ParticlesThisDrawCall
			);
	}
}

/*-----------------------------------------------------------------------------
	Shaders used for visualizing the state of particle simulation on the GPU.
-----------------------------------------------------------------------------*/

/**
 * Uniform buffer to hold parameters for visualizing particle simulation.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FParticleSimVisualizeParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, ScaleBias )
END_UNIFORM_BUFFER_STRUCT( FParticleSimVisualizeParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FParticleSimVisualizeParameters,TEXT("PSV"));

typedef TUniformBufferRef<FParticleSimVisualizeParameters> FParticleSimVisualizeBufferRef;

/**
 * Vertex shader for visualizing particle simulation.
 */
class FParticleSimVisualizeVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleSimVisualizeVS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return SupportsGPUParticles(Platform);
	}

	/** Default constructor. */
	FParticleSimVisualizeVS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleSimVisualizeVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(FRHICommandList& RHICmdList, const FParticleSimVisualizeBufferRef& UniformBuffer )
	{
		FVertexShaderRHIParamRef VertexShader = GetVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShader, GetUniformBufferParameter<FParticleSimVisualizeParameters>(), UniformBuffer );
	}
};

/**
 * Pixel shader for visualizing particle simulation.
 */
class FParticleSimVisualizePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleSimVisualizePS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return SupportsGPUParticles(Platform);
	}

	/** Default constructor. */
	FParticleSimVisualizePS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleSimVisualizePS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		VisualizationMode.Bind( Initializer.ParameterMap, TEXT("VisualizationMode") );
		PositionTexture.Bind( Initializer.ParameterMap, TEXT("PositionTexture") );
		PositionTextureSampler.Bind( Initializer.ParameterMap, TEXT("PositionTextureSampler") );
		CurveTexture.Bind( Initializer.ParameterMap, TEXT("CurveTexture") );
		CurveTextureSampler.Bind( Initializer.ParameterMap, TEXT("CurveTextureSampler") );
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << VisualizationMode;
		Ar << PositionTexture;
		Ar << PositionTextureSampler;
		Ar << CurveTexture;
		Ar << CurveTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set parameters for this shader.
	 */
	void SetParameters(FRHICommandList& RHICmdList, int32 InVisualizationMode, FTexture2DRHIParamRef PositionTextureRHI, FTexture2DRHIParamRef CurveTextureRHI )
	{
		FPixelShaderRHIParamRef PixelShader = GetPixelShader();
		SetShaderValue(RHICmdList, PixelShader, VisualizationMode, InVisualizationMode );
		FSamplerStateRHIParamRef SamplerStatePoint = TStaticSamplerState<SF_Point>::GetRHI();
		SetTextureParameter(RHICmdList, PixelShader, PositionTexture, PositionTextureSampler, SamplerStatePoint, PositionTextureRHI );
		SetTextureParameter(RHICmdList, PixelShader, CurveTexture, CurveTextureSampler, SamplerStatePoint, CurveTextureRHI );
	}

private:

	FShaderParameter VisualizationMode;
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter PositionTextureSampler;
	FShaderResourceParameter CurveTexture;
	FShaderResourceParameter CurveTextureSampler;
};

/** Implementation for all shaders used for visualization. */
IMPLEMENT_SHADER_TYPE(,FParticleSimVisualizeVS,TEXT("/Engine/Private/ParticleSimVisualizeShader.usf"),TEXT("VertexMain"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FParticleSimVisualizePS,TEXT("/Engine/Private/ParticleSimVisualizeShader.usf"),TEXT("PixelMain"),SF_Pixel);

/**
 * Vertex declaration for particle simulation visualization.
 */
class FParticleSimVisualizeVertexDeclaration : public FRenderResource
{
public:

	/** The vertex declaration. */
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2D)));
		VertexDeclarationRHI = RHICreateVertexDeclaration( Elements );
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** Global vertex declaration resource for particle sim visualization. */
TGlobalResource<FParticleSimVisualizeVertexDeclaration> GParticleSimVisualizeVertexDeclaration;

/**
 * Visualizes the current state of simulation on the GPU.
 * @param RenderTarget - The render target on which to draw the visualization.
 */
static void VisualizeGPUSimulation(
	FRHICommandList& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	int32 VisualizationMode,
	FRenderTarget* RenderTarget,
	const FParticleStateTextures& StateTextures,
	FTexture2DRHIParamRef CurveTextureRHI
	)
{
	check(IsInRenderingThread());
	SCOPED_DRAW_EVENT(RHICmdList, ParticleSimDebugDraw);

	// Some constants for laying out the debug view.
	const float DisplaySizeX = 256.0f;
	const float DisplaySizeY = 256.0f;
	const float DisplayOffsetX = 60.0f;
	const float DisplayOffsetY = 60.0f;
	
	FGraphicsPipelineStateInitializer GraphicsPSOInit;

	// Setup render states.
	FIntPoint TargetSize = RenderTarget->GetSizeXY();
	SetRenderTarget(RHICmdList, RenderTarget->GetRenderTargetTexture(), FTextureRHIParamRef());
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	RHICmdList.SetViewport(0, 0, 0.0f, TargetSize.X, TargetSize.Y, 1.0f);
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	// Grab shaders.
	TShaderMapRef<FParticleSimVisualizeVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
	TShaderMapRef<FParticleSimVisualizePS> PixelShader(GetGlobalShaderMap(FeatureLevel));

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GParticleSimVisualizeVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	// Parameters for the visualization.
	FParticleSimVisualizeParameters Parameters;
	Parameters.ScaleBias.X = 2.0f * DisplaySizeX / (float)TargetSize.X;
	Parameters.ScaleBias.Y = 2.0f * DisplaySizeY / (float)TargetSize.Y;
	Parameters.ScaleBias.Z = 2.0f * DisplayOffsetX / (float)TargetSize.X - 1.0f;
	Parameters.ScaleBias.W = 2.0f * DisplayOffsetY / (float)TargetSize.Y - 1.0f;
	FParticleSimVisualizeBufferRef UniformBuffer = FParticleSimVisualizeBufferRef::CreateUniformBufferImmediate( Parameters, UniformBuffer_SingleDraw );
	VertexShader->SetParameters(RHICmdList, UniformBuffer);
	PixelShader->SetParameters(RHICmdList, VisualizationMode, StateTextures.PositionTextureRHI, CurveTextureRHI);

	const int32 VertexStride = sizeof(FVector2D);
	
	// Bind vertex stream.
	RHICmdList.SetStreamSource(
		0,
		GParticleTexCoordVertexBuffer.VertexBufferRHI,
		/*VertexOffset=*/ 0
		);

	// Draw.
	RHICmdList.DrawIndexedPrimitive(
		GParticleIndexBuffer.IndexBufferRHI,
		PT_TriangleList,
		/*BaseVertexIndex=*/ 0,
		/*MinIndex=*/ 0,
		/*NumVertices=*/ 4,
		/*StartIndex=*/ 0,
		/*NumPrimitives=*/ 2,
		/*NumInstances=*/ 1
		);
}

/**
 * Constructs a particle vertex buffer on the CPU for a given set of tiles.
 * @param VertexBuffer - The buffer with which to fill with particle indices.
 * @param InTiles - The list of tiles for which to generate indices.
 */
static void BuildParticleVertexBuffer( FVertexBufferRHIParamRef VertexBufferRHI, const TArray<uint32>& InTiles )
{
	check( IsInRenderingThread() );

	const int32 TileCount = InTiles.Num();
	const int32 IndexCount = TileCount * GParticlesPerTile;
	const int32 BufferSize = IndexCount * sizeof(FParticleIndex);
	const int32 Stride = 1;
	FParticleIndex* RESTRICT ParticleIndices = (FParticleIndex*)RHILockVertexBuffer( VertexBufferRHI, 0, BufferSize, RLM_WriteOnly );

	for ( int32 Index = 0; Index < TileCount; ++Index )
	{
		const uint32 TileIndex = InTiles[Index];
		const FVector2D TileOffset(
			FMath::Fractional( (float)TileIndex / (float)GParticleSimulationTileCountX ),
			FMath::Fractional( FMath::TruncToFloat( (float)TileIndex / (float)GParticleSimulationTileCountX ) / (float)GParticleSimulationTileCountY )
			);
		for ( int32 ParticleY = 0; ParticleY < GParticleSimulationTileSize; ++ParticleY )
		{
			for ( int32 ParticleX = 0; ParticleX < GParticleSimulationTileSize; ++ParticleX )
			{
				const float IndexX = TileOffset.X + ((float)ParticleX / (float)GParticleSimulationTextureSizeX) + (0.5f / (float)GParticleSimulationTextureSizeX);
				const float IndexY = TileOffset.Y + ((float)ParticleY / (float)GParticleSimulationTextureSizeY) + (0.5f / (float)GParticleSimulationTextureSizeY);
				ParticleIndices->X.SetWithoutBoundsChecks(IndexX);
				ParticleIndices->Y.SetWithoutBoundsChecks(IndexY);					

				// move to next particle
				ParticleIndices += Stride;
			}
		}
	}
	RHIUnlockVertexBuffer( VertexBufferRHI );
}

/*-----------------------------------------------------------------------------
	Determine bounds for GPU particles.
-----------------------------------------------------------------------------*/

/** The number of threads per group used to generate particle keys. */
#define PARTICLE_BOUNDS_THREADS 64

/**
 * Uniform buffer parameters for generating particle bounds.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FParticleBoundsParameters, )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, ChunksPerGroup )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, ExtraChunkCount )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( uint32, ParticleCount )
END_UNIFORM_BUFFER_STRUCT( FParticleBoundsParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FParticleBoundsParameters,TEXT("ParticleBounds"));

typedef TUniformBufferRef<FParticleBoundsParameters> FParticleBoundsUniformBufferRef;

/**
 * Compute shader used to generate particle bounds.
 */
class FParticleBoundsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FParticleBoundsCS,Global);

public:

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("THREAD_COUNT"), PARTICLE_BOUNDS_THREADS );
		OutEnvironment.SetDefine( TEXT("TEXTURE_SIZE_X"), GParticleSimulationTextureSizeX );
		OutEnvironment.SetDefine( TEXT("TEXTURE_SIZE_Y"), GParticleSimulationTextureSizeY );
		OutEnvironment.CompilerFlags.Add( CFLAG_StandardOptimization );
	}

	/** Default constructor. */
	FParticleBoundsCS()
	{
	}

	/** Initialization constructor. */
	explicit FParticleBoundsCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		InParticleIndices.Bind( Initializer.ParameterMap, TEXT("InParticleIndices") );
		PositionTexture.Bind( Initializer.ParameterMap, TEXT("PositionTexture") );
		PositionTextureSampler.Bind( Initializer.ParameterMap, TEXT("PositionTextureSampler") );
		OutBounds.Bind( Initializer.ParameterMap, TEXT("OutBounds") );
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << InParticleIndices;
		Ar << PositionTexture;
		Ar << PositionTextureSampler;
		Ar << OutBounds;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set output buffers for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef OutBoundsUAV )
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( OutBounds.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutBounds.GetBaseIndex(), OutBoundsUAV);
		}
	}

	/**
	 * Set input parameters.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList,
		FParticleBoundsUniformBufferRef& UniformBuffer,
		FShaderResourceViewRHIParamRef InIndicesSRV,
		FTexture2DRHIParamRef PositionTextureRHI
		)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FParticleBoundsParameters>(), UniformBuffer );
		if ( InParticleIndices.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), InIndicesSRV);
		}
		if ( PositionTexture.IsBound() )
		{
			RHICmdList.SetShaderTexture(ComputeShaderRHI, PositionTexture.GetBaseIndex(), PositionTextureRHI);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		if ( InParticleIndices.IsBound() )
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, InParticleIndices.GetBaseIndex(), FShaderResourceViewRHIParamRef());
		}
		if ( OutBounds.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutBounds.GetBaseIndex(), FUnorderedAccessViewRHIParamRef());
		}
	}

private:

	/** Input buffer containing particle indices. */
	FShaderResourceParameter InParticleIndices;
	/** Texture containing particle positions. */
	FShaderResourceParameter PositionTexture;
	FShaderResourceParameter PositionTextureSampler;
	/** Output key buffer. */
	FShaderResourceParameter OutBounds;
};
IMPLEMENT_SHADER_TYPE(,FParticleBoundsCS,TEXT("/Engine/Private/ParticleBoundsShader.usf"),TEXT("ComputeParticleBounds"),SF_Compute);

/**
 * Returns true if the Mins and Maxs consistutue valid bounds, i.e. Mins <= Maxs.
 */
static bool AreBoundsValid( const FVector& Mins, const FVector& Maxs )
{
	return Mins.X <= Maxs.X && Mins.Y <= Maxs.Y && Mins.Z <= Maxs.Z;
}

/**
 * Computes bounds for GPU particles. Note that this is slow as it requires
 * syncing with the GPU!
 * @param VertexBufferSRV - Vertex buffer containing particle indices.
 * @param PositionTextureRHI - Texture holding particle positions.
 * @param ParticleCount - The number of particles in the emitter.
 */
static FBox ComputeParticleBounds(
	FRHICommandListImmediate& RHICmdList,
	FShaderResourceViewRHIParamRef VertexBufferSRV,
	FTexture2DRHIParamRef PositionTextureRHI,
	int32 ParticleCount )
{
	FBox BoundingBox;
	FParticleBoundsParameters Parameters;
	FParticleBoundsUniformBufferRef UniformBuffer;

	if (ParticleCount > 0 && GMaxRHIFeatureLevel == ERHIFeatureLevel::SM5)
	{
		// Determine how to break the work up over individual work groups.
		const uint32 MaxGroupCount = 128;
		const uint32 AlignedParticleCount = ((ParticleCount + PARTICLE_BOUNDS_THREADS - 1) & (~(PARTICLE_BOUNDS_THREADS - 1)));
		const uint32 ChunkCount = AlignedParticleCount / PARTICLE_BOUNDS_THREADS;
		const uint32 GroupCount = FMath::Clamp<uint32>( ChunkCount, 1, MaxGroupCount );

		// Create the uniform buffer.
		Parameters.ChunksPerGroup = ChunkCount / GroupCount;
		Parameters.ExtraChunkCount = ChunkCount % GroupCount;
		Parameters.ParticleCount = ParticleCount;
		UniformBuffer = FParticleBoundsUniformBufferRef::CreateUniformBufferImmediate( Parameters, UniformBuffer_SingleFrame );

		// Create a buffer for storing bounds.
		const int32 BufferSize = GroupCount * 2 * sizeof(FVector4);
		FRHIResourceCreateInfo CreateInfo;
		FVertexBufferRHIRef BoundsVertexBufferRHI = RHICreateVertexBuffer(
			BufferSize,
			BUF_Static | BUF_UnorderedAccess,
			CreateInfo);
		FUnorderedAccessViewRHIRef BoundsVertexBufferUAV = RHICreateUnorderedAccessView(
			BoundsVertexBufferRHI,
			PF_A32B32G32R32F );

		// Grab the shader.
		TShaderMapRef<FParticleBoundsCS> ParticleBoundsCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		RHICmdList.SetComputeShader(ParticleBoundsCS->GetComputeShader());

		// Dispatch shader to compute bounds.
		ParticleBoundsCS->SetOutput(RHICmdList, BoundsVertexBufferUAV);
		ParticleBoundsCS->SetParameters(RHICmdList, UniformBuffer, VertexBufferSRV, PositionTextureRHI);
		DispatchComputeShader(
			RHICmdList, 
			*ParticleBoundsCS, 
			GroupCount,
			1,
			1 );
		ParticleBoundsCS->UnbindBuffers(RHICmdList);

		// Read back bounds.
		FVector4* GroupBounds = (FVector4*)RHILockVertexBuffer( BoundsVertexBufferRHI, 0, BufferSize, RLM_ReadOnly );

		// Find valid starting bounds.
		uint32 GroupIndex = 0;
		do
		{
			BoundingBox.Min = FVector(GroupBounds[GroupIndex * 2 + 0]);
			BoundingBox.Max = FVector(GroupBounds[GroupIndex * 2 + 1]);
			GroupIndex++;
		} while ( GroupIndex < GroupCount && !AreBoundsValid( BoundingBox.Min, BoundingBox.Max ) );

		if ( GroupIndex == GroupCount )
		{
			// No valid bounds!
			BoundingBox.Init();
		}
		else
		{
			// Bounds are valid. Add any other valid bounds.
			BoundingBox.IsValid = true;
			while ( GroupIndex < GroupCount )
			{
				const FVector Mins( GroupBounds[GroupIndex * 2 + 0] );
				const FVector Maxs( GroupBounds[GroupIndex * 2 + 1] );
				if ( AreBoundsValid( Mins, Maxs ) )
				{
					BoundingBox += Mins;
					BoundingBox += Maxs;
				}
				GroupIndex++;
			}
		}

		// Release buffer.
		RHICmdList.UnlockVertexBuffer(BoundsVertexBufferRHI);
		BoundsVertexBufferUAV.SafeRelease();
		BoundsVertexBufferRHI.SafeRelease();
	}
	else
	{
		BoundingBox.Init();
	}

	return BoundingBox;
}

/*-----------------------------------------------------------------------------
	Per-emitter GPU particle simulation.
-----------------------------------------------------------------------------*/

/**
 * Per-emitter resources for simulation.
 */
struct FParticleEmitterSimulationResources
{
	/** Emitter uniform buffer used for simulation. */
	FParticleSimulationBufferRef SimulationUniformBuffer;
	/** Scale to apply to global vector fields. */
	float GlobalVectorFieldScale;
	/** Tightness override value to apply to global vector fields. */
	float GlobalVectorFieldTightness;
};

/**
 * Vertex buffer used to hold tile offsets.
 */
class FParticleTileVertexBuffer : public FVertexBuffer
{
public:
	/** Shader resource of the vertex buffer. */
	FShaderResourceViewRHIRef VertexBufferSRV;
	/** The number of tiles held by this vertex buffer. */
	int32 TileCount;
	/** The number of tiles held by this vertex buffer, aligned for tile rendering. */
	int32 AlignedTileCount;

	/** Default constructor. */
	FParticleTileVertexBuffer()
		: TileCount(0)
		, AlignedTileCount(0)
	{
	}
	
	
	FParticleShaderParamRef GetShaderParam() { return VertexBufferSRV; }

	/**
	 * Initializes the vertex buffer from a list of tiles.
	 */
	void Init( const TArray<uint32>& Tiles )
	{
		check( IsInRenderingThread() );
		TileCount = Tiles.Num();
		AlignedTileCount = ComputeAlignedTileCount(TileCount);
		InitResource();
		if ( Tiles.Num() )
		{
			BuildTileVertexBuffer( VertexBufferRHI, Tiles );
		}
	}

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		if ( AlignedTileCount > 0 )
		{
			const int32 TileBufferSize = AlignedTileCount * sizeof(FVector2D);
			check(TileBufferSize > 0);
			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer( TileBufferSize, BUF_Static | BUF_KeepCPUAccessible | BUF_ShaderResource, CreateInfo );
			VertexBufferSRV = RHICreateShaderResourceView( VertexBufferRHI, /*Stride=*/ sizeof(FVector2D), PF_G32R32F );
		}
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		TileCount = 0;
		AlignedTileCount = 0;
		VertexBufferSRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}
};

/**
 * Vertex buffer used to hold particle indices.
 */
class FGPUParticleVertexBuffer : public FParticleIndicesVertexBuffer
{
public:

	/** The number of particles referenced by this vertex buffer. */
	int32 ParticleCount;

	/** Default constructor. */
	FGPUParticleVertexBuffer()
		: ParticleCount(0)
	{
	}

	/**
	 * Initializes the vertex buffer from a list of tiles.
	 */
	void Init( const TArray<uint32>& Tiles )
	{
		check( IsInRenderingThread() );
		ParticleCount = Tiles.Num() * GParticlesPerTile;
		InitResource();
		if ( Tiles.Num() )
		{
			BuildParticleVertexBuffer( VertexBufferRHI, Tiles );
		}
	}

	/** Initialize RHI resources. */
	virtual void InitRHI() override
	{
		if ( RHISupportsGPUParticles() )
		{
			// Metal *requires* that a buffer be bound - you cannot protect access with a branch in the shader.
			int32 Count = FMath::Max(ParticleCount, 1);
			const int32 BufferStride = sizeof(FParticleIndex);
			const int32 BufferSize = Count * BufferStride;
			uint32 Flags = BUF_Static | /*BUF_KeepCPUAccessible | */BUF_ShaderResource;
			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer(BufferSize, Flags, CreateInfo);
			VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, BufferStride, PF_G16R16F);
		}
	}
};

/**
 * Resources for simulating a set of particles on the GPU.
 */
class FParticleSimulationGPU
{
public:

	/** The vertex buffer used to access tiles in the simulation. */
	FParticleTileVertexBuffer TileVertexBuffer;
	/** Reference to the GPU sprite resources. */
	TRefCountPtr<FGPUSpriteResources> GPUSpriteResources;
	/** The per-emitter simulation resources. */
	const FParticleEmitterSimulationResources* EmitterSimulationResources;
	/** The per-frame simulation uniform buffer. */
	FParticlePerFrameSimulationParameters PerFrameSimulationParameters;
	/** Bounds for particles in the simulation. */
	FBox Bounds;

	/** A list of new particles to inject in to the simulation for this emitter. */
	TArray<FNewParticle> NewParticles;
	/** A list of tiles to clear that were newly allocated for this emitter. */
	TArray<uint32> TilesToClear;

	/** Local vector field. */
	FVectorFieldInstance LocalVectorField;

	/** The vertex buffer used to access particles in the simulation. */
	FGPUParticleVertexBuffer VertexBuffer;
	/** The vertex factory for visualizing the local vector field. */
	FVectorFieldVisualizationVertexFactory* VectorFieldVisualizationVertexFactory;

	/** The simulation index within the associated FX system. */
	int32 SimulationIndex;

	/**
	 * The phase in which these particles should simulate.
	 */
	EParticleSimulatePhase::Type SimulationPhase;

	/** True if the simulation wants collision enabled. */
	bool bWantsCollision;

	EParticleCollisionMode::Type CollisionMode;

	/** Flag that specifies the simulation's resources are dirty and need to be updated. */
	bool bDirty_GameThread;
	bool bReleased_GameThread;
	bool bDestroyed_GameThread;

	/** Allows disabling of simulation. */
	bool bEnabled;

	// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
	bool bEnableGridInteraction;
	TEnumAsByte<enum EInteractionChannelNvFlow> InteractionChannel;
	struct FInteractionResponseContainerNvFlow ResponseToInteractionChannels;
#endif
	// NvFlow end

	/** Default constructor. */
	FParticleSimulationGPU()
		: EmitterSimulationResources(NULL)
		, VectorFieldVisualizationVertexFactory(NULL)
		, SimulationIndex(INDEX_NONE)
		, SimulationPhase(EParticleSimulatePhase::Main)
		, bWantsCollision(false)
		, CollisionMode(EParticleCollisionMode::SceneDepth)
		, bDirty_GameThread(true)
		, bReleased_GameThread(true)
		, bDestroyed_GameThread(false)
		, bEnabled(true)
		// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
		, bEnableGridInteraction(false)
		, InteractionChannel(EIC_Channel1)
#endif
		// NvFlow end
	{
	}

	/** Destructor. */
	~FParticleSimulationGPU()
	{
		delete VectorFieldVisualizationVertexFactory;
		VectorFieldVisualizationVertexFactory = NULL;
	}

	/**
	 * Initializes resources for simulating particles on the GPU.
	 * @param Tiles							The list of tiles to include in the simulation.
	 * @param InEmitterSimulationResources	The emitter resources used by this simulation.
	 */
	void InitResources(const TArray<uint32>& Tiles, FGPUSpriteResources* InGPUSpriteResources);

	/**
	 * Create and initializes a visualization vertex factory if needed.
	 */
	void CreateVectorFieldVisualizationVertexFactory()
	{
		if (VectorFieldVisualizationVertexFactory == NULL)
		{
			check(IsInRenderingThread());
			VectorFieldVisualizationVertexFactory = new FVectorFieldVisualizationVertexFactory();
			VectorFieldVisualizationVertexFactory->InitResource();
		}
	}

	/**
	 * Release and destroy simulation resources.
	 */
	void Destroy()
	{
		bDestroyed_GameThread = true;
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FReleaseParticleSimulationGPUCommand,
			FParticleSimulationGPU*, Simulation, this,
		{
			Simulation->Destroy_RenderThread();
		});
	}

	/**
	 * Destroy the simulation on the rendering thread.
	 */
	void Destroy_RenderThread()
	{
		// The check for GIsRequestingExit is done because at shut down UWorld can be destroyed before particle emitters(?)
		check(GIsRequestingExit || SimulationIndex == INDEX_NONE);
		ReleaseRenderResources();
		delete this;
	}

	/**
	 * Enqueues commands to release render resources.
	 */
	void BeginReleaseResources()
	{
		bReleased_GameThread = true;
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FReleaseParticleSimulationResourcesGPUCommand,
			FParticleSimulationGPU*, Simulation, this,
		{
			Simulation->ReleaseRenderResources();
		});
	}

private:

	/**
	 * Release resources on the rendering thread.
	 */
	void ReleaseRenderResources()
	{
		check( IsInRenderingThread() );
		VertexBuffer.ReleaseResource();
		TileVertexBuffer.ReleaseResource();
		if ( VectorFieldVisualizationVertexFactory )
		{
			VectorFieldVisualizationVertexFactory->ReleaseResource();
		}
	}
};

/*-----------------------------------------------------------------------------
	Dynamic emitter data for GPU sprite particles.
-----------------------------------------------------------------------------*/

/**
 * Per-emitter resources for GPU sprites.
 */
class FGPUSpriteResources : public FRenderResource
{
public:

	/** Emitter uniform buffer used for rendering. */
	FGPUSpriteEmitterUniformBufferRef UniformBuffer;
	/** Emitter simulation resources. */
	FParticleEmitterSimulationResources EmitterSimulationResources;
	/** Texel allocation for the color curve. */
	FTexelAllocation ColorTexelAllocation;
	/** Texel allocation for the misc attributes curve. */
	FTexelAllocation MiscTexelAllocation;
	/** Texel allocation for the simulation attributes curve. */
	FTexelAllocation SimulationAttrTexelAllocation;
	/** Emitter uniform parameters used for rendering. */
	FGPUSpriteEmitterUniformParameters UniformParameters;
	/** Emitter uniform parameters used for simulation. */
	FParticleSimulationParameters SimulationParameters;

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		UniformBuffer = FGPUSpriteEmitterUniformBufferRef::CreateUniformBufferImmediate( UniformParameters, UniformBuffer_MultiFrame );
		EmitterSimulationResources.SimulationUniformBuffer =
			FParticleSimulationBufferRef::CreateUniformBufferImmediate( SimulationParameters, UniformBuffer_MultiFrame );
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		UniformBuffer.SafeRelease();
		EmitterSimulationResources.SimulationUniformBuffer.SafeRelease();
	}

	inline uint32 AddRef()
	{
		return NumRefs.Increment();
	}

	inline uint32 Release()
	{
		int32 Refs = NumRefs.Decrement();
		check(Refs >= 0);

		if (Refs == 0)
		{
			// When all references are released, we need the render thread
			// to release RHI resources and delete this instance.
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				ReleaseCommand,
				FRenderResource*, Resource, this,
				{
					Resource->ReleaseResource();
					delete Resource;
				});
		}
		return Refs;
	}

private:
	FThreadSafeCounter NumRefs;
};

void FParticleSimulationGPU::InitResources(const TArray<uint32>& Tiles, FGPUSpriteResources* InGPUSpriteResources)
{
	ensure(InGPUSpriteResources);

	if (InGPUSpriteResources)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			FInitParticleSimulationGPUCommand,
			FParticleSimulationGPU*, Simulation, this,
			TArray<uint32>, Tiles, Tiles,
			TRefCountPtr<FGPUSpriteResources>, InGPUSpriteResources, InGPUSpriteResources, // TRefCountPtr to take reference for lifetime of this render command
			{
				// Release vertex buffers.
				Simulation->VertexBuffer.ReleaseResource();
				Simulation->TileVertexBuffer.ReleaseResource();

				// Initialize new buffers with list of tiles.
				Simulation->VertexBuffer.Init(Tiles);
				Simulation->TileVertexBuffer.Init(Tiles);

				// Store simulation resources for this emitter.
				Simulation->GPUSpriteResources = InGPUSpriteResources;
				Simulation->EmitterSimulationResources = &InGPUSpriteResources->EmitterSimulationResources;

				// If a visualization vertex factory has been created, initialize it.
				if (Simulation->VectorFieldVisualizationVertexFactory)
				{
					Simulation->VectorFieldVisualizationVertexFactory->InitResource();
				}
		});
	}

	bDirty_GameThread = false;
	bReleased_GameThread = false;
}

class FGPUSpriteCollectorResources : public FOneFrameResource
{
public:
	FGPUSpriteVertexFactory *VertexFactory;

	~FGPUSpriteCollectorResources()
	{
	}
};

// recycle memory blocks for the NewParticle array
static void FreeNewParticleArray(TArray<FNewParticle>& NewParticles)
{
	NewParticles.Reset();
}

static void GetNewParticleArray(TArray<FNewParticle>& NewParticles, int32 NumParticlesNeeded = -1)
{
	if (NumParticlesNeeded > 0)
	{
		NewParticles.Reserve(NumParticlesNeeded);
	}
}
/**
 * Dynamic emitter data for Cascade.
 */
struct FGPUSpriteDynamicEmitterData : FDynamicEmitterDataBase
{
	/** FX system. */
	FFXSystem* FXSystem;
	/** Per-emitter resources. */
	FGPUSpriteResources* Resources;
	/** Simulation resources. */
	FParticleSimulationGPU* Simulation;
	/** Bounds for particles in the simulation. */
	FBox SimulationBounds;
	/** The material with which to render sprites. */
	UMaterialInterface* Material;
	/** A list of new particles to inject in to the simulation for this emitter. */
	TArray<FNewParticle> NewParticles;
	/** A list of tiles to clear that were newly allocated for this emitter. */
	TArray<uint32> TilesToClear;
	/** Vector field-to-world transform. */
	FMatrix LocalVectorFieldToWorld;
	/** Vector field scale. */
	float LocalVectorFieldIntensity;
	/** Vector field tightness. */
	float LocalVectorFieldTightness;
	/** Per-frame simulation parameters. */
	FParticlePerFrameSimulationParameters PerFrameSimulationParameters;
	/** Per-emitter parameters that may change*/
	FGPUSpriteEmitterDynamicUniformParameters EmitterDynamicParameters;
	/** How the particles should be sorted, if at all. */
	EParticleSortMode SortMode;
	/** Whether to render particles in local space or world space. */
	bool bUseLocalSpace;	
	/** Tile vector field in x axis? */
	uint32 bLocalVectorFieldTileX : 1;
	/** Tile vector field in y axis? */
	uint32 bLocalVectorFieldTileY : 1;
	/** Tile vector field in z axis? */
	uint32 bLocalVectorFieldTileZ : 1;
	/** Tile vector field in z axis? */
	uint32 bLocalVectorFieldUseFixDT : 1;


	/** Current MacroUV override settings */
	FMacroUVOverride MacroUVOverride;

	/** Constructor. */
	explicit FGPUSpriteDynamicEmitterData( const UParticleModuleRequired* InRequiredModule )
		: FDynamicEmitterDataBase( InRequiredModule )
		, FXSystem(NULL)
		, Resources(NULL)
		, Simulation(NULL)
		, Material(NULL)
		, SortMode(PSORTMODE_None)
		, bLocalVectorFieldTileX(false)
		, bLocalVectorFieldTileY(false)
		, bLocalVectorFieldTileZ(false)
		, bLocalVectorFieldUseFixDT(false)
	{
		GetNewParticleArray(NewParticles);
	}
	~FGPUSpriteDynamicEmitterData()
	{
		FreeNewParticleArray(NewParticles);
	}

	bool RendersWithTranslucentMaterial() const
	{
		EBlendMode BlendMode = Material->GetBlendMode();
		return IsTranslucentBlendMode(BlendMode);
	}

	/**
	 * Called to create render thread resources.
	 */
	virtual void UpdateRenderThreadResourcesEmitter(const FParticleSystemSceneProxy* InOwnerProxy) override
	{
		check(Simulation);

		// Update the per-frame simulation parameters with those provided from the game thread.
		Simulation->PerFrameSimulationParameters = PerFrameSimulationParameters;

		// Local vector field parameters.
		Simulation->LocalVectorField.Intensity = LocalVectorFieldIntensity;
		Simulation->LocalVectorField.Tightness = LocalVectorFieldTightness;
		Simulation->LocalVectorField.bTileX = bLocalVectorFieldTileX;
		Simulation->LocalVectorField.bTileY = bLocalVectorFieldTileY;
		Simulation->LocalVectorField.bTileZ = bLocalVectorFieldTileZ;
		Simulation->LocalVectorField.bUseFixDT = bLocalVectorFieldUseFixDT;

		if (Simulation->LocalVectorField.Resource)
		{
			Simulation->LocalVectorField.UpdateTransforms(LocalVectorFieldToWorld);
		}

		// Update world bounds.
		Simulation->Bounds = SimulationBounds;

		// Transfer ownership of new data.
		if (NewParticles.Num())
		{
			Exchange(Simulation->NewParticles, NewParticles);
		}
		if (TilesToClear.Num())
		{
			Exchange(Simulation->TilesToClear, TilesToClear);
		}

		const bool bTranslucent = RendersWithTranslucentMaterial();
		const bool bSupportsDepthBufferCollision = IsParticleCollisionModeSupported(FXSystem->GetShaderPlatform(), PCM_DepthBuffer);

		// If the simulation wants to collide against the depth buffer
		// and we're not rendering with an opaque material put the 
		// simulation in the collision phase.
		if (bTranslucent && Simulation->bWantsCollision && Simulation->CollisionMode == EParticleCollisionMode::SceneDepth)
		{
			Simulation->SimulationPhase = bSupportsDepthBufferCollision ? EParticleSimulatePhase::CollisionDepthBuffer : EParticleSimulatePhase::Main;
		}
		else if (Simulation->bWantsCollision && Simulation->CollisionMode == EParticleCollisionMode::DistanceField)
		{
			if (IsParticleCollisionModeSupported(FXSystem->GetShaderPlatform(), PCM_DistanceField))
			{
				Simulation->SimulationPhase = EParticleSimulatePhase::CollisionDistanceField;
			}
			else if (bTranslucent && bSupportsDepthBufferCollision)
			{
				// Fall back to scene depth collision if translucent
				Simulation->SimulationPhase = EParticleSimulatePhase::CollisionDepthBuffer;
			}
			else
			{
				Simulation->SimulationPhase = EParticleSimulatePhase::Main;
			}
		}
	}

	/**
	 * Called to release render thread resources.
	 */
	virtual void ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy) override
	{		
	}

	virtual FParticleVertexFactoryBase *CreateVertexFactory() override
	{
		FGPUSpriteVertexFactory *VertexFactory = new FGPUSpriteVertexFactory();
		VertexFactory->InitResource();
		return VertexFactory;
	}

	virtual void GetDynamicMeshElementsEmitter(const FParticleSystemSceneProxy* Proxy, const FSceneView* View, const FSceneViewFamily& ViewFamily, int32 ViewIndex, FMeshElementCollector& Collector, FParticleVertexFactoryBase *InVertexFactory) const override
	{
		auto FeatureLevel = ViewFamily.GetFeatureLevel();

		if (RHISupportsGPUParticles())
		{
			SCOPE_CYCLE_COUNTER(STAT_GPUSpritePreRenderTime);

			check(Simulation);

			// Do not render orphaned emitters. This can happen if the emitter
			// instance has been destroyed but we are rendering before the
			// scene proxy has received the update to clear dynamic data.
			if (Simulation->SimulationIndex != INDEX_NONE
				&& Simulation->VertexBuffer.ParticleCount > 0)
			{
				FGPUSpriteEmitterDynamicUniformParameters PerViewDynamicParameters = EmitterDynamicParameters;
				FVector2D ObjectNDCPosition;
				FVector2D ObjectMacroUVScales;
				Proxy->GetObjectPositionAndScale(*View,ObjectNDCPosition, ObjectMacroUVScales);
				PerViewDynamicParameters.MacroUVParameters = FVector4(ObjectNDCPosition.X, ObjectNDCPosition.Y, ObjectMacroUVScales.X, ObjectMacroUVScales.Y); 

				FGPUSpriteEmitterDynamicUniformBufferRef LocalDynamicUniformBuffer;
				// Do here rather than in CreateRenderThreadResources because in some cases Render can be called before CreateRenderThreadResources
				{
					// Create per-emitter uniform buffer for dynamic parameters
					LocalDynamicUniformBuffer = FGPUSpriteEmitterDynamicUniformBufferRef::CreateUniformBufferImmediate(PerViewDynamicParameters, UniformBuffer_SingleFrame);
				}

				if (bUseLocalSpace == false)
				{
					Proxy->UpdateWorldSpacePrimitiveUniformBuffer();
				}

				const bool bTranslucent = RendersWithTranslucentMaterial();
				const bool bAllowSorting = FXConsoleVariables::bAllowGPUSorting
					&& FeatureLevel == ERHIFeatureLevel::SM5
					&& bTranslucent;

				// Iterate over views and assign parameters for each.
				FParticleSimulationResources* SimulationResources = FXSystem->GetParticleSimulationResources();
				FGPUSpriteCollectorResources& CollectorResources = Collector.AllocateOneFrameResource<FGPUSpriteCollectorResources>();
				//CollectorResources.VertexFactory.InitResource();
				CollectorResources.VertexFactory = static_cast<FGPUSpriteVertexFactory*>(InVertexFactory);
				CollectorResources.VertexFactory->SetFeatureLevel(FeatureLevel);
				FGPUSpriteVertexFactory& VertexFactory = *CollectorResources.VertexFactory;

				if (bAllowSorting && SortMode == PSORTMODE_DistanceToView)
				{
					// Extensibility TODO: This call to AddSortedGPUSimulation is very awkward. When rendering a frame we need to
					// accumulate all GPU particle emitters that need to be sorted. That is so they can be sorted in one big radix
					// sort for efficiency. Ideally that state is per-scene renderer but the renderer doesn't know anything about particles.
					const int32 SortedBufferOffset = FXSystem->AddSortedGPUSimulation(Simulation, View->ViewMatrices.GetViewOrigin());
					check(SimulationResources->SortedVertexBuffer.IsInitialized());
					VertexFactory.SetVertexBuffer(&SimulationResources->SortedVertexBuffer, SortedBufferOffset);
				}
				else
				{
					check(Simulation->VertexBuffer.IsInitialized());
					VertexFactory.SetVertexBuffer(&Simulation->VertexBuffer, 0);
				}

				const int32 ParticleCount = Simulation->VertexBuffer.ParticleCount;
				const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;

				{
					SCOPE_CYCLE_COUNTER(STAT_GPUSpriteRenderingTime);

					FParticleSimulationResources* ParticleSimulationResources = FXSystem->GetParticleSimulationResources();
					FParticleStateTextures& StateTextures = ParticleSimulationResources->GetVisualizeStateTextures();
							
					VertexFactory.EmitterUniformBuffer = Resources->UniformBuffer;
					VertexFactory.EmitterDynamicUniformBuffer = LocalDynamicUniformBuffer;
					VertexFactory.PositionTextureRHI = StateTextures.PositionTextureRHI;
					VertexFactory.VelocityTextureRHI = StateTextures.VelocityTextureRHI;
					VertexFactory.AttributesTextureRHI = ParticleSimulationResources->RenderAttributesTexture.TextureRHI;

					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.IndexBuffer = &GParticleIndexBuffer;
					BatchElement.NumPrimitives = MAX_PARTICLES_PER_INSTANCE * 2;
					BatchElement.NumInstances = ParticleCount / MAX_PARTICLES_PER_INSTANCE;
					BatchElement.FirstIndex = 0;
					BatchElement.bIsInstancedMesh = true;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.LCI = NULL;
					if ( bUseLocalSpace )
					{
						BatchElement.PrimitiveUniformBufferResource = &Proxy->GetUniformBuffer();
					}
					else
					{
						BatchElement.PrimitiveUniformBufferResource = &Proxy->GetWorldSpacePrimitiveUniformBuffer();
					}
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = 3;
					Mesh.ReverseCulling = Proxy->IsLocalToWorldDeterminantNegative();
					Mesh.CastShadow = Proxy->GetCastShadow();
					Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)Proxy->GetDepthPriorityGroup(View);
					const bool bUseSelectedMaterial = GIsEditor && (ViewFamily.EngineShowFlags.Selection) ? bSelected : 0;
					Mesh.MaterialRenderProxy = Material->GetRenderProxy(bUseSelectedMaterial);
					Mesh.Type = PT_TriangleList;
					Mesh.bCanApplyViewModeOverrides = true;
					Mesh.bUseWireframeSelectionColoring = Proxy->IsSelected();

					Collector.AddMesh(ViewIndex, Mesh);
				}

				const bool bHaveLocalVectorField = Simulation && Simulation->LocalVectorField.Resource;
				if (bHaveLocalVectorField && ViewFamily.EngineShowFlags.VectorFields)
				{
					// Create a vertex factory for visualization if needed.
					Simulation->CreateVectorFieldVisualizationVertexFactory();
					check(Simulation->VectorFieldVisualizationVertexFactory);
					DrawVectorFieldBounds(Collector.GetPDI(ViewIndex), View, &Simulation->LocalVectorField);
					GetVectorFieldMesh(Simulation->VectorFieldVisualizationVertexFactory, &Simulation->LocalVectorField, ViewIndex, Collector);
				}
			}
		}
	}

	/**
	 * Retrieves the material render proxy with which to render sprites.
	 */
	virtual const FMaterialRenderProxy* GetMaterialRenderProxy(bool bInSelected) override
	{
		check( Material );
		return Material->GetRenderProxy( bInSelected );
	}

	/**
	 * Emitter replay data. A dummy value is returned as data is stored on the GPU.
	 */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		static FDynamicEmitterReplayDataBase DummyData;
		return DummyData;
	}

	/** Returns the current macro uv override. */
	virtual const FMacroUVOverride& GetMacroUVOverride() const { return MacroUVOverride; }

};

/*-----------------------------------------------------------------------------
	Particle emitter instance for GPU particles.
-----------------------------------------------------------------------------*/

#if TRACK_TILE_ALLOCATIONS
TMap<FFXSystem*,TSet<class FGPUSpriteParticleEmitterInstance*> > GPUSpriteParticleEmitterInstances;
#endif // #if TRACK_TILE_ALLOCATIONS

/**
 * Particle emitter instance for Cascade.
 */
class FGPUSpriteParticleEmitterInstance : public FParticleEmitterInstance
{
	/** Pointer the the FX system with which the instance is associated. */
	FFXSystem* FXSystem;
	/** Information on how to emit and simulate particles. */
	FGPUSpriteEmitterInfo& EmitterInfo;
	/** GPU simulation resources. */
	FParticleSimulationGPU* Simulation;
	/** The list of tiles active for this emitter. */
	TArray<uint32> AllocatedTiles;
	/** Bit array specifying which tiles are free for spawning new particles. */
	TBitArray<> ActiveTiles;
	/** The time at which each active tile will no longer have active particles. */
	TArray<float> TileTimeOfDeath;
	/** The list of tiles that need to be cleared. */
	TArray<uint32> TilesToClear;
	/** The list of new particles generated this time step. */
	TArray<FNewParticle> NewParticles;
	/** The list of force spawned particles from events */
	TArray<FNewParticle> ForceSpawnedParticles;
	/** The list of force spawned particles from events using Bursts */
	TArray<FNewParticle> ForceBurstSpawnedParticles;
	/** The rotation to apply to the local vector field. */
	FRotator LocalVectorFieldRotation;
	/** The strength of the point attractor. */
	float PointAttractorStrength;
	/** The amount of time by which the GPU needs to simulate particles during its next update. */
	float PendingDeltaSeconds;
	/** The offset for simulation time, used when we are not updating time FrameIndex. */
	float OffsetSeconds;

	/** Tile to allocate new particles from. */
	int32 TileToAllocateFrom;
	/** How many particles are free in the most recently allocated tile. */
	int32 FreeParticlesInTile;
	/** Random stream for this emitter. */
	FRandomStream RandomStream;
	/** The number of times this emitter should loop. */
	int32 AllowedLoopCount;

	/**
	 * Information used to spawn particles.
	 */
	struct FSpawnInfo
	{
		/** Number of particles to spawn. */
		int32 Count;
		/** Time at which the first particle spawned. */
		float StartTime;
		/** Amount by which to increment time for each subsequent particle. */
		float Increment;

		/** Default constructor. */
		FSpawnInfo()
			: Count(0)
			, StartTime(0.0f)
			, Increment(0.0f)
		{
		}
	};

public:

	/** Initialization constructor. */
	FGPUSpriteParticleEmitterInstance(FFXSystem* InFXSystem, FGPUSpriteEmitterInfo& InEmitterInfo)
		: FXSystem(InFXSystem)
		, EmitterInfo(InEmitterInfo)
		, LocalVectorFieldRotation(FRotator::ZeroRotator)
		, PointAttractorStrength(0.0f)
		, PendingDeltaSeconds(0.0f)
		, OffsetSeconds(0.0f)
		, TileToAllocateFrom(INDEX_NONE)
		, FreeParticlesInTile(0)
		, AllowedLoopCount(0)
	{
		Simulation = new FParticleSimulationGPU();
		if (EmitterInfo.LocalVectorField.Field)
		{
			EmitterInfo.LocalVectorField.Field->InitInstance(&Simulation->LocalVectorField, /*bPreviewInstance=*/ false);
		}
		Simulation->bWantsCollision = InEmitterInfo.bEnableCollision;
		Simulation->CollisionMode = InEmitterInfo.CollisionMode;

		// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
		Simulation->bEnableGridInteraction = InEmitterInfo.bEnableGridInteraction;
		Simulation->InteractionChannel = InEmitterInfo.InteractionChannel;
		Simulation->ResponseToInteractionChannels = InEmitterInfo.ResponseToInteractionChannels;
#endif
		// NvFlow end

#if TRACK_TILE_ALLOCATIONS
		TSet<class FGPUSpriteParticleEmitterInstance*>* EmitterSet = GPUSpriteParticleEmitterInstances.Find(FXSystem);
		if (!EmitterSet)
		{
			EmitterSet = &GPUSpriteParticleEmitterInstances.Add(FXSystem,TSet<class FGPUSpriteParticleEmitterInstance*>());
		}
		EmitterSet->Add(this);
#endif // #if TRACK_TILE_ALLOCATIONS
	}

	/** Destructor. */
	virtual ~FGPUSpriteParticleEmitterInstance()
	{
		ReleaseSimulationResources();
		Simulation->Destroy();
		Simulation = NULL;

#if TRACK_TILE_ALLOCATIONS
		TSet<class FGPUSpriteParticleEmitterInstance*>* EmitterSet = GPUSpriteParticleEmitterInstances.Find(FXSystem);
		check(EmitterSet);
		EmitterSet->Remove(this);
		if (EmitterSet->Num() == 0)
		{
			GPUSpriteParticleEmitterInstances.Remove(FXSystem);
		}
#endif // #if TRACK_TILE_ALLOCATIONS
	}

	/**
	 * Returns the number of tiles allocated to this emitter.
	 */
	int32 GetAllocatedTileCount() const
	{
		return AllocatedTiles.Num();
	}

	/**
	 *	Checks some common values for GetDynamicData validity
	 *
	 *	@return	bool		true if GetDynamicData should continue, false if it should return NULL
	 */
	virtual bool IsDynamicDataRequired(UParticleLODLevel* InCurrentLODLevel) override
	{
		bool bShouldRender = (ActiveParticles >= 0 || TilesToClear.Num() || NewParticles.Num());
		bool bCanRender = (FXSystem != NULL) && (Component != NULL) && (Component->FXSystem == FXSystem);
		return bShouldRender && bCanRender;
	}

	/**
	 *	Retrieves the dynamic data for the emitter
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected, ERHIFeatureLevel::Type InFeatureLevel) override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicEmitterDataBase_GetDynamicData);
		check(Component);
		check(SpriteTemplate);
		check(FXSystem);
		check(Component->FXSystem == FXSystem);

		// Grab the current LOD level
		UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
		if (LODLevel->bEnabled == false || !bEnabled)
		{
			return NULL;
		}

		UParticleSystem *Template = Component->Template;

		const bool bLocalSpace = EmitterInfo.RequiredModule->bUseLocalSpace;
		const FMatrix ComponentToWorldMatrix = Component->GetComponentTransform().ToMatrixWithScale();
		const FMatrix ComponentToWorld = (bLocalSpace || EmitterInfo.LocalVectorField.bIgnoreComponentTransform) ? FMatrix::Identity : ComponentToWorldMatrix;

		const FRotationMatrix VectorFieldTransform(LocalVectorFieldRotation);
		const FMatrix VectorFieldToWorld = VectorFieldTransform * EmitterInfo.LocalVectorField.Transform.ToMatrixWithScale() * ComponentToWorld;
		FGPUSpriteDynamicEmitterData* DynamicData = new FGPUSpriteDynamicEmitterData(EmitterInfo.RequiredModule);
		DynamicData->FXSystem = FXSystem;
		DynamicData->Resources = EmitterInfo.Resources;
		DynamicData->Material = GetCurrentMaterial();
		DynamicData->Simulation = Simulation;
		DynamicData->SimulationBounds = Template->bUseFixedRelativeBoundingBox ? Template->FixedRelativeBoundingBox.TransformBy(ComponentToWorldMatrix) : Component->Bounds.GetBox();
		DynamicData->LocalVectorFieldToWorld = VectorFieldToWorld;
		DynamicData->LocalVectorFieldIntensity = EmitterInfo.LocalVectorField.Intensity;
		DynamicData->LocalVectorFieldTightness = EmitterInfo.LocalVectorField.Tightness;	
		DynamicData->bLocalVectorFieldTileX = EmitterInfo.LocalVectorField.bTileX;	
		DynamicData->bLocalVectorFieldTileY = EmitterInfo.LocalVectorField.bTileY;	
		DynamicData->bLocalVectorFieldTileZ = EmitterInfo.LocalVectorField.bTileZ;	
		DynamicData->bLocalVectorFieldUseFixDT = EmitterInfo.LocalVectorField.bUseFixDT;
		DynamicData->SortMode = EmitterInfo.RequiredModule->SortMode;
		DynamicData->bSelected = bSelected;
		DynamicData->bUseLocalSpace = EmitterInfo.RequiredModule->bUseLocalSpace;

		// Account for LocalToWorld scaling
		FVector ComponentScale = Component->GetComponentTransform().GetScale3D();
		// Figure out if we need to replicate the X channel of size to Y.
		const bool bSquare = (EmitterInfo.ScreenAlignment == PSA_Square)
			|| (EmitterInfo.ScreenAlignment == PSA_FacingCameraPosition)
			|| (EmitterInfo.ScreenAlignment == PSA_FacingCameraDistanceBlend);

		DynamicData->EmitterDynamicParameters.LocalToWorldScale.X = ComponentScale.X;
		DynamicData->EmitterDynamicParameters.LocalToWorldScale.Y = (bSquare) ? ComponentScale.X : ComponentScale.Y;

		// Setup axis lock parameters if required.
		const FMatrix& LocalToWorld = ComponentToWorld;
		const EParticleAxisLock LockAxisFlag = (EParticleAxisLock)EmitterInfo.LockAxisFlag;
		DynamicData->EmitterDynamicParameters.AxisLockRight = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		DynamicData->EmitterDynamicParameters.AxisLockUp = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

		if(LockAxisFlag != EPAL_NONE)
		{
			FVector AxisLockUp, AxisLockRight;
			const FMatrix& AxisLocalToWorld = bLocalSpace ? LocalToWorld : FMatrix::Identity;
			extern void ComputeLockedAxes(EParticleAxisLock, const FMatrix&, FVector&, FVector&);
			ComputeLockedAxes( LockAxisFlag, AxisLocalToWorld, AxisLockUp, AxisLockRight );

			DynamicData->EmitterDynamicParameters.AxisLockRight = AxisLockRight;
			DynamicData->EmitterDynamicParameters.AxisLockRight.W = 1.0f;
			DynamicData->EmitterDynamicParameters.AxisLockUp = AxisLockUp;
			DynamicData->EmitterDynamicParameters.AxisLockUp.W = 1.0f;
		}

		
		// Setup dynamic color parameter. Only set when using particle parameter distributions.
		FVector4 ColorOverLife(1.0f, 1.0f, 1.0f, 1.0f);
		FVector4 ColorScaleOverLife(1.0f, 1.0f, 1.0f, 1.0f);
		if( EmitterInfo.DynamicColorScale.IsCreated() )
		{
			ColorScaleOverLife = EmitterInfo.DynamicColorScale.GetValue(0.0f,Component);
		}
		if( EmitterInfo.DynamicAlphaScale.IsCreated() )
		{
			ColorScaleOverLife.W = EmitterInfo.DynamicAlphaScale.GetValue(0.0f,Component);
		}

		if( EmitterInfo.DynamicColor.IsCreated() )
		{
			ColorOverLife = EmitterInfo.DynamicColor.GetValue(0.0f,Component);
		}
		if( EmitterInfo.DynamicAlpha.IsCreated() )
		{
			ColorOverLife.W = EmitterInfo.DynamicAlpha.GetValue(0.0f,Component);
		}
		DynamicData->EmitterDynamicParameters.DynamicColor = ColorOverLife * ColorScaleOverLife;

		DynamicData->MacroUVOverride.bOverride = LODLevel->RequiredModule->bOverrideSystemMacroUV;
		DynamicData->MacroUVOverride.Radius = LODLevel->RequiredModule->MacroUVRadius;
		DynamicData->MacroUVOverride.Position = LODLevel->RequiredModule->MacroUVPosition;

		const bool bSimulateGPUParticles = 
			FXConsoleVariables::bFreezeGPUSimulation == false &&
			FXConsoleVariables::bFreezeParticleSimulation == false &&
			RHISupportsGPUParticles();

		if (bSimulateGPUParticles)
		{
			float& DeltaSecondsInFix = DynamicData->PerFrameSimulationParameters.DeltaSecondsInFix;
			int32& NumIterationsInFix = DynamicData->PerFrameSimulationParameters.NumIterationsInFix;

			float& DeltaSecondsInVar = DynamicData->PerFrameSimulationParameters.DeltaSecondsInVar;
			int32& NumIterationsInVar = DynamicData->PerFrameSimulationParameters.NumIterationsInVar;
			
			const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnAnyThread();
			const float FixTolerance = CVarGPUParticleFixTolerance.GetValueOnAnyThread();
			const int32 MaxNumIterations = CVarGPUParticleMaxNumIterations.GetValueOnAnyThread();

			DeltaSecondsInFix = FixDeltaSeconds;
			NumIterationsInFix = 0;

			DeltaSecondsInVar = PendingDeltaSeconds + OffsetSeconds;
			NumIterationsInVar = 1;
			OffsetSeconds = 0;

			// If using fixDT strategy
			if (FixDeltaSeconds > 0)
			{
				if (!Simulation->LocalVectorField.bUseFixDT)
				{
					// With FixDeltaSeconds > 0, "InFix" is the persistent delta time, while "InVar" is only used for interpolation.
					Swap(DeltaSecondsInFix, DeltaSecondsInVar);
					Swap(NumIterationsInFix, NumIterationsInVar);
				}
				else
				{
					// Move some time from varying DT to fix DT simulation.
					NumIterationsInFix = FMath::FloorToInt(DeltaSecondsInVar / FixDeltaSeconds);
					DeltaSecondsInVar -= NumIterationsInFix * FixDeltaSeconds;

					float SecondsInFix = NumIterationsInFix * FixDeltaSeconds;

					const float RelativeVar = DeltaSecondsInVar / FixDeltaSeconds;

					// If we had some fixed steps, try to move a small value from var dt to fix dt as an optimization (skips on full simulation step)
					if (NumIterationsInFix > 0 && RelativeVar < FixTolerance)
					{
						SecondsInFix += DeltaSecondsInVar;
						DeltaSecondsInVar = 0;
						NumIterationsInVar = 0;
					}
					// Also check if there is almost one full step.
					else if (1.f - RelativeVar < FixTolerance) 
					{
						SecondsInFix += DeltaSecondsInVar;
						NumIterationsInFix += 1;
						DeltaSecondsInVar = 0;
						NumIterationsInVar = 0;
					}
					// Otherwise, transfer a part from the varying time to the fix time. At this point, we know we will have both fix and var iterations.
					// This prevents DT that are multiple of FixDT, from keeping an non zero OffsetSeconds.
					else if (NumIterationsInFix > 0)
					{
						const float TransferedSeconds = FixTolerance * FixDeltaSeconds;
						DeltaSecondsInVar -= TransferedSeconds;
						SecondsInFix += TransferedSeconds;
					}

					if (NumIterationsInFix > 0)
					{
						// Here we limit the iteration count to prevent long frames from taking even longer.
						NumIterationsInFix = FMath::Min<int32>(NumIterationsInFix, MaxNumIterations);
						DeltaSecondsInFix = SecondsInFix / (float)NumIterationsInFix;
					}

					OffsetSeconds = DeltaSecondsInVar;
				}

			#if STATS
				if (NumIterationsInFix + NumIterationsInVar == 1)
				{
					INC_DWORD_STAT_BY(STAT_GPUSingleIterationEmitters, 1);
				}
				else if (NumIterationsInFix + NumIterationsInVar > 1)
				{
					INC_DWORD_STAT_BY(STAT_GPUMultiIterationsEmitters, 1);
				}
			#endif

			}

			FVector PointAttractorPosition = ComponentToWorld.TransformPosition(EmitterInfo.PointAttractorPosition);
			DynamicData->PerFrameSimulationParameters.PointAttractor = FVector4(PointAttractorPosition, EmitterInfo.PointAttractorRadiusSq);
			DynamicData->PerFrameSimulationParameters.PositionOffsetAndAttractorStrength = FVector4(PositionOffsetThisTick, PointAttractorStrength);
			DynamicData->PerFrameSimulationParameters.LocalToWorldScale = DynamicData->EmitterDynamicParameters.LocalToWorldScale;
			DynamicData->PerFrameSimulationParameters.DeltaSeconds = PendingDeltaSeconds; // This value is used when updating vector fields.
			Exchange(DynamicData->TilesToClear, TilesToClear);
			Exchange(DynamicData->NewParticles, NewParticles);
		}
		FreeNewParticleArray(NewParticles);
		PendingDeltaSeconds = 0.0f;
		PositionOffsetThisTick = FVector::ZeroVector;

		if (Simulation->bDirty_GameThread)
		{
			Simulation->InitResources(AllocatedTiles, EmitterInfo.Resources);
		}
		check(!Simulation->bReleased_GameThread);
		check(!Simulation->bDestroyed_GameThread);

		return DynamicData;
	}

	/**
	 * Initializes the emitter.
	 */
	virtual void Init() override
	{
		SCOPE_CYCLE_COUNTER(STAT_GPUSpriteEmitterInstance_Init);

		FParticleEmitterInstance::Init();

		if (EmitterInfo.RequiredModule)
		{
			MaxActiveParticles = 0;
			ActiveParticles = 0;
			AllowedLoopCount = EmitterInfo.RequiredModule->EmitterLoops;
		}
		else
		{
			MaxActiveParticles = 0;
			ActiveParticles = 0;
			AllowedLoopCount = 0;
		}

		check(AllocatedTiles.Num() == TileTimeOfDeath.Num());
		FreeParticlesInTile = 0;

		RandomStream.Initialize( FMath::Rand() );

		FParticleSimulationResources* ParticleSimulationResources = FXSystem->GetParticleSimulationResources();
		const int32 MinTileCount = GetMinTileCount();
		int32 NumAllocated = 0;
		{
			while (AllocatedTiles.Num() < MinTileCount)
			{
				uint32 TileIndex = ParticleSimulationResources->AllocateTile();
				if ( TileIndex != INDEX_NONE )
				{
					AllocatedTiles.Add( TileIndex );
					TileTimeOfDeath.Add( 0.0f );
					NumAllocated++;
				}
				else
				{
					break;
				}
			}
		}
		
#if TRACK_TILE_ALLOCATIONS
		UE_LOG(LogParticles,VeryVerbose,
			TEXT("%s|%s|0x%016x [Init] %d tiles"),
			*Component->GetName(),*Component->Template->GetName(),(PTRINT)this, AllocatedTiles.Num());
#endif // #if TRACK_TILE_ALLOCATIONS

		bool bClearExistingParticles = false;
		UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels[0];
		if (LODLevel)
		{
			UParticleModuleTypeDataGpu* TypeDataModule = CastChecked<UParticleModuleTypeDataGpu>(LODLevel->TypeDataModule);
			bClearExistingParticles = TypeDataModule->bClearExistingParticlesOnInit;
		}

		if (bClearExistingParticles || ActiveTiles.Num() != AllocatedTiles.Num())
		{
			ActiveTiles.Init(false, AllocatedTiles.Num());
			ClearAllocatedTiles();
		}

		Simulation->bDirty_GameThread = true;
		FXSystem->AddGPUSimulation(Simulation);

		CurrentMaterial = EmitterInfo.RequiredModule ? EmitterInfo.RequiredModule->Material : UMaterial::GetDefaultMaterial(MD_Surface);

		InitLocalVectorField();
	}

	FORCENOINLINE void ReserveNewParticles(int32 Num)
	{
		if (Num)
		{
			if (!(NewParticles.Num() + NewParticles.GetSlack()))
			{
				GetNewParticleArray(NewParticles, Num);
			}
			else
			{
				NewParticles.Reserve(Num);
			}
		}
	}

	/**
	 * Simulates the emitter forward by the specified amount of time.
	 */
	virtual void Tick(float DeltaSeconds, bool bSuppressSpawning) override
	{
		FreeNewParticleArray(NewParticles);

		SCOPE_CYCLE_COUNTER(STAT_GPUSpriteTickTime);

		check(AllocatedTiles.Num() == TileTimeOfDeath.Num());

		if (FXConsoleVariables::bFreezeGPUSimulation ||
			FXConsoleVariables::bFreezeParticleSimulation ||
			!RHISupportsGPUParticles())
		{
			return;
		}

		// Grab the current LOD level
		UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

		// Handle EmitterTime setup, looping, etc.
		float EmitterDelay = Tick_EmitterTimeSetup( DeltaSeconds, LODLevel );

		Simulation->bEnabled = bEnabled;
		if (bEnabled)
		{
			// If the emitter is warming up but any particle spawned now will die
			// anyway, suppress spawning.
			if (Component && Component->bWarmingUp &&
				Component->WarmupTime - SecondsSinceCreation > EmitterInfo.MaxLifetime)
			{
				bSuppressSpawning = true;
			}

			// Mark any tiles with all dead particles as free.
			int32 ActiveTileCount = MarkTilesInactive();

			// Update modules
			Tick_ModuleUpdate(DeltaSeconds, LODLevel);

			// Spawn particles.
			bool bRefreshTiles = false;
			const bool bPreventSpawning = bHaltSpawning || bHaltSpawningExternal || bSuppressSpawning;
			const bool bValidEmitterTime = (EmitterTime >= 0.0f);
			const bool bValidLoop = AllowedLoopCount == 0 || LoopCount < AllowedLoopCount;
			if (!bPreventSpawning && bValidEmitterTime && bValidLoop)
			{
				SCOPE_CYCLE_COUNTER(STAT_GPUSpriteSpawnTime);

				// Determine burst count.
				FSpawnInfo BurstInfo;
				int32 LeftoverBurst = 0;
				{
					float BurstDeltaTime = DeltaSeconds;
					GetCurrentBurstRateOffset(BurstDeltaTime, BurstInfo.Count);

					BurstInfo.Count += ForceBurstSpawnedParticles.Num();

					if (BurstInfo.Count > FXConsoleVariables::MaxGPUParticlesSpawnedPerFrame)
					{
						LeftoverBurst = BurstInfo.Count - FXConsoleVariables::MaxGPUParticlesSpawnedPerFrame;
						BurstInfo.Count = FXConsoleVariables::MaxGPUParticlesSpawnedPerFrame;
					}
				}


				// Determine spawn count based on rate.
				FSpawnInfo SpawnInfo = GetNumParticlesToSpawn(DeltaSeconds);
				SpawnInfo.Count += ForceSpawnedParticles.Num();

				float SpawnRateMult = SpriteTemplate->GetQualityLevelSpawnRateMult();
				SpawnInfo.Count *= SpawnRateMult;
				BurstInfo.Count *= SpawnRateMult;

				int32 FirstBurstParticleIndex = NewParticles.Num();

				ReserveNewParticles(FirstBurstParticleIndex + BurstInfo.Count + SpawnInfo.Count);

				BurstInfo.Count = AllocateTilesForParticles(NewParticles, BurstInfo.Count, ActiveTileCount);

				int32 FirstSpawnParticleIndex = NewParticles.Num();
				SpawnInfo.Count = AllocateTilesForParticles(NewParticles, SpawnInfo.Count, ActiveTileCount);
				SpawnFraction += LeftoverBurst;

				if (BurstInfo.Count > 0)
				{
					// Spawn burst particles.
					BuildNewParticles(NewParticles.GetData() + FirstBurstParticleIndex, BurstInfo, ForceBurstSpawnedParticles);
				}

				if (SpawnInfo.Count > 0)
				{
					// Spawn normal particles.
					BuildNewParticles(NewParticles.GetData() + FirstSpawnParticleIndex, SpawnInfo, ForceSpawnedParticles);
				}

				FreeNewParticleArray(ForceSpawnedParticles);
				FreeNewParticleArray(ForceBurstSpawnedParticles);

				int32 NewParticleCount = BurstInfo.Count + SpawnInfo.Count;
				INC_DWORD_STAT_BY(STAT_GPUSpritesSpawned, NewParticleCount);
	#if STATS
				if (NewParticleCount > FXConsoleVariables::GPUSpawnWarningThreshold)
				{
					UE_LOG(LogParticles,Warning,TEXT("Spawning %d GPU particles in one frame[%d]: %s/%s"),
						NewParticleCount,
						GFrameNumber,
						*SpriteTemplate->GetOuter()->GetName(),
						*SpriteTemplate->EmitterName.ToString()
						);

				}
	#endif

				if (Component && Component->bWarmingUp)
				{
					SimulateWarmupParticles(
						NewParticles.GetData() + (NewParticles.Num() - NewParticleCount),
						NewParticleCount,
						Component->WarmupTime - SecondsSinceCreation );
				}
			}
			else if (bFakeBurstsWhenSpawningSupressed)
			{
				FakeBursts();
			}

			// Free any tiles that we no longer need.
			FreeInactiveTiles();

			// Update current material.
			if (EmitterInfo.RequiredModule->Material)
			{
				CurrentMaterial = EmitterInfo.RequiredModule->Material;
			}

			// Update the local vector field.
			TickLocalVectorField(DeltaSeconds);

			// Look up the strength of the point attractor.
			EmitterInfo.PointAttractorStrength.GetValue(EmitterTime, &PointAttractorStrength);

			// Store the amount of time by which the GPU needs to update the simulation.
			PendingDeltaSeconds = DeltaSeconds;

			// Store the number of active particles.
			ActiveParticles = ActiveTileCount * GParticlesPerTile;
			INC_DWORD_STAT_BY(STAT_GPUSpriteParticles, ActiveParticles);

			// 'Reset' the emitter time so that the delay functions correctly
			EmitterTime += EmitterDelay;

			// Update the bounding box.
			UpdateBoundingBox(DeltaSeconds);

			// Final update for modules.
			Tick_ModuleFinalUpdate(DeltaSeconds, LODLevel);

			// Queue an update to the GPU simulation if needed.
			if (Simulation->bDirty_GameThread)
			{
				Simulation->InitResources(AllocatedTiles, EmitterInfo.Resources);
			}

			CheckEmitterFinished();
		}
		else
		{
			// 'Reset' the emitter time so that the delay functions correctly
			EmitterTime += EmitterDelay;

			FakeBursts();
		}

		check(AllocatedTiles.Num() == TileTimeOfDeath.Num());
	}

	/**
	 * Clears all active particle tiles.
	 */
	void ClearAllocatedTiles()
	{
		TilesToClear.Reset();
		TilesToClear = AllocatedTiles;
		TileToAllocateFrom = INDEX_NONE;
		FreeParticlesInTile = 0;
		ActiveTiles.Init(false,ActiveTiles.Num());
	}

	/**
	 *	Force kill all particles in the emitter.
	 *	@param	bFireEvents		If true, fire events for the particles being killed.
	 */
	virtual void KillParticlesForced(bool bFireEvents) override
	{
		// Clear all active tiles. This will effectively kill all particles.
		ClearAllocatedTiles();
	}

	/**
	 *	Called when the particle system is deactivating...
	 */
	virtual void OnDeactivateSystem() override
	{
	}

	virtual void Rewind() override
	{
		FParticleEmitterInstance::Rewind();
		InitLocalVectorField();
	}

	/**
	 * Returns true if the emitter has completed.
	 */
	virtual bool HasCompleted() override
	{
		if ( AllowedLoopCount == 0 || LoopCount < AllowedLoopCount )
		{
			return false;
		}
		return ActiveParticles == 0;
	}

	/**
	 * Force the bounding box to be updated.
	 *		WARNING: This is an expensive operation for GPU particles. It
	 *		requires syncing with the GPU to read back the emitter's bounds.
	 *		This function should NEVER be called at runtime!
	 */
	virtual void ForceUpdateBoundingBox() override
	{
		if ( !GIsEditor )
		{
			UE_LOG(LogParticles, Warning, TEXT("ForceUpdateBoundingBox called on a GPU sprite emitter outside of the Editor!") );
			return;
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FComputeGPUSpriteBoundsCommand,
			FGPUSpriteParticleEmitterInstance*, EmitterInstance, this,
		{
			
			EmitterInstance->ParticleBoundingBox = ComputeParticleBounds(
				RHICmdList,
				EmitterInstance->Simulation->VertexBuffer.VertexBufferSRV,
				EmitterInstance->FXSystem->GetParticleSimulationResources()->GetVisualizeStateTextures().PositionTextureRHI,
				EmitterInstance->Simulation->VertexBuffer.ParticleCount
				);
		});
		FlushRenderingCommands();

		// Take the size of sprites in to account.
		const float MaxSizeX = EmitterInfo.Resources->UniformParameters.MiscScale.X + EmitterInfo.Resources->UniformParameters.MiscBias.X;
		const float MaxSizeY = EmitterInfo.Resources->UniformParameters.MiscScale.Y + EmitterInfo.Resources->UniformParameters.MiscBias.Y;
		const float MaxSize = FMath::Max<float>( MaxSizeX, MaxSizeY );
		ParticleBoundingBox = ParticleBoundingBox.ExpandBy( MaxSize );
	}

private:

	/**
	 * Mark tiles as inactive if all particles in them have died.
	 */
	int32 MarkTilesInactive()
	{
		int32 ActiveTileCount = 0;
		for (TBitArray<>::FConstIterator BitIt(ActiveTiles); BitIt; ++BitIt)
		{
			const int32 BitIndex = BitIt.GetIndex();
			if (TileTimeOfDeath[BitIndex] <= SecondsSinceCreation)
			{
				ActiveTiles.AccessCorrespondingBit(BitIt) = false;
				if ( TileToAllocateFrom == BitIndex )
				{
					TileToAllocateFrom = INDEX_NONE;
					FreeParticlesInTile = 0;
				}
			}
			else
			{
				ActiveTileCount++;
			}
		}
		return ActiveTileCount;
	}

	/**
	 * Initialize the local vector field.
	 */
	void InitLocalVectorField()
	{
		LocalVectorFieldRotation = FMath::LerpRange(
			EmitterInfo.LocalVectorField.MinInitialRotation,
			EmitterInfo.LocalVectorField.MaxInitialRotation,
			RandomStream.GetFraction() );

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FResetVectorFieldCommand,
			FParticleSimulationGPU*,Simulation,Simulation,
		{
			if (Simulation && Simulation->LocalVectorField.Resource)
			{
				Simulation->LocalVectorField.Resource->ResetVectorField();
			}
		});
	}

	/**
	 * Computes the minimum number of tiles that should be allocated for this emitter.
	 */
	int32 GetMinTileCount() const
	{
		if (AllowedLoopCount == 0)
		{
			const int32 EstMaxTiles = (EmitterInfo.MaxParticleCount + GParticlesPerTile - 1) / GParticlesPerTile;
			const int32 SlackTiles = FMath::CeilToInt(FXConsoleVariables::ParticleSlackGPU * (float)EstMaxTiles);
			return FMath::Min<int32>(EstMaxTiles + SlackTiles, FXConsoleVariables::MaxParticleTilePreAllocation);
		}
		return 0;
	}

	/**
	 * Release any inactive tiles.
	 * @returns the number of tiles released.
	 */
	int32 FreeInactiveTiles()
	{
		const int32 MinTileCount = GetMinTileCount();
		int32 TilesToFree = 0;
		TBitArray<>::FConstReverseIterator BitIter(ActiveTiles);
		while (BitIter && BitIter.GetIndex() >= MinTileCount)
		{
			if (BitIter.GetValue())
			{
				break;
			}
			++TilesToFree;
			++BitIter;
		}
		if (TilesToFree > 0)
		{
			FParticleSimulationResources* SimulationResources = FXSystem->GetParticleSimulationResources();
			const int32 FirstTileIndex = AllocatedTiles.Num() - TilesToFree;
			const int32 LastTileIndex = FirstTileIndex + TilesToFree;
			for (int32 TileIndex = FirstTileIndex; TileIndex < LastTileIndex; ++TileIndex)
			{
				SimulationResources->FreeTile(AllocatedTiles[TileIndex]);
			}
			ActiveTiles.RemoveAt(FirstTileIndex, TilesToFree);
			AllocatedTiles.RemoveAt(FirstTileIndex, TilesToFree);
			TileTimeOfDeath.RemoveAt(FirstTileIndex, TilesToFree);
			Simulation->bDirty_GameThread = true;
		}
		return TilesToFree;
	}

	/**
	 * Releases resources allocated for GPU simulation.
	 */
	void ReleaseSimulationResources()
	{
		if (FXSystem)
		{
			FXSystem->RemoveGPUSimulation( Simulation );

			// The check for GIsRequestingExit is done because at shut down UWorld can be destroyed before particle emitters(?)
			if (!GIsRequestingExit)
			{
				FParticleSimulationResources* ParticleSimulationResources = FXSystem->GetParticleSimulationResources();
				const int32 TileCount = AllocatedTiles.Num();
				for ( int32 ActiveTileIndex = 0; ActiveTileIndex < TileCount; ++ActiveTileIndex )
				{
					const uint32 TileIndex = AllocatedTiles[ActiveTileIndex];
					ParticleSimulationResources->FreeTile( TileIndex );
				}
				AllocatedTiles.Reset();
#if TRACK_TILE_ALLOCATIONS
				UE_LOG(LogParticles,VeryVerbose,
					TEXT("%s|%s|0x%016p [ReleaseSimulationResources] %d tiles"),
					*Component->GetName(),*Component->Template->GetName(),(PTRINT)this, AllocatedTiles.Num());
#endif // #if TRACK_TILE_ALLOCATIONS
			}
		}
		else if (!GIsRequestingExit)
		{
			UE_LOG(LogParticles,Warning,
				TEXT("%s|%s|0x%016p [ReleaseSimulationResources] LEAKING %d tiles FXSystem=0x%016x"),
				*Component->GetName(),*Component->Template->GetName(),(PTRINT)this, AllocatedTiles.Num(), (PTRINT)FXSystem);
		}


		ActiveTiles.Reset();
		AllocatedTiles.Reset();
		TileTimeOfDeath.Reset();
		TilesToClear.Reset();
		
		TileToAllocateFrom = INDEX_NONE;
		FreeParticlesInTile = 0;

		Simulation->BeginReleaseResources();
	}

	/**
	 * Allocates space in a particle tile for all new particles.
	 * @param NewParticles - Array in which to store new particles.
	 * @param NumNewParticles - The number of new particles that need an allocation.
	 * @param ActiveTileCount - Number of active tiles, incremented each time a new tile is allocated.
	 * @returns the number of particles which were successfully allocated.
	 */
	int32 AllocateTilesForParticles(TArray<FNewParticle>& InNewParticles, int32 NumNewParticles, int32& ActiveTileCount)
	{
		if (!NumNewParticles)
		{
			return 0;
		}
		// Need to allocate space in tiles for all new particles.
		FParticleSimulationResources* SimulationResources = FXSystem->GetParticleSimulationResources();
		uint32 TileIndex = (AllocatedTiles.IsValidIndex(TileToAllocateFrom)) ? AllocatedTiles[TileToAllocateFrom] : INDEX_NONE;
		FVector2D TileOffset(
			FMath::Fractional((float)TileIndex / (float)GParticleSimulationTileCountX),
			FMath::Fractional(FMath::TruncToFloat((float)TileIndex / (float)GParticleSimulationTileCountX) / (float)GParticleSimulationTileCountY)
			);

		for (int32 ParticleIndex = 0; ParticleIndex < NumNewParticles; ++ParticleIndex)
		{
			if (FreeParticlesInTile <= 0)
			{
				// Start adding particles to the first inactive tile.
				if (ActiveTileCount < AllocatedTiles.Num())
				{
					TileToAllocateFrom = ActiveTiles.FindAndSetFirstZeroBit();
				}
				else
				{
					uint32 NewTile = SimulationResources->AllocateTile();
					if (NewTile == INDEX_NONE)
					{
						// Out of particle tiles.
						UE_LOG(LogParticles,Warning,
							TEXT("Failed to allocate tiles for %s! %d new particles truncated to %d."),
							*Component->Template->GetName(), NumNewParticles, ParticleIndex);
						return ParticleIndex;
					}

					TileToAllocateFrom = AllocatedTiles.Add(NewTile);
					TileTimeOfDeath.Add(0.0f);
					TilesToClear.Add(NewTile);
					ActiveTiles.Add(true);
					Simulation->bDirty_GameThread = true;
				}

				ActiveTileCount++;
				TileIndex = AllocatedTiles[TileToAllocateFrom];
				TileOffset.X = FMath::Fractional((float)TileIndex / (float)GParticleSimulationTileCountX);
				TileOffset.Y = FMath::Fractional(FMath::TruncToFloat((float)TileIndex / (float)GParticleSimulationTileCountX) / (float)GParticleSimulationTileCountY);
				FreeParticlesInTile = GParticlesPerTile;
			}
			FNewParticle& Particle = *new(InNewParticles) FNewParticle();
			const int32 SubTileIndex = GParticlesPerTile - FreeParticlesInTile;
			const int32 SubTileX = SubTileIndex % GParticleSimulationTileSize;
			const int32 SubTileY = SubTileIndex / GParticleSimulationTileSize;
			Particle.Offset.X = TileOffset.X + ((float)SubTileX / (float)GParticleSimulationTextureSizeX);
			Particle.Offset.Y = TileOffset.Y + ((float)SubTileY / (float)GParticleSimulationTextureSizeY);
			Particle.ResilienceAndTileIndex.AllocatedTileIndex = TileToAllocateFrom;
			FreeParticlesInTile--;
		}

		return NumNewParticles;
	}

	/**
	 * Computes how many particles should be spawned this frame. Does not account for bursts.
	 * @param DeltaSeconds - The amount of time for which to spawn.
	 */
	FSpawnInfo GetNumParticlesToSpawn(float DeltaSeconds)
	{
		UParticleModuleRequired* RequiredModule = EmitterInfo.RequiredModule;
		UParticleModuleSpawn* SpawnModule = EmitterInfo.SpawnModule;

		// Determine spawn rate.
		check(SpawnModule && RequiredModule);
		const float RateScale = CurrentLODLevel->SpawnModule->RateScale.GetValue(EmitterTime, Component) * CurrentLODLevel->SpawnModule->GetGlobalRateScale();
		float SpawnRate = CurrentLODLevel->SpawnModule->Rate.GetValue(EmitterTime, Component) * RateScale;
		SpawnRate = FMath::Max<float>(0.0f, SpawnRate);

		if (EmitterInfo.SpawnPerUnitModule)
		{
			int32 Number = 0;
			float Rate = 0.0f;
			if (EmitterInfo.SpawnPerUnitModule->GetSpawnAmount(this, 0, 0.0f, DeltaSeconds, Number, Rate) == false)
			{
				SpawnRate = Rate;
			}
			else
			{
				SpawnRate += Rate;
			}
		}

		// Determine how many to spawn.
		FSpawnInfo Info;
		float AccumSpawnCount = SpawnFraction + SpawnRate * DeltaSeconds;
		Info.Count = FMath::Min(FMath::TruncToInt(AccumSpawnCount), FXConsoleVariables::MaxGPUParticlesSpawnedPerFrame);
		Info.Increment = (SpawnRate > 0.0f) ? (1.f / SpawnRate) : 0.0f;
		Info.StartTime = DeltaSeconds + SpawnFraction * Info.Increment - Info.Increment;
		SpawnFraction = AccumSpawnCount - Info.Count;

		return Info;
	}

	/**
	 * Perform a simple simulation for particles during the warmup period. This
	 * Simulation only takes in to account constant acceleration, initial
	 * velocity, and initial position.
	 * @param InNewParticles - The first new particle to simulate.
	 * @param ParticleCount - The number of particles to simulate.
	 * @param WarmupTime - The amount of warmup time by which to simulate.
	 */
	void SimulateWarmupParticles(
		FNewParticle* InNewParticles,
		int32 ParticleCount,
		float WarmupTime )
	{
		const FVector Acceleration = EmitterInfo.ConstantAcceleration;
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
		{
			FNewParticle* Particle = InNewParticles + ParticleIndex;
			Particle->Position += (Particle->Velocity + 0.5f * Acceleration * WarmupTime) * WarmupTime;
			Particle->Velocity += Acceleration * WarmupTime;
			Particle->RelativeTime += Particle->TimeScale * WarmupTime;
		}
	}

	/**
	 * Builds new particles to be injected in to the GPU simulation.
	 * @param OutNewParticles - Array in which to store new particles.
	 * @param SpawnCount - The number of particles to spawn.
	 * @param SpawnTime - The time at which to begin spawning particles.
	 * @param Increment - The amount by which to increment time for each particle spawned.
	 */
	void BuildNewParticles(FNewParticle* InNewParticles, FSpawnInfo SpawnInfo, TArray<FNewParticle> &ForceSpawned)
	{
		const float OneOverTwoPi = 1.0f / (2.0f * PI);
		UParticleModuleRequired* RequiredModule = EmitterInfo.RequiredModule;

		// Allocate stack memory for a dummy particle.
		const UPTRINT Alignment = 16;
		uint8* Mem = (uint8*)FMemory_Alloca( ParticleSize + (int32)Alignment );
		Mem += Alignment - 1;
		Mem = (uint8*)(UPTRINT(Mem) & ~(Alignment - 1));

		FBaseParticle* TempParticle = (FBaseParticle*)Mem;


		// Figure out if we need to replicate the X channel of size to Y.
		const bool bSquare = (EmitterInfo.ScreenAlignment == PSA_Square)
			|| (EmitterInfo.ScreenAlignment == PSA_FacingCameraPosition)
			|| (EmitterInfo.ScreenAlignment == PSA_FacingCameraDistanceBlend);

		// Compute the distance covered by the emitter during this time period.
		const FVector EmitterLocation = (RequiredModule->bUseLocalSpace) ? FVector::ZeroVector : Location;
		const FVector EmitterDelta = (RequiredModule->bUseLocalSpace) ? FVector::ZeroVector : (OldLocation - Location);

		// Construct new particles.
		for (int32 i = SpawnInfo.Count; i > 0; --i)
		{
			// Reset the temporary particle.
			FMemory::Memzero( TempParticle, ParticleSize );

			// Set the particle's location and invoke each spawn module on the particle.
			TempParticle->Location = EmitterToSimulation.GetOrigin();

			int32 ForceSpawnedOffset = SpawnInfo.Count - ForceSpawned.Num();
			if (ForceSpawned.Num() && i > ForceSpawnedOffset)
			{
				TempParticle->Location = ForceSpawned[i - ForceSpawnedOffset - 1].Position;
				TempParticle->RelativeTime = ForceSpawned[i - ForceSpawnedOffset - 1].RelativeTime;
				TempParticle->Velocity += ForceSpawned[i - ForceSpawnedOffset - 1].Velocity;
			}

			for (int32 ModuleIndex = 0; ModuleIndex < EmitterInfo.SpawnModules.Num(); ModuleIndex++)
			{
				UParticleModule* SpawnModule = EmitterInfo.SpawnModules[ModuleIndex];
				if (SpawnModule->bEnabled)
				{
					SpawnModule->Spawn(this, /*Offset=*/ 0, SpawnInfo.StartTime, TempParticle);
				}
			}

			const float RandomOrbit = RandomStream.GetFraction();
			FNewParticle* NewParticle = InNewParticles++;
			int32 AllocatedTileIndex = NewParticle->ResilienceAndTileIndex.AllocatedTileIndex;
			float InterpFraction = (float)i / (float)SpawnInfo.Count;

			NewParticle->Velocity = TempParticle->BaseVelocity;
			NewParticle->Position = TempParticle->Location + InterpFraction * EmitterDelta + SpawnInfo.StartTime * NewParticle->Velocity + EmitterInfo.OrbitOffsetBase + EmitterInfo.OrbitOffsetRange * RandomOrbit;
			NewParticle->RelativeTime = TempParticle->RelativeTime;
			NewParticle->TimeScale = FMath::Max<float>(TempParticle->OneOverMaxLifetime, 0.001f);

			//So here I'm reducing the size to 0-0.5 range and using < 0.5 to indicate flipped UVs.
			FVector BaseSize = GetParticleBaseSize(*TempParticle, true);
			FVector2D UVFlipSizeOffset = FVector2D(BaseSize.X < 0.0f ? 0.0f : 0.5f, BaseSize.Y < 0.0f ? 0.0f : 0.5f);
			NewParticle->Size.X = (FMath::Abs(BaseSize.X) * EmitterInfo.InvMaxSize.X * 0.5f);
			NewParticle->Size.Y = bSquare ? (NewParticle->Size.X) : (FMath::Abs(BaseSize.Y) * EmitterInfo.InvMaxSize.Y * 0.5f);
			NewParticle->Size += UVFlipSizeOffset;

			NewParticle->Rotation = FMath::Fractional( TempParticle->Rotation * OneOverTwoPi );
			NewParticle->RelativeRotationRate = TempParticle->BaseRotationRate * OneOverTwoPi * EmitterInfo.InvRotationRateScale / NewParticle->TimeScale;
			NewParticle->RandomOrbit = RandomOrbit;
			EmitterInfo.VectorFieldScale.GetRandomValue(EmitterTime, &NewParticle->VectorFieldScale, RandomStream);
			EmitterInfo.DragCoefficient.GetRandomValue(EmitterTime, &NewParticle->DragCoefficient, RandomStream);
			EmitterInfo.Resilience.GetRandomValue(EmitterTime, &NewParticle->ResilienceAndTileIndex.Resilience, RandomStream);
			SpawnInfo.StartTime -= SpawnInfo.Increment;

			const float PrevTileTimeOfDeath = TileTimeOfDeath[AllocatedTileIndex];
			const float ParticleTimeOfDeath = SecondsSinceCreation + 1.0f / NewParticle->TimeScale;
			const float NewTileTimeOfDeath = FMath::Max(PrevTileTimeOfDeath, ParticleTimeOfDeath);
			TileTimeOfDeath[AllocatedTileIndex] = NewTileTimeOfDeath;
		}
	}

	/**
	 * Update the local vector field.
	 * @param DeltaSeconds - The amount of time by which to move forward simulation.
	 */
	void TickLocalVectorField(float DeltaSeconds)
	{
		LocalVectorFieldRotation += EmitterInfo.LocalVectorField.RotationRate * DeltaSeconds;
	}

	virtual void UpdateBoundingBox(float DeltaSeconds) override
	{
		// Setup a bogus bounding box at the origin. GPU emitters must use fixed bounds.
		FVector Origin = Component ? Component->GetComponentToWorld().GetTranslation() : FVector::ZeroVector;
		ParticleBoundingBox = FBox::BuildAABB(Origin, FVector(1.0f));
	}

	virtual bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true) override
	{
		return false;
	}

	virtual float Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bSuppressSpawning, bool bFirstTime) override
	{
		return 0.0f;
	}

	virtual void Tick_ModulePreUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
	{
	}

	virtual void Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel) override
	{
		// We cannot update particles that have spawned, but modules such as BoneSocket and Skel Vert/Surface may need to perform calculations each tick.
		UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
		check(HighestLODLevel);
		for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.Num(); ModuleIndex++)
		{
			UParticleModule* CurrentModule	= InCurrentLODLevel->UpdateModules[ModuleIndex];
			if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bUpdateModule && CurrentModule->bUpdateForGPUEmitter)
			{
				CurrentModule->Update(this, GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]), DeltaTime);
			}
		}
	}

	virtual void Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel) override
	{
	}

	virtual void Tick_ModuleFinalUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel) override
	{
		// We cannot update particles that have spawned, but modules such as BoneSocket and Skel Vert/Surface may need to perform calculations each tick.
		UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
		check(HighestLODLevel);
		for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.Num(); ModuleIndex++)
		{
			UParticleModule* CurrentModule	= InCurrentLODLevel->UpdateModules[ModuleIndex];
			if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bFinalUpdateModule && CurrentModule->bUpdateForGPUEmitter)
			{
				CurrentModule->FinalUpdate(this, GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]), DeltaTime);
			}
		}
	}

	virtual void SetCurrentLODIndex(int32 InLODIndex, bool bInFullyProcess) override
	{
		bool bDifferent = (InLODIndex != CurrentLODLevelIndex);
		FParticleEmitterInstance::SetCurrentLODIndex(InLODIndex, bInFullyProcess);
	}

	virtual uint32 RequiredBytes() override
	{
		return 0;
	}

	virtual uint8* GetTypeDataModuleInstanceData() override
	{
		return NULL;
	}

	virtual uint32 CalculateParticleStride(uint32 InParticleSize) override
	{
		return InParticleSize;
	}

	virtual void ResetParticleParameters(float DeltaTime) override
	{
	}

	void CalculateOrbitOffset(FOrbitChainModuleInstancePayload& Payload, 
		FVector& AccumOffset, FVector& AccumRotation, FVector& AccumRotationRate, 
		float DeltaTime, FVector& Result, FMatrix& RotationMat)
	{
	}

	virtual void UpdateOrbitData(float DeltaTime) override
	{

	}

	virtual void ParticlePrefetch() override
	{
	}

	virtual float Spawn(float DeltaTime) override
	{
		return 0.0f;
	}

	virtual void ForceSpawn(float DeltaTime, int32 InSpawnCount, int32 InBurstCount, FVector& InLocation, FVector& InVelocity) override
	{
		const bool bUseLocalSpace = GetCurrentLODLevelChecked()->RequiredModule->bUseLocalSpace;
		FVector SpawnLocation = bUseLocalSpace ? FVector::ZeroVector : InLocation;

		float Increment = DeltaTime / InSpawnCount;
		if (InSpawnCount && !(ForceSpawnedParticles.Num() + ForceSpawnedParticles.GetSlack()))
		{
			GetNewParticleArray(ForceSpawnedParticles, InSpawnCount);
		}
		for (int32 i = 0; i < InSpawnCount; i++)
		{

			FNewParticle Particle;
			Particle.Position = SpawnLocation;
			Particle.Velocity = InVelocity;
			Particle.RelativeTime = Increment*i;
			ForceSpawnedParticles.Add(Particle);
		}
		if (InBurstCount && !(ForceBurstSpawnedParticles.Num() + ForceBurstSpawnedParticles.GetSlack()))
		{
			GetNewParticleArray(ForceBurstSpawnedParticles, InBurstCount);
		}
		for (int32 i = 0; i < InBurstCount; i++)
		{
			FNewParticle Particle;
			Particle.Position = SpawnLocation;
			Particle.Velocity = InVelocity;
			Particle.RelativeTime = 0.0f;
			ForceBurstSpawnedParticles.Add(Particle);
		}
	}

	virtual void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity) override
	{
	}

	virtual void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime) override
	{
	}

	virtual void KillParticles() override
	{
	}

	virtual void KillParticle(int32 Index) override
	{
	}

	virtual void RemovedFromScene()
	{
	}

	virtual FBaseParticle* GetParticle(int32 Index) override
	{
		return NULL;
	}

	virtual FBaseParticle* GetParticleDirect(int32 InDirectIndex) override
	{
		return NULL;
	}

protected:

	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override
	{
		return true;
	}
};

#if TRACK_TILE_ALLOCATIONS
void DumpTileAllocations()
{
	for (TMap<FFXSystem*,TSet<FGPUSpriteParticleEmitterInstance*> >::TConstIterator It(GPUSpriteParticleEmitterInstances); It; ++It)
	{
		FFXSystem* FXSystem = It.Key();
		const TSet<FGPUSpriteParticleEmitterInstance*>& Emitters = It.Value();
		int32 TotalAllocatedTiles = 0;

		UE_LOG(LogParticles,Display,TEXT("---------- GPU Particle Tile Allocations : FXSystem=0x%016x ----------"), (PTRINT)FXSystem);
		for (TSet<FGPUSpriteParticleEmitterInstance*>::TConstIterator It(Emitters); It; ++It)
		{
			FGPUSpriteParticleEmitterInstance* Emitter = *It;
			int32 TileCount = Emitter->GetAllocatedTileCount();
			UE_LOG(LogParticles,Display,
				TEXT("%s|%s|0x%016x %d tiles"),
				*Emitter->Component->GetName(),*Emitter->Component->Template->GetName(),(PTRINT)Emitter, TileCount);
			TotalAllocatedTiles += TileCount;
		}

		UE_LOG(LogParticles,Display,TEXT("---"));
		UE_LOG(LogParticles,Display,TEXT("Total Allocated: %d"), TotalAllocatedTiles);
		UE_LOG(LogParticles,Display,TEXT("Free (est.): %d"), GParticleSimulationTileCount - TotalAllocatedTiles);
		if (FXSystem)
		{
			UE_LOG(LogParticles,Display,TEXT("Free (actual): %d"), FXSystem->GetParticleSimulationResources()->GetFreeTileCount());
			UE_LOG(LogParticles,Display,TEXT("Leaked: %d"), GParticleSimulationTileCount - TotalAllocatedTiles - FXSystem->GetParticleSimulationResources()->GetFreeTileCount());
		}
	}
}

FAutoConsoleCommand DumpTileAllocsCommand(
	TEXT("FX.DumpTileAllocations"),
	TEXT("Dump GPU particle tile allocations."),
	FConsoleCommandDelegate::CreateStatic(DumpTileAllocations)
	);
#endif // #if TRACK_TILE_ALLOCATIONS

/*-----------------------------------------------------------------------------
	Internal interface.
-----------------------------------------------------------------------------*/

void FFXSystem::InitGPUSimulation()
{
	check(ParticleSimulationResources == NULL);
	ParticleSimulationResources = new FParticleSimulationResources();
	InitGPUResources();
}

void FFXSystem::DestroyGPUSimulation()
{
	UE_LOG(LogParticles,Verbose,
		TEXT("Destroying %d GPU particle simulations for FXSystem 0x%p"),
		GPUSimulations.Num(),
		this
		);
	for ( TSparseArray<FParticleSimulationGPU*>::TIterator It(GPUSimulations); It; ++It )
	{
		FParticleSimulationGPU* Simulation = *It;
		Simulation->SimulationIndex = INDEX_NONE;
	}
	GPUSimulations.Empty();
	ReleaseGPUResources();
	ParticleSimulationResources->Destroy();
	ParticleSimulationResources = NULL;
}

void FFXSystem::InitGPUResources()
{
	if (RHISupportsGPUParticles())
	{
		check(ParticleSimulationResources);
		ParticleSimulationResources->Init();
	}
}

void FFXSystem::ReleaseGPUResources()
{
	if (RHISupportsGPUParticles())
	{
		check(ParticleSimulationResources);
		ParticleSimulationResources->Release();
	}
}

void FFXSystem::AddGPUSimulation(FParticleSimulationGPU* Simulation)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FAddGPUSimulationCommand,
		FFXSystem*, FXSystem, this,
		FParticleSimulationGPU*, Simulation, Simulation,
	{
		if (Simulation->SimulationIndex == INDEX_NONE)
		{
			FSparseArrayAllocationInfo Allocation = FXSystem->GPUSimulations.AddUninitialized();
			Simulation->SimulationIndex = Allocation.Index;
			FXSystem->GPUSimulations[Allocation.Index] = Simulation;
		}
		check(FXSystem->GPUSimulations[Simulation->SimulationIndex] == Simulation);
	});
}

void FFXSystem::RemoveGPUSimulation(FParticleSimulationGPU* Simulation)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FRemoveGPUSimulationCommand,
		FFXSystem*, FXSystem, this,
		FParticleSimulationGPU*, Simulation, Simulation,
	{
		if (Simulation->SimulationIndex != INDEX_NONE)
		{
			check(FXSystem->GPUSimulations[Simulation->SimulationIndex] == Simulation);
			FXSystem->GPUSimulations.RemoveAt(Simulation->SimulationIndex);
		}
		Simulation->SimulationIndex = INDEX_NONE;
	});
}

int32 FFXSystem::AddSortedGPUSimulation(FParticleSimulationGPU* Simulation, const FVector& ViewOrigin)
{
	check(FeatureLevel == ERHIFeatureLevel::SM5);
	const int32 BufferOffset = ParticleSimulationResources->SortedParticleCount;
	ParticleSimulationResources->SortedParticleCount += Simulation->VertexBuffer.ParticleCount;
	FParticleSimulationSortInfo* SortInfo = new(ParticleSimulationResources->SimulationsToSort) FParticleSimulationSortInfo();
	SortInfo->VertexBufferSRV = Simulation->VertexBuffer.VertexBufferSRV;
	SortInfo->ViewOrigin = ViewOrigin;
	SortInfo->ParticleCount = Simulation->VertexBuffer.ParticleCount;
	return BufferOffset;
}

void FFXSystem::AdvanceGPUParticleFrame()
{
	// We double buffer, so swap the current and previous textures.
	ParticleSimulationResources->FrameIndex ^= 0x1;

	// Reset the list of sorted simulations. As PreRenderView is called on GPU simulations we'll
	// allocate space for them in the sorted index buffer.
	ParticleSimulationResources->SimulationsToSort.Reset();
	ParticleSimulationResources->SortedParticleCount = 0;
}

void FFXSystem::SortGPUParticles(FRHICommandListImmediate& RHICmdList)
{
	if (ParticleSimulationResources->SimulationsToSort.Num() > 0)
	{
		int32 BufferIndex = SortParticlesGPU(
			RHICmdList,
			GParticleSortBuffers,
			ParticleSimulationResources->GetVisualizeStateTextures().PositionTextureRHI,
			ParticleSimulationResources->SimulationsToSort,
			GetFeatureLevel()
			);
		ParticleSimulationResources->SortedVertexBuffer.VertexBufferRHI =
			GParticleSortBuffers.GetSortedVertexBufferRHI(BufferIndex);
		ParticleSimulationResources->SortedVertexBuffer.VertexBufferSRV =
			GParticleSortBuffers.GetSortedVertexBufferSRV(BufferIndex);
	}
	else
	{
		ParticleSimulationResources->SortedVertexBuffer.VertexBufferRHI =
		GParticleSortBuffers.GetSortedVertexBufferRHI(0);
		ParticleSimulationResources->SortedVertexBuffer.VertexBufferSRV =
		GParticleSortBuffers.GetSortedVertexBufferSRV(0);
	}
}

/**
 * Sets parameters for the vector field instance.
 * @param OutParameters - The uniform parameters structure.
 * @param VectorFieldInstance - The vector field instance.
 * @param EmitterScale - Amount to scale the vector field by.
 * @param EmitterTightness - Tightness override for the vector field.
 * @param Index - Index of the vector field.
 */
static void SetParametersForVectorField(FVectorFieldUniformParameters& OutParameters, FVectorFieldInstance* VectorFieldInstance, float EmitterScale, float EmitterTightness, int32 Index)
{
	check(VectorFieldInstance && VectorFieldInstance->Resource);
	check(Index < MAX_VECTOR_FIELDS);

	FVectorFieldResource* Resource = VectorFieldInstance->Resource;
	const float Intensity = VectorFieldInstance->Intensity * Resource->Intensity * EmitterScale;

	// Override vector field tightness if value is set (greater than 0). This override is only used for global vector fields.
	float Tightness = EmitterTightness;
	if(EmitterTightness == -1)
	{
		Tightness = FMath::Clamp<float>(VectorFieldInstance->Tightness, 0.0f, 1.0f);
	}

	OutParameters.WorldToVolume[Index] = VectorFieldInstance->WorldToVolume;
	OutParameters.VolumeToWorld[Index] = VectorFieldInstance->VolumeToWorldNoScale;
	OutParameters.VolumeSize[Index] = FVector4(Resource->SizeX, Resource->SizeY, Resource->SizeZ, 0);
	OutParameters.IntensityAndTightness[Index] = FVector4(Intensity, Tightness, 0, 0 );
	OutParameters.TilingAxes[Index].X = VectorFieldInstance->bTileX ? 1.0f : 0.0f;
	OutParameters.TilingAxes[Index].Y = VectorFieldInstance->bTileY ? 1.0f : 0.0f;
	OutParameters.TilingAxes[Index].Z = VectorFieldInstance->bTileZ ? 1.0f : 0.0f;
}

bool FFXSystem::UsesGlobalDistanceFieldInternal() const
{
	for (TSparseArray<FParticleSimulationGPU*>::TConstIterator It(GPUSimulations); It; ++It)
	{
		const FParticleSimulationGPU* Simulation = *It;

		if (Simulation->SimulationPhase == EParticleSimulatePhase::CollisionDistanceField
			&& Simulation->TileVertexBuffer.AlignedTileCount > 0)
		{
			return true;
		}
	}

	return false;
}

void FFXSystem::PrepareGPUSimulation(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SceneDepthTexture)
{
	// Grab resources.
	FParticleStateTextures& CurrentStateTextures = ParticleSimulationResources->GetCurrentStateTextures();

	// Setup render states.
	FTextureRHIParamRef RenderTargets[2] =
	{
		CurrentStateTextures.PositionTextureTargetRHI,
		CurrentStateTextures.VelocityTextureTargetRHI,
	};

	RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, RenderTargets, 2);
	if (SceneDepthTexture)
	{
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneDepthTexture);
	}
}

void FFXSystem::FinalizeGPUSimulation(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SceneDepthTexture)
{
	// Grab resources.
	FParticleStateTextures& CurrentStateTextures = ParticleSimulationResources->GetVisualizeStateTextures();

	// Setup render states.
	FTextureRHIParamRef RenderTargets[2] =
	{
		CurrentStateTextures.PositionTextureTargetRHI,
		CurrentStateTextures.VelocityTextureTargetRHI,
	};
	
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, RenderTargets, 2);
	if (SceneDepthTexture)
	{
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, SceneDepthTexture);
	}
}

void FFXSystem::SimulateGPUParticles(
	FRHICommandListImmediate& RHICmdList,
	EParticleSimulatePhase::Type Phase,
	const FUniformBufferRHIParamRef ViewUniformBuffer,
	const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData,
	FTexture2DRHIParamRef SceneDepthTexture,
	FTexture2DRHIParamRef GBufferATexture
	)
{
	check(IsInRenderingThread());
	SCOPE_CYCLE_COUNTER(STAT_GPUParticleTickTime);

	FMemMark Mark(FMemStack::Get());

	const float FixDeltaSeconds = CVarGPUParticleFixDeltaSeconds.GetValueOnRenderThread();

	// Grab resources.
	FParticleStateTextures& CurrentStateTextures = ParticleSimulationResources->GetCurrentStateTextures();
	FParticleStateTextures& PrevStateTextures = ParticleSimulationResources->GetPreviousStateTextures();	

	// Setup render states.
	FTextureRHIParamRef CurrentStateRenderTargets[2] = { CurrentStateTextures.PositionTextureTargetRHI, CurrentStateTextures.VelocityTextureTargetRHI };
	FTextureRHIParamRef PreviousStateRenderTargets[2] = { PrevStateTextures.PositionTextureTargetRHI, PrevStateTextures.VelocityTextureTargetRHI };
	{
		
		// On some platforms, the textures are filled with garbage after creation, so we need to clear them to black the first time we use them
		if ( !CurrentStateTextures.bTexturesCleared )
		{
			
			RHICmdList.BeginUpdateMultiFrameResource(CurrentStateRenderTargets[0]);
			RHICmdList.BeginUpdateMultiFrameResource(CurrentStateRenderTargets[1]);
			
			SetRenderTarget(RHICmdList, CurrentStateTextures.PositionTextureTargetRHI, FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
			SetRenderTarget(RHICmdList, CurrentStateTextures.VelocityTextureTargetRHI, FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
			
			CurrentStateTextures.bTexturesCleared = true;
			
			RHICmdList.EndUpdateMultiFrameResource(CurrentStateRenderTargets[0]);
			RHICmdList.EndUpdateMultiFrameResource(CurrentStateRenderTargets[1]);
		}
		
		if ( !PrevStateTextures.bTexturesCleared )
		{
			RHICmdList.BeginUpdateMultiFrameResource(PreviousStateRenderTargets[0]);
			RHICmdList.BeginUpdateMultiFrameResource(PreviousStateRenderTargets[1]);
			
			SetRenderTarget(RHICmdList, PrevStateTextures.PositionTextureTargetRHI, FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
			RHICmdList.CopyToResolveTarget(PrevStateTextures.PositionTextureTargetRHI, PrevStateTextures.PositionTextureTargetRHI, true, FResolveParams());
			SetRenderTarget(RHICmdList, PrevStateTextures.VelocityTextureTargetRHI, FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
			RHICmdList.CopyToResolveTarget(PrevStateTextures.VelocityTextureTargetRHI, PrevStateTextures.VelocityTextureTargetRHI, true, FResolveParams());
			
			PrevStateTextures.bTexturesCleared = true;
			
			RHICmdList.EndUpdateMultiFrameResource(PreviousStateRenderTargets[0]);
			RHICmdList.EndUpdateMultiFrameResource(PreviousStateRenderTargets[1]);
		}
	}
	
	// Simulations that don't use vector fields can share some state.
	FVectorFieldUniformBufferRef EmptyVectorFieldUniformBuffer;
	{
		FVectorFieldUniformParameters VectorFieldParameters;
		FTexture3DRHIParamRef BlackVolumeTextureRHI = (FTexture3DRHIParamRef)(FTextureRHIParamRef)GBlackVolumeTexture->TextureRHI;
		for (int32 Index = 0; Index < MAX_VECTOR_FIELDS; ++Index)
		{
			VectorFieldParameters.WorldToVolume[Index] = FMatrix::Identity;
			VectorFieldParameters.VolumeToWorld[Index] = FMatrix::Identity;
			VectorFieldParameters.VolumeSize[Index] = FVector4(1.0f);
			VectorFieldParameters.IntensityAndTightness[Index] = FVector4(0.0f);
		}
		VectorFieldParameters.Count = 0;
		EmptyVectorFieldUniformBuffer = FVectorFieldUniformBufferRef::CreateUniformBufferImmediate(VectorFieldParameters, UniformBuffer_SingleFrame);
	}

	// Gather simulation commands from all active simulations.
	static TArray<FSimulationCommandGPU> SimulationCommands;
	static TArray<uint32> TilesToClear;
	static TArray<FNewParticle> NewParticles;

	// One-time register delegate with Trim() so the data above can be freed on demand
	static FDelegateHandle Clear = FCoreDelegates::GetMemoryTrimDelegate().AddLambda([]()
	{
		SimulationCommands.Empty();
		TilesToClear.Empty();
		NewParticles.Empty();
	});

	for (TSparseArray<FParticleSimulationGPU*>::TIterator It(GPUSimulations); It; ++It)
	{
		//SCOPE_CYCLE_COUNTER(STAT_GPUParticleBuildSimCmdsTime);

		FParticleSimulationGPU* Simulation = *It;
		if (Simulation->SimulationPhase == Phase
			&& Simulation->TileVertexBuffer.AlignedTileCount > 0
			&& Simulation->bEnabled)
		{
			FSimulationCommandGPU* SimulationCommand = new(SimulationCommands) FSimulationCommandGPU(
				Simulation->TileVertexBuffer.GetShaderParam(),
				Simulation->EmitterSimulationResources->SimulationUniformBuffer,
				Simulation->PerFrameSimulationParameters,
				EmptyVectorFieldUniformBuffer,
				Simulation->TileVertexBuffer.AlignedTileCount
				);

			// Determine which vector fields affect this simulation and build the appropriate parameters.
			{
				SCOPE_CYCLE_COUNTER(STAT_GPUParticleVFCullTime);
				FVectorFieldUniformParameters VectorFieldParameters;
				const FBox SimulationBounds = Simulation->Bounds;

				// Add the local vector field.
				VectorFieldParameters.Count = 0;
				if (Simulation->LocalVectorField.Resource)
				{
					const float Intensity = Simulation->LocalVectorField.Intensity * Simulation->LocalVectorField.Resource->Intensity;
					if (FMath::Abs(Intensity) > 0.0f)
					{
						Simulation->LocalVectorField.Resource->Update(RHICmdList, Simulation->PerFrameSimulationParameters.DeltaSeconds);
						SimulationCommand->VectorFieldTexturesRHI[0] = Simulation->LocalVectorField.Resource->VolumeTextureRHI;
						SetParametersForVectorField(VectorFieldParameters, &Simulation->LocalVectorField, /*EmitterScale=*/ 1.0f, /*EmitterTightness=*/ -1, VectorFieldParameters.Count++);
					}
				}

				// Add any world vector fields that intersect the simulation.
				const float GlobalVectorFieldScale = Simulation->EmitterSimulationResources->GlobalVectorFieldScale;
				const float GlobalVectorFieldTightness = Simulation->EmitterSimulationResources->GlobalVectorFieldTightness;
				if (FMath::Abs(GlobalVectorFieldScale) > 0.0f)
				{
					for (TSparseArray<FVectorFieldInstance*>::TIterator VectorFieldIt(VectorFields); VectorFieldIt && VectorFieldParameters.Count < MAX_VECTOR_FIELDS; ++VectorFieldIt)
					{
						FVectorFieldInstance* Instance = *VectorFieldIt;
						check(Instance && Instance->Resource);
						const float Intensity = Instance->Intensity * Instance->Resource->Intensity;
						if (SimulationBounds.Intersect(Instance->WorldBounds) &&
							FMath::Abs(Intensity) > 0.0f)
						{
							SimulationCommand->VectorFieldTexturesRHI[VectorFieldParameters.Count] = Instance->Resource->VolumeTextureRHI;
							SetParametersForVectorField(VectorFieldParameters, Instance, GlobalVectorFieldScale, GlobalVectorFieldTightness, VectorFieldParameters.Count++);
						}
					}
				}

				// Fill out any remaining vector field entries.
				if (VectorFieldParameters.Count > 0)
				{
					int32 PadCount = VectorFieldParameters.Count;
					while (PadCount < MAX_VECTOR_FIELDS)
					{
						const int32 Index = PadCount++;
						VectorFieldParameters.WorldToVolume[Index] = FMatrix::Identity;
						VectorFieldParameters.VolumeToWorld[Index] = FMatrix::Identity;
						VectorFieldParameters.VolumeSize[Index] = FVector4(1.0f);
						VectorFieldParameters.IntensityAndTightness[Index] = FVector4(0.0f);
					}
					SimulationCommand->VectorFieldsUniformBuffer = FVectorFieldUniformBufferRef::CreateUniformBufferImmediate(VectorFieldParameters, UniformBuffer_SingleFrame);
				}
			}
			// NvFlow begin
#if NV_FLOW_WITH_GPU_PARTICLES
			{
				for (int32 i = 0; i < MAX_NVFLOW_GRIDS; ++i)
				{
					SimulationCommand->NvFlowGridDataSRV[i] = FShaderResourceViewRHIRef();
					SimulationCommand->NvFlowGridBlockTableSRV[i] = FShaderResourceViewRHIRef();
				}

				FNvFlowGridUniformParameters NvFlowGridParameters;
				NvFlowGridParameters.Count = 0;
				if (GGridAccessNvFlowHooks && Simulation->bEnableGridInteraction)
				{
					ParticleSimulationParamsNvFlow ParticleSimulationParams;
					ParticleSimulationParams.InteractionChannel = Simulation->InteractionChannel;
					ParticleSimulationParams.ResponseToInteractionChannels = Simulation->ResponseToInteractionChannels;
					ParticleSimulationParams.Bounds = Simulation->Bounds;
					ParticleSimulationParams.TextureSizeX = GParticleSimulationTextureSizeX;
					ParticleSimulationParams.TextureSizeY = GParticleSimulationTextureSizeY;
					ParticleSimulationParams.PositionTextureRHI = ParticleSimulationResources->GetVisualizeStateTextures().PositionTextureRHI;
					ParticleSimulationParams.VelocityTextureRHI = ParticleSimulationResources->GetVisualizeStateTextures().VelocityTextureRHI;
					ParticleSimulationParams.ParticleCount = Simulation->VertexBuffer.ParticleCount;
					ParticleSimulationParams.VertexBufferSRV = Simulation->VertexBuffer.VertexBufferSRV;

					GridExportParamsNvFlow NvFlowGridParams[MAX_NVFLOW_GRIDS];
					NvFlowGridParameters.Count = GGridAccessNvFlowHooks->NvFlowQueryGridExportParams(RHICmdList, ParticleSimulationParams, MAX_NVFLOW_GRIDS, NvFlowGridParams);
					for (int32 i = 0; i < NvFlowGridParameters.Count; ++i)
					{
						NvFlowGridParameters.BlockDim[i] = NvFlowGridParams[i].BlockDim;
						NvFlowGridParameters.BlockDimBits[i] = NvFlowGridParams[i].BlockDimBits;
						NvFlowGridParameters.BlockDimInv[i] = NvFlowGridParams[i].BlockDimInv;
						NvFlowGridParameters.LinearBlockDim[i] = NvFlowGridParams[i].LinearBlockDim;
						NvFlowGridParameters.LinearBlockOffset[i] = NvFlowGridParams[i].LinearBlockOffset;
						NvFlowGridParameters.DimInv[i] = NvFlowGridParams[i].DimInv;
						NvFlowGridParameters.VDim[i] = NvFlowGridParams[i].VDim;
						NvFlowGridParameters.VDimInv[i] = NvFlowGridParams[i].VDimInv;
						NvFlowGridParameters.PoolGridDim[i] = NvFlowGridParams[i].PoolGridDim;
						NvFlowGridParameters.GridDim[i] = NvFlowGridParams[i].GridDim;
						NvFlowGridParameters.IsVTR[i] = NvFlowGridParams[i].IsVTR ? 1 : 0;
						NvFlowGridParameters.WorldToVolume[i] = NvFlowGridParams[i].WorldToVolume;
						NvFlowGridParameters.VelocityScale[i] = NvFlowGridParams[i].VelocityScale;

						NvFlowGridParameters.GridToParticleAccelRate[i] = Simulation->PerFrameSimulationParameters.DeltaSeconds / NvFlowGridParams[i].GridToParticleAccelTimeConstant;
						NvFlowGridParameters.GridToParticleDecelRate[i] = Simulation->PerFrameSimulationParameters.DeltaSeconds / NvFlowGridParams[i].GridToParticleDecelTimeConstant;
						NvFlowGridParameters.GridToParticleThreshold[i] = NvFlowGridParams[i].GridToParticleThresholdMultiplier;

						SimulationCommand->NvFlowGridDataSRV[i] = NvFlowGridParams[i].DataSRV;
						SimulationCommand->NvFlowGridBlockTableSRV[i] = NvFlowGridParams[i].BlockTableSRV;
					}
				}
				SimulationCommand->NvFlowGridUniformBuffer = FNvFlowGridUniformBufferRef::CreateUniformBufferImmediate(NvFlowGridParameters, UniformBuffer_SingleFrame);
			}
#endif
			// NvFlow end

			// Add to the list of tiles to clear.
			TilesToClear.Append(Simulation->TilesToClear);
			Simulation->TilesToClear.Reset();

			// Add to the list of new particles.
			NewParticles.Append(Simulation->NewParticles);
			FreeNewParticleArray(Simulation->NewParticles);

			// Reset pending simulation time. This prevents an emitter from simulating twice if we don't get an update from the game thread, e.g. the component didn't tick last frame.
			Simulation->PerFrameSimulationParameters.ResetDeltaSeconds();
		}
	}
	
	RHICmdList.BeginUpdateMultiFrameResource(CurrentStateRenderTargets[0]);
	RHICmdList.BeginUpdateMultiFrameResource(CurrentStateRenderTargets[1]);
	
	if ( SimulationCommands.Num() || TilesToClear.Num())
	{
		SetRenderTargets(RHICmdList, 2, CurrentStateRenderTargets, FTextureRHIParamRef(), 0, NULL);
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		
		RHICmdList.SetViewport(0, 0, 0.0f, GParticleSimulationTextureSizeX, GParticleSimulationTextureSizeY, 1.0f);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		
		// Simulate particles in all active tiles.
		if ( SimulationCommands.Num() )
		{
			SCOPED_DRAW_EVENT(RHICmdList, ParticleSimulationCommands);
			
			ExecuteSimulationCommands(
						RHICmdList,
						GraphicsPSOInit,
						FeatureLevel,
						SimulationCommands,
						ParticleSimulationResources,
						ViewUniformBuffer,
						GlobalDistanceFieldParameterData,
						SceneDepthTexture,
						GBufferATexture,
						Phase,
						FixDeltaSeconds > 0
						);
		}

		// Clear any newly allocated tiles.
		if (TilesToClear.Num())
		{
			SCOPED_DRAW_EVENT(RHICmdList, ParticleTilesClear);
			
			ClearTiles(RHICmdList, GraphicsPSOInit, FeatureLevel, TilesToClear);
		}
	}

	// Inject any new particles that have spawned into the simulation.
	if (NewParticles.Num())
	{
		SCOPED_DRAW_EVENT(RHICmdList, ParticleInjection);
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ParticleSimulation);		

		// Set render targets.
		FTextureRHIParamRef InjectRenderTargets[4] =
		{
			CurrentStateTextures.PositionTextureTargetRHI,
			CurrentStateTextures.VelocityTextureTargetRHI,
			ParticleSimulationResources->RenderAttributesTexture.TextureTargetRHI,
			ParticleSimulationResources->SimulationAttributesTexture.TextureTargetRHI
		};
		RHICmdList.BeginUpdateMultiFrameResource(ParticleSimulationResources->RenderAttributesTexture.TextureTargetRHI);
		RHICmdList.BeginUpdateMultiFrameResource(ParticleSimulationResources->SimulationAttributesTexture.TextureTargetRHI);

		SetRenderTargets(RHICmdList, 4, InjectRenderTargets, FTextureRHIParamRef(), 0, NULL, true);
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		
		RHICmdList.SetViewport(0, 0, 0.0f, GParticleSimulationTextureSizeX, GParticleSimulationTextureSizeY, 1.0f);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		// Inject particles.
		InjectNewParticles<false>(RHICmdList, GraphicsPSOInit, FeatureLevel, NewParticles);

		// Resolve attributes textures. State textures are resolved later.
		RHICmdList.CopyToResolveTarget(
			ParticleSimulationResources->RenderAttributesTexture.TextureTargetRHI,
			ParticleSimulationResources->RenderAttributesTexture.TextureRHI,
			/*bKeepOriginalSurface=*/ false,
			FResolveParams()
			);
		RHICmdList.CopyToResolveTarget(
			ParticleSimulationResources->SimulationAttributesTexture.TextureTargetRHI,
			ParticleSimulationResources->SimulationAttributesTexture.TextureRHI,
			/*bKeepOriginalSurface=*/ false,
			FResolveParams()
			);

		if (GNumActiveGPUsForRendering > 1 && CVarGPUParticleAFRReinject.GetValueOnRenderThread() == 1)
		{			
			ensureMsgf(GNumActiveGPUsForRendering == 2, TEXT("GPU Particles running on an AFR depth > 2 not supported.  Currently: %i"), GNumActiveGPUsForRendering);

			// Place these particles into the multi-gpu update queue
			LastFrameNewParticles.Append(NewParticles);
		}
		RHICmdList.EndUpdateMultiFrameResource(ParticleSimulationResources->RenderAttributesTexture.TextureTargetRHI);
		RHICmdList.EndUpdateMultiFrameResource(ParticleSimulationResources->SimulationAttributesTexture.TextureTargetRHI);
	}
	
	// finish current state render
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, CurrentStateRenderTargets, 2);
	RHICmdList.EndUpdateMultiFrameResource(CurrentStateRenderTargets[0]);
	RHICmdList.EndUpdateMultiFrameResource(CurrentStateRenderTargets[1]);

	if (SimulationCommands.Num() && FixDeltaSeconds > 0)
	{
		//the fixed timestep works in two stages.  A first stage which simulates the fixed timestep and this second stage which simulates any remaining time from the actual delta time.  e.g.  fixed timestep of 16ms and actual dt of 23ms
		//will make this second step simulate an interpolated extra 7ms.  This second interpolated step is what we render on THIS frame, but it is NOT fed into the next frame's simulation.  Thus we do not need to transfer it between GPUs in AFR mode.
		FParticleStateTextures& VisualizeStateTextures = ParticleSimulationResources->GetPreviousStateTextures();
		FTextureRHIParamRef VisualizeStateRHIs[2] = { VisualizeStateTextures.PositionTextureTargetRHI, VisualizeStateTextures.VelocityTextureTargetRHI };
		RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, VisualizeStateRHIs, 2);
		
		SetRenderTargets(RHICmdList, 2, VisualizeStateRHIs, FTextureRHIParamRef(), 0, NULL);
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		
		ExecuteSimulationCommands(
					RHICmdList,
					GraphicsPSOInit,
					FeatureLevel,
					SimulationCommands,
					ParticleSimulationResources,
					ViewUniformBuffer,
					GlobalDistanceFieldParameterData,
					SceneDepthTexture,
					GBufferATexture,
					Phase,
					false
					);
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, VisualizeStateRHIs, 2);		
	}

	SimulationCommands.Reset();
	TilesToClear.Reset();
	NewParticles.Reset();

	// Clear render targets so we can safely read from them.
	SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());

	// Stats.
	if (Phase == GetLastParticleSimulationPhase(GetShaderPlatform()))
	{
		INC_DWORD_STAT_BY(STAT_FreeGPUTiles,ParticleSimulationResources->GetFreeTileCount());
	}
}

void FFXSystem::UpdateMultiGPUResources(FRHICommandListImmediate& RHICmdList)
{
	if (LastFrameNewParticles.Num())
	{		
		//Inject particles spawned in the last frame, but only update the attribute textures
		SCOPED_DRAW_EVENT(RHICmdList, ParticleInjection);
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ParticleSimulation);

		// Set render targets.
		FTextureRHIParamRef InjectRenderTargets[2] =
		{
			ParticleSimulationResources->RenderAttributesTexture.TextureTargetRHI,
			ParticleSimulationResources->SimulationAttributesTexture.TextureTargetRHI
		};
		SetRenderTargets(RHICmdList, 2, InjectRenderTargets, FTextureRHIParamRef(), 0, NULL, true);
		RHICmdList.SetViewport(0, 0, 0.0f, GParticleSimulationTextureSizeX, GParticleSimulationTextureSizeY, 1.0f);
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		// Inject particles.
		InjectNewParticles<true>(RHICmdList, GraphicsPSOInit, FeatureLevel, LastFrameNewParticles);

		// Resolve attributes textures. State textures are resolved later.
		RHICmdList.CopyToResolveTarget(
			ParticleSimulationResources->RenderAttributesTexture.TextureTargetRHI,
			ParticleSimulationResources->RenderAttributesTexture.TextureRHI,
			/*bKeepOriginalSurface=*/ false,
			FResolveParams()
		);
		RHICmdList.CopyToResolveTarget(
			ParticleSimulationResources->SimulationAttributesTexture.TextureTargetRHI,
			ParticleSimulationResources->SimulationAttributesTexture.TextureRHI,
			/*bKeepOriginalSurface=*/ false,
			FResolveParams()
		);

	}

	// Clear out particles from last frame
	LastFrameNewParticles.Reset();
}

void FFXSystem::VisualizeGPUParticles(FCanvas* Canvas)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
		FVisualizeGPUParticlesCommand,
		FFXSystem*, FXSystem, this,
		int32, VisualizationMode, FXConsoleVariables::VisualizeGPUSimulation,
		FRenderTarget*, RenderTarget, Canvas->GetRenderTarget(),
		ERHIFeatureLevel::Type, FeatureLevel, GetFeatureLevel(),
	{
		FParticleSimulationResources* Resources = FXSystem->GetParticleSimulationResources();
		FParticleStateTextures& CurrentStateTextures = Resources->GetVisualizeStateTextures();
		VisualizeGPUSimulation(RHICmdList, FeatureLevel, VisualizationMode, RenderTarget, CurrentStateTextures, GParticleCurveTexture.GetCurveTexture());
	});
}

/*-----------------------------------------------------------------------------
	External interface.
-----------------------------------------------------------------------------*/

FParticleEmitterInstance* FFXSystem::CreateGPUSpriteEmitterInstance( FGPUSpriteEmitterInfo& EmitterInfo )
{
	return new FGPUSpriteParticleEmitterInstance( this, EmitterInfo );
}

/**
 * Sets GPU sprite resource data.
 * @param Resources - Sprite resources to update.
 * @param InResourceData - Data with which to update resources.
 */
static void SetGPUSpriteResourceData( FGPUSpriteResources* Resources, const FGPUSpriteResourceData& InResourceData )
{
	// Allocate texels for all curves.
	Resources->ColorTexelAllocation = GParticleCurveTexture.AddCurve( InResourceData.QuantizedColorSamples );
	Resources->MiscTexelAllocation = GParticleCurveTexture.AddCurve( InResourceData.QuantizedMiscSamples );
	Resources->SimulationAttrTexelAllocation = GParticleCurveTexture.AddCurve( InResourceData.QuantizedSimulationAttrSamples );

	// Setup uniform parameters for the emitter.
	Resources->UniformParameters.ColorCurve = GParticleCurveTexture.ComputeCurveScaleBias(Resources->ColorTexelAllocation);
	Resources->UniformParameters.ColorScale = InResourceData.ColorScale;
	Resources->UniformParameters.ColorBias = InResourceData.ColorBias;

	Resources->UniformParameters.MiscCurve = GParticleCurveTexture.ComputeCurveScaleBias(Resources->MiscTexelAllocation);
	Resources->UniformParameters.MiscScale = InResourceData.MiscScale;
	Resources->UniformParameters.MiscBias = InResourceData.MiscBias;

	Resources->UniformParameters.SizeBySpeed = InResourceData.SizeBySpeed;
	Resources->UniformParameters.SubImageSize = InResourceData.SubImageSize;

	// Setup tangent selector parameter.
	const EParticleAxisLock LockAxisFlag = (EParticleAxisLock)InResourceData.LockAxisFlag;
	const bool bRotationLock = (LockAxisFlag >= EPAL_ROTATE_X) && (LockAxisFlag <= EPAL_ROTATE_Z);

	Resources->UniformParameters.TangentSelector = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	Resources->UniformParameters.RotationBias = 0.0f;

	if (InResourceData.ScreenAlignment == PSA_Velocity)
	{
		Resources->UniformParameters.TangentSelector.Y = 1;
	}
	else if(LockAxisFlag == EPAL_NONE )
	{
		if (InResourceData.ScreenAlignment == PSA_Square)
		{
			Resources->UniformParameters.TangentSelector.X = 1;
		}
		else if (InResourceData.ScreenAlignment == PSA_FacingCameraPosition)
		{
			Resources->UniformParameters.TangentSelector.W = 1;
		}
	}
	else
	{
		if ( bRotationLock )
		{
			Resources->UniformParameters.TangentSelector.Z = 1.0f;
		}
		else
		{
			Resources->UniformParameters.TangentSelector.X = 1.0f;
		}

		// For locked rotation about Z the particle should be rotated by 90 degrees.
		Resources->UniformParameters.RotationBias = (LockAxisFlag == EPAL_ROTATE_Z) ? (0.5f * PI) : 0.0f;
	}

	// Alignment overrides
	Resources->UniformParameters.RemoveHMDRoll = InResourceData.bRemoveHMDRoll ? 1.f : 0.f;

	if (InResourceData.ScreenAlignment == PSA_FacingCameraDistanceBlend)
	{
		float DistanceBlendMinSq = InResourceData.MinFacingCameraBlendDistance * InResourceData.MinFacingCameraBlendDistance;
		float DistanceBlendMaxSq = InResourceData.MaxFacingCameraBlendDistance * InResourceData.MaxFacingCameraBlendDistance;
		float InvBlendRange = 1.f / FMath::Max(DistanceBlendMaxSq - DistanceBlendMinSq, 1.f);
		float BlendScaledMinDistace = DistanceBlendMinSq * InvBlendRange;

		Resources->UniformParameters.CameraFacingBlend.X = 1.f;
		Resources->UniformParameters.CameraFacingBlend.Y = InvBlendRange;
		Resources->UniformParameters.CameraFacingBlend.Z = BlendScaledMinDistace;

		// Treat as camera facing if needed
		Resources->UniformParameters.TangentSelector.W = 1.0f;
	}
	else
	{
		Resources->UniformParameters.CameraFacingBlend.X = 0.f;
		Resources->UniformParameters.CameraFacingBlend.Y = 0.f;
		Resources->UniformParameters.CameraFacingBlend.Z = 0.f;
	}

	Resources->UniformParameters.RotationRateScale = InResourceData.RotationRateScale;
	Resources->UniformParameters.CameraMotionBlurAmount = InResourceData.CameraMotionBlurAmount;

	Resources->UniformParameters.PivotOffset = InResourceData.PivotOffset;

	Resources->SimulationParameters.AttributeCurve = GParticleCurveTexture.ComputeCurveScaleBias(Resources->SimulationAttrTexelAllocation);
	Resources->SimulationParameters.AttributeCurveScale = InResourceData.SimulationAttrCurveScale;
	Resources->SimulationParameters.AttributeCurveBias = InResourceData.SimulationAttrCurveBias;
	Resources->SimulationParameters.AttributeScale = FVector4(
		InResourceData.DragCoefficientScale,
		InResourceData.PerParticleVectorFieldScale,
		InResourceData.ResilienceScale,
		1.0f  // OrbitRandom
		);
	Resources->SimulationParameters.AttributeBias = FVector4(
		InResourceData.DragCoefficientBias,
		InResourceData.PerParticleVectorFieldBias,
		InResourceData.ResilienceBias,
		0.0f  // OrbitRandom
		);
	Resources->SimulationParameters.MiscCurve = Resources->UniformParameters.MiscCurve;
	Resources->SimulationParameters.MiscScale = Resources->UniformParameters.MiscScale;
	Resources->SimulationParameters.MiscBias = Resources->UniformParameters.MiscBias;
	Resources->SimulationParameters.Acceleration = InResourceData.ConstantAcceleration;
	Resources->SimulationParameters.OrbitOffsetBase = InResourceData.OrbitOffsetBase;
	Resources->SimulationParameters.OrbitOffsetRange = InResourceData.OrbitOffsetRange;
	Resources->SimulationParameters.OrbitFrequencyBase = InResourceData.OrbitFrequencyBase;
	Resources->SimulationParameters.OrbitFrequencyRange = InResourceData.OrbitFrequencyRange;
	Resources->SimulationParameters.OrbitPhaseBase = InResourceData.OrbitPhaseBase;
	Resources->SimulationParameters.OrbitPhaseRange = InResourceData.OrbitPhaseRange;
	Resources->SimulationParameters.CollisionRadiusScale = InResourceData.CollisionRadiusScale;
	Resources->SimulationParameters.CollisionRadiusBias = InResourceData.CollisionRadiusBias;
	Resources->SimulationParameters.CollisionTimeBias = InResourceData.CollisionTimeBias;
	Resources->SimulationParameters.CollisionRandomSpread = InResourceData.CollisionRandomSpread;
	Resources->SimulationParameters.CollisionRandomDistribution = InResourceData.CollisionRandomDistribution;
	Resources->SimulationParameters.OneMinusFriction = InResourceData.OneMinusFriction;
	Resources->EmitterSimulationResources.GlobalVectorFieldScale = InResourceData.GlobalVectorFieldScale;
	Resources->EmitterSimulationResources.GlobalVectorFieldTightness = InResourceData.GlobalVectorFieldTightness;
}

/**
 * Clears GPU sprite resource data.
 * @param Resources - Sprite resources to update.
 * @param InResourceData - Data with which to update resources.
 */
static void ClearGPUSpriteResourceData( FGPUSpriteResources* Resources )
{
	GParticleCurveTexture.RemoveCurve( Resources->ColorTexelAllocation );
	GParticleCurveTexture.RemoveCurve( Resources->MiscTexelAllocation );
	GParticleCurveTexture.RemoveCurve( Resources->SimulationAttrTexelAllocation );
}

FGPUSpriteResources* BeginCreateGPUSpriteResources( const FGPUSpriteResourceData& InResourceData )
{
	FGPUSpriteResources* Resources = NULL;
	if (RHISupportsGPUParticles())
	{
		Resources = new FGPUSpriteResources;
		//@TODO Ideally FGPUSpriteEmitterInfo::Resources would be a TRefCountPtr<FGPUSpriteResources>, but
		//since that class is defined in this file, we can't do that, so we just addref here instead
		Resources->AddRef();
		SetGPUSpriteResourceData( Resources, InResourceData );
		BeginInitResource( Resources );
	}
	return Resources;
}

void BeginUpdateGPUSpriteResources( FGPUSpriteResources* Resources, const FGPUSpriteResourceData& InResourceData )
{
	check( Resources );
	ClearGPUSpriteResourceData( Resources );
	SetGPUSpriteResourceData( Resources, InResourceData );
	BeginUpdateResourceRHI( Resources );
}

void BeginReleaseGPUSpriteResources( FGPUSpriteResources* Resources )
{
	if ( Resources )
	{
		ClearGPUSpriteResourceData( Resources );
		// Deletion of this resource is deferred until all particle
		// systems on the render thread have finished with it.
		Resources->Release();
	}
}
