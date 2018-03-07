// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRenderer.h: Base class for Niagara render modules
==============================================================================*/
#pragma once

#include "NiagaraRibbonVertexFactory.h"
#include "NiagaraRenderer.h"

class FNiagaraDataSet;

/**
* NiagaraRendererRibbons renders an FNiagaraEmitterInstance as a ribbon connecting all particles
* in order by particle age.
*/
class NIAGARA_API NiagaraRendererRibbons : public NiagaraRenderer
{
public:
	NiagaraRendererRibbons(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *Props);
	~NiagaraRendererRibbons()
	{
		ReleaseRenderThreadResources();
	}

	virtual void ReleaseRenderThreadResources() override;
	virtual void CreateRenderThreadResources() override;


	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;

	virtual bool SetMaterialUsage() override;

	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target) override;

	void AddRibbonVert(TArray<FNiagaraRibbonVertex>& RenderData, FVector ParticlePos, FVector2D UV1,
		const FLinearColor &Color, const float Age, const float Rotation, const FVector2D &Size)
	{
		FNiagaraRibbonVertex& NewVertex = *new(RenderData)FNiagaraRibbonVertex;
		NewVertex.Position = ParticlePos;
		NewVertex.OldPosition = NewVertex.Position;
		NewVertex.Color = Color;
		NewVertex.ParticleId = RenderData.Num();
		NewVertex.RelativeTime = Age;
		NewVertex.Size = Size;
		NewVertex.Rotation = Rotation;
		NewVertex.SubImageIndex = 0.f;
		NewVertex.Tex_U = UV1.X;
		NewVertex.Tex_V = UV1.Y;
		NewVertex.Tex_U2 = UV1.X;
		NewVertex.Tex_V2 = UV1.Y;
	}



	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

#if WITH_EDITORONLY_DATA
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif

	UClass *GetPropertiesClass() override { return UNiagaraRibbonRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraRendererProperties *Props) override { Properties = Cast<UNiagaraRibbonRendererProperties>(Props); }

private:
	class FNiagaraRibbonVertexFactory *VertexFactory;
	UNiagaraRibbonRendererProperties *Properties;
	mutable TUniformBuffer<FPrimitiveUniformShaderParameters> WorldSpacePrimitiveUniformBuffer;
};


