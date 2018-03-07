// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LandscapeRenderMobile.cpp: Landscape Rendering without using vertex texture fetch
=============================================================================*/

#include "LandscapeRenderMobile.h"
#include "ShaderParameterUtils.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

void FLandscapeVertexFactoryMobile::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;

	// position decls
	Elements.Add(AccessStreamComponent(MobileData.PositionComponent,0));

	if (MobileData.LODHeightsComponent.Num())
	{
		const int32 BaseAttribute = 1;
		for(int32 Index = 0;Index < MobileData.LODHeightsComponent.Num();Index++)
		{
			Elements.Add(AccessStreamComponent(MobileData.LODHeightsComponent[Index], BaseAttribute + Index));
		}
	}

	// create the actual device decls
	InitDeclaration(Elements);
}

/** Shader parameters for use with FLandscapeVertexFactory */
class FLandscapeVertexFactoryMobileVertexShaderParameters : public FVertexFactoryShaderParameters
{
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		LodValuesParameter.Bind(ParameterMap,TEXT("LodValues"));
		NeighborSectionLodParameter.Bind(ParameterMap,TEXT("NeighborSectionLod"));
		LodBiasParameter.Bind(ParameterMap,TEXT("LodBias"));
		SectionLodsParameter.Bind(ParameterMap,TEXT("SectionLods"));
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar) override
	{
		Ar << LodValuesParameter;
		Ar << NeighborSectionLodParameter;
		Ar << LodBiasParameter;
		Ar << SectionLodsParameter;
	}
	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* VertexShader,const class FVertexFactory* VertexFactory,const class FSceneView& View,const struct FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		SCOPE_CYCLE_COUNTER(STAT_LandscapeVFDrawTime);

		const FLandscapeBatchElementParams* BatchElementParams = (const FLandscapeBatchElementParams*)BatchElement.UserData;
		check(BatchElementParams);

		const FLandscapeComponentSceneProxyMobile* SceneProxy = (const FLandscapeComponentSceneProxyMobile*)BatchElementParams->SceneProxy;
		SetUniformBufferParameter(RHICmdList, VertexShader->GetVertexShader(),VertexShader->GetUniformBufferParameter<FLandscapeUniformShaderParameters>(),*BatchElementParams->LandscapeUniformShaderParametersResource);

		FVector CameraLocalPos3D = SceneProxy->WorldToLocal.TransformPosition(View.ViewMatrices.GetViewOrigin()); 
		FVector2D CameraLocalPos = FVector2D(CameraLocalPos3D.X, CameraLocalPos3D.Y);

		if( LodBiasParameter.IsBound() )
		{
			FVector4 LodBias(
				0.0f, // unused
				0.0f, // unused
				CameraLocalPos3D.X + SceneProxy->SectionBase.X,
				CameraLocalPos3D.Y + SceneProxy->SectionBase.Y 
				);
			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(), LodBiasParameter, LodBias);
		}

		// Calculate LOD params
		FVector4 fCurrentLODs;
		FVector4 CurrentNeighborLODs[4];

		if( BatchElementParams->SubX == -1 )
		{
			for( int32 SubY = 0; SubY < SceneProxy->NumSubsections; SubY++ )
			{
				for( int32 SubX = 0; SubX < SceneProxy->NumSubsections; SubX++ )
				{
					int32 SubIndex = SubX + 2 * SubY;
					SceneProxy->CalcLODParamsForSubsection(View, CameraLocalPos, SubX, SubY, BatchElementParams->CurrentLOD, fCurrentLODs[SubIndex], CurrentNeighborLODs[SubIndex]);
				}
			}
		}
		else
		{
			int32 SubIndex = BatchElementParams->SubX + 2 * BatchElementParams->SubY;
			SceneProxy->CalcLODParamsForSubsection(View, CameraLocalPos, BatchElementParams->SubX, BatchElementParams->SubY, BatchElementParams->CurrentLOD, fCurrentLODs[SubIndex], CurrentNeighborLODs[SubIndex]);
		}

		if( SectionLodsParameter.IsBound() )
		{
			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(), SectionLodsParameter, fCurrentLODs);
		}

		if( NeighborSectionLodParameter.IsBound() )
		{
			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(), NeighborSectionLodParameter, CurrentNeighborLODs);
		}

		if( LodValuesParameter.IsBound() )
		{
			FVector4 LodValues(
				0.0f, // this is the mesh's LOD, ES2 always uses the LOD0 mesh
				0.0f, // unused
				(float)SceneProxy->SubsectionSizeQuads,
				1.f / (float)SceneProxy->SubsectionSizeQuads );

			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(),LodValuesParameter,LodValues);
		}
	}
protected:
	FShaderParameter LodValuesParameter;
	FShaderParameter NeighborSectionLodParameter;
	FShaderParameter LodBiasParameter;
	FShaderParameter SectionLodsParameter;
	TShaderUniformBufferParameter<FLandscapeUniformShaderParameters> LandscapeShaderParameters;
};

/** Shader parameters for use with FLandscapeVertexFactory */
class FLandscapeVertexFactoryMobilePixelShaderParameters : public FLandscapeVertexFactoryPixelShaderParameters
{
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		FLandscapeVertexFactoryPixelShaderParameters::Bind(ParameterMap);
		BlendableLayerMaskParameter.Bind(ParameterMap, TEXT("BlendableLayerMask"));
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar) override
	{
		FLandscapeVertexFactoryPixelShaderParameters::Serialize(Ar);
		Ar << BlendableLayerMaskParameter;
	}
	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* PixelShader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		SCOPE_CYCLE_COUNTER(STAT_LandscapeVFDrawTime);

		FLandscapeVertexFactoryPixelShaderParameters::SetMesh(RHICmdList, PixelShader, VertexFactory, View, BatchElement, DataFlags);

		const FLandscapeBatchElementParams* BatchElementParams = (const FLandscapeBatchElementParams*)BatchElement.UserData;
		check(BatchElementParams);
		const FLandscapeComponentSceneProxyMobile* SceneProxy = (const FLandscapeComponentSceneProxyMobile*)BatchElementParams->SceneProxy;

		if (BlendableLayerMaskParameter.IsBound())
		{
			FVector MaskVector;
			MaskVector[0] = (SceneProxy->BlendableLayerMask & (1 << 0)) ? 1 : 0;
			MaskVector[1] = (SceneProxy->BlendableLayerMask & (1 << 1)) ? 1 : 0;
			MaskVector[2] = (SceneProxy->BlendableLayerMask & (1 << 2)) ? 1 : 0;
			SetShaderValue(RHICmdList, PixelShader->GetPixelShader(), BlendableLayerMaskParameter, MaskVector);
		}
	}
protected:
	FShaderParameter BlendableLayerMaskParameter;
};

FVertexFactoryShaderParameters* FLandscapeVertexFactoryMobile::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	switch( ShaderFrequency )
	{
	case SF_Vertex:
		return new FLandscapeVertexFactoryMobileVertexShaderParameters();
	case SF_Pixel:
		return new FLandscapeVertexFactoryMobilePixelShaderParameters();
	default:
		return NULL;
	}
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FLandscapeVertexFactoryMobile, "/Engine/Private/LandscapeVertexFactory.ush", true, true, true, false, false);

/**
* Initialize the RHI for this rendering resource
*/
void FLandscapeVertexBufferMobile::InitRHI()
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo;
	void* VertexDataPtr = nullptr;
	VertexBufferRHI = RHICreateAndLockVertexBuffer(VertexData.Num(), BUF_Static, CreateInfo, VertexDataPtr);

	// Copy stored platform data and free CPU copy
	FMemory::Memcpy(VertexDataPtr, VertexData.GetData(), VertexData.Num());
	VertexData.Empty();

	RHIUnlockVertexBuffer(VertexBufferRHI);
}

/**
 * Container for FLandscapeVertexBufferMobile that we can reference from a thread-safe shared pointer
 * while ensuring the vertex buffer is always destroyed on the render thread.
 **/
struct FLandscapeMobileRenderData
{
	FLandscapeVertexBufferMobile* VertexBuffer;

	FLandscapeMobileRenderData(TArray<uint8> InVertexData)
	:	VertexBuffer(new FLandscapeVertexBufferMobile(MoveTemp(InVertexData)))
	{}

	~FLandscapeMobileRenderData()
	{
		// Make sure the vertex buffer is always destroyed from the render thread 
		if (VertexBuffer != nullptr)
		{
			if (IsInRenderingThread())
			{
				delete VertexBuffer;
			}
			else
			{
				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
					InitCommand,
					FLandscapeVertexBufferMobile*, VertexBuffer, VertexBuffer,
					{
						delete VertexBuffer;
					});
			}
		}
	}
};

FLandscapeComponentSceneProxyMobile::FLandscapeComponentSceneProxyMobile(ULandscapeComponent* InComponent)
	: FLandscapeComponentSceneProxy(InComponent, {InComponent->MobileMaterialInterface})
	, MobileRenderData(InComponent->PlatformData.GetRenderData())
{
	check(InComponent);
	
	check(InComponent->MobileMaterialInterface);
	check(InComponent->MobileWeightNormalmapTexture);

	WeightmapTextures.Empty(1);
	WeightmapTextures.Add(InComponent->MobileWeightNormalmapTexture);
	NormalmapTexture = InComponent->MobileWeightNormalmapTexture;

	BlendableLayerMask = InComponent->MobileBlendableLayerMask;
}

FLandscapeComponentSceneProxyMobile::~FLandscapeComponentSceneProxyMobile()
{
	if (VertexFactory)
	{
		delete VertexFactory;
		VertexFactory = NULL;
	}
}

void FLandscapeComponentSceneProxyMobile::CreateRenderThreadResources()
{
	// Use only Index buffers
	SharedBuffers = FLandscapeComponentSceneProxy::SharedBuffersMap.FindRef(SharedBuffersKey);
	if (SharedBuffers == nullptr)
	{
		SharedBuffers = new FLandscapeSharedBuffers(
			SharedBuffersKey, SubsectionSizeQuads, NumSubsections,
			GetScene().GetFeatureLevel(), false);

		FLandscapeComponentSceneProxy::SharedBuffersMap.Add(SharedBuffersKey, SharedBuffers);
	}

	SharedBuffers->AddRef();

	// Init vertex buffer
	check(MobileRenderData->VertexBuffer);
	MobileRenderData->VertexBuffer->InitResource();

	FLandscapeVertexFactoryMobile* LandscapeVertexFactory = new FLandscapeVertexFactoryMobile();
	LandscapeVertexFactory->MobileData.PositionComponent = FVertexStreamComponent(MobileRenderData->VertexBuffer, STRUCT_OFFSET(FLandscapeMobileVertex,Position), sizeof(FLandscapeMobileVertex), VET_UByte4N);
	for( uint32 Index = 0; Index < LANDSCAPE_MAX_ES_LOD_COMP; ++Index )
	{
		LandscapeVertexFactory->MobileData.LODHeightsComponent.Add
			(FVertexStreamComponent(MobileRenderData->VertexBuffer, STRUCT_OFFSET(FLandscapeMobileVertex,LODHeights) + sizeof(uint8) * 4 * Index, sizeof(FLandscapeMobileVertex), VET_UByte4N));
	}

	LandscapeVertexFactory->InitResource();
	VertexFactory = LandscapeVertexFactory;

	// Assign LandscapeUniformShaderParameters
	LandscapeUniformShaderParameters.InitResource();
}

TSharedPtr<FLandscapeMobileRenderData, ESPMode::ThreadSafe> FLandscapeComponentDerivedData::GetRenderData()
{
	check(IsInGameThread());

	if (FPlatformProperties::RequiresCookedData() && CachedRenderData.IsValid())
	{
		// on device we can re-use the cached data if we are re-registering our component.
		return CachedRenderData;
	}
	else
	{
		check(CompressedLandscapeData.Num() > 0);

		FMemoryReader Ar(CompressedLandscapeData);

		// Note: change LANDSCAPE_FULL_DERIVEDDATA_VER when modifying the serialization layout
		int32 UncompressedSize;
		Ar << UncompressedSize;

		int32 CompressedSize;
		Ar << CompressedSize;

		TArray<uint8> CompressedData;
		CompressedData.Empty(CompressedSize);
		CompressedData.AddUninitialized(CompressedSize);
		Ar.Serialize(CompressedData.GetData(), CompressedSize);

		TArray<uint8> UncompressedData;
		UncompressedData.Empty(UncompressedSize);
		UncompressedData.AddUninitialized(UncompressedSize);

		verify(FCompression::UncompressMemory((ECompressionFlags)COMPRESS_ZLIB, UncompressedData.GetData(), UncompressedSize, CompressedData.GetData(), CompressedSize));

		TSharedPtr<FLandscapeMobileRenderData, ESPMode::ThreadSafe> RenderData = MakeShareable(new FLandscapeMobileRenderData(MoveTemp(UncompressedData)));

		// if running on device		
		if (FPlatformProperties::RequiresCookedData())
		{
			// free the compressed data now that we have used it to create the render data.
			CompressedLandscapeData.Empty();
			// store a reference to the render data so we can use it again should the component be reregistered.
			CachedRenderData = RenderData;
		}

		return RenderData;
	}
}