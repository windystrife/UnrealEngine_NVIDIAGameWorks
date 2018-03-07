// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Components.h"
#include "VertexFactory.h"

class FMaterial;
class FSceneView;
struct FMeshBatchElement;

/*=============================================================================
	LocalVertexFactory.h: Local vertex factory definitions.
=============================================================================*/

/**
 * A vertex factory which simply transforms explicit vertex attributes from local to world space.
 */
class ENGINE_API FLocalVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLocalVertexFactory);
public:

	FLocalVertexFactory()
		: ColorStreamIndex(-1)
	{
	}

	struct FDataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		TArray<FVertexStreamComponent,TFixedAllocator<MAX_STATIC_TEXCOORDS/2> > TextureCoordinates;

		/** The stream to read the shadow map texture coordinates from. */
		FVertexStreamComponent LightMapCoordinateComponent;

		/** The stream to read the vertex color from. */
		FVertexStreamComponent ColorComponent;
	};

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_SPEEDTREE_WIND"),TEXT("1"));
	}

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FDataType& InData);

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FLocalVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;

	static bool SupportsTessellationShaders() { return true; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	FORCEINLINE_DEBUGGABLE void SetColorOverrideStream(FRHICommandList& RHICmdList, const FVertexBuffer* ColorVertexBuffer) const
	{
		checkf(ColorVertexBuffer->IsInitialized(), TEXT("Color Vertex buffer was not initialized! Name %s"), *ColorVertexBuffer->GetFriendlyName());
		checkf(IsInitialized() && Data.ColorComponent.bSetByVertexFactoryInSetMesh && ColorStreamIndex > 0, TEXT("Per-mesh colors with bad stream setup! Name %s"), *ColorVertexBuffer->GetFriendlyName());
		RHICmdList.SetStreamSource(ColorStreamIndex, ColorVertexBuffer->VertexBufferRHI, 0);
	}

protected:

	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	virtual void AddVertexElements(FDataType& InData, FVertexDeclarationElementList& OutElements);

	/**
	* Add the decl elements for the position only streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/	
	virtual void AddVertexPositionElements(FDataType& InData, FVertexDeclarationElementList& OutElements);

	FDataType Data;
	int32 ColorStreamIndex;

	const FDataType& GetData() const { return Data; }
};

/**
 * Shader parameters for LocalVertexFactory.
 */
class FLocalVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override;

	FLocalVertexFactoryShaderParameters()
		: bAnySpeedTreeParamIsBound(false)
	{
	}

	// True if either of the two below is bound, which puts us on the slow path in SetMesh
	bool bAnySpeedTreeParamIsBound;
	// SpeedTree LOD parameter
	FShaderParameter LODParameter;
};
