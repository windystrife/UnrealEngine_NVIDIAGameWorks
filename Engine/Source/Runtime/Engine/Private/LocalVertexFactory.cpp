// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LocalVertexFactory.cpp: Local vertex factory implementation
=============================================================================*/

#include "LocalVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "SpeedTreeWind.h"
#include "ShaderParameterUtils.h"
#include "Rendering/ColorVertexBuffer.h"

class FSpeedTreeWindNullUniformBuffer : public TUniformBuffer<FSpeedTreeUniformParameters>
{
	typedef TUniformBuffer< FSpeedTreeUniformParameters > Super;
public:
	virtual void InitDynamicRHI() override;
};

void FSpeedTreeWindNullUniformBuffer::InitDynamicRHI()
{
	FSpeedTreeUniformParameters Parameters;
	FMemory::Memzero(Parameters);
	SetContentsNoUpdate(Parameters);
	
	Super::InitDynamicRHI();
}

static TGlobalResource< FSpeedTreeWindNullUniformBuffer > GSpeedTreeWindNullUniformBuffer;

void FLocalVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	LODParameter.Bind(ParameterMap, TEXT("SpeedTreeLODInfo"));
	bAnySpeedTreeParamIsBound = LODParameter.IsBound() || ParameterMap.ContainsParameterAllocation(TEXT("SpeedTreeData"));
}

void FLocalVertexFactoryShaderParameters::Serialize(FArchive& Ar)
{
	Ar << LODParameter << bAnySpeedTreeParamIsBound;
}

void FLocalVertexFactoryShaderParameters::SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const
{
	if (BatchElement.bUserDataIsColorVertexBuffer)
	{
		FColorVertexBuffer* OverrideColorVertexBuffer = (FColorVertexBuffer*)BatchElement.UserData;
		check(OverrideColorVertexBuffer);
		static_cast<const FLocalVertexFactory*>(VertexFactory)->SetColorOverrideStream(RHICmdList, OverrideColorVertexBuffer);
	}
	if (bAnySpeedTreeParamIsBound && View.Family != NULL && View.Family->Scene != NULL)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FLocalVertexFactoryShaderParameters_SetMesh_SpeedTree);
		FUniformBufferRHIParamRef SpeedTreeUniformBuffer = View.Family->Scene->GetSpeedTreeUniformBuffer(VertexFactory);
		if (SpeedTreeUniformBuffer == NULL)
		{
			SpeedTreeUniformBuffer = GSpeedTreeWindNullUniformBuffer.GetUniformBufferRHI();
		}
		check(SpeedTreeUniformBuffer != NULL);

		SetUniformBufferParameter(RHICmdList, Shader->GetVertexShader(), Shader->GetUniformBufferParameter<FSpeedTreeUniformParameters>(), SpeedTreeUniformBuffer);

		if (LODParameter.IsBound())
		{
			FVector LODData(BatchElement.MinScreenSize, BatchElement.MaxScreenSize, BatchElement.MaxScreenSize - BatchElement.MinScreenSize);
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), LODParameter, LODData);
		}
	}
}

/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
bool FLocalVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return true; 
}

void FLocalVertexFactory::SetData(const FDataType& InData)
{
	check(IsInRenderingThread());

	// The shader code makes assumptions that the color component is a FColor, performing swizzles on ES2 and Metal platforms as necessary
	// If the color is sent down as anything other than VET_Color then you'll get an undesired swizzle on those platforms
	check((InData.ColorComponent.Type == VET_None) || (InData.ColorComponent.Type == VET_Color));

	Data = InData;
	UpdateRHI();
}

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FLocalVertexFactory::Copy(const FLocalVertexFactory& Other)
{
	FLocalVertexFactory* VertexFactory = this;
	const FDataType* DataCopy = &Other.Data;
	ENQUEUE_RENDER_COMMAND(FLocalVertexFactoryCopyData)(
		[VertexFactory, DataCopy](FRHICommandListImmediate& RHICmdList)
		{
			VertexFactory->Data = *DataCopy;
		});
	BeginUpdateResourceRHI(this);
}

void FLocalVertexFactory::InitRHI()
{
	// If the vertex buffer containing position is not the same vertex buffer containing the rest of the data,
	// then initialize PositionStream and PositionDeclaration.
	if(Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
	{
		FVertexDeclarationElementList PositionOnlyStreamElements;
		AddVertexPositionElements(Data, PositionOnlyStreamElements);		
		InitPositionDeclaration(PositionOnlyStreamElements);
	}

	FVertexDeclarationElementList Elements;
	AddVertexElements(Data, Elements);

	check(Streams.Num() > 0);

	InitDeclaration(Elements);

	check(IsValidRef(GetDeclaration()));
}

void FLocalVertexFactory::AddVertexPositionElements(FDataType& InData, FVertexDeclarationElementList& Elements)
{
	Elements.Add(AccessPositionStreamComponent(InData.PositionComponent,0));
}

void FLocalVertexFactory::AddVertexElements(FDataType& InData, FVertexDeclarationElementList& Elements)
{
	if(Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(InData.PositionComponent,0));
	}

	// only tangent,normal are used by the stream. the binormal is derived in the shader
	uint8 TangentBasisAttributes[2] = { 1, 2 };
	for(int32 AxisIndex = 0;AxisIndex < 2;AxisIndex++)
	{
		if(InData.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(InData.TangentBasisComponents[AxisIndex],TangentBasisAttributes[AxisIndex]));
		}
	}

	if(InData.ColorComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(InData.ColorComponent,3));
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
		Elements.Add(AccessStreamComponent(NullColorComponent,3));
	}
	ColorStreamIndex = Elements.Last().StreamIndex;

	if(InData.TextureCoordinates.Num())
	{
		const int32 BaseTexCoordAttribute = 4;
		for(int32 CoordinateIndex = 0;CoordinateIndex < InData.TextureCoordinates.Num();CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				InData.TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
				));
		}

		for (int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < MAX_STATIC_TEXCOORDS / 2; CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
				));
		}
	}

	if(Data.LightMapCoordinateComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.LightMapCoordinateComponent,15));
	}
	else if(Data.TextureCoordinates.Num())
	{
		Elements.Add(AccessStreamComponent(Data.TextureCoordinates[0],15));
	}

	/*
	check(Streams.Num() > 0);

	InitDeclaration(Elements);

	check(IsValidRef(GetDeclaration()));
	*/
}

FVertexFactoryShaderParameters* FLocalVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	if (ShaderFrequency == SF_Vertex)
	{
		return new FLocalVertexFactoryShaderParameters();
	}

	return NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FLocalVertexFactory,"/Engine/Private/LocalVertexFactory.ush",true,true,true,true,true);
