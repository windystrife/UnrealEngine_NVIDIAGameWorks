// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LandscapeRenderMobile.h: Mobile landscape rendering
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RenderResource.h"
#include "VertexFactory.h"
#include "LandscapeRender.h"
#include "Runtime/Landscape/Private/LandscapePrivate.h"

#define LANDSCAPE_MAX_ES_LOD_COMP	2
#define LANDSCAPE_MAX_ES_LOD		6

struct FLandscapeMobileVertex
{
	uint8 Position[4]; // Pos + LOD 0 Height
	uint8 LODHeights[LANDSCAPE_MAX_ES_LOD_COMP*4];
};

/** vertex factory for VTF-heightmap terrain  */
class FLandscapeVertexFactoryMobile : public FLandscapeVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeVertexFactoryMobile);

	typedef FLandscapeVertexFactory Super;
public:

	struct FDataType : FLandscapeVertexFactory::FDataType
	{
		/** stream which has heights of each LOD levels */
		TArray<FVertexStreamComponent,TFixedAllocator<LANDSCAPE_MAX_ES_LOD_COMP> > LODHeightsComponent;
	};

	FLandscapeVertexFactoryMobile()
	{
	}

	virtual ~FLandscapeVertexFactoryMobile()
	{
		ReleaseResource();
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	/**
	* Should we cache the material's shadertype on this platform with this vertex factory? 
	*/
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		auto FeatureLevel = GetMaxSupportedFeatureLevel(Platform);
		return (FeatureLevel == ERHIFeatureLevel::ES2 || FeatureLevel == ERHIFeatureLevel::ES3_1) &&
			(Material->IsUsedWithLandscape() || Material->IsSpecialEngineMaterial());
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		FVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_VF_PACKED_INTERPOLANTS"), TEXT("1"));
	}

	// FRenderResource interface.
	virtual void InitRHI() override;

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FDataType& InData)
	{
		MobileData = InData;
		UpdateRHI();
	}

private:
	/** stream component data bound to this vertex factory */
	FDataType MobileData; 

	friend class FLandscapeComponentSceneProxyMobile;
};

//
// FLandscapeVertexBuffer
//
class FLandscapeVertexBufferMobile : public FVertexBuffer
{
	TArray<uint8> VertexData;
	int32 DataSize;
public:

	/** Constructor. */
	FLandscapeVertexBufferMobile(TArray<uint8> InVertexData)
	:	VertexData(InVertexData)
	,	DataSize(InVertexData.Num())
	{
		INC_DWORD_STAT_BY(STAT_LandscapeVertexMem, DataSize);
	}

	/** Destructor. */
	virtual ~FLandscapeVertexBufferMobile()
	{
		ReleaseResource();
		DEC_DWORD_STAT_BY(STAT_LandscapeVertexMem, DataSize);
	}

	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI() override;
};

//
// FLandscapeComponentSceneProxy
//
class FLandscapeComponentSceneProxyMobile : public FLandscapeComponentSceneProxy
{
	TSharedPtr<FLandscapeMobileRenderData, ESPMode::ThreadSafe> MobileRenderData;

	virtual ~FLandscapeComponentSceneProxyMobile();

public:
	FLandscapeComponentSceneProxyMobile(ULandscapeComponent* InComponent);

	virtual void CreateRenderThreadResources() override;

	uint8 BlendableLayerMask;

	friend class FLandscapeVertexBufferMobile;
};
