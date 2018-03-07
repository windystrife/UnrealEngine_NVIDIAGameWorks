// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRenderer.h: Base class for Niagara render modules
==============================================================================*/
#pragma once

#include "NiagaraMeshVertexFactory.h"
#include "NiagaraRenderer.h"

class FNiagaraDataSet;



struct FNiagaraDynamicDataMesh : public FNiagaraDynamicDataBase
{
	const FNiagaraDataSet *DataSet;
	int32 PositionDataOffset;
	int32 VelocityDataOffset;
	int32 ColorDataOffset;
	int32 TransformDataOffset;
	int32 ScaleDataOffset;
	int32 SizeDataOffset;
	int32 MaterialParamDataOffset;
};



/**
* NiagaraRendererSprites renders an FNiagaraEmitterInstance as sprite particles
*/
class NIAGARA_API NiagaraRendererMeshes : public NiagaraRenderer
{
public:

	explicit NiagaraRendererMeshes(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *Props);
	~NiagaraRendererMeshes()
	{
		ReleaseRenderThreadResources();
	}


	virtual void ReleaseRenderThreadResources() override;

	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;
	virtual bool SetMaterialUsage() override;
	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target) override;

	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)
	{
		FPrimitiveViewRelevance Result;
		bool bHasDynamicData = HasDynamicData();
		Result.bDrawRelevance = bHasDynamicData && SceneProxy->IsShown(View) && View->Family->EngineShowFlags.Particles;
		Result.bShadowRelevance = bHasDynamicData && SceneProxy->IsShadowCast(View);
		Result.bDynamicRelevance = bHasDynamicData;

		if (bHasDynamicData)
		{
			Result.bOpaqueRelevance = MaterialRelevance.bOpaque;
			Result.bNormalTranslucencyRelevance = MaterialRelevance.bNormalTranslucency;
			Result.bSeparateTranslucencyRelevance = MaterialRelevance.bSeparateTranslucency;
			Result.bDistortionRelevance = MaterialRelevance.bDistortion;
		}

		return Result;
	}




	UClass *GetPropertiesClass() override { return UNiagaraMeshRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraRendererProperties *Props) override { Properties = Cast<UNiagaraMeshRendererProperties>(Props); }
#if WITH_EDITORONLY_DATA
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif
	void SetupVertexFactory(FNiagaraMeshVertexFactory *InVertexFactory, const FStaticMeshLODResources& LODResources) const;

private:
	UNiagaraMeshRendererProperties *Properties;
	mutable TUniformBuffer<FPrimitiveUniformShaderParameters> WorldSpacePrimitiveUniformBuffer;
	class FNiagaraMeshVertexFactory* VertexFactory;
};
