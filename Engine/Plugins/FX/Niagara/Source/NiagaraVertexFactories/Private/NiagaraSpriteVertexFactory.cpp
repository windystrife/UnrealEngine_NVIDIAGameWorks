// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.cpp: Particle vertex factory implementation.
=============================================================================*/

#include "NiagaraSpriteVertexFactory.h"
#include "ParticleHelper.h"
#include "ParticleResources.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNiagaraSpriteUniformParameters,TEXT("NiagaraSpriteVF"));

TGlobalResource<FNullDynamicParameterVertexBuffer> GNullNiagaraDynamicParameterVertexBuffer;

class FNiagaraNullSubUVCutoutVertexBuffer : public FVertexBuffer
{
public:
	/**
	 * Initialize the RHI for this rendering resource
	 */
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		void* BufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(sizeof(FVector2D) * 4, BUF_Static | BUF_ShaderResource, CreateInfo, BufferData);
		FMemory::Memzero(BufferData, sizeof(FVector2D) * 4);
		RHIUnlockVertexBuffer(VertexBufferRHI);
		
		VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(FVector2D), PF_G32R32F);
	}
	
	virtual void ReleaseRHI() override
	{
		VertexBufferSRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}
	
	FShaderResourceViewRHIRef VertexBufferSRV;
};
TGlobalResource<FNiagaraNullSubUVCutoutVertexBuffer> GFNiagaraNullSubUVCutoutVertexBuffer;

/**
 * Shader parameters for the particle vertex factory.
 */
class FNiagaraSpriteVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:

	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
	}

	virtual void Serialize(FArchive& Ar) override
	{
	}
};

class FNiagaraSpriteVertexFactoryShaderParametersVS : public FNiagaraSpriteVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		NumCutoutVerticesPerFrame.Bind(ParameterMap, TEXT("NumCutoutVerticesPerFrame"));
		CutoutGeometry.Bind(ParameterMap, TEXT("CutoutGeometry"));
		NiagaraParticleDataFloat.Bind(ParameterMap, TEXT("NiagaraParticleDataFloat"));
		NiagaraParticleDataInt.Bind(ParameterMap, TEXT("NiagaraParticleDataInt"));
		SafeComponentBufferSizeParam.Bind(ParameterMap, TEXT("SafeComponentBufferSize"));
		UseCustomAlignmentVector.Bind(ParameterMap, TEXT("UseCustomAlignment"));
		UseVectorAlignment.Bind(ParameterMap, TEXT("UseVectorAlignment"));
		UseCameraPlaneFacing.Bind(ParameterMap, TEXT("CameraPlaneFacing"));
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << NumCutoutVerticesPerFrame;
		Ar << CutoutGeometry;
		Ar << UseCustomAlignmentVector;
		Ar << UseVectorAlignment;
		Ar << UseCameraPlaneFacing;
		Ar << NiagaraParticleDataFloat;
		Ar << NiagaraParticleDataInt;
		Ar << SafeComponentBufferSizeParam;

		Ar << PositionOffsetParam;
		Ar << SizeOffsetParam;
		Ar << RotationOffsetParam;
		Ar << SubimgOffsetParam;
		Ar << ColorOffsetParam;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		FNiagaraSpriteVertexFactory* SpriteVF = (FNiagaraSpriteVertexFactory*)VertexFactory;
		FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShaderRHI, Shader->GetUniformBufferParameter<FNiagaraSpriteUniformParameters>(), SpriteVF->GetSpriteUniformBuffer() );
		
		SetShaderValue(RHICmdList, VertexShaderRHI, NumCutoutVerticesPerFrame, SpriteVF->GetNumCutoutVerticesPerFrame());
		FShaderResourceViewRHIParamRef NullSRV = GFNiagaraNullSubUVCutoutVertexBuffer.VertexBufferSRV;
		SetSRVParameter(RHICmdList, VertexShaderRHI, CutoutGeometry, SpriteVF->GetCutoutGeometrySRV() ? SpriteVF->GetCutoutGeometrySRV() : NullSRV);

		SetShaderValue(RHICmdList, VertexShaderRHI, UseCustomAlignmentVector, SpriteVF->GetCustomAlignment());
		SetShaderValue(RHICmdList, VertexShaderRHI, UseVectorAlignment, SpriteVF->GetVectorAligned());
		SetShaderValue(RHICmdList, VertexShaderRHI, UseCameraPlaneFacing, SpriteVF->GetCameraPlaneFacing());

		SetSRVParameter(RHICmdList, VertexShaderRHI, NiagaraParticleDataFloat, SpriteVF->GetFloatDataSRV());
		SetSRVParameter(RHICmdList, VertexShaderRHI, NiagaraParticleDataInt, SpriteVF->GetIntDataSRV());
		SetShaderValue(RHICmdList, VertexShaderRHI, SafeComponentBufferSizeParam, SpriteVF->GetComponentBufferSize());
	}

private:
	FShaderParameter NumCutoutVerticesPerFrame;
	FShaderParameter UseCustomAlignmentVector;
	FShaderParameter UseVectorAlignment;
	FShaderParameter UseCameraPlaneFacing;
	FShaderResourceParameter CutoutGeometry;
	FShaderResourceParameter NiagaraParticleDataFloat;
	FShaderResourceParameter NiagaraParticleDataInt;
	FShaderParameter SafeComponentBufferSizeParam;

	FShaderParameter PositionOffsetParam;
	FShaderParameter SizeOffsetParam;
	FShaderParameter RotationOffsetParam;
	FShaderParameter SubimgOffsetParam;
	FShaderParameter ColorOffsetParam;
};

class FNiagaraSpriteVertexFactoryShaderParametersPS : public FNiagaraSpriteVertexFactoryShaderParameters
{
public:

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		FNiagaraSpriteVertexFactory* SpriteVF = (FNiagaraSpriteVertexFactory*)VertexFactory;
		FPixelShaderRHIParamRef PixelShaderRHI = Shader->GetPixelShader();
		SetUniformBufferParameter(RHICmdList, PixelShaderRHI, Shader->GetUniformBufferParameter<FNiagaraSpriteUniformParameters>(), SpriteVF->GetSpriteUniformBuffer() );
	}
};

/**
 * The particle system vertex declaration resource type.
 */
class FNiagaraSpriteVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Constructor.
	FNiagaraSpriteVertexDeclaration(bool bInInstanced, int32 InNumVertsInInstanceBuffer) :
		bInstanced(bInInstanced),
		NumVertsInInstanceBuffer(InNumVertsInInstanceBuffer)
	{

	}

	// Destructor.
	virtual ~FNiagaraSpriteVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, int32& Offset)
	{
		uint32 InitialStride = sizeof(float) * 2;
		uint32 PerParticleStride = (sizeof(float) * 4*4 + sizeof(float)*3*2);

		/** The stream to read the texture coordinates from. */
		check( Offset == 0 );
		uint32 Stride = bInstanced ? InitialStride : InitialStride + PerParticleStride;
		Elements.Add(FVertexElement(0, Offset, VET_Float2, 4, Stride, false));
		Offset += sizeof(float) * 2;

		/** The per-particle streams follow. */
		if(bInstanced) 
		{
			Offset = 0;
			// update stride
			Stride = PerParticleStride;
		}

		/** The stream to read the vertex position from. */
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float4, 0, Stride, bInstanced));
		Offset += sizeof(float) * 4;
		/** The stream to read the vertex old position from. */
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float4, 1, Stride, bInstanced));
		Offset += sizeof(float) * 4;
		/** The stream to read the vertex size/rot/subimage from. */
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float4, 2, Stride, bInstanced));
		Offset += sizeof(float) * 4;
		/** The stream to read the color from.					*/
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float4, 3, Stride, bInstanced));
		Offset += sizeof(float) * 4;

		/** The stream to read the custom alignment vector from. */
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float3, 6, Stride, bInstanced));
		Offset += sizeof(float) * 3;
		/** The stream to read the custom facing vector from. */
		Elements.Add(FVertexElement(bInstanced ? 1 : 0, Offset, VET_Float3, 7, Stride, bInstanced));
		Offset += sizeof(float) * 3;

		/** The per-particle dynamic parameter stream */

		// The -V519 disables a warning from PVS-Studio's static analyzer. It noticed that offset is assigned
		// twice before being read. It is probably safer to leave the redundant assignments here to reduce
		// the chance of an error being introduced if this code is modified.
		Offset = 0;  //-V519
		Stride = sizeof(float) * 4;
		Elements.Add(FVertexElement(bInstanced ? 2 : 1, Offset, VET_Float4, 5, Stride, bInstanced));
		Offset += sizeof(float) * 4;

	}

	virtual void InitDynamicRHI()
	{
		FVertexDeclarationElementList Elements;
		int32	Offset = 0;

		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		// This is done in InitDynamicRHI instead of InitRHI to allow FParticleSpriteVertexFactory::InitRHI
		// to rely on it being initialized, since InitDynamicRHI is called before InitRHI.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseDynamicRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}

private:

	bool bInstanced;
	int32 NumVertsInInstanceBuffer;
};

/** The simple element vertex declaration. */
static TGlobalResource<FNiagaraSpriteVertexDeclaration> GParticleSpriteVertexDeclarationInstanced(true, 4);
static TGlobalResource<FNiagaraSpriteVertexDeclaration> GParticleSpriteEightVertexDeclarationInstanced(true, 8);
static TGlobalResource<FNiagaraSpriteVertexDeclaration> GParticleSpriteVertexDeclarationNonInstanced(false, 4);
static TGlobalResource<FNiagaraSpriteVertexDeclaration> GParticleSpriteEightVertexDeclarationNonInstanced(false, 8);

inline TGlobalResource<FNiagaraSpriteVertexDeclaration>& GetNiagaraSpriteVertexDeclaration(bool SupportsInstancing, int32 NumVertsInInstanceBuffer)
{
	check(NumVertsInInstanceBuffer == 4 || NumVertsInInstanceBuffer == 8);
	if (SupportsInstancing)
	{
		return NumVertsInInstanceBuffer == 4 ? GParticleSpriteVertexDeclarationInstanced : GParticleSpriteEightVertexDeclarationInstanced;
	}
	else
	{
		return NumVertsInInstanceBuffer == 4 ? GParticleSpriteVertexDeclarationNonInstanced : GParticleSpriteEightVertexDeclarationNonInstanced;
	}
}

bool FNiagaraSpriteVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return Material->IsUsedWithNiagaraSprites() || Material->IsSpecialEngineMaterial();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FNiagaraSpriteVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FParticleVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

	// Set a define so we can tell in MaterialTemplate.usf when we are compiling a sprite vertex factory
	OutEnvironment.SetDefine(TEXT("PARTICLE_SPRITE_FACTORY"),TEXT("1"));
}

/**
 *	Initialize the Render Hardware Interface for this vertex factory
 */
void FNiagaraSpriteVertexFactory::InitRHI()
{
	InitStreams();
	SetDeclaration(GetNiagaraSpriteVertexDeclaration(GRHISupportsInstancing, NumVertsInInstanceBuffer).VertexDeclarationRHI);
}

void FNiagaraSpriteVertexFactory::InitStreams()
{
    const bool bInstanced = GRHISupportsInstancing;

	check(Streams.Num() == 0);
	if(bInstanced) 
	{
		FVertexStream* TexCoordStream = new(Streams) FVertexStream;
		TexCoordStream->VertexBuffer = &GParticleTexCoordVertexBuffer;
		TexCoordStream->Stride = sizeof(FVector2D);
		TexCoordStream->Offset = 0;
	}
	FVertexStream* InstanceStream = new(Streams) FVertexStream;
	FVertexStream* DynamicParameterStream = new(Streams) FVertexStream;
}

void FNiagaraSpriteVertexFactory::SetInstanceBuffer(const FVertexBuffer* InInstanceBuffer, uint32 StreamOffset, uint32 Stride, bool bInstanced)
{
	check(Streams.Num() == (bInstanced ? 3 : 2));
	FVertexStream& InstanceStream = Streams[bInstanced ? 1 : 0];
	InstanceStream.VertexBuffer = InInstanceBuffer;
	InstanceStream.Stride = Stride;
	InstanceStream.Offset = StreamOffset;
}

void FNiagaraSpriteVertexFactory::SetTexCoordBuffer(const FVertexBuffer* InTexCoordBuffer)
{
	FVertexStream& TexCoordStream = Streams[0];
	TexCoordStream.VertexBuffer = InTexCoordBuffer;
}

void FNiagaraSpriteVertexFactory::SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride, bool bInstanced)
{
	check(Streams.Num() == (bInstanced ? 3 : 2));
	FVertexStream& DynamicParameterStream = Streams[bInstanced ? 2 : 1];
	if (InDynamicParameterBuffer)
	{
		DynamicParameterStream.VertexBuffer = InDynamicParameterBuffer;
		DynamicParameterStream.Stride = Stride;
		DynamicParameterStream.Offset = StreamOffset;
	}
	else
	{
		DynamicParameterStream.VertexBuffer = &GNullNiagaraDynamicParameterVertexBuffer;
		DynamicParameterStream.Stride = 0;
		DynamicParameterStream.Offset = 0;
	}
}

FVertexFactoryShaderParameters* FNiagaraSpriteVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	if (ShaderFrequency == SF_Vertex)
	{
		return new FNiagaraSpriteVertexFactoryShaderParametersVS();
	}
	else if (ShaderFrequency == SF_Pixel)
	{
		return new FNiagaraSpriteVertexFactoryShaderParametersPS();
	}
	return NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FNiagaraSpriteVertexFactory,"/Engine/Private/NiagaraSpriteVertexFactory.ush",true,false,true,false,false);
